#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <atomic>
#include <thread>
#include <vector>
#include <list>
#include <functional>
#include <algorithm>
#include <type_traits>
#include <future>
#include <memory>
#include <chrono>
#include <mutex>
#include <queue>
#include <exception>
#include <condition_variable>
#include "threadsafe_queue.h"
#include "concurrent_code.h"

class simple_thread_pool{//use can't return values
    std::atomic_bool done;
    threadsafe_queue<std::function<void()> > work_queue;
    std::vector<std::thread> threads;
    join_threads joiner;

    void worker_thread(){
        while(!done){
            while(!done){
                std::function<void()> task;
                if(work_queue.try_pop(task)){
                    task();
                }else
                    std::this_thread::yield();
            }
        }
    }
public:
    simple_thread_pool():
        done(false),joiner(threads){
        unsigned const thread_count = std::thread::hardware_concurrency();
        try{
            for(unsigned i = 0;i < thread_count;++ i){
                threads.push_back(std::thread(&simple_thread_pool::worker_thread,
                                              this));
            }
        }catch(...){
            done = true;
            throw;
        }
    }
    ~simple_thread_pool(){
        done = true;
    }
    template<typename FunctionType>
    void submit(FunctionType f){
        work_queue.push(std::function<void()>(f));
    }
};

class function_wrapper{
    struct impl_base{
        virtual void call()=0;
        virtual ~impl_base(){}
    };

    std::unique_ptr<impl_base> impl;

    template<typename F>
    struct impl_type:impl_base{
        F f;
        impl_type(F&& f_):f(std::move(f_)){}
        void call() {f();}
    };
public:
    template<typename F>
    function_wrapper(F&& f)://can be initialized with packaged_task<someFunc>();
        impl(new impl_type<F>(std::move(f))){}

    void operator()(){impl->call();}
    function_wrapper() = default;
    function_wrapper(function_wrapper&& other):
        impl(std::move(other.impl)){}
    function_wrapper& operator=(function_wrapper&& other){
        impl = std::move(other.impl);
        return *this;
    }
    function_wrapper(const function_wrapper&)=delete;
    function_wrapper(function_wrapper&)=delete;
    function_wrapper& operator=(const function_wrapper&)=delete;
};

class simple_thread_pool_v2{
    std::atomic_bool done;
    threadsafe_queue<function_wrapper> work_queue;
    std::vector<std::thread> threads;
    join_threads joiner;
    unsigned max_threads;

    void worker_thread(){
            while(!done){
                function_wrapper task;
                if(work_queue.try_pop(task)){//this judge is crucial^_*
                    task();
                }else
                    std::this_thread::yield();
            }
    }
public:
    simple_thread_pool_v2():
        done(false),joiner(threads){
        unsigned const thread_count = std::thread::hardware_concurrency();
        max_threads = thread_count;
        try{
            for(unsigned i = 0;i < thread_count;++ i){
                threads.push_back(std::thread(&simple_thread_pool_v2::worker_thread,
                                              this));
            }
        }catch(...){
            done = true;
            throw;
        }
    }
    ~simple_thread_pool_v2(){
        done = true;
    }
    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type>
    submit(FunctionType f){//f should be result_type();
        typedef typename std::result_of<FunctionType()>::type
                result_type;
        std::packaged_task<result_type()> task(std::move(f));
        std::future<result_type> res(task.get_future());
        work_queue.push(std::move(task));//so packaged_task is a
                                    //callable object no need to use return
        return res;
    }
    bool is_not_full(){
        return work_queue.size() < max_threads;
    }

    void run_pending_task(){
        function_wrapper task;
        if(work_queue.try_pop(task)){
            task();
        }else{
            std::this_thread::yield();
        }
    }
};

class simple_thread_pool_v3{
    std::atomic_bool done;
    threadsafe_queue<function_wrapper> pool_work_queue;
    typedef std::queue<function_wrapper> local_queue_type;
    static thread_local std::unique_ptr<local_queue_type>
                            local_work_queue;

    std::vector<std::thread> threads;
    join_threads joiner;
    unsigned max_threads;

    //every thread is working on it once started
    void worker_thread(){
        local_work_queue.reset(new local_queue_type);
        while(!done){
            run_pending_task();
        }
    }
public:
    simple_thread_pool_v3():
        done(false),joiner(threads){
        unsigned const thread_count = std::thread::hardware_concurrency();
        max_threads = thread_count;
        try{//worker_thread function entry
            for(unsigned i = 0;i < thread_count;++ i){
                threads.push_back(std::thread(&simple_thread_pool_v3::worker_thread,
                                              this));
            }
        }catch(...){
            done = true;
            throw;
        }
    }
    ~simple_thread_pool_v3(){
        done = true;
    }

    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type>
    submit(FunctionType f){//f should be result_type();
        typedef typename std::result_of<FunctionType()>::type
                result_type;
        std::packaged_task<result_type()> task(std::move(f));//so packaged_task is a
                                                            //callable object no need to use return
        std::future<result_type> res(task.get_future());
        //local_work_queue first
        if(local_work_queue)
            local_work_queue->push(std::move(task));
        else
            pool_work_queue.push(std::move(task));
        return res;
    }

