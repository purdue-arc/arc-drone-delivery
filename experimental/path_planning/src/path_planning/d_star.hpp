// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Imports
#include <math.h>
#include <tuple>
#include <vector>

// Local Imports
#include "src/node.hpp"
#include "src/node_map.hpp"
#include "src/priority_queue.hpp"

// Declaring a namespace
using namespace std;


// Defines another approach to path planning
class DStar {
	// Attributes
	node_t** node_map;
	int rows, cols, start_x, start_y, goal_x, goal_y;
	NodePriorityQueue pq;

	// Helper functions
	bool compute();
	float get_heuristic(int x_i, int y_i, int x_f, int y_f);

public:
	// Constructors
	DStar(bool** occ_matrix, int rows, int cols);
	DStar(bool** occ_matrix, int rows, int cols, int start_x, int start_y, int goal_x, int goal_y);

	// API
	node_t** get_node_map();
	vector<tuple<int, int>> generate_path();
	void update_occupancy_map(bool**);
};