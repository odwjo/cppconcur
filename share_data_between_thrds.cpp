#include <list>
#include <mutex>
#include <algorithm>

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

//
