#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <memory>
#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#include <fstream>
#include <limits.h>
#include <termios.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <cstring>
#include <stdexcept>

namespace CLI
{
    std::vector<std::string> _ParseArguments(const std::string& args) {
        std::vector<std::string> argList;
        std::istringstream iss(args);
        std::string arg;

        while (iss >> arg) {
            argList.push_back(arg);
        }

        return argList;
    }

    std::string RunCommand(const char* cmd, const char* args = nullptr) {
        int pipefd[2];
        pid_t pid;
        char buf;
        std::string output;

        if (pipe(pipefd) == -1) {
            throw std::system_error(errno, std::system_category(), "Failed to create pipe");
        }

        pid = fork();
        if (pid == -1) {
            throw std::system_error(errno, std::system_category(), "Failed to fork");
        }

        if (pid == 0) { // Child process
            close(pipefd[0]); // Close unused read end
            dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
            close(pipefd[1]); // Close write end

            // Parse args and prepare for execvp execpt from /bin/bash
            if (strcmp(cmd, "/bin/bash") != 0) {
                std::vector<std::string> argList = _ParseArguments(args ? args : "");

                // Convert to char* array for execvp
                std::vector<char*> argv;
                argv.push_back(const_cast<char*>(cmd));
                for (auto& a : argList) {
                    argv.push_back(&a[0]);
                }
                argv.push_back(nullptr);

                execvp(cmd, argv.data());
            }
            else {
                execl(cmd, cmd, args, nullptr);
            }
            // execvp only returns on error
            _exit(EXIT_FAILURE);
        }
        else { // Parent process
            close(pipefd[1]); // Close unused write end

            while (read(pipefd[0], &buf, 1) > 0) {
                output += buf;
            }

            close(pipefd[0]); // Close read end
            wait(nullptr); // Wait for child process
        }

        return output;
    }

    // Function to set the terminal into raw mode
    void _SetRawMode(int fd, struct termios* original) {
        struct termios raw;

        // Get the current terminal settings
        tcgetattr(fd, original);

        // Start with the current settings
        raw = *original;

        // Disable canonical mode, input echoing, and other settings
        raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_cflag &= ~(CSIZE | PARENB);
        raw.c_cflag |= CS8;
        raw.c_oflag &= ~(OPOST);

        // Set the terminal attributes
        tcsetattr(fd, TCSAFLUSH, &raw);
    }

    int RunInteractiveCommand(const char* cmd, const char* arg = nullptr) {
        pid_t pid;
        int master_fd;

        // Set the terminal to raw mode
        struct termios original;
        _SetRawMode(STDIN_FILENO, &original);

        // Create a pseudo-terminal
        pid = forkpty(&master_fd, NULL, NULL, NULL);

        if (pid < 0) {
            std::ostringstream msg;
            msg << "Failed to fork at line " << __LINE__ << " in function " << __FILE__;
            throw std::system_error(errno, std::system_category(), msg.str());
        }

        if (pid == 0) { // Child process
            // Parse args and prepare for execvp
            std::vector<std::string> argList = _ParseArguments(arg ? arg : "");

            // Convert to char* array for execvp
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(cmd));
            for (auto& a : argList) {
                argv.push_back(&a[0]);
            }
            argv.push_back(nullptr);

            execvp(cmd, argv.data());
            // execvp only returns on error
            std::ostringstream msg;
            msg << "Failed to execute command " << cmd << " " << arg <<
                " at line " << __LINE__ << " in function " << __FILE__;
            throw std::system_error(errno, std::system_category(), msg.str());
            exit(EXIT_FAILURE);
        }
        else { // Parent process
            char buffer[256];
            ssize_t bytes_read;
            fd_set read_fds;
            int max_fd = std::max(STDIN_FILENO, master_fd) + 1;

            while (true) {
                FD_ZERO(&read_fds);
                FD_SET(STDIN_FILENO, &read_fds);
                FD_SET(master_fd, &read_fds);

                int activity = select(max_fd, &read_fds, NULL, NULL, NULL);

                if (activity < 0 && errno != EINTR) {
                    std::cerr << "Select error\n";
                    break;
                }

                if (FD_ISSET(master_fd, &read_fds)) { // Data from the child process
                    bytes_read = read(master_fd, buffer, sizeof(buffer) - 1);
                    if (bytes_read <= 0) {
                        break; // Child process has terminated or an error occurred
                    }
                    buffer[bytes_read] = '\0';
                    std::cout << buffer << std::flush;
                }

                if (FD_ISSET(STDIN_FILENO, &read_fds)) { // Data from the user
                    bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        write(master_fd, buffer, bytes_read); // Send user input to the child process
                    }
                }
            }

            // Wait for the child process to finish
            int status;
            waitpid(pid, &status, 0);
            close(master_fd);
            // Restore the terminal settings
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
            return WEXITSTATUS(status);
        }
    }

    std::string _GetExeDir() {
        char result[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
        std::string filePath(result, (count > 0) ? count : 0);
        size_t found = filePath.find_last_of("/\\");
        return (found != std::string::npos) ? filePath.substr(0, found) : "";
    }


    void WriteToFile(const std::string& filePath, const std::string& content) {
        std::ofstream fileStream(filePath, std::ios::out | std::ios::trunc);

        if (!fileStream) {
            throw std::runtime_error("Failed to open file: " + filePath);
        }

        fileStream << content;

        if (!fileStream) {
            throw std::runtime_error("Failed to write to file: " + filePath);
        }
    }


    bool _IsStepLine(const std::string& line) {
        if (line.rfind("#", 0) != 0) {
            return false;
        }
        std::stringstream ss(line.substr(1)); // Exclude '#'
        int num;
        char dot;
        ss >> num >> dot;
        return ss && dot == '.' && num > 0; // Check if it's a step line
    }

    std::vector<std::vector<std::string>> ParseCommands(int stepNumber) {
        std::string filePath = _GetExeDir() + "/Commands";
        std::vector<std::vector<std::string>> commands;
        std::ifstream file(filePath);
        std::string line;
        bool startCapture = false;

        if (file.is_open()) {
            while (std::getline(file, line)) {
                if (line.rfind("#", 0) == 0) {
                    if (_IsStepLine(line)) {
                        if (startCapture) {
                            // If it's another step line, stop capturing
                            break;
                        }
                        else if (line.find("# " + std::to_string(stepNumber) + ".") == 0) {
                            // If it's the starting step line, start capturing
                            startCapture = true;
                            continue;
                        }
                    }
                    // Ignore other comments
                    continue;
                }

                if (startCapture && !line.empty()) {
                    std::istringstream iss(line);
                    std::string command, rest;
                    iss >> command;
                    getline(iss, rest);
                    if (!rest.empty() && rest[0] == ' ') {
                        rest.erase(0, 1); // Remove the leading space
                    }
                    commands.push_back({ command, rest });
                }
            }
            file.close();
        }
        else {
            std::cerr << "Unable to open file: " << filePath << std::endl;
        }

        return commands;
    }

    std::string ExtractDiskOrPartitionName(const std::string& line) {
        // Check if the line starts with '|', '`'
        if (line[0] == '|' || line[0] == '`') {
            // For partitions, remove the leading '|-' or '`-'
            size_t start = line.find_first_not_of("|`-");
            size_t end = line.find(' ', start); // Find the end of the partition name
            if (end == std::string::npos) {
                end = line.length(); // If there's no space, use the length of the line
            }
            return line.substr(start, end - start);
        }
        else {
            // It's a disk name; extract the first word
            std::istringstream iss(line);
            std::string diskName;
            iss >> diskName;
            return diskName;
        }
    }
} // namespace CLI
