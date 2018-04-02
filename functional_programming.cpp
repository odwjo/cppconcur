#include <iostream>
#include <list>
#include <algorithm>
#include <chrono>
#include <string>
#include <future>
#include <thread>
#include <type_traits>
#include <random>

template<typename T>
void show_list(const std::list<T>& l, const std::string&& name = ""){
    std::cout << name << " : ";
    for(auto i:l)
        std::cout << i << ", ";
    std::cout << std::endl;
}

auto id = std::this_thread::get_id();

template<typename T>
std::list<T> sequential_quick_sort(std::list<T> input){
    if(input.empty() || (input.size() == 1)){
        return input;
    }
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());
    T const& pivot = *(result.begin());

    auto divide_point = std::partition(input.begin(), input.end(),
                                       [&](T const& t){return t<pivot;});
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input.begin(),divide_point);
    auto new_lower(sequential_quick_sort(std::move(lower_part)));
    auto new_higher(sequential_quick_sort(std::move(input)));
    result.splice(result.end(),new_higher);
    result.splice(result.begin(), new_lower);
    //if(std::this_thread::get_id() != id)
    //    std::cout << "thread: " << std::this_thread::get_id() << std::endl;

    return result;
}
template<typename T>
std::list<T> parallel_quick_sort(std::list<T> input){
    if(input.empty() || (input.size() == 1)){
        return input;
    }
    std::list<T> result;
    result.splice(result.begin(), input, input.begin());//remove first elem from input to result;
    T const& pivot = *(result.begin());

    auto divide_point = std::partition(input.begin(), input.end(),
                                       [&](T const& t){return t<pivot;});
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input.begin(),divide_point);
    //show_list(lower_part, std::string("lower_part"));//*_*//
    std::future<std::list<T>>
            new_lower(std::async(&parallel_quick_sort<T>,std::move(lower_part)));
    //show_list(input, std::string("input"));//*_*//
    auto new_higher(parallel_quick_sort(std::move(input)));
    //show_list(input, std::string("higher_part"));//*_*//
    result.splice(result.end(),new_higher);
    result.splice(result.begin(), new_lower.get());
    return result;
}
template<typename T>
std::list<T> parallel_quick_sort_v2(std::list<T> input){
    if(input.empty() || (input.size() == 1)){
        return input;
    }
    std::list<T> result;

    std::packaged_task<std::list<T>(std::list<T>)> task(sequential_quick_sort<T>);

    result.splice(result.begin(), input, input.begin());//remove first elem from input to result;
    T const& pivot = *(result.begin());

    auto divide_point = std::partition(input.begin(), input.end(),
                                       [&](T const& t){return t<pivot;});
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input.begin(),divide_point);

    std::future<std::list<T>> new_lower(task.get_future());
    std::thread new_th(std::move(task),std::move(lower_part));

    auto new_higher(sequential_quick_sort(std::move(input)));

    result.splice(result.end(),new_higher);

    //std::cout << new_th.get_id() << std::endl;
    new_th.join();
    //std::cout << std::this_thread::get_id() << std::endl;

    result.splice(result.begin(), new_lower.get());
    return result;
}

template<typename F, typename A>
std::future<typename std::result_of<F(A&&)>::type>
spawn_task(F&& f,A &&a){
    typedef typename std::result_of<F(A&&)>::type result_type;
    std::packaged_task<result_type(A&&)>
            task(std::move(f));
    std::future<result_type> res(task.get_future());
    std::thread t(std::move(task),std::move(a));
    t.detach();
    return res;
}

void test_sequential_quick_sort(){
    std::list<int> il;

    std::random_device rd;
    std::default_random_engine ue(rd());
    std::uniform_int_distribution<unsigned> random_gen_uint(1,100);
    for(size_t i = 0;i != 10000;++ i){
        il.push_back(random_gen_uint(ue));
    }

    auto x1 = std::chrono::high_resolution_clock::now();
    auto res1 = sequential_quick_sort(il);
    auto y1 = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(y1 - x1).count() << std::endl;
    auto x2 = std::chrono::high_resolution_clock::now();
    auto res2 = parallel_quick_sort(il);
    auto y2 = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(y2 - x2).count() << std::endl;
    auto x3 = std::chrono::high_resolution_clock::now();
    auto res3 = parallel_quick_sort_v2(il);
    auto y3 = std::chrono::high_resolution_clock::now();
    std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(y3 - x3).count() << std::endl;
    //show_list(res2);
    //std::cout << res;
}
