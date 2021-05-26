#include <atomic>
#include <memory>
#include <thread>
#include <iostream>
#include <functional>

template <typename T>
struct Node
{
	std::shared_ptr<T> data;
	Node<T>* next;

	Node(const T& data_):
	data(std::make_shared<T>(data_))
	{}
};


template <typename T>
class Stack
{
private:
	std::atomic<Node<T>*> head_;
	std::atomic<unsigned> reader_cnt; 		// number of thread which use pop() and may be reading node
	std::atomic<Node<T>*> to_be_deleted_; 	// list for nodes which should be deleted when it is possible

	void return_delete_list(Node<T>* head_node);
public:
	Stack();
	~Stack();
	
	void push(const T& data);
	std::shared_ptr<T> pop(); 				// we use shared_ptr to avoid exceptions during stack changing
	void retire(Node<T>* node);

	void print();
};

template <typename T>
void Stack<T>:: push(const T& data)
{
	Node<T>* new_node = new Node<T>(data);
	new_node->next = head_;
	while(!head_.compare_exchange_weak(new_node->next, new_node));
}

template <typename T>
std::shared_ptr<T> Stack<T>:: pop()
{
	reader_cnt++;
	Node<T>* old_head = head_;
	while(old_head && !head_.compare_exchange_weak(old_head, old_head->next));

	std::shared_ptr<T> res;
	if (old_head)
	{
		res = old_head->data;
		retire(old_head);
	}
	return res;
}


template <typename T>
void Stack<T>:: return_delete_list(Node<T>* head_node)
{
	Node<T>* node = head_node;
	while(node)
	{
		Node<T>* tmp = node;
		node = node->next;

		tmp->next = to_be_deleted_;
		while(!to_be_deleted_.compare_exchange_weak(tmp->next, tmp));
	}

}

template <typename T>
void Stack<T>:: retire(Node<T>* node)
{
	if (reader_cnt == 1)
	{
		Node<T>* head_node = to_be_deleted_;
		while(!to_be_deleted_.compare_exchange_weak(head_node, nullptr));
		if (reader_cnt != 1)
		{
			return_delete_list(head_node);
			node->next = to_be_deleted_;
			while(!to_be_deleted_.compare_exchange_weak(node->next, node));
		}
		else
		{
			while(head_node)
			{
				Node<T>* tmp = head_node;
				head_node = head_node->next;
				delete tmp;
			}
			delete node;
		}

	}
	else
	{
		node->next = to_be_deleted_;
		while(!to_be_deleted_.compare_exchange_weak(node->next, node));
	}

	reader_cnt--;
}

template <typename T>
void Stack<T>:: print()
{
	Node<T>* tmp = head_;
	unsigned cnt = 0;
	while(tmp)
	{
		std::cout << *(tmp->data) << " ";
		tmp = tmp->next;
		cnt++;
	}
	std::cout << std::endl;
	std::cout << "Result: " << cnt << std::endl;
}




template <typename T>
Stack<T>:: Stack():
head_(nullptr),
reader_cnt(0),
to_be_deleted_(nullptr)
{}

template <typename T>
Stack<T>:: ~Stack()
{
	Node<T>* curr = head_;

	while(curr)
	{
		Node<T>* tmp = curr;
		curr = curr->next;
		delete tmp;
	}

	curr = to_be_deleted_;
	while(curr)
	{
		Node<T>* tmp = curr;
		curr = curr->next;
		delete tmp;	
	}
}