    void run_pending_task(){
        function_wrapper task;
        //local_work_queue first
        if(local_work_queue && !local_work_queue->empty()){
            task = std::move(local_work_queue->front());
            local_work_queue->pop();
            task();
        }else if(pool_work_queue.try_pop(task)){
            task();
        }else{
            std::this_thread::yield();
        }
    }
};

class work_stealing_queue{
private:
    typedef function_wrapper data_type;
    std::deque<data_type> the_queue;
    mutable std::mutex the_mutex;
public:
    work_stealing_queue(){

    }
    work_stealing_queue(const work_stealing_queue&)=delete;
    work_stealing_queue& operator=(const work_stealing_queue&)=delete;

    void push(data_type data){
        std::lock_guard<std::mutex> lock(the_mutex);
        the_queue.push_front(std::move(data));
    }
    bool empty()const{
        std::lock_guard<std::mutex> lock(the_mutex);
        return the_queue.empty();
    }
    bool try_pop(data_type& res){
        std::lock_guard<std::mutex> lock(the_mutex);
        if(the_queue.empty())
            return false;
        res = std::move(the_queue.front());
        the_queue.pop_front();
        return true;
    }
    bool try_steal(data_type& res){
        std::lock_guard<std::mutex> lock(the_mutex);
        if(the_queue.empty())
            return false;
        res = std::move(the_queue.back());
        the_queue.pop_back();
        return true;
    }
};

class simple_thread_pool_v4{
    typedef function_wrapper task_type;
    std::atomic_bool done;
    threadsafe_queue<task_type> pool_work_queue;
    std::vector<std::unique_ptr<work_stealing_queue>> queues;
    std::vector<std::thread> threads;
    join_threads joiner;

    static thread_local work_stealing_queue* local_work_queue;
    static thread_local unsigned my_index;

    //every thread is working on it once started
    void worker_thread(unsigned my_index_){
        my_index = my_index_;
        local_work_queue = queues[my_index].get();
        while(!done){
            run_pending_task();
        }
    }
    bool pop_task_from_local_queue(task_type& task){
        return local_work_queue && local_work_queue->try_pop(task);
    }
    bool pop_task_from_pool_queue(task_type& task){
        return pool_work_queue.try_pop(task);
    }
    bool pop_task_from_other_thread_queue(task_type& task){
        for(unsigned i = 0;i < queues.size();++ i){
            unsigned const index = (my_index +i+1) % queues.size();
            if(queues[index]->try_steal(task)){
                return true;
            }
        }
        return false;
    }

public:
    simple_thread_pool_v4():
        done(false),joiner(threads){
        unsigned const thread_count = std::thread::hardware_concurrency();

        try{//worker_thread function entry
            for(unsigned i = 0;i < thread_count;++ i){
                queues.push_back(std::unique_ptr<work_stealing_queue>(
                                     new work_stealing_queue));
                threads.push_back(std::thread(&simple_thread_pool_v4::worker_thread,
                                              this,i));
            }
        }catch(...){
            done = true;
            throw;
        }
    }
    ~simple_thread_pool_v4(){
        done = true;
    }

    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type>
    submit(FunctionType f){//f should be result_type();
        typedef typename std::result_of<FunctionType()>::type
                result_type;
        std::packaged_task<result_type()> task(std::move(f));//so packaged_task is a
                                                            //callable object no need to use return
        std::future<result_type> res(task.get_future());
        //local_work_queue first
        if(local_work_queue)
            local_work_queue->push(std::move(task));
        else
            pool_work_queue.push(std::move(task));
        return res;
    }

    void run_pending_task(){
        task_type task;
        //local_work_queue first
        if(pop_task_from_local_queue(task) ||
           pop_task_from_pool_queue(task)  ||
           pop_task_from_other_thread_queue(task)){
            task();
        }else{
            std::this_thread::yield();
        }
    }
};

