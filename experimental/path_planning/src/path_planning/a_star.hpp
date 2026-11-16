// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Imports
#include <math.h>
#include <vector>
#include <tuple>
#include <queue>

// Local Imports
#include "src/node.hpp"
#include "src/node_map.hpp"

// Namespaces
using namespace std;


// A Star class
class AStar {
private:
	// Attributes
	node_t** node_map;
	int rows, cols, start_x, start_y, goal_x, goal_y;
	priority_queue<node_t*, vector<node_t*>, nodeComp> pq;
	vector<tuple<int, int>> path;

	// Helper functions
	float get_heuristic(int x_i, int y_i, int x_f, int y_f);
	void initializePriorityQueue();
	node_t* pop_min();
	bool compute();

public:
	// Constructors
	AStar(bool** occ_matrix, int rows, int cols);
	AStar(bool** occ_matrix, int rows, int cols, int start_x, int start_y, int goal_x, int goal_y);

	// API
	node_t** get_node_map();
	vector<tuple<int, int>> generate_path();
	bool update_occupancy_map(bool **);
};