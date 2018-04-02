#include <future>
#include <iostream>
#include <cassert>

struct Y{
    int operator()(int t){
        return t;
    }
};

int find_the_answer_to_ltuae(){
    return 42;
}

void do_other_stuff(){
    std::cout << "Balabala." << std::endl;
}

int test_fu0(){//std::async() returns a std::future<>
    std::future<int> answer = std::async(find_the_answer_to_ltuae);
    do_other_stuff();
    std::cout << "The answer is " << answer.get() << std::endl;
}

//f1 run in new thread
//auto f1 = std::async(std::launch::async,Y(),55);
//f2 run in wait or get
//auto f2 = std::async(std::launch::deferred,Y(),55);
//f3 run with impementation chooses
//auto f3 = std::async(std::launch::async |
//                     std::launch::deferred,Y(),55);
/*
std::promise<int> p;
std::future<int> f(p.get_future());
assert(f.valid());
std::shared_future<int> sf(std::move(f));
assert(!f.valid());
assert(sf.valid());
std::shared_future<int> sf(p.get_future());
auto sf = p.get_future().share();
*/
void some_task(){}
void test_fu1(){
    std::future<void> f = std::async(some_task);
    if(f.wait_for(std::chrono::milliseconds(35))
            == std::future_status::ready)
        std::cout << "Good." << std::endl;
}
