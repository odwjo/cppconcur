#ifndef THREADSAFE_QUEUE_H
#define THREADSAFE_QUEUE_H
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

template <typename T>
class threadsafe_queue{
private:
    struct node{
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    std::unique_ptr<node> head;
    node* tail;
    std::atomic<unsigned> sz;
    std::mutex head_mutex;
    std::mutex tail_mutex;
    std::condition_variable data_cond;

    node* get_tail(){
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    std::unique_ptr<node> pop_head(){
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        unsigned old_sz = sz.load();
        while(!sz.compare_exchange_weak(old_sz,old_sz-1));
        return old_head;
    }
    std::unique_lock<std::mutex> wait_for_data(){
        std::unique_lock<std::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock, [&]{return head.get() != get_tail();});
        return std::move(head_lock);
    }
    std::unique_ptr<node> wait_pop_head(){
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }
    std::unique_ptr<node> wait_pop_head(T &value){
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        value = std::move(*head -> data);
        return pop_head();
    }
    std::unique_ptr<node> try_pop_head(){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if(head.get() == get_tail())
            return std::unique_ptr<node>();
        return pop_head();
    }
    std::unique_ptr<node> try_pop_head(T &value){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if(head.get() == get_tail()){
            return std::unique_ptr<node>();
        }
        value = std::move(*head -> data);
        return pop_head();
    }

public:
    threadsafe_queue():head(new node),tail(head.get()),sz(0){}
    threadsafe_queue(const threadsafe_queue&) = delete;
    threadsafe_queue& operator=(const threadsafe_queue&) = delete;

    std::shared_ptr<T> try_pop(){
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }

    bool try_pop(T &value){
        std::unique_ptr<node> old_head = try_pop_head(value);
        return bool(old_head);
    }

    std::shared_ptr<T> wait_and_pop(){
        std::unique_ptr<node> const old_head = wait_pop_head();
        return old_head -> data;
    }

    void wait_and_pop(T &value){
        std::unique_ptr<node> const old_head = wait_pop_head(value);
    }

    void push(T&& new_value);
    void empty(){
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return (head.get() == get_tail());
    }
    unsigned size(){
        return sz.load();
    }
};
template <typename T>
void threadsafe_queue<T>::push(T&& new_value){
    std::shared_ptr<T> new_data(
                std::make_shared<T>(std::move(new_value)));
    std::unique_ptr<node> p(new node);//so node should be small
    {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->data = new_data;//shared_ptr<T>
        node* const new_tail = p.get();
        tail->next = std::move(p);//make shure p will be release
        tail = new_tail;
        unsigned old_sz = sz.load();
        while(sz.compare_exchange_weak(old_sz,old_sz+1));
    }
    data_cond.notify_one();
}

#endif // THREADSAFE_QUEUE_H
