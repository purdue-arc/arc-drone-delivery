// Imports
#include "tests/pq_tests.hpp"


// Tests the heapify function
void pq_heapify(bool print=false) {
	printf("Testing heapify:\n");

	node_t* test = (node_t*)malloc(sizeof(node_t) * 8);
	node_t** test_ptrs = (node_t**)malloc(sizeof(node_t*) * 8);
	for (int index = 0; index < 8; index++) {
		int cost = 10 * (8 - index);
		CNode::set_cost(&test[index], cost);
        CNode::set_occupancy(&test[index], 0);
		CNode::set_heuristic(&test[index], 0);
		test_ptrs[index] = &test[index];
	}

	NodePriorityQueue pq = NodePriorityQueue(8, test_ptrs);

    if (print) {
        pq.print_heap();
    }

	int prev_score = -1;
    while (!pq.isEmpty()) {
        node_t* ptr = pq.pop();
		int new_score = CNode::get_cost(ptr) + CNode::get_heuristic(ptr);

        if (print) {
            printf("Ptr (cost + heuristic) is: %d\n", new_score);
        }

        assert(new_score >= prev_score);
    }

    printf("Passed!\n");
}


// Tests adding elements
void pq_add(bool print=false) {
	printf("Testing add:\n");

	node_t* test = (node_t*)malloc(sizeof(node_t) * 5);
	node_t** test_ptrs = (node_t**)malloc(sizeof(node_t*) * 5);
	for (int index = 0; index < 5; index++) {
		int cost = 10 * (5 - index);
		CNode::set_cost(&test[index], cost);
        CNode::set_occupancy(&test[index], 0);
		CNode::set_heuristic(&test[index], 0);
		test_ptrs[index] = &test[index];
	}

	NodePriorityQueue pq = NodePriorityQueue(5, test_ptrs);

    if (print) {
        pq.print_heap();
    }

	test = (node_t*)malloc(sizeof(node_t));
	CNode::initialize_node(test, 0, false, 0);
	pq.push(test);

	test = (node_t*)malloc(sizeof(node_t));
	CNode::initialize_node(test, 100, false, 0);
	pq.push(test);

    if (print) {
        pq.print_heap();
    }

	int prev_score = -1;
    while (!pq.isEmpty()) {
        node_t* ptr = pq.pop();
		int new_score = CNode::get_cost(ptr) + CNode::get_heuristic(ptr);

        if (print) {
            printf("Ptr (cost + heuristic) is: %d\n", new_score);
        }

        assert(new_score >= prev_score);
    }

    printf("Passed!\n");
}


// Tests array resizing
void pq_resize(bool print=false) {
	printf("Testing resize:");

	node_t* test = (node_t*)malloc(sizeof(node_t) * 8);
	node_t** test_ptrs = (node_t**)malloc(sizeof(node_t*) * 8);
	for (int index = 0; index < 8; index++) {
		int cost = 10 * (8 - index);
		CNode::set_cost(&test[index], cost);
        CNode::set_occupancy(&test[index], 0);
		CNode::set_heuristic(&test[index], 0);
		test_ptrs[index] = &test[index];
	}

	NodePriorityQueue pq = NodePriorityQueue(8, test_ptrs);

    if (print) {
	    pq.print_heap();
    }

	for (int i = 0; i < 15; i++) {
        if (print) {
            printf("Adding: %d\n", i);
        }
		
		test = (node_t*)malloc(sizeof(node_t));
		CNode::initialize_node(test, i, false, 0);
		pq.push(test);
	}

    if (print) {
	    pq.print_heap();
    }

	
	int prev_score = -1;
    while (!pq.isEmpty()) {
        node_t* ptr = pq.pop();
		int new_score = CNode::get_cost(ptr) + CNode::get_heuristic(ptr);

        if (print) {
            printf("Ptr (cost + heuristic) is: %d\n", new_score);
        }

        assert(new_score >= prev_score);
    }

    printf("Passed!\n");
}


// Tests pop
void pq_pop(bool print=false) {
	printf("Testing pop:");

	node_t* test = (node_t*)malloc(sizeof(node_t) * 8);
	node_t** test_ptrs = (node_t**)malloc(sizeof(node_t*) * 8);
	for (int index = 0; index < 8; index++) {
		int cost = 10 * (8 - index);
		CNode::set_cost(&test[index], cost);
        CNode::set_occupancy(&test[index], 0);
		CNode::set_heuristic(&test[index], 0);
		test_ptrs[index] = &test[index];
	}

	NodePriorityQueue pq = NodePriorityQueue(8, test_ptrs);

    if (print) {
        pq.print_heap();
    }

    int prev_score = -1;
    while (!pq.isEmpty()) {
        node_t* ptr = pq.pop();
		int new_score = CNode::get_cost(ptr) + CNode::get_heuristic(ptr);

        if (print) {
            printf("Ptr (cost + heuristic) is: %d\n", new_score);
        }

        assert(new_score >= prev_score);
    }

    printf("Passed!\n");
}


// Runs all the pq tests
int main() {
	pq_heapify();
	pq_add();
	pq_pop();
	pq_resize();
}