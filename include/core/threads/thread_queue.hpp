#pragma once

#include <atomic>
#include <cstdint>

template <typename T>
class MultiThreadQueue
{
    struct Node
    {
        Node* next = nullptr;
        Node* prev = nullptr;

        T data{};
    };

    Node* _queue_front = nullptr;
    Node* _queue_back = nullptr;

    std::size_t length = 0;

    std::mutex queue_lock_mutex;

    Node* new_node(const T& data)
    {
        Node* node = new Node;
        node->data = data;
        return node;
    }

public:
    MultiThreadQueue() {}

    ~MultiThreadQueue()
    {
        // Delete the linked list.
        Node* curr = _queue_front;
        while (curr)
        {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
    }

    const T& Pop()
    {
        std::lock_guard<std::mutex> lock(queue_lock_mutex);

        if (length == 0)
            return T{}; // Dummy
        
        const T& data = _queue_front->data;
        if (length == 1)
        {
            delete _queue_back;
            _queue_back = nullptr;
            _queue_front = nullptr;
            length = 0;
            return data;
        }

        Node* next = _queue_front->next;
        delete _queue_front;
        _queue_front = next;

        return data;
    }


    void Push(const T& data)
    {
        std::lock_guard<std::mutex> lock(queue_lock_mutex);

        Node* node = new_node(data);

        // If this is set, then _queue_back is set too.
        if (_queue_front)
        {
            // Push to the back
            _queue_back->next = node;
            node->prev = _queue_back;
            _queue_back = node;
        }
        else
        {
            _queue_front = _queue_back = node;
        }

        length++;
    }
};
