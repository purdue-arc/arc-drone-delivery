// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Imports
#include <math.h>
#include <vector>
#include <tuple>
#include <unordered_set>

// Local Imports
#include "src/node.hpp"
#include "src/node_map.hpp"
#include "src/priority_queue.hpp"

// Namespaces
using namespace std;


// Defines another approach to path planning
class Dijkstra {
	// Attributes
	node** node_map;
	unordered_set<node_t*> explored;
	int rows, cols, start_x, start_y, goal_x, goal_y;
	NodePriorityQueue pq;

	// Helper functions
	bool compute();

public:
	// Constructors
	Dijkstra(bool** occ_matrix, int rows, int cols);
	Dijkstra(bool** occ_matrix, int rows, int cols, int start_x, int start_y, int goal_x, int goal_y);

	// API
	node_t** get_node_map();
	vector<tuple<int, int>> generate_path();
};