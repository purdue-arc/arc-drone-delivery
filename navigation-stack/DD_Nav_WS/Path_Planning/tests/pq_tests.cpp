// Imports
#include "../header.hh"


// Tests the heapify function
void pq_heapify() {
	printf("Testing heapify:\n");

	node_t* test = (node_t*)malloc(sizeof(node_t) * 8);
	node_t** test_ptrs = (node_t**)malloc(sizeof(node_t*) * 8);
	for (int index = 0; index < 8; index++) {
		int cost = 10 * (8 - index);
		CNode::set_cost_and_occupancy(&test[index], cost, 0);
		CNode::set_heuristic(&test[index], 0);
		test_ptrs[index] = &test[index];
	}

	NodePriorityQueue pq = NodePriorityQueue(8, test_ptrs);
	pq.print_heap();

	while (!pq.isEmpty()) {
		node_t* ptr = pq.pop();
		printf("Ptr (cost + heuristic) is: %d\n", CNode::get_cost(ptr) + CNode::get_heuristic(ptr));
	}
}

// Tests adding elements
void pq_add() {
	printf("Testing add:\n");

	node_t* test = (node_t*)malloc(sizeof(node_t) * 5);
	node_t** test_ptrs = (node_t**)malloc(sizeof(node_t*) * 5);
	for (int index = 0; index < 5; index++) {
		int cost = 10 * (5 - index);
		CNode::set_cost_and_occupancy(&test[index], cost, 0);
		CNode::set_heuristic(&test[index], 0);
		test_ptrs[index] = &test[index];
	}

	NodePriorityQueue pq = NodePriorityQueue(5, test_ptrs);
	pq.print_heap();

	test = (node_t*)malloc(sizeof(node_t));
	CNode::initialize_node(test, 0, false, 0);
	pq.push(test);

	test = (node_t*)malloc(sizeof(node_t));
	CNode::initialize_node(test, 100, false, 0);
	pq.push(test);
	pq.print_heap();

	while (!pq.isEmpty()) {
		node_t* ptr = pq.pop();
		printf("Ptr (cost + heuristic) is: %d\n", CNode::get_cost(ptr) + CNode::get_heuristic(ptr));
	}
}


// Tests array resizing
void pq_resize() {
	printf("Testing resize:\n");

	node_t* test = (node_t*)malloc(sizeof(node_t) * 8);
	node_t** test_ptrs = (node_t**)malloc(sizeof(node_t*) * 8);
	for (int index = 0; index < 8; index++) {
		int cost = 10 * (8 - index);
		CNode::set_cost_and_occupancy(&test[index], cost, 0);
		CNode::set_heuristic(&test[index], 0);
		test_ptrs[index] = &test[index];
	}

	NodePriorityQueue pq = NodePriorityQueue(8, test_ptrs);
	pq.print_heap();

	for (int i = 0; i < 15; i++) {
		printf("Adding: %d\n", i);
		test = (node_t*)malloc(sizeof(node_t));
		CNode::initialize_node(test, i, false, 0);
		pq.push(test);
		pq.print_heap();
	}

	
	while (!pq.isEmpty()) {
		node_t* ptr = pq.pop();
		printf("Ptr (cost + heuristic) is: %d\n", CNode::get_cost(ptr) + CNode::get_heuristic(ptr));
	}
}

// Tests pop
void pq_pop() {
	printf("Testing pop:\n");

	node_t* test = (node_t*)malloc(sizeof(node_t) * 8);
	node_t** test_ptrs = (node_t**)malloc(sizeof(node_t*) * 8);
	for (int index = 0; index < 8; index++) {
		int cost = 10 * (8 - index);
		CNode::set_cost_and_occupancy(&test[index], cost, 0);
		CNode::set_heuristic(&test[index], 0);
		test_ptrs[index] = &test[index];
	}

	NodePriorityQueue pq = NodePriorityQueue(8, test_ptrs);
	pq.print_heap();

	while (!pq.isEmpty()) {
		node_t* ptr = pq.pop();
		printf("Ptr (cost + heuristic) is: %d\n", CNode::get_cost(ptr) + CNode::get_heuristic(ptr));
		pq.print_heap();
	}
}


// Runs all the pq tests
void test_pq() {
	pq_heapify();
	pq_add();
	pq_pop();
	pq_resize();
}