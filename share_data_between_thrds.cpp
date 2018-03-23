#include <list>
#include <mutex>
#include <algorithm>
#include <exception>
#include <memory>
#include <stack>
#include <thread>
#include <functional>//std::mem_fn
#include <iostream>
#include <climits> //ULONG_MAX

std::list<int> ilist;
std::mutex some_mutex;

//just some demo
/*
void add_to_list(int new_v){
    std::lock_guard<std::mutex> guard(some_mutex);
    some_list.push_back(new_v);
}

bool list_contains(int value_to_find){
    std::lock_guard<std::mutex> guard(some_mutex);
    return std::find(ilist.begin(),ilist.end(),value_to_find)
                     != ilist.end();
}*/


//common error
class some_data{
    int a;
    std::string b;
public:
    void do_something();
};
class data_wrapper{
    some_data data;
    std::mutex m;
public:
    template<typename Function>
    void process_data(Function func){
        std::lock_guard<std::mutex> l(m);
        func(data);
    }
};
some_data *unprotected;
void malicious_function(some_data &protected_data){
    unprotected = &protected_data;
}
data_wrapper x;
void foo(){
    x.process_data(malicious_function);
    //unprotected->do_something();
}

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

    void push(T new_value){
        std::lock_guard<std::mutex> lock(m);
        data.push(new_value);
    }

    std::shared_ptr<T> pop(){
        std::lock_guard<std::mutex> lock(m);
        if(data.empty()) throw empty_stack();
        std::shared_ptr<T> const res(std::make_shared<T>(data.top()));
        data.pop();
        return res;
    }

    void pop(T &value){
        std::lock_guard<std::mutex> lock(m);
        if(data.empty()) throw empty_stack();
        value = data.top();
        data.pop();
    }

    bool empty() const{
        std::lock_guard<std::mutex> lock(m);
        return data.empty();
    }
};

template<typename T>
void popshow(T &t){
    auto r = t.pop();
    std::cout << *r << std::endl;
}
//template void popshow(threadsafe_stack<int> &);

void use_thrdsafe_stack(){
    threadsafe_stack<int> tss;
    std::vector<std::thread> vth(5);
    for(size_t i = 0;i != 5;++ i){
        tss.push(i);
    }
    for(size_t i = 0;i != 5;++ i){
        vth[i] = std::thread(popshow<threadsafe_stack<int> &>,std::ref(tss));
    }
    std::for_each(vth.begin(),vth.end(),std::mem_fn(&std::thread::join));

}

//using std::lock() and std::lock_guard in a swap operation
void swap(some_big_object &lhs, some_big_object& rhs);
class X{
private:
    some_big_object some_detail;
    std::mutex m;
public:
    X(some_big_object const& sd):some_detail(sd){}

    friend void swap(X& lhs, X& rhs){
        if(&lhs == &rhs)
            return ;
        std::lock(lhs.m,rhs.m);
        //std::adopt_lock indicate that the mutex is locked already
        std::lock_guard<std::mutex> lock_a(lhs.m, std::adopt_lock);
        std::lock_guard<std::mutex> lock_b(rhs.m, std::adopt_lock);
        swap(lhs.some_detail, rhs.some_detail);
    }

};
//hierarchical_mutex
class hierarchical_mutex{
    std::mutex internal_mutex;
    unsigned long const hierarchy_value;
    unsigned long previous_hierarchy_value;
    static thread_local unsigned long this_thread_hierarchy_value;

    void chect_for_hierarchy_violation(){//in this thread,the new hierarchy_value should be smaller.
        if(this_thread_hierarchy_value < hierarchy_value){
            throw std::logic_error("mutex hierarchy violated");
        }
    }
    void update_hierarchy_value(){//after update,this_thread_hierarchy_value is seted.
        previous_hierarchy_value = this_thread_hierarchy_value;
        this_thread_hierarchy_value = hierarchy_value;
    }
public:
    explicit hierarchical_mutex(unsigned long value):
        hierarchy_value(value),
        previous_hierarchy_value(0){}
    void lock(){
        check_for_hierarchy_violation();
        internal_mutex.lock();
        update_hierarchy_value();
    }
    void unlock(){
        this_thread_hierarchy_value = previous_hierarchy_value;
        internal_mutex.unlock();
    }
    bool try_lock(){
        check_for_hierarchy_violation();
        if(!internal_mutex.try_lock())
            return false;
        update_hierarchy_value();
        return true;
    }
};
//begining,make sure any ULONG value is valid,so the first check will be good.
thread_local unsigned long hierarchical_mutex::this_thread_hierarchy_value(ULONG_MAX);
//so the key point is using a thread_local value to label the new value seted by
//the new hierarchy_mutex instance.