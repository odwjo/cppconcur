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
data_wrapper xdw;
void foo(){
    xdw.process_data(malicious_function);
    //unprotected->do_something();
}
/*
//an outline class definition for a thread-safe stack
struct empty_stack:std::exception{
    const char* what() const throw();
};

const char* empty_stack::what()const throw(){
    return "empty stack.^.";
}
*/


template<typename T>
void popshow(T &t){
    auto r = t.pop();
    std::cout << *r << std::endl;
}
//template void popshow(threadsafe_stack<int> &);
/*
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

}*/

//using std::lock() and std::lock_guard in a swap operation
/*void swap(some_big_object &lhs, some_big_object& rhs);
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

};*/
//hierarchical_mutex
//lock  : this_hv -> previous_hv,hv -> this_hv
//unlock: this_hv <- previous_hv
class hierarchical_mutex{
    std::mutex internal_mutex;
    unsigned long const hierarchy_value;
    unsigned long previous_hierarchy_value;
    static thread_local unsigned long this_thread_hierarchy_value;

    void check_for_hierarchy_violation(){//in this thread,the new hierarchy_value should be smaller.
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
hierarchical_mutex hm(10000);
hierarchical_mutex lm(500);

void high_rank(){
    std::lock_guard<hierarchical_mutex> hk(hm);
    std::cout << "In high rank." << std::endl;
}
void low_rank(){
    std::lock_guard<hierarchical_mutex> lk(lm);
    std::cout << "In high rank." << std::endl;
}

void usual(){
    high_rank();
    low_rank();
}
void unusual(){
    try{
        std::lock_guard<hierarchical_mutex> lk(lm);
        std::lock_guard<hierarchical_mutex> hk(hm);
    }catch(std::exception e){
        std::cout << e.what() << std::endl;
    }

}

void test_hierarchy_mutex(){
    usual();
    unusual();
}

//std::unique_lock<std::mutex> lock_a(lhs.m, std::defer_lock);
/*transfer ownership of a mutex
 *std::unique_lock<std::mutex> get_lock(){
 *    std::mutex sm;
 *    std::unique_lock<std::mutex> lk(sm);
 *    do_something();
 *    return lk;
 * }
 * void prodess_data(){
 *    std::unique_lock<std::mutex> lk(get_lock());
 *    do_something_else();
 * }
*/
//protecting shared data during initialization
/*
class some_resource{public:void do_something(){}};
std::shared_ptr<some_resource> resource_ptr;
std::mutex resource_mutex;
void foo0(){// race condition exists
    if(!resource_ptr){
        std::lock_guard<std::mutex> lk(resource_mutex);
        if(!resource_ptr){
            resource_ptr.reset(new some_resource);
        }
        resource_ptr.reset(new some_resource);
    }
    resource_ptr->do_something();
}

std::once_flag resource_flag;
void init_resource(){
    resource_ptr.reset(new some_resource);
}
void foo1(){
    std::call_once(resource_flag,init_resource);
    resource_ptr->do_something();
}
class X{
private:
    connection_info connection_details;
    connection_handle connection;
    std::once_flag connection_init_flag;

    void open_connection(){
        connection = connection_manager.open(connection_details);
    }
public:
    X(connection_info const& connection_details_):
        connection_details(connection_details_){}
    void send_data(data_packet const &data){
        std::call_once(connection_init_flag, &X::open_connection, this);
        connection.send_data(data);
    }
    data_packet receive_data(){
        std::call_once(connection_init_flag, &X::open_connection, this);
        return connection.receive_data();
    }
};

class some_class;
some_class& get_my_class_instance(){
    static my_class instance;//thread-safe
    return instance;
}
*/
//use std::recursive_mutex can be locked multitimes
//See one member-func want to call another member-func
//But it's quick-and-dirty, should extract a new private member function.
