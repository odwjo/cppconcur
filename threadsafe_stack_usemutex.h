#ifndef THREADSAFE_STACK_USEMUTEX_H
#define THREADSAFE_STACK_USEMUTEX_H
#include <mutex>
#include <stack>
#include <exception>
//an outline class definition for a thread-safe stack
struct empty_stack:std::exception{
    const char* what() const throw();
};

const char* empty_stack::what()const throw(){
    return "empty stack.^.";
}


template<typename T>
class threadsafe_stack{
    mutable std::mutex m;
    std::stack<T> data;
public:
    threadsafe_stack(){}
    threadsafe_stack(const threadsafe_stack& other){
        std::lock_guard<std::mutex> lock(other.m);
        data = other.data;
    }

    threadsafe_stack& operator=(const threadsafe_stack&)=delete;

    void push(T&& new_value){
        std::lock_guard<std::mutex> lock(m);
        data.push(std::forward<T>(new_value));
    }

    std::shared_ptr<T> pop(){
        std::lock_guard<std::mutex> lock(m);
        if(data.empty()) //throw empty_stack();
            return std::shared_ptr<T>();
        std::shared_ptr<T> const res(std::make_shared<T>(std::move(data.top())));
        data.pop();
        return res;
    }

    void pop(T &value){
        std::lock_guard<std::mutex> lock(m);
        if(data.empty()){ //throw empty_stack();
            value = std::move(T());
            return ;
        }
        value = std::move(data.top());
        data.pop();
    }

    bool empty() const{
        std::lock_guard<std::mutex> lock(m);
        return data.empty();
    }
};

#endif // THREADSAFE_STACK_USEMUTEX_H
