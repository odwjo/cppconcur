#include <iostream>
#include <thread>
#include <exception>
#include <algorithm>//std::min,
#include <iterator>//std::distance, std::advance
#include <functional>//std::mem_fn

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

class scoped_thread{
    std::thread t;
public:
    explicit scoped_thread(std::thread t_):
        t(std::move(t_)){
        if(!t.joinable())
            throw std::logic_error("No thread");
    }
    ~scoped_thread(){
        t.join();
    }
    scoped_thread(scoped_thread const&)=delete;
    scoped_thread& operator=(scoped_thread const&)=delete;
};

template<typename Iterator,typename T>
struct accumulate_block{
    void operator()(Iterator first,Iterator last,T &result){
        result = std::accumulate(first,last,result);
    }
};

template<typename Iterator,typename T>
T parallel_accumulate(Iterator first,Iterator last,T init){
    unsigned long const length = std::distance(first,last);
    if(!length)
        return init;
    using ulc = unsigned long const;
    ulc min_per_thread = 25;
    ulc max_threads =
            (length + min_per_thread - 1)/min_per_thread;
    ulc hardware_threads = std::thread::hardware_concurrency();
    ulc num_threads = std::min(hardware_threads != 0 ?
                hardware_threads:2, max_threads);
    ulc block_size = length / num_threads;

    std::vector<T> results(num_threads);
    std::vector<std::thread> threads(num_threads - 1);

    Iterator block_start = first;
    for(unsigned long i = 0;i < (num_threads - 1);++ i){
        Iterator block_end = block_start;
        std::advance(block_end,block_size);
        threads[i] = std::thread(
                    accumulate_block<Iterator, T>(),
                    block_start, block_end, std::ref(results[i]));
        block_start = block_end;
    }
    accumulate_block<Iterator, T>()(
                block_start, last, results[num_threads - 1]);
    std::for_each(threads.begin(),threads.end(),
                  std::mem_fn(&std::thread::join));
    return std::accumulate(results.begin(),results.end(),init);
}

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

void show_num_cores(){
    std::cout << std::thread::hardware_concurrency()
              << std::endl;
}


void ff(){
    std::vector<int> vi(1000);
    auto cur_thrd_id = std::this_thread::get_id();

    for(size_t i = 0;i != vi.size();++ i)
        vi[i] = i+1;
    std::cout << parallel_accumulate(vi.begin(), vi.end(), 0) << std::endl;
    std::cout << cur_thrd_id << std::endl;
}