template<typename Iterator,typename T>
T parallel_accumulate_with_threadpool(Iterator first,Iterator last,T init){
    struct accumulate_block{
        T operator()(Iterator b,Iterator e){
            return std::accumulate(b,e,T());
        }
    };

    using ul = unsigned long;
    ul const length = std::distance(first, last);
    if(!length)
        return init;
    ul const block_size = 25;
    ul const num_blocks = (length + block_size -1)/block_size;

    std::vector<std::future<T> > futures(num_blocks -1);
    simple_thread_pool_v2 pool;

    Iterator block_start = first;
    for(unsigned long i = 0;i < (num_blocks -1);++ i){
        Iterator block_end = block_start;
        std::advance(block_end,block_size);
        auto func = std::bind(accumulate_block(),block_start,block_end);
        futures[i] = pool.submit(func);
        block_start = block_end;
    }
    T last_result = accumulate_block()(block_start,last);
    T result = init;
    for(unsigned long i = 0;i < (num_blocks-1);++ i){
        result += futures[i].get();
    }
    result += last_result;
    return result;//pool is still waiting...
}

void test_thread_pool_accumulate(){
    std::vector<int> vi;
    for(int i = 0;i != 100;++ i)
        vi.push_back(i);
    std::cout << parallel_accumulate_with_threadpool(vi.begin(),
                                                     vi.end(),
                                                     0);
}
/*
//using thread_pool = simple_thread_pool_v4;
template<typename T>
struct sorter_with_threadpool{
    simple_thread_pool_v4 pool;
    std::list<T> do_sort(std::list<T> &chunk_data){
        if(chunk_data.size() < 2){
            return chunk_data;
        }
        std::list<T> result;
        result.splice(result.begin(),chunk_data,chunk_data.begin());
        T const& partition_val = *result.begin();
        typename std::list<T>::iterator divide_point =
                std::partition(chunk_data.begin(),chunk_data.end(),
                               [&](T const& val){return val < partition_val;});
        std::list<T> new_lower_chunk;
        new_lower_chunk.splice(new_lower_chunk.end(),chunk_data,chunk_data.begin(),
                               divide_point);
        std::future<std::list<T> > new_lower =
                pool.submit(std::bind(&sorter_with_threadpool::do_sort,this,
                                      std::move(new_lower_chunk)));
        //std::future<std::list<T> > new_higher =
        //        pool.submit(std::bind(&sorter_with_threadpool::do_sort,this,
        //                              std::move(chunk_data)));
        std::list<T> new_higher(do_sort(chunk_data));
        //deadlock happening
        while(!(new_lower.wait_for(std::chrono::milliseconds(20))
              == std::future_status::timeout)){
            pool.run_pending_task();
        }
        result.splice(result.end(),new_higher);
        result.splice(result.begin(),new_lower.get());
        return result;
    }
    std::list<T> do_sort_no_waiting(std::list<T> &chunk_data){
        if(chunk_data.size() < 2){
            return chunk_data;
        }
        std::list<T> result;
        result.splice(result.begin(),chunk_data,chunk_data.begin());
        T const& partition_val = *result.begin();
        typename std::list<T>::iterator divide_point =
                std::partition(chunk_data.begin(),chunk_data.end(),
                               [&](T const& val){return val < partition_val;});
        std::list<T> new_lower_chunk;
        new_lower_chunk.splice(new_lower_chunk.end(),chunk_data,chunk_data.begin(),
                               divide_point);
        std::list<T> new_lower = do_sort_no_waiting(std::move(new_lower_chunk));
        std::list<T> new_higher = do_sort_no_waiting(std::move(chunk_data));
        //std::list<T> new_higher(do_sort(chunk_data));

        result.splice(result.end(),new_higher);
        result.splice(result.begin(),new_lower);
        return result;
    }
};
template <typename T>
std::list<T> parallel_quick_sort_with_threadpool(std::list<T> input){
    if(input.empty()){
        return input;
    }
    sorter_with_threadpool<T> s;
    return s.do_sort(input);
}

//////////failed///////
void test_parallel_quick_sort_with_threadpool(){
    std::list<int> il;
    for(size_t i = 0;i != 100;i ++){
        il.push_back(random_uint_gen(1,100));
    }
    show_vec(il.begin(),il.end());
    std::cout << "--------------^>^-----------------" << std::endl;
    auto x = parallel_quick_sort_with_threadpool(il);
    show_vec(x.begin(),x.end());
}*/

void interruption_point();

class interrupt_flag{
    std::atomic<bool> flag;
    std::condition_variable* thread_cond;
    std::condition_variable_any* thread_cond_any;
    std::mutex set_clear_mutex;
public:
    interrupt_flag():
        thread_cond(nullptr),thread_cond_any(nullptr){}

    void set(){
        flag.store(true,std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        if(thread_cond)
            thread_cond->notify_all();
        else if(thread_cond_any)
            thread_cond_any->notify_all();
    }

    template<typename Lockable>//use set_clear_mutex to protect wait
    void wait(std::condition_variable_any& cv,Lockable& lk){
        struct custom_lock{
            interrupt_flag* self;
            Lockable& lk;
            custom_lock(interrupt_flag* self_,
                        std::condition_variable_any& cond,
                        Lockable& lk_):self(self_),lk(lk_){
                self->set_clear_mutex.lock();
                self->thread_cond_any = &cond;
            }
            void unlock(){
                lk.unlock();
                self->set_clear_mutex.unlock();
            }
            void lock(){
                std::lock(self->set_clear_mutex, lk);//when locked,other threads cannot change lk
            }
            ~custom_lock(){
                self->thread_cond_any = nullptr;
                self->set_clear_mutex.unlock();
            }
        };
        custom_lock cl(this,cv,lk);//set_clear_mutex is locked
        interruption_point();
        //when wait,call unlocked() firstly until cv is notified
        //then call lock()
        cv.wait(cl);//condition_variable_any can work on any BasicLoackable type;
        interruption_point();
    }

    bool is_set() const{
        return flag.load(std::memory_order_relaxed);
    }
    void set_condition_variable(std::condition_variable &cv){
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        thread_cond = &cv;
    }
    void clear_condition_variable(){
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        thread_cond = nullptr;
    }
    struct clear_cv_on_destruct;
};

thread_local interrupt_flag this_thread_interrupt_flag;

struct interrupt_flag::clear_cv_on_destruct{
    ~clear_cv_on_destruct(){
        this_thread_interrupt_flag.clear_condition_variable();
    }
};

class thread_interrupted:std::exception{
public:
    virtual const char* what() const noexcept{
        return "Thread interrupted";
    }
};

void interruption_point(){
    if(this_thread_interrupt_flag.is_set()){
        throw thread_interrupted();
    }
}

class interruptible_thread{
    std::thread interanl_thread;
    interrupt_flag* flag;
public:
    template<typename FunctionType>
    interruptible_thread(FunctionType f){
        std::promise<interrupt_flag*> p;//used to communicate with the internal thread
        interanl_thread = std::thread([f,&p]{
            p.set_value(&this_thread_interrupt_flag);
            try{
                f();
            }catch(thread_interrupted const&)
            {}
        });
        flag = p.get_future().get();
    }
    void interrupt(){
        if(flag){
            flag->set();
        }
    }
};

void interruptible_wait(std::condition_variable& cv,
                        std::unique_lock<std::mutex>& lk){
    interruption_point();
    this_thread_interrupt_flag.set_condition_variable(cv);
    interrupt_flag::clear_cv_on_destruct guard;//if thread was interrupted,
                                    //make sure remove the cv
    interruption_point();
    cv.wait_for(lk,std::chrono::milliseconds(1));
    interruption_point();
}
template<typename Lockable>
void interruptible_wait(std::condition_variable_any& cv,
                        Lockable& lk){
    this_thread_interrupt_flag.wait(cv,lk);
}
template<typename Predicate>
void interruptible_wait(std::condition_variable& cv,
                        std::unique_lock<std::mutex>& lk,
                        Predicate pred){
    interruption_point();
    this_thread_interrupt_flag.set_condition_variable(cv);
    interrupt_flag::clear_cv_on_destruct guard;//if thread was interrupted,
                                     //make sure remove the cv
    //interruption_point();
    while(!this_thread_interrupt_flag.is_set() && !pred)
        cv.wait_for(lk,std::chrono::milliseconds(1));
    interruption_point();
}
template<typename T>
void interruptible_wait(std::future<T>& uf){
    while(!this_thread_interrupt_flag.is_set()){
        if(uf.wait_for(std::chrono::milliseconds(1))//??in book
                == std::future_status::ready)
            break;
    }
}
/*
//use the technic
std::mutex config_mutex;
std::vector<interruptible_thread> background_threads;
void background_thread(int disk_id){
    while(true){
        interruptible_point();
        fs_change fsc = get_fs_changes(disk_id);
        if(fsc.has_changes()){
            update_index(fsc);
        }
    }
}
void start_background_prcessing(){
    background_threads.push_back(
                interruptible_thread(background_thread,disk_1));
    background_threads.push_back(
                interruptible_thread(background_thread,disk_2));
}
int main_fake(){
    start_background_prcessing();
    process_gui_until_exit();
    std::unique_lock<std::mutex> lk(config_mutex);
    for(unsigned i = 0;i < background_threads.size();++ i)
        background_threads[i].interrupt();
    for(unsigned i = 0;i < background_threads.size();++ i)
        background_threads[i].join();
}*/

#endif // THREAD_POOL_H
