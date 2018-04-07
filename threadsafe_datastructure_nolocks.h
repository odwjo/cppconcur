#pragma once
#ifndef threadsafe_datastructure_nolocks_h
#define threadsafe_datastructure_nolocks_h
#include <atomic>
#include <memory>
#include "hazard_pointer.h"

template <typename T>
class lock_free_stack_v1{
private:
	struct node {
		std::shared_ptr<T> data;
		node* next;
		node(T const& data_) :data(std::make_shared<T>(data_)) {}
	};
	std::atomic<node*> head;
	std::atomic<unsigned> thrds_on_pop;
	std::atomic<node*> to_be_deleted;
	static void delete_nodes(node* nodes) {
		while (nodes) {
			node *next = nodes->next;
			delete nodes;
			nodes = next;
		}
	}
	void try_reclaim(node *old_node) {
		if (thrds_on_pop == 1) {
			node* nodes_to_delete = to_be_deleted.exchange(nullptr);
			if (!--thrds_on_pop) {
				delete_nodes(nodes_to_delete);
			}
			else {
				//chain to the last, make to_be_deleted point to nodes_to_delete.
				chain_pending_nodes(nodes_to_delete);
			}
			delete old_head;
		}
		else {
			//
			chain_pending_node(old_node);
			--thrds_on_pop;
		}
	}
	//all chain function to to_be_deleted list
	void chain_pending_nodes(node *nodes) {
		node* last = nodes;
		while (node* const next = last->next)
			last = next;
		chain_pending_nodes(nodes, last);
	}
	void chain_pending_nodes(node *first, node *last) {
		last->next = to_be_deleted;
		//keep changing last->next to to_be_deleted, then first >> to_be_deleted
		//if (first == last), whick indicates to_be_deleted is correct 
		//to_be_deleted points to first node,when being used in other threads,
		//thrds_on_pop is be 1,so to_be_deleted is nullptr
		while (!to_be_deleted.compare_exchange_weak(last->next, first));
	}
	void chain_pending_node(node *n) {
		//thrds_on_pop is not 1
		chain_pending_nodes(n, n);
	}
public:
	void push(const T& value) {
		node *new_node = new node(value);
		new_node->next = head.load();
		while (!head.compare_exchange_weak(new_node->next, new_node));
	}
	std::shared_ptr<T> pop() {
		++ thrds_on_pop;
		node* old_head = head.load();
		//make sure >> head = head->next;setting old_head is correct
		while (old_head && !head.compare_exchange_weak(old_head, old_head->next));
		std::shared_ptr<T> res;
		if (old_head)
			res.swap(old_head->data);
		try_reclaim(old_head);//delete old_head
		return old_head ? old_head->data : std::shared_ptr<T>();
	}
};

template <typename T>
class lock_free_stack_v2 {
private:
	struct node {
		std::shared_ptr<T> data;
		node* next;
		node(T const& data_) :data(std::make_shared<T>(data_)) {}
	};
	std::atomic<node*> head;
public:
	void push(const T& value) {
		node *new_node = new node(value);
		new_node->next = head.load();
		//other thread may change >head< now
		while (!head.compare_exchange_weak(new_node->next, new_node));
	}
	std::shared_ptr<T> pop() {
		std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();
		node* old_head = head.load();
		node* temp;
		//make sure old_head == head,then set it to hazard
        do {//in case other threads pop
			temp = old_head;
			hp.store(old_head);
			old_head = head.load();
		} while (temp != old_head);
        //in case other threads push
        while (old_head && !head.compare_exchange_strong(old_head, old_head->next));

        hp.store(nullptr);
		std::shared_ptr<T> res;
		if (old_head) {
			res.swap(old_head->data);
            reclaim_later(old_head);
            /*
            *if (outstanding_hazard_pointers_for(old_head)){
            *	reclaim_later(old_head);
            *}
            *else
            *	delete old_head;
            */
            if(nodes_to_reclaim_count.load() == 2*max_hazard_pointers)
                delete_nodes_with_no_hazards();
		}
		return res;
	}
};

template <typename T>
class lock_free_stack_v3 {
private:
    struct node;
    struct counted_node_ptr{
        int external_count;
        node* ptr;
    };

    struct node {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;
        counted_node_ptr next;
        node(T const& data_) :
            data(std::make_shared<T>(data_)),
            internal_count(0){}
    };
    //use reference to make sure old_count is the head
    void increase_head_count(counted_node_ptr& old_counter){
        counted_node_ptr new_counter;
        do{
            new_counter = old_counter;
            ++ new_counter.external_count;
        }while(!head.compare_exchange_strong(old_counter,new_counter,
                std::memory_order_acquire, std::memory_order_relaxed));

        old_counter.external_count = new_counter.external_count;
    }

    std::atomic<counted_node_ptr> head;//only need to count head;
public:
    ~lock_free_stack(){
        while(pop());
    }

    void push(const T& value) {
        counted_node_ptr new_node;
        new_node.ptr = new node(data);
        new_node.external_count = 1;
        new_node.ptr->next = head.load(std::memory_order_relaxed);
        //other thread may change >head< now
        while (!head.compare_exchange_strong(new_node.ptr->next, new_node,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed));
    }
    std::shared_ptr<T> pop() {
        counted_node_ptr old_head = head.load(std::memory_order_relaxed);
        for(;;){
            increase_head_count(old_head);//in case other threads delete it
            node* const ptr = old_head.ptr;
            if(!ptr){//empty stack
                return std::shared_ptr<T>();
            }//if success,old_head gets head, else find head again
            //and ex_count is 2 + inter_count,and then won't affect stack
            if(head.compare_exchange_strong(old_head, ptr->next,
                                            std::memory_order_relaxed)){
                //only one thread can be here
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                int const count_increase = old_head.external_count-2;
                if(ptr->internal_count.fetch_add(count_increase,
                                                 std::memory_order_release) ==
                                                 -count_increase)
                    //if if() failed,means some threads hold ptr within for
                    delete ptr;
                return res;
            }else if(ptr->internal_count.fetch_add(-1,
                                     std::memory_order_relaxed) == 1)//part of release
                //do leave sub(to earlier ex add) then check
                //if == is true,means no other threas reference
                ptr->internal_count.load(std::memory_order_acquire);
                delete ptr;
        }
    }
};

template <typename T>
class lock_free_queue_v1_broke{//single producer,single consumer
private:
    struct node{
        std::shared_ptr<T> data;
        node* next;
        node():next(nullptr){}
    };
    std::atomic<node*> head;
    std::atomic<node*> tail;
    node* pop_head(){
        node* const old_head = head.load();
        if(old_head == tail.load()){
            return nullptr;
        }
        head.store(old_head->next);
        return old_head;
    }
public:
    lock_free_queue_v1_broke():head(new node), tail(head.load()){}

    lock_free_queue_v1_broke(const lock_free_queue_v1_broke&)=delete;
    lock_free_queue_v1_broke& operator=(const lock_free_queue_v1_broke&)=delete;

    ~lock_free_queue(){
        while(node* const old_head = head.load()){
            head.store(old_head->next);
            delete old_head;
        }
    }
    std::shared_ptr<T> pop(){
        node* old_head = pop_head();
        if(!old_head)
            return std::shared_ptr<T>();
        std::shared_ptr<T> const res(old_head->data);
        delete old_head;//old_head may be tail,which is used to be assigned
        return res;
    }
    void push(T new_value){
        std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
        node* p = new node;//new dummy node

        node* const old_tail = tail.load();
        old_tail->data.swap(new_data);
        old_tail->next = p;
        tail.store(p);
    }
};

template<typename T>
class lock_free_queue_v2{
private :
    struct node;
    struct counted_node_ptr{
        int external_count;
        node* ptr;
    };
    std::atomic<counted_node_ptr> head;
    std::atomic<counted_node_ptr> tail;

    struct node_counter{
        unsigned internal_count:30;
        unsigned external_counters:2;
    };

    struct node{
        std::atomic<T*> data;
        std::atomic<node_counter> count;
        counted_node_ptr next;

        node(){
            node_counter new_count;
            new_count.internal_count = 0;
            new_count.external_counters = 2;
            count.store(new_count);

            next.ptr = nullptr;
            next.external_count = 0;
        }
        void release_ref(){
            //every thread touches the node will
            //decrease the internal_counter
            node_conter old_counter =
                    count.load(std::memory_order_relaxed);
            node_counter new_counter;
            do{
                new_counter = old_counter;
                --new_counter.internal_count;
            }while(!count.compare_exchange_strong(
                       old_counter,new_counter,
                       std::memory_order_acquire,
                       std::memory_order_relaxed));
            if(!new_counter.internal_count &&
               !new_counter.external_counters){
                delete this;
            }
        }
    };
    static void increase_external_count(
            std::atmic<counted_node_ptr>& counter,
            counted_node_ptr& old_counter){
        //every thread touches the node will
        //increase the external_count
        counted_node_ptr new_counter;
        do{
            new_counter = old_counter;
            ++new_counter.external_count;
        }while(!counter.compare_exchange_strong(
                   old_counter, new_counter,
                   std::memory_order_acquire,
                   std::memory_order_relaxed));
        old_counter.external_count = new_counter.external_count;
    }
    static void free_external_counter(counter_node_ptr& old_node_ptr){
        node* const ptr = old_node_ptr.ptr;
        //initialize external_count = internal_count + 2
        int const count_increase = old_node_ptr.external_count-2;

        node_counter old_counter = ptr->count.load(std::memory_order_relaxed);
        node_counter new_counter;
        do{//when catch the ptr->count
            new_counter = old_counter;
            //side effect of this thread
            --new_counter.external_counters;//change in-node ex_counter
            new_counter.internal_count += count_increase;
        }while(!counter.compare_exchange_strong(
                   old_counter, new_counter,
                   std::memory_order_acquire,
                   std::memory_order_relaxed));
        if(!new_counter.internal_count &&
                !new_counter.external_counters){
            delete ptr;
        }
    }

public:
    lock_free_queue_v2(){
        counted_node_ptr new_counted_node;
        new_counted_node.external_count = 1;
        new_counted_node.ptr = new node;
        head.store(new_counted_node);
        tail.store(new_counted_node);
    }

    void push(T new_value){
        std::unique_ptr<T> new_data(new T(new_value));
        counted_node_ptr new_next;
        new_next.ptr = new node;
        new_next.external_count = 1;
        counted_node_ptr old_tail = tail.load();

        for(;;){
            increase_external_count(tail,old_tail);

            T* old_data = nullptr;
            if(old_tail.ptr->data.compare_exchange_strong(
                        old_data, new_data.get())){//one thread enter
                old_tail.ptr->next = new_next;
                old_tail = tail.exchange(new_next);
                free_external_counter(old_tail);
                new_data.release();
                break;
            }
            old_tail.ptr->release_ref();//be waited by free_external_counter()
        }
    }
    std::unique_ptr<T> pop(){
        counted_node_ptr old_head = head.loal(std::memory_order_relaxed);
        for(;;){
            increase_external_count(head, old_head);
            node* const ptr = old_head.ptr;
            if(ptr == tail.load().ptr){
                ptr->release_ref();//be waited by free_external_counter()
                return std::unique_ptr<T>();
            }
            if(head.compare_exchange_strong(old_head, ptr->next)){
                T* const res = ptr->data.exchange(nullptr);
                free_external_counter(old_head);
                return std::unique_ptr<T>(res);
            }
            ptr->release_ref();//be waited by free_external_counter
        }
    }
};


#endif
