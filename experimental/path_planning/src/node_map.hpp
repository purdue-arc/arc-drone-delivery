// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Imports
#include <algorithm>

// Local Imports
#include "constants.hpp"
#include "node.hpp"
#include "util.hpp"

// Declaring Namespace
using namespace std;


// For easier use/debugging of a node map
class NodeMap {
public:
	static node_t** initialize_node_map(int, int, bool**, int, int);
	static void free_node_map(node_t**);
	static void print_cost_matrix(node_t**, int, int);
	static void print_occupancy_matrix(node_t**, int, int);
	static void print_heuristic_matrix(node_t**, int, int);
	static bool outOfBounds(node_t**, int, int, int, int);
	static vector<node*> get_neighbors(node_t**, int, int, int, int);
	static vector<tuple<int, int>> trace_path(node_t**, int, int, int, int);
	static void print_generated_path(node_t**, vector<tuple<int, int>>, int, int);
};