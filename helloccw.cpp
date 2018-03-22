#include <iostream>
#include <thread>
#include <exception>

class thread_guard{
    std::thread& t;
public:
    explicit thread_guard(std::thread& t_):t(t_){}
    ~thread_guard(){
        if(t.joinable()){
            t.join();
        }
    }
    thread_guard(const std::thread& t_)=delete;
    thread_guard& operator=(const std::thread& t_)=delete;
};

struct Func{
    int &i;
    Func(int& i_):i(i_){}

    void operator()(int it){
        std::cout << "Hello *^* from func -> "
                  << it << std::endl;
    }
};

void do_in_current_thread(){
    std::cout << "Hello *_* from current -> "
              << std::endl;
}


void ff(){
    int ilcl = 0;
    Func f(ilcl);
    std::thread t(f, 999);

    thread_guard g(t);

    do_in_current_thread();
}
