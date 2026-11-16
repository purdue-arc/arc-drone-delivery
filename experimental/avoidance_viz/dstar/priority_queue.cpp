// Imports
#include "header.hh"


// Swaps 2 positions
inline void NodePriorityQueue::swap(int index1, int index2) {
	node_t* tmp = nodes[index1];
	nodes[index1] = nodes[index2];
	nodes[index2] = tmp;
}

// Gets next size for expanding pq
inline int NodePriorityQueue::getNextSize() {
	this->size *= 2;
	return this->size;
}

// Gets next size for shrinking pq
inline int NodePriorityQueue::getPrevSize() {
	if (this->size > 10) {
		this->size /= 2;
	}
	return this->size;
}

// Returns the parent of current entry
inline int NodePriorityQueue::getParentIndex(int index) {
	if (index == 0) return -1;
	return (index - 1) / 2;
}

// Returns the left child of current entry
inline int NodePriorityQueue::getLeftChildIndex(int index) {
	int leftChildIndex = (2 * index) + 1;
	if (leftChildIndex >= this->insertIndex) {
		return -1;
	}
	return leftChildIndex;
}

// Returns the right child of current entry
inline int NodePriorityQueue::getRightChildIndex(int index) {
	int rightChildIndex = (2 * index) + 2;
	if (rightChildIndex >= this->insertIndex) {
		return -1;
	}
	return rightChildIndex;
}

// Returns the larger element
int NodePriorityQueue::getLargerElement(int index1, int index2) {
	if ((this->nodes[index1]->heuristic + (float) CNode::get_cost(this->nodes[index1])) >
		(this->nodes[index2]->heuristic + (float) CNode::get_cost(this->nodes[index2]))) {
		return index1;
	}
	return index2;
}

// Returns the larger element
int NodePriorityQueue::getSmallerElement(int index1, int index2) {
	if ((this->nodes[index1]->heuristic + (float) CNode::get_cost(this->nodes[index1])) <
		(this->nodes[index2]->heuristic + (float) CNode::get_cost(this->nodes[index2]))) {
		return index1;
	}
	return index2;
}

// Returns the smaller child
inline int NodePriorityQueue::getSmallerChild(int index) {
	int leftChildIndex = getLeftChildIndex(index),
		rightChildIndex = getRightChildIndex(index);

	// Error checking
	if ((leftChildIndex == -1) && (rightChildIndex == -1)) {
		return -1;
	}
	else if (leftChildIndex == -1) {
		return rightChildIndex;
	}
	else if (rightChildIndex == -1) {
		return leftChildIndex;
	}

	return getSmallerElement(leftChildIndex, rightChildIndex);
}

// Returns the larger child
inline int NodePriorityQueue::getLargerChild(int index) {
	int leftChildIndex = getLeftChildIndex(index),
		rightChildIndex = getRightChildIndex(index);

	// Error checking
	if ((leftChildIndex == -1) && (rightChildIndex == -1)) {
		return -1;
	}
	else if (leftChildIndex == -1) {
		return rightChildIndex;
	}
	else if (rightChildIndex == -1) {
		return leftChildIndex;
	}

	return getLargerElement(leftChildIndex, rightChildIndex);
}

// Bubble Up Method
void NodePriorityQueue::bubbleUp(int index) {
	int parentIndex = getParentIndex(index);
	while ((parentIndex != -1) && (getSmallerElement(parentIndex, index) == index)) {
		// Swapping
		swap(index, parentIndex);

		// Updating indices
		index = parentIndex;
		parentIndex = getParentIndex(index);
	}
}

// Bubble Down Method
void NodePriorityQueue::bubbleDown(int index) {
	int smallerChildIndex = getSmallerChild(index);
	while ((smallerChildIndex != -1) && (getLargerElement(index, smallerChildIndex) == index)) {
		// Swapping
		swap(index, smallerChildIndex);

		// Updating indices
		index = smallerChildIndex;
		smallerChildIndex = getSmallerChild(index);
	}
}

// Given exisitng elements, this heapifies them
void NodePriorityQueue::heapify() {
	for (int index = this->insertIndex / 2; index >= 0; index--) {
		heapify_helper(index);
	}
}

// Heapify helper
void NodePriorityQueue::heapify_helper(int index) {
	int smallerChild = getSmallerChild(index);
	if ((smallerChild != -1) && (getLargerElement(index, smallerChild) == index)) {
		swap(index, smallerChild);
		heapify_helper(smallerChild);
	}
}

// Constructor if no initial elements
NodePriorityQueue::NodePriorityQueue() {
	this->size = 10;
	this->insertIndex = 0;
	this->nodes = (node_t**)malloc(sizeof(node_t*) * this->size);
}

// Constructor is exisitng elements for faster runtime
NodePriorityQueue::NodePriorityQueue(int arr_size, node_t** ptrs) {
	this->size = 10;
	this->insertIndex = arr_size;
	while (this->size < arr_size) {
		getNextSize();
	}

	// Allocating space and moving ptrs to pq
	this->nodes = (node_t**)malloc(sizeof(node_t*) * size);
	for (int index = 0; index < arr_size; index++) {
		this->nodes[index] = ptrs[index];
	}

	// Converting initial entries into heap
	heapify();
}

// Returns whether the pq is empty
bool NodePriorityQueue::isEmpty() {
	return (this->insertIndex == 0);
}

// Adds a new element to the pq
void NodePriorityQueue::push(node_t* newNode) {
	// Expanding the array is necessary
	if (insertIndex == size) {
		this->nodes = (node_t **) realloc((void *) this->nodes,  sizeof(node_t *)  * getNextSize());
	}

	// Inserting into the pq
	nodes[insertIndex] = newNode;

	// Placing element into correct element in the pq
	bubbleUp(insertIndex);
	insertIndex++;
}

// Removes the min element from the heap
node_t* NodePriorityQueue::pop() {
	// Edge case
	if (isEmpty()) {
		return NULL;
	}

	// Swapping root with end element
	swap(0, --insertIndex);

	// Storing the min element
	node_t* min_node = this->nodes[insertIndex];

	// Placing swapped element in right spot
	bubbleDown(0);

	// Shrinking the array if necessary
	int prevSize = getPrevSize();
	if ((prevSize != 10) && ((double)insertIndex / (double)this->size) < .5) {
		this->nodes = (node_t**)realloc((void*)this->nodes, sizeof(node_t*) * prevSize);
	}

	return min_node;
}

int NodePriorityQueue::find(node_t* to_find) {
	for (int index = 0; index < insertIndex; index++) {
		if (nodes[index] == to_find) {
			return index;
		}
	}
	
	return -1;
}

void NodePriorityQueue::update_node_cost(node_t *to_update, int cost) {
	// Updating cost
	CNode::set_cost(to_update, cost);

	int index = find(to_update);
	if (index == -1) {
		// Element not found
		return;
	}

	// Updating location in pq
	bubbleUp(index);
	bubbleDown(index);
}

int NodePriorityQueue::get_size() {
	return this->insertIndex;
}


void NodePriorityQueue::print_heap() {
	printf("Heap (index: cost + heuristic): ");
	for (int index = 0; index < insertIndex; index++) {
		//  + CNode::get_heuristic(nodes[index])
		//printf("%d: %d, ", index, CNode::get_cost(nodes[index]));
		printf("%d -> %p, ", index, nodes[index]);
	}
	printf("\n");
}