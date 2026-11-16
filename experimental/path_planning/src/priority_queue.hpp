// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Local Imports
#include "node.hpp"

// Declaring namespaces
using namespace std;


// Custom priority queue
class NodePriorityQueue {
private:
	// Attributes
	int size, insertIndex;
	node_t** nodes;

	// Helper functions
	inline void swap(int, int);

	inline int getNextSize();
	inline int getPrevSize();

	inline int getParentIndex(int);
	inline int getLeftChildIndex(int);
	inline int getRightChildIndex(int);

	inline int getLargerElement(int, int);
	inline int getSmallerElement(int, int);

	inline int getSmallerChild(int);
	inline int getLargerChild(int);

	void bubbleUp(int);
	void bubbleDown(int);

	void heapify();
	void heapify_helper(int);

public:
	// Constructors
	NodePriorityQueue();
	NodePriorityQueue(int, node_t**);

	// API Functions
	bool isEmpty();
	void push(node_t*);
	node_t* pop();
	int find(node_t*);
	void update_node_cost(node_t*, int);
	int get_size();
	void print_heap();
};