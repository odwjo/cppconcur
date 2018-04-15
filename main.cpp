#include <iostream>
#include <functional>//std::ref, std::bind, std::mem_fn
#include <utility>//std::move
#include <algorithm>//std::for_each
#include <list>
#include <vector>
#include <thread>
#include "cppconcurrent.h"
#include "threadsafe_lookup_table.h"
#include "concurrent_code.h"
#include "thread_pool.h"
#include <atomic>
#include <memory>
constexpr int square(int x){
    return x*x;
}
int array[square(5)];

using namespace std;

int main(int argc, char *argv[])
{
    //ff();
    //test_Solution();
    //use_thrdsafe_stack();
    //test_hierarchy_mutex();
    //foo1();
    //test_wait();
    //test_fu0();
    //use_clock();
    //test_sequential_quick_sort();
    //drive2();
    //threadsafe_lookup_table_v1<int, int> lt;
    //auto sp = std::shared_ptr<int>();
    //std::cout << std::atomic_is_lock_free(&sp) << std::endl;
    //test_parallel_quick_sort();
    //test_parallel_find();
    //test_parallel_partial();
    //test_parallel_partial_sum();
    //test_thread_pool_accumulate();
    //test_parallel_quick_sort_with_threadpool();
    return 0;
}
