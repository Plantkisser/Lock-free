#define THREAD_NUMBER 10
#define ELEMENT_NUMBER 1000


#define ORDER_CHECKING

#include "skiplist.hpp"
#include <iostream>
#include <functional>
#include <vector>

std::atomic<bool> start;

void routine(Skiplist<int>& sl, int indx)
{
	while(!start);

	int step = THREAD_NUMBER;
	for (int i = indx; i < ELEMENT_NUMBER; i+= step)
	{
		while(!sl.remove(i));
	}
}


int main()
{
	start = false;
	Skiplist<int> sl;

	for (int i = 0; i < ELEMENT_NUMBER; ++i)
	{
		sl.add(i, i, i % 6 + 1);
	}

	std::vector<std::thread> vec;
	for (int i = 0; i < THREAD_NUMBER; ++i)
	{
		vec.push_back(std::thread(routine, std::ref(sl), i));
	}

	start = true;

	for (int i = 0; i < THREAD_NUMBER; ++i)
	{
		vec[i].join();
	}	

	sl.print();
	return 0;
}