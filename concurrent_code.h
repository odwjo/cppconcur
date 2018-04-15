#ifndef CONCURRENT_CODE_H
#define CONCURRENT_CODE_H
#include <list>
#include <iostream>
#include <iomanip>
#include <future>
#include <shared_mutex>
#include <random>
#include <algorithm>
#include <atomic>
#include <iterator>
#include "threadsafe_stack_usemutex.h"

template<typename T>
struct sorter{
    struct chunk_to_sort{
        std::list<T> data;
        std::promise<std::list<T>> promise;
    };

    threadsafe_stack<chunk_to_sort> chunks;
    std::vector<std::thread> threads;
    unsigned const max_thread_count;
    std::atomic<bool> end_of_data;

    sorter():max_thread_count(std::thread::hardware_concurrency()-1),
        end_of_data(false){}
    ~sorter(){
        end_of_data = true;
        for(unsigned i = 0;i < threads.size();++ i){
            threads[i].join();
        }
    }
    void try_sort_chunk(){
        std::shared_ptr<chunk_to_sort> chunk = chunks.pop();
        if(chunk){
            sort_chunk(chunk);
        }
    }
    std::list<T> do_sort(std::list<T> &chunk_data){
        if(chunk_data.empty())
            return chunk_data;
        std::list<T> result;

        result.splice(result.begin(),chunk_data,chunk_data.begin());
        T const& partition_val = *result.begin();

        typename std::list<T>::iterator divide_point =
                std::partition(chunk_data.begin(), chunk_data.end(),
                               [&](T const& val)
                                {return val < partition_val;});
        chunk_to_sort new_lower_chunk;
        new_lower_chunk.data.splice(new_lower_chunk.data.begin(),
                                    chunk_data,chunk_data.begin(),
                                    divide_point);

        std::future<std::list<T> > new_lower =
                new_lower_chunk.promise.get_future();
        chunks.push(std::move(new_lower_chunk));//push can't copy
        //new thread keep poping chunks to sort ele of chunks
        if(threads.size() < max_thread_count){
            threads.push_back(std::thread(&sorter<T>::sort_thread,this));
        }

        std::list<T> new_higher(do_sort(chunk_data));//recursive call

        result.splice(result.end(),new_higher);
        while(new_lower.wait_for(std::chrono::milliseconds(1))//all threads do sort
              != std::future_status::ready){
            try_sort_chunk();
        }
        result.splice(result.begin(),new_lower.get());

        return result;
    }
    void sort_chunk(std::shared_ptr<chunk_to_sort> const& chunk ){
        chunk->promise.set_value(do_sort(chunk->data));
    }
    void sort_thread(){
        while(!end_of_data){
            try_sort_chunk();
            std::this_thread::yield();
        }
    }
};
template<typename T>
std::list<T> parallel_quick_sort(std::list<T> input){
    if(input.empty()){
        return input;
    }
    sorter<T> s;
    return s.do_sort(input);
}

unsigned random_uint_gen(unsigned low_bound=1,unsigned high_bound=100){
    std::random_device rd;
    static std::default_random_engine ue(rd());
    std::uniform_int_distribution<unsigned> random_gen_uint(1,100);
    return random_gen_uint(ue);
}

void test_parallel_quick_sort(){
    std::list<int> lsi;
    for(size_t i = 0;i != 10000;++ i){
        lsi.push_back(random_uint_gen());
    }

    auto x1 = std::chrono::high_resolution_clock::now();
    auto res1 = parallel_quick_sort(lsi);
    auto y1 = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<
                 std::chrono::nanoseconds>(y1 - x1).count()
              << std::endl;
}

//////// using std::packaged_task
template<typename Iterator,typename T>
struct accumulate_block{
    T operator()(Iterator first,Iterator last){
        return std::accumulate(first,last,T());
    }
};

class join_threads{
    std::vector<std::thread>& threads;
public:
    explicit join_threads(std::vector<std::thread>& threads_):
        threads(threads_){}
    ~join_threads(){
        for(unsigned long i = 0;i < threads.size();++ i){
            if(threads[i].joinable())
                threads[i].join();
        }
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

    //std::vector<T> results(num_threads);
    std::vector<std::future<T> > futures(num_threads -1);///*///
    std::vector<std::thread> threads(num_threads - 1);
    join_threads joiner(threads);///////////////////////////*///

    Iterator block_start = first;
    for(unsigned long i = 0;i < (num_threads - 1);++ i){
        Iterator block_end = block_start;
        std::advance(block_end,block_size);

        std::packaged_task<T(Iterator,Iterator)> task(    ///*///
                    accumulate_block<Iterator,T>());      ///*///
        futures[i] = task.get_future();                   ///*///
        threads[i] = std::thread(
                    std::move(task),/////////////////////////*///
                    block_start, block_end);
        block_start = block_end;
    }
    T last_result = accumulate_block<Iterator,T>()(block_start, last);
    T result = init;
    for(unsigned long i = 0;i < (num_threads-1);++ i){
        result += futures[i].get();
    }
    result += last_result;

    return result;
}

template<typename Iterator,typename T>
T parallel_accumulate_using_async(Iterator first,Iterator last,T init){
    using ulc = unsigned long const;
    ulc length = std::distance(first,last);
    if(!length)
        return init;

    ulc min_per_thread = 25;
    if(length <= min_per_thread)
        return std::accumulate(first, last, init);
    Iterator mid_point = first;
    std::advance(mid_point, length/2);
    std::future<T> first_half_result =
            std::async(parallel_accumulate_using_async<Iterator,T>,
                       first,mid_point,init);
    T second_half_result = parallel_accumulate_using_async(mid_point,last,T());
    return first_half_result.get() + second_half_result;
}

template<typename Iterator, typename Func>
void parallel_for_each(Iterator first,Iterator last,Func f){
    using ul = unsigned long;
    ul const length = std::distance(first, last);
    if(length)
        return ;
    ul const min_per_thread = 25;
    ul const max_threads = (length + min_per_thread -1)/min_per_thread;

    ul const hardware_threads = std::thread::hardware_concurrency();
    ul const num_threads = std::min(hardware_threads != 0
            ? hardware_threads:2, max_threads);
    ul const block_size = length/num_threads;

    std::vector<std::future<void> > futures(num_threads -1);
    std::vector<std::thread> threads(num_threads - 1);
    join_threads joiner(threads);

    Iterator block_start = first;
    for(ul i = 0;i < (num_threads -1);++ i){
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        std::packaged_task<void(void)> task(
                    [=](){std::for_each(block_start, block_end, f);});
        futures[i] = task.get_future();
        threads[i] = std::thread(std::move(task));
        block_start = block_end;
    }
    std::for_each(block_start, last, f);
    for(ul i = 0;i < (num_threads -1);++ i)
        futures[i].get();
}

template<typename Iterator, typename Func>
void parallel_for_each_async(Iterator first,Iterator last,Func f){
    unsigned long const length = std::distance(first, last);
    if(!length)
        return ;
    unsigned long const min_per_thread = 25;
    if(length < (2*min_per_thread))
            std::for_each(first,last,f);
    else{
        Iterator const mid_point = first + length/2;
        std::future<void> first_half = std::async(
                    &parallel_for_each_async<Iterator,Func>,
                    first, mid_point, f);
        parallel_for_each_async(mid_point,last,f);
        first_half.get();
    }
}

template<typename Iterator, typename MatchType>
Iterator parrallel_find(Iterator first,Iterator last,MatchType match){
    struct find_element{
        void operator()(Iterator begin,Iterator end,MatchType match,
                        std::promise<Iterator>* result,
                        std::atomic<bool>* done_flag){
            try{
                for(;(begin != end) && !done_flag->load();++ begin){
                    if(*begin == match){
                        result->set_value(begin);//may throw
                        done_flag->store(true);
                        return;
                    }
                }
            }catch(...){
                try{
                    result->set_exception(std::current_exception());
                    done_flag->store(true);
                }catch(...){}
            }
        }
    };
    unsigned long const length = std::distance(first, last);
    if(!length)
        return last;
    using ulc = unsigned long const;
    ulc min_per_thread = 25;
    ulc max_threads = (length + min_per_thread -1)/min_per_thread;

    ulc hardware_threads = std::thread::hardware_concurrency();
    ulc num_threads = std::min(hardware_threads != 0
            ? hardware_threads:2, max_threads);
    ulc block_size = length/num_threads;

    std::promise<Iterator> result;
    std::atomic<bool> done_flag(false);
    std::vector<std::thread> threads(num_threads - 1);
    {
        join_threads joiner(threads);
        Iterator block_start = first;
        for(unsigned long i = 0;i < (num_threads -1);++ i){
            Iterator block_end = block_start;
            std::advance(block_end, block_size);
            threads[i] = std::thread(find_element(),
                                     block_start,block_end,match,
                                     &result,&done_flag);
            block_start = block_end;
        }
        find_element()(block_start,last,match,&result,&done_flag);
    }
    if(!done_flag.load()){
        return last;
    }
    return result.get_future.get();
}

template<typename Iterator,typename MatchType>
Iterator parallel_find_impl_async(Iterator first,Iterator last,
                            MatchType match, std::atomic<bool>& done){
    try{
        unsigned long const length = std::distance(first, last);
        unsigned long const min_per_thread = 25;
        if(length < (2*min_per_thread)){
            for(;(first != last) && !done.load();++ first){
                if(*first == match){
                    done = true;
                    return first;
                }
            }
            return last;
        }else{
            Iterator const mid_point = first + (length/2);
            std::future<Iterator> async_result =
                    std::async(&parallel_find_impl_async<Iterator,MatchType>,
                               mid_point,last,match,std::ref(done));
            Iterator const direct_result =
                    parallel_find_impl_async(first,mid_point,match,done);
            return (direct_result == mid_point)?
                        async_result.get() : direct_result;
        }
    }catch(...){
        done = true;
        throw;
    }
}
template<typename Iterator,typename MatchType>
Iterator parallel_find(Iterator first,Iterator last,MatchType match){
    std::atomic<bool> done(false);
    return parallel_find_impl_async(first, last, match, done);
}

void test_parallel_find(){
    std::vector<unsigned> vi;
    for(size_t i = 0;i < 100;++ i){
        vi.push_back(random_uint_gen());
    }
    auto it_find = parallel_find(vi.begin(),vi.end(),47);
    if(it_find != vi.end())
        std::cout << "Find it: " << std::distance(vi.begin(),it_find)
                  << "th, " << *it_find << std::endl;
    else
        std::cout << "No 47\n";
}

//////////////////////partial_sum
template<typename Iterator>
void parallel_partial_sum(Iterator first,Iterator last){
    typedef typename Iterator::value_type value_type;

    struct process_chunk{
        void operator()(Iterator begin,Iterator last,
                        std::future<value_type>* previous_end_value,
                        std::promise<value_type>* end_value){
            try{
                Iterator end = last;
                ++ end;
                std::partial_sum(begin,end,begin);
                if(previous_end_value){
                    const value_type& addend = previous_end_value->get();
                    *last += addend;
                    if(end_value)
                        end_value->set_value(*last);
                    std::for_each(begin,last,[addend](value_type& item)
                                            {item += addend;});
                }else if(end_value)
                    end_value->set_value(*last);
            }catch(...){
                if(end_value){
                    end_value->set_exception(std::current_exception());
                }else{
                    throw;
                }
            }
        }
    };
    using ulc = unsigned long const;
    ulc length = std::distance(first,last);
    if(!length){
        return;
    }
    ulc min_per_thread = 25;
    ulc max_threads = (length + min_per_thread -1)/min_per_thread;
    ulc hardware_threads = std::thread::hardware_concurrency();
    ulc num_threads = std::min(hardware_threads != 0
            ? hardware_threads:2, max_threads);
    ulc block_size = length/num_threads;
    typedef typename Iterator::value_type value_type;
    std::vector<std::thread> threads(num_threads -1);
    std::vector<std::promise<value_type> > end_values(num_threads-1);
    std::vector<std::future<value_type> > previous_end_values;
    previous_end_values.reserve(num_threads-1);
    join_threads joiner(threads);

    Iterator block_start = first;
    for(unsigned long i = 0;i < (num_threads -1);++ i){
        Iterator block_last = block_start;
        std::advance(block_last, block_size -1);
        threads[i] = std::thread(process_chunk(),
                                 block_start, block_last,
                                 (i!=0)?&previous_end_values[i-1]:nullptr,
                                 &end_values[i]);
        block_start = block_last;
        ++ block_start;
        previous_end_values.push_back(end_values[i].get_future());
    }
    Iterator final_element = block_start;
    std::advance(final_element, std::distance(block_start,last)-1);
    process_chunk()(block_start,final_element,
                    (num_threads>1)?&previous_end_values.back():nullptr,nullptr);

}
template<typename T>
void show_vec(const std::vector<T>& vt){
    for(size_t i = 0;i != vt.size();++ i){
        std::cout << std::setw(5) << vt[i] << " ";;
        if(i%10==0 && i!=0)
            std::cout << "\n";
    }
    std::cout << std::endl;
}
template<typename Iterator>
void show_vec(Iterator b,Iterator l){
    for(Iterator i = b;i != l;++ i){
        std::cout << std::setw(5) << *i << " ";;
    }
    std::cout << std::endl;
}

void test_parallel_partial(){
    std::vector<unsigned> vui(100);
    for(size_t i = 0;i != 100;++ i)
        vui[i] = i;
    parallel_partial_sum(vui.begin(),vui.end());
    show_vec(vui);
}

class barrier_not_good{
    unsigned const count;
    std::atomic<unsigned> spaces;
    std::atomic<unsigned> generation;
public:
    explicit barrier_not_good(unsigned count_):
        count(count_),spaces(count),generation(0){}
    void wait(){
        unsigned const my_generation = generation;
        if(!--spaces){
            spaces = count;
            ++generation;
        }else{
            while(generation == my_generation)
                std::this_thread::yield();
        }
    }
};

//every thread wait in every level
//            15
//      6  10 14
//   3  5  7  9
//1  2  3  4  5
class barrier{
    std::atomic<unsigned> count;
    std::atomic<unsigned> spaces;
    std::atomic<unsigned> generation;
public:
    barrier(unsigned count_):
        count(count_),spaces(count_),generation(0){}
    void wait(){
        unsigned const gen = generation.load();
        if(!--spaces){//last count with some stride(like 1)
            spaces = count.load();
            ++ generation;
        }else{
            while(generation.load() == gen)
                std::this_thread::yield();
        }/*
        std::cout << "Wait: " << "count: "<< count
                  << " spaces " <<  spaces
                  << " generation: "
                  << generation << std::endl;
                  */
    }
    void done_waiting(){
        --count;//every level number is less than next level
        if(!--spaces){
            spaces = count.load();
            ++ generation;
        }/*
        std::cout << "Done_Waitint: " << "count: "<< count
                  << " spaces " <<  spaces
                  << " generation: "
                  << generation << std::endl;*/
    }
};
template<typename Iterator>
void parallel_partial_sum_with_barrier(Iterator first,Iterator last){
    typedef typename Iterator::value_type value_type;
    struct process_element{
        void operator()(Iterator first, Iterator last,
                        std::vector<value_type>& buffer,
                        unsigned i, barrier& b){
            value_type& ith_element = *(first + i);
            bool update_source = false;
            for(unsigned step = 0,stride = 1;
                stride <= i;++ step,stride *= 2){
                value_type const& source = (step%2)?
                            buffer[i] : ith_element;
                //change buffer first,than ith_element
                value_type& dest = (step%2)?
                            ith_element : buffer[i];
                value_type const& addend = (step%2)?
                            buffer[i-stride] : *(first+i-stride);

                dest = source + addend;
                update_source = !(step%2);//if step is odd,false
                //std::cout << "step " << step << " stride " <<
                //             stride << " i " << i << std::endl;
                //show_vec(first,last);////-_-
                b.wait();//wait for back;
            }
            //show_vec(first,last);////////-_-
            //make sure ith loop changes ith_element at last
            if(update_source)
                ith_element = buffer[i];
            b.done_waiting();
        }
    };
    unsigned long const length = std::distance(first, last);
    if(length <= 1)
        return ;
    std::vector<value_type> buffer(length);
    barrier b(length);//length threads in total
    ///std::cout << length << std::endl;
    std::vector<std::thread> threads(length -1);
    join_threads joiner(threads);

    //Iterator block_start = first;
    for(unsigned long i = 0;i < (length -1);++ i){
        threads[i] = std::thread(process_element(),first,last,
                                 std::ref(buffer),i,std::ref(b));
    }
    process_element()(first,last,buffer,length-1,b);
}

void test_parallel_partial_sum(){
    std::vector<int> vi{1,2,3,4,5,6,7,8,9};
    parallel_partial_sum_with_barrier(vi.begin(),vi.end());
    show_vec(vi);
}

#endif // CONCURRENT_CODE_H
