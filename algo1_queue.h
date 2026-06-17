#pragma once
// algo1_queue.h -- Algorithm 1: Linked-List Queue
// Singly-linked list pe bana hua generic FIFO queue -- ye aaise hota hai linked list

template<typename T>
struct Node {
    T     data;
    Node* next;
};

template<typename T>
class Queue {
    Node<T>* fp = nullptr;  // front pointer -- yahan se nikalte hain
    Node<T>* rp = nullptr;  // rear pointer -- yahan pe daalte hain
public:
    // destructor -- saari memory free karo warna leak ho jaayegi
    ~Queue() { while (!isEmpty()) { delete dequeue(); } }

    // queue mein daalo -- naya node rear pe attach hota hai
    void enqueue(const T& item) {
        Node<T>* n = new Node<T>{item, nullptr};
        if (isEmpty()) fp = rp = n;
        else { rp->next = n; rp = n; }
    }

    // queue se nikalo -- front se nikaalo aur return karo
    T* dequeue() {
        if (isEmpty()) return nullptr;
        Node<T>* tmp = fp;
        T* d = new T(fp->data);
        fp = fp->next;
        if (!fp) rp = nullptr;
        delete tmp;
        return d;
    }

    // front element dekho bina nikale
    T* front() { return isEmpty() ? nullptr : &fp->data; }

    bool isEmpty() const { return fp == nullptr; }

    // cur ke baad waala element return karo -- traverse ke liye
    T* next(T* cur) {
        for (Node<T>* n = fp; n; n = n->next)
            if (&n->data == cur && n->next) return &n->next->data;
        return nullptr;
    }
};
