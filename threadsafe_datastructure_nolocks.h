#pragma once
#ifndef threadsafe_datastructure_nolocks_h
#define threadsafe_datastructure_nolocks_h
#include <atomic>
#include <memory>

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
		do {
			temp = old_head;
			hp.store(old_head);
			old_head = head.load();
		} while (temp != old_head);
		while (old_head && !head.compare_exchange_strong(old_head, old_head->next);
		hp.store(nullptr);
		std::shared_ptr<T> res;
		if (old_head) {
			res.swap(old_head->data);
			if (outstanding_hazard_pointers_for(old_head) {
				reclaim_later(old_head);
			}
			else
				delete old_head;
			delete_nodes_with_no_hazards();
		}
		return res;
	}
}

#endif