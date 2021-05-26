#include <atomic>
#include <thread>
#include <iostream>
#include "hazard_ptr.h"

#define MAX_HEIGHT 20
#define MASK 0xfffffffffffffffe


struct retired_tower
{
	void* nodes;
	unsigned height;
};

std::atomic <int> dcount;
retired_tower dlist[DLIST_SIZE];
template <typename T>
bool retire(T* arr_nodes, unsigned height)
{
	std::thread::id tmp;
	bool is_read = find_hp(arr_nodes, height);
	int n = 0;	
	if (!is_read)
	{	
		delete arr_nodes[0].data;
		delete[] (arr_nodes);
	}
	else
	{
		retired_tower new_tower;
		new_tower.nodes = arr_nodes;
		new_tower.height = height;
		do
		{
			n = dcount.load();
			if (n == DLIST_SIZE)
				throw(std::runtime_error("dlist full"));
		} while(!dcount.compare_exchange_strong(n, n+1)); 
		dlist[n+1] = new_tower;
	}


	if (n+1 == DLIST_SIZE)
	{
		for(int i = 0; i < DLIST_SIZE; ++i)
		{
			if (!find_hp(static_cast<T*> (dlist[i].nodes), dlist[i].height))
			{
				delete[] (static_cast<T*> (dlist[i].nodes));
				dlist[i].nodes = nullptr;
			}
		}
	}
	return true;
}




template <typename T>
struct Node
{
	std::atomic<Node<T>*> next;
	unsigned key;
	T* data;
	unsigned height; // height of the tower
};

template<typename T>
T* to_ptr(T* ptr)
{
	return (T*)((long)ptr & MASK);
}

template<typename T>
long check_ptr(T* ptr)
{
	return (long)ptr & 1;
}

template<typename T>
T* mark_ptr(T* ptr)
{
	return (T*)((long)ptr | 1);
}




template <typename T>
class Skiplist
{
private:
	Node<T> *head_, *tail_;
	bool add_node(Node<T>* new_node, unsigned level);
	bool find_node(int key, unsigned level, Node<T>** left, Node<T>** right);
public:
	bool add(const T& data, int key, unsigned height = 1);
	bool remove(int key, bool debug = false);
	bool find(int key, T* result);
	void print();



	Skiplist():
	head_(new Node<T> [MAX_HEIGHT]),
	tail_(new Node<T> [MAX_HEIGHT])
	{
		for(int i = 0; i < MAX_HEIGHT; ++i)
		{
			head_[i].next = &tail_[i];
			tail_[i].next = nullptr;
			head_[i].height = MAX_HEIGHT;
			tail_[i].height = MAX_HEIGHT;
			head_[i].key=tail_[i].key=100001;
		}
	}
	~Skiplist()
	{
		Node<T>* ptr = head_[0].next.load();
		Node<T>* prev = ptr; 
		while (ptr != &tail_[0])
		{
			ptr = to_ptr(ptr->next.load());
			delete prev->data;
			delete[] prev;
			prev = ptr;
		}
		delete[] head_;
		delete[] tail_;

		for (int i = 0; i < DLIST_SIZE; ++i)
			if (dlist[i].nodes)
			{
				delete ((Node<T>*) dlist[i].nodes)->data;
				delete[] (Node<T>*) dlist[i].nodes;	
			}
	}
};

template <typename T>
bool Skiplist<T>:: find_node(int key, unsigned level, Node<T>** left, Node<T>** right)
{
	unsigned curr_level = MAX_HEIGHT-1;
	static int count = 0;
	
	HazardPointers* hps = get_hazard_pointers_current_thread();

	Node<T> *backward, *forward;
	bool restart = true;
	
	while(1)
	{
		if (restart)
		{
			hps->nodes[0] = (void*)&head_[curr_level];
			hps->nodes[1] = (void*)head_[curr_level].next.load();
			hps->nodes[2] = nullptr;

			forward = (Node<T>*) hps->nodes[1].load();
			backward = (Node<T>*) hps->nodes[0].load();

			curr_level = MAX_HEIGHT-1;
			restart = false;	
		}

		// delete marked nodes
		if (check_ptr(forward->next.load()))
		{
			hps->nodes[2] = (void*) to_ptr(forward->next.load());
			Node<T>* tmp = (Node<T>*) hps->nodes[2].load();
			
			
			if (!backward->next.compare_exchange_strong(forward, tmp))
			{
				//std::cout << backward->key << std::endl;
				restart = true;
				continue;
			}
			else if (curr_level == 0)
			{
				//count++;
				//std::cout << count << std::endl;
				retire(forward, forward->height);
			}
			forward = tmp;
			hps->nodes[1] = hps->nodes[2].load();
			hps->nodes[2] = nullptr;
			restart = false;
			continue;
		}


		//std::cout << curr_level << "\n";
		if ((forward == &(tail_[curr_level]) || forward->key > key) && curr_level != level)
		{
			hps->nodes[0].store((void*)(backward - 1));
			backward = (Node<T>*) hps->nodes[0].load();
			hps->nodes[1].store(to_ptr((void*)(backward->next.load())));
			forward = (Node<T>*) hps->nodes[1].load();
			curr_level--;
			continue;
		}
		else if (forward->key < key && forward != &(tail_[curr_level]))
		{
			hps->nodes[0].store((void*)(forward));
			backward = (Node<T>*) hps->nodes[0].load();
			hps->nodes[1].store((void*)to_ptr(forward->next.load()));
			forward = (Node<T>*) hps->nodes[1].load();
			continue;
		}

		if (forward == &(tail_[curr_level]) || forward->key != key)
		{
			*left = backward;
			*right = forward;
			return false;
		}
		else 
		{
			*left = backward;
			hps->nodes[2] = to_ptr(forward->next.load());
			*right =(Node<T>*) hps->nodes[2].load();
			return true;
		}
	}
}



template <typename T>
bool Skiplist<T>:: add_node(Node<T>* new_node, unsigned level)
{
	unsigned curr_level = MAX_HEIGHT-1;
	
	HazardPointers* hps = get_hazard_pointers_current_thread();

	Node<T> *backward, *forward;

	while(1)
	{
		if (find_node(new_node->key, level, &backward, &forward))
		{
			return false;
		}
		else
		{
			new_node->next = forward;

			if (!backward->next.compare_exchange_strong(forward, new_node))
				continue;
			else
			{
				hps->nodes[0] = nullptr;
				hps->nodes[1] = nullptr;
				hps->nodes[2] = nullptr;
 				return true;
			}
		}
	}
}




template <typename T>
bool Skiplist<T>:: add(const T& data, int key, unsigned height)
{
	if (height == 0)
		return false;


	Node<T>* tower = new Node<T> [height];
	T* data_ptr = new T (data);
	for(int i = 0; i < height; ++i)
	{
		tower[i].key = key;
		tower[i].data = data_ptr;
		tower[i].height = height;

		if (!add_node(tower + i, i))
		{
			std::cout <<"Error\n";
			return false;
		}
	}
	return true;
}


template <typename T>
bool Skiplist<T>:: remove(int key, bool debug)
{
	HazardPointers* hps = get_hazard_pointers_current_thread();

	Node<T> *prev, *found;

	bool is_found = false;
	for (int curr_level = MAX_HEIGHT-1; curr_level >= 0; --curr_level)
	{
		if (find_node(key, curr_level, &prev, &found))
		{
			if (!is_found && curr_level != ((Node<T>*) hps->nodes[1].load())->height - 1)
			{
				return false;
			}

			is_found = true;
			if (!((Node<T>*) hps->nodes[1].load())->next.compare_exchange_strong(found, (Node<T>*) mark_ptr(((Node<T>*) hps->nodes[1].load())->next.load())))
			{
				if (check_ptr(((Node<T>*)(hps->nodes[1].load()))->next.load()))
				{
					return false;
				}
				else
				{
					std::cout << "res\n";
					++curr_level;
				}
			}
		}
	}
	return true;
}


template <typename T>
bool Skiplist<T>:: find(int key, T* result)
{
	HazardPointers *hps = get_hazard_pointers_current_thread();

	Node<T> *backward, *forward;

	if (!find_node(key, 0, &backward, &forward))
		return false;
	else
	{
		if (result)
			*result = *(((Node<T>*) hps->nodes[1].load())->data);
		return true;
	}
}


template <typename T>
void Skiplist<T>:: print()
{
	int layer = 0;
	Node<T>* ptr = head_[layer].next.load(); 
	int prev_key = ptr->key;
	int i = 0;
	while (ptr != &tail_[layer])
	{
		#ifdef ORDER_CHECKING
		if (prev_key > ptr->key)
			return;
		#endif
		if (!check_ptr(ptr->next.load()))
		{
			++i;
			std::cout<<"Value: " << *ptr->data << " Key: " << ptr->key << " Height: " << ptr->height << std::endl;
		}
		
		ptr = to_ptr(ptr->next.load());
	}
	std::cout << "SIZE: " << i << std::endl;
}