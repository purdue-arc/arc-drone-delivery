// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Imports
#include <tuple>
#include <vector>

// Declaring namespaces
using namespace std;


// Node struct
typedef struct node {
	int x, y;
	/*
	 * Using final bit of cost to store occupancy(1 if occupied and 0
	 * otherwise). Final bit is unused becasue the blocks are stored
	 * in units of 2. Also, since units of 2, messing with final bit
	 * will still allow correct comparision. Finally, we can use "&"
	 * to get occupancy.
	 */
	unsigned int cost;
	float heuristic;
	float rhs;
} node_t;


// Creating a node wrapper/api class
class CNode {
public:
	// Functions
	static void initialize_node(node_t*, int, bool, int);

	static bool get_occupancy(node_t*);
	static void set_occupancy(node_t*, bool);

	static int get_cost(node_t*);
	static void set_cost(node_t*, int);

	static float get_heuristic(node_t*);
	static void set_heuristic(node_t*, float);
	
	static bool nodeCompare(node_t*, node_t*);
};


// For general priority queue syntax purposes
struct nodeComp {
	bool operator()(const node_t* n1, const node_t* n2) {
		return CNode::nodeCompare((node_t*)n1, (node_t*)n2);
	}
};