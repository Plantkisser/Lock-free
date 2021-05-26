#define ELEMENTS 1000
#include "Stack.hpp"



template <typename T>
void routine_push(Stack<T>& stk)
{
	for(int i = 0; i < ELEMENTS; ++i)
		stk.push(i);
}

template <typename T>
void routine_pop(Stack<T>& stk)
{
	for(int i = 0; i < ELEMENTS; ++i)
	{
		if (stk.pop() == nullptr)
			std::cout << "Error\n";
	}
}




int main()
{
	Stack<int> stk;
	

	std::thread first_push(routine_push<int>, std::ref(stk));
	std::thread second_push(routine_push<int>, std::ref(stk));

	first_push.join();
	second_push.join();

	std::thread first_pop(routine_pop<int>, std::ref(stk));
	std::thread second_pop(routine_pop<int>, std::ref(stk));

	first_pop.join();
	second_pop.join();


	stk.print();
}