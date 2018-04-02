#include <atomic>
#include <vector>
#include <iostream>
#include <chrono>
#include <thread>

std::atomic<int> z1;
std::atomic<bool> x1,y1;

void write_x(){
    x1.store(true,std::memory_order_release);
}
void write_y(){
    y1.store(true,std::memory_order_release);
}
void read_x_then_y(){
    while(!x1.load(std::memory_order_acquire));
    if(y1.load(std::memory_order_acquire))
        ++ z1;
}
void read_y_then_x(){
    while(!y1.load(std::memory_order_acquire));
    if(x1.load(std::memory_order_acquire))
        ++ z1;
}
void drive(){
    x1 = false;
    y1 = false;
    z1 = 0;
    std::thread a(write_x);
    std::thread b(write_y);
    std::thread c(read_x_then_y);
    std::thread d(read_y_then_x);
    a.join();
    b.join();
    c.join();
    d.join();
    std::cout << z1.load() << std::endl;
}

std::atomic<int> z2;
std::atomic<bool> x2,y2;

void write_x_then_y2(){
    x2.store(true,std::memory_order_relaxed);//happens before operation
    //std::atomic_thread_fence(std::memory_order_release);
    y2.store(true,std::memory_order_release);//seen by fence(acquire)
}
//a load before acquire_fence sees a store after the release fence
void read_y_then_x2(){
    while(!y2.load(std::memory_order_acquire));//synchroniazed by fence
    //std::atomic_thread_fence(std::memory_order_acquire);
    if(x2.load(std::memory_order_relaxed))
        ++ z2;
}
void drive2(){
    x2 = false;
    y2 = false;
    z2 = 0;
    std::thread a(write_x_then_y2);
    std::thread b(read_y_then_x2);

    a.join();
    b.join();

    std::cout << z2.load() << std::endl;
}

std::atomic<int> x(0), y(0),z(0);
std::atomic<bool> go(false);

unsigned const loop_count = 10;

struct read_values{
    int x,y,z;
};

read_values values1[loop_count];
read_values values2[loop_count];
read_values values3[loop_count];
read_values values4[loop_count];
read_values values5[loop_count];

void increment(std::atomic<int>* var_to_inc,
               read_values* values){
    while(!go)
        std::this_thread::yield();
    for(unsigned i = 0;i < loop_count;++ i){
        values[i].x = x.load(std::memory_order_relaxed);
        values[i].y = y.load(std::memory_order_relaxed);
        values[i].z = z.load(std::memory_order_relaxed);
        var_to_inc -> store(i+1, std::memory_order_relaxed);
        std::this_thread::yield();
    }
}
void read_vals(read_values* values){
    while(!go)
        std::this_thread::yield();
    for(unsigned i = 0;i < loop_count;++ i){
        values[i].x = x.load(std::memory_order_relaxed);
        values[i].y = y.load(std::memory_order_relaxed);
        values[i].z = z.load(std::memory_order_relaxed);
        std::this_thread::yield();
    }
}
void print(read_values* v){
    std::cout << "\n";
    for(unsigned i = 0;i < loop_count;++ i){
        if(i)
            std::cout << ",";
        else std::cout << " ";
        std::cout << "("<< v[i].x << "," << v[i].y << "," <<
                     v[i].z << "," << ")" << "\n";
    }
}

void drive3(){
    std::thread t1(increment, &x, values1);
    std::thread t2(increment, &y, values2);
    std::thread t3(increment, &z, values3);
    std::thread t4(read_vals, values4);
    std::thread t5(read_vals, values5);
    go = true;

    t5.join();
    t4.join();
    t3.join();
    t2.join();
    t1.join();
    //std::this_thread::sleep_for(std::chrono::seconds(2));

    print(values1);
    print(values2);
    print(values3);
    print(values4);
    print(values5);

}
/*
void reader_thread(){
    while(!data_ready.load()){
        std::this_thread::sleep(std::chrono::milliseconds(5));
    }
    std::cout << "The answer = " << data[0] << std::endl;
}
void writer_thread(){
    data.push_back(42);
    data_ready = true;
}

class spinlock_mutex{
    std::atomic_flag flag;
public:
    spinlock_mutex():
        flag(ATOMIC_FLAG_INIT){}
    void lock(){
        while(flag.test_and_set(std::memory_order_acquire));
    }
    void unlock(){
        flag.clear(std::memory_order_release);
    }
};


void use_atomic(){
    std::atomic<bool> b;
    bool x = b.load(std::memory_order_acquire);
    b.store(true);
    x = b.exchange(false, std::memory_order_acq_rel);

    bool expected = false;
    //extern atomic<bool> b;
    //this.compare_exchange_weak(expected, desired);
    //(expected == *this) ? (*this = desired):(expected = *this);
    while(!b.compare_exchange_weak(expected, true) && !expected);


}

void atomic_in_action(){
    std::atomic_flag f = ATOMIC_FLAG_INIT;
    //store:: memory_order_relaxed,
            //memory_order_release, memory_order_seq_cst
    //load :: memory_order_relaxed, memory_order_consume
            //memory_order_acquire, memory_order_seq_cst
    //read-modify-write::
            //memory_order_relaxed, memory_order_consume
            //memory_order_acquire, memory_order_seq_cst
            //memory_order_release, memory_order_acq_rel

}
*/
