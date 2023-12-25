#ifndef INPUT_H_
#define INPUT_H_

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <functional>


class InputHandler {
public:
    InputHandler() = default;
    void Init() {
        // Save current terminal settings
        tcgetattr(STDIN_FILENO, &m_OrigTermios);

        // Set terminal to non-canonical mode
        _SetTerminalMode();

        // Create a pipe
        if (pipe(m_PipeFds) == -1) {
            
        }

        // Set read end of the pipe to non-blocking
        int flags = fcntl(m_PipeFds[0], F_GETFL, 0);
        fcntl(m_PipeFds[0], F_SETFL, flags | O_NONBLOCK);

        // Start the input thread
        m_InputThread = std::thread(&InputHandler::_InputThread, this);
    }

    ~InputHandler() {
        ResumeInputHandler();
        _Terminate();

        // Join the thread
        if (m_InputThread.joinable()) {
            m_InputThread.join();
        }

        _ClosePipe();
        _RestoreTerminal();
    }

    void _Terminate() {
        m_Mutex.lock();
        write(m_PipeFds[1], "exit", 4);
        m_Mutex.unlock();
    }

    void SetCallback(std::function<bool(char*)> callback) {
        m_Callback = callback;
    }

    void PauseInputHandler() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Paused = true;
        _RestoreTerminal();
    }

    void ResumeInputHandler() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Paused = false;
        _SetTerminalMode();
        m_CondVar.notify_one();
    }

private:
    void _InputThread() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                m_CondVar.wait(lock, [this]() { return !m_Paused; });
            }

            if (!_ProcessInput()) {
                break;
            }
        }
    }

    bool _ProcessInput() {
        fd_set read_fds;
        char buffer[10] = { 0 };
        int max_fd = std::max(STDIN_FILENO, m_PipeFds[0]) + 1;

        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(m_PipeFds[0], &read_fds);

        int activity = select(max_fd, &read_fds, nullptr, nullptr, nullptr);

        if (activity < 0) {
            perror("select");
            return false;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (count > 0 && m_Callback) {
                m_Callback(buffer);
                memset(buffer, 0, sizeof(buffer));
            }
        }

        if (FD_ISSET(m_PipeFds[0], &read_fds)) {
            return false;
        }

        return true;
    }

    void _SetTerminalMode() {
        struct termios new_termios = m_OrigTermios;
        new_termios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    }

    void _RestoreTerminal() {
        tcsetattr(STDIN_FILENO, TCSANOW, &m_OrigTermios);
    }

    void _ClosePipe() {
        close(m_PipeFds[0]);
        close(m_PipeFds[1]);
    }

private:
    int m_PipeFds[2];
    std::thread m_InputThread;
    std::mutex m_Mutex;
    std::condition_variable m_CondVar;
    struct termios m_OrigTermios;
    std::function<bool(char*)> m_Callback = nullptr;
    bool m_Paused = false;
};

#endif /*INPUT_H_*/