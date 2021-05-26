#define ELEMENTS 10000
#include "Stack.hpp"



template <typename T>
void routine_push_pop(Stack<T>& stk)
{
	for(int i = 0; i < ELEMENTS; ++i)
	{
		stk.push(i);
		stk.pop();
	}

}






int main()
{
	Stack<int> stk;
	

	std::thread first_push(routine_push_pop<int>, std::ref(stk));
	std::thread second_push(routine_push_pop<int>, std::ref(stk));

	first_push.join();
	second_push.join();



	stk.print();
}