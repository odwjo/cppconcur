#include <chrono>
#include <mutex>
#include <iostream>
#include <thread>
//#include <queue>
#include <condition_variable>

bool flag = false;
std::mutex m;

void wait_for_flag(){
    std::unique_lock<std::mutex> lk(m);
    while(!flag){
        lk.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        lk.lock();
    }
    std::cout << "Wala." << std::endl;
}

void set_flag(){
    std::lock_guard<std::mutex> lk(m);
    std::this_thread::sleep_for(std::chrono::seconds(4));
    flag = true;
}

void test_wait(){
    std::thread th(set_flag);
    wait_for_flag();
    th.join();
}
/*
//building a thread-safe queue
template <class T,class Container = std::deque<T> >
class queue{
public:
    explicit queue(const Container &);
    explicit queue(Container&& = Container());

    template <class Alloc> explicit queue(const Alloc&);
    template <class Alloc> queue(const Container&,const Alloc&);
    template <class Alloc> queue(Container&&,const Alloc&);
    template <class Alloc> queue(queue&&,const Alloc&);

    void swap(queue& q);
    bool empty() const;
    size_t size() const;
    T& front();
    const T& front()const;
    T& back();
    const T& back()const;

    void push(const T& x);
    void push(T&& x);
    void pop();
    template <class... Args> void emplace(Args&&... args);
};



class data_chunk{};
std::mutex mut;
std::queue<data_chunk> data_queue;
std::condition_variable data_cond;
void data_preparation_thread(){
    while(more_data_to_prepare()){
        data_chunk const data = prepare_data();
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);
        data_cond.notify_one();
    }
}
void data_processing_thread(){
    while(true){
        std::unique_lock,std::mutex> lk(mut);
        data_cond.wait(//if lambda return false, wait() will unlock->lk
                    lk, []{return !data_queue.empty();});
        data_chunk data = data_queue.front();
        data_queue.pop();
        lk.unlock();
        process(data);
        if(is_last_chunk(data))
            break;
    }
}
*/
//
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>

template<typename T>
class threadsafe_queue{
private:
    mutable std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue(){

    }
    threadsafe_queue(threadsafe_queue const& other){
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = other.data_queue;
    }
    void push(T new_value){
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(new_value);
        data_cond.notify_one();
    }
    void wait_and_pop(T& value){
        std::unique_lock<std::mutex> lk(mut);
        //when data is not empty,lk is locked and wait() exits
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        value = data_queue.front();
        data_queue.pop();
    }
    std::shared_ptr<T> wait_and_pop(){
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }
    bool try_pop(T& value){
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return false;
        value = data_queue.front();
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> try_pop(){
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }
    bool empty()const {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};

template<typename T>
class threadsafe_queue2{
private:
    mutable std::mutex mut;
    std::queue<std::shared_ptr<T> > data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue2(){

    }
    threadsafe_queue2(threadsafe_queue2 const& other){
        std::lock_guard<std::mutex> lk(other.mut);
        data_queue = other.data_queue;
    }
    void push(T new_value){
        std::shared_ptr<T> data(
                    std::make_shared<T>(std::move(new_value)));
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);
        data_cond.notify_one();
    }
    void wait_and_pop(T& value){
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        value = std::move(*data_queue.front());
        data_queue.pop();
    }
    std::shared_ptr<T> wait_and_pop(){
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk,[this]{return !data_queue.empty();});
        std::shared_ptr<T> res = data_queue.front();
        data_queue.pop();
        return res;
    }
    bool try_pop(T& value){
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return false;
        value = std::move(*data_queue.front());
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> try_pop(){
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res = data_queue.front();
        data_queue.pop();
        return res;
    }
    bool empty()const {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};

template <typename T>
class que{
private:
    struct node{
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    std::unique_ptr<node> head;
    node* tail;
    std::mutex head_mutex;
    std::mutex tail_mutex;
    node* get_tail(){
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    std::unique_ptr<node> pop_head(){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if(head.get() == get_tail()){
            return nullptr;
        }
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }

public:
    que():head(new node),tail(head.get()){}
    que(const que&) = delete;
    que& operator=(const que&) = delete;

    std::shared_ptr<T> try_pop(){
        std::unique_ptr<node> old_head = pop_head();
        return old_head? old_head->data : std::shared_ptr<T>();
    }
    void push(T new_value){
        std::shared_ptr<T> new_data(
                    std::make_shared<T>(std::move(new_value)));
        std::unique_ptr<node> p(new node);//so node should be small
        node* const new_tail = p.get();
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data = new_data;//shared_ptr<T>
        tail->next = std::move(p);//make shure p will be release
        tail = new_tail;
    }
};



//use clock
void use_clock(){
    using sys_clk = std::chrono::system_clock;
    sys_clk sl;
    sys_clk::time_point start = sl.now();
    int j;
    for(size_t i = 0;i != 100000;++ i)
        ++ j;
    sys_clk::time_point end = sl.now();
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << diff.count();
    auto future = std::chrono::high_resolution_clock::now()
            + std::chrono::nanoseconds(500);

}

//use clock_diff
std::condition_variable cv;
bool done;
//std::mutex m;

bool wait_loop(){
    auto const timeout = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(500);
    std::unique_lock<std::mutex> lk(m);
    while(!done){
        if(cv.wait_until(lk, timeout)
                == std::cv_status::timeout)
            break;
    }
    return done;
}
