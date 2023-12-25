#ifndef _KEYEVENT_H_
#define _KEYEVENT_H_

#include <functional>
#include <queue>
#include <memory>
#include <cstring>
#include <mutex>

class KeyEvent {
public:
    KeyEvent(char* key) {
        std::memcpy(m_Key, key, 10);
    }
    inline const char* GetKey() const { return m_Key; }
private:
    char m_Key[10];
};

class EventQueue {
public:
    EventQueue() = default;
    ~EventQueue() = default;

    static inline EventQueue& Get() {
        static EventQueue instance;
        return instance;
    }

    void Push(std::unique_ptr<KeyEvent> event) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_EventQueue.push(std::move(event));
    }

    std::unique_ptr<KeyEvent> Pop() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_EventQueue.empty()) return nullptr;
        std::unique_ptr<KeyEvent> event = std::move(m_EventQueue.front());
        m_EventQueue.pop();
        return event;
    }

    bool IsEmpty() const {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_EventQueue.empty();
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        while (!m_EventQueue.empty()) {
            m_EventQueue.pop();
        }
    }

private:
    mutable std::mutex m_Mutex;
    std::queue<std::unique_ptr<KeyEvent>> m_EventQueue;
};

#define EVENT_PUSH(event) EventQueue::Get().Push(event)
#define EVENT_POP() EventQueue::Get().Pop()
#define QUEUE_CLEAR() EventQueue::Get().Clear()

#endif /*_KEYEVENT_H_*/
