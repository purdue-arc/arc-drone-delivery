// Includes
#include "header.hh"


// Defining AStar Class
// Gets heuristic from given point to end distance
float AStar::get_heuristic(int x_i, int y_i, int x_f, int y_f) {
	return sqrt(pow((x_f - x_i), 2) + pow((y_f - y_i), 2));
}


// When called, adds all elements to a priority queue
void AStar::initializePriorityQueue() {
	// Make heap is O(n) while adding 1 @ a time is O(nlogn)
	pq.push(&node_map[0][0]);
	vector<node_t*> node_ptrs;
	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {
			node_ptrs.push_back(&node_map[row][col]);
		}
	}
	make_heap(node_ptrs.begin(), node_ptrs.end(), CNode::nodeCompare);
}

// ADT method to simplify popping min element
node_t* AStar::pop_min() {
	node_t* of_interest = (node_t*) pq.top();
	pq.pop();
	return of_interest;
}

// Performs necessary computations, stores them in node, & returns if path was found
bool AStar::compute() {
	bool reached_dest = false;

	node_map[start_x][start_y].cost = 0;
	node_map[start_x][start_y].heuristic = get_heuristic(start_x, start_y, goal_x, goal_y);

	//initializePriorityQueue();

	pq.push(&node_map[start_x][start_y]);

	while (!pq.empty()) {
		node_t* of_interest = pop_min();
		int parent_x = of_interest->x, parent_y = of_interest->y;

		// Checking if destination reached
		if ((parent_x == goal_x) && (parent_y == goal_y)) {
			reached_dest = true;
			break;
		}

		// Expanding neighbors
		vector<node_t*> neighbors = NodeMap::get_neighbors(node_map, parent_x, parent_y, rows, cols);
		for (int index = 0; index < neighbors.size(); index++) {
			neighbors[index]->heuristic = get_heuristic(neighbors[index]->x, neighbors[index]->y, goal_x, goal_y);
			if ((CNode::get_cost(of_interest) + COST) < CNode::get_cost(neighbors[index])) {
				CNode::set_cost(neighbors[index], CNode::get_cost(of_interest) + COST);
				pq.push(neighbors[index]);
			}
		}
	}

	return reached_dest;
}

// constructors
AStar::AStar(bool** occ_matrix, int rows, int cols) {
	// By default, setting bottom left as start and top right corner as goal
	start_x = 0;
	start_y = 0;
	goal_x = rows - 1;
	goal_y = cols - 1;
	this->rows = rows;
	this->cols = cols;
	node_map = NodeMap::initialize_node_map(rows, cols, occ_matrix, start_x, start_y);
}

AStar::AStar(bool **occ_matrix, int rows, int cols, int start_x, int start_y, int goal_x, int goal_y) {
	this->start_x = start_x;
	this->start_y = start_y;
	this->goal_x = goal_x;
	this->goal_y = goal_y;
	this->rows = rows;
	this->cols = cols;
	node_map = NodeMap::initialize_node_map(rows, cols, occ_matrix, start_x, start_y);
}

// Returns the node map assosiated with this instance
node_t** AStar::get_node_map() {
	return this->node_map;
}

/*
 * Heuristically computes all nodes' cost and heuristic distance and if
 * path exists, traces it out and returns it.
 */
vector<tuple<int, int>> AStar::generate_path() {
	bool path_exists = compute();
	if (path_exists) {
		this->path = NodeMap::trace_path(this->node_map, goal_x, goal_y, rows, cols);
	}
	return this->path;
}

/*
 * Given a new occupancy map, this regenerates a valid new path. Furthermore,
 * returns true if path needs to be recomputed.
 */
bool AStar::update_occupancy_map(bool** occupancy_map) {
	// Checking if path update is necessary
	bool recomputePath = (this->path.size() == 0);
	if (recomputePath == false) {
		for (tuple<int, int> step: path) {
			int row = get<0>(step), col = get<1>(step);
			if (occupancy_map[row][col] == true) {
				recomputePath = true;
				break;
			}
		}
	}

	// Updating the occupancy matrix
	for (int row = 0; row < this->rows; row++) {
		for (int col = 0; col < this->cols; col++) {
			CNode::initialize_node(& node_map[row][col], max_cost, occupancy_map[row][col], max_heuristic);
		}
	}

	return recomputePath;
}