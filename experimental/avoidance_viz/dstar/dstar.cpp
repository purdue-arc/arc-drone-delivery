// Includes
#include "header.hh"


// Defining DStar Class
// Gets heuristic from given point to end distance
float DStar::get_heuristic(int x_i, int y_i, int x_f, int y_f) {
	return sqrt(pow((x_f - x_i), 2) + pow((y_f - y_i), 2));
}

// Performs necessary computations, stores them in node, & returns if path was found
bool DStar::compute() {
	bool reached_dest = false;

	node_map[start_x][start_y].cost = 0;
	node_map[start_x][start_y].heuristic = get_heuristic(start_x, start_y, goal_x, goal_y);

	//initializePriorityQueue();

	pq.push(&node_map[start_x][start_y]);

	while (!pq.isEmpty()) {
		node_t* of_interest = pq.pop();
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
				neighbors[index]->rhs = CNode::get_cost(of_interest) + COST;
				pq.push(neighbors[index]);
			}
		}
	}

	return reached_dest;
}

// constructors
DStar::DStar(bool** occ_matrix, int rows, int cols) {
	// By default, setting bottom left as start and top right corner as goal
	start_x = 0;
	start_y = 0;
	goal_x = rows - 1;
	goal_y = cols - 1;
	this->rows = rows;
	this->cols = cols;
	this->pq = NodePriorityQueue();
	node_map = NodeMap::initialize_node_map(rows, cols, occ_matrix, start_x, start_y);
}

DStar::DStar(bool** occ_matrix, int rows, int cols, int start_x, int start_y, int goal_x, int goal_y) {
	this->start_x = start_x;
	this->start_y = start_y;
	this->goal_x = goal_x;
	this->goal_y = goal_y;
	this->rows = rows;
	this->cols = cols;
	this->pq = NodePriorityQueue();
	node_map = NodeMap::initialize_node_map(rows, cols, occ_matrix, start_x, start_y);
}

// Returns the node map assosiated with this instance
node_t** DStar::get_node_map() {
	return this->node_map;
}

/*
 * Heuristically computes all nodes' cost and heuristic distance and if
 * path exists, traces it out and returns it.
 */
vector<tuple<int, int>> DStar::generate_path() {
	bool path_exists = compute();
	vector<tuple<int, int>> path;
	if (path_exists) {
		path = NodeMap::trace_path(this->node_map, goal_x, goal_y, rows, cols);
	}
	return path;
}

float min(float i, float j) {
	return (i < j) ? i : j;
}


// Updates the occupancy map
void DStar::update_occupancy_map(bool** occupancy_map) {
	// Updating the occupancy matrix
	for (int row = 0; row < this->rows; row++) {
		for (int col = 0; col < this->cols; col++) {
			if (CNode::get_occupancy(&node_map[row][col]) != occupancy_map[row][col]) {
				CNode::set_occupancy(&node_map[row][col], occupancy_map[row][col]);
				if (occupancy_map[row][col] = 1) {
					node_map[row][col].rhs = max_rhs;
				}
				else {
					vector<node_t*> neighbors = NodeMap::get_neighbors(node_map, row, col, rows, cols);
					float min_rhs = max_rhs;
					for (node_t* neighbor : neighbors) {
						if ((neighbor->cost + COST) < min_rhs) {
							min_rhs = neighbor->cost + COST;
						}
					}
					node_map[row][col].rhs = min_rhs;
				}

				// Adding to priority queue
				pq.push(&node_map[row][col]);
			}
		}
	}

	// Expanding on all elements in the pq
	node_t* goal_node = &node_map[goal_x][goal_y];
	while (!pq.isEmpty()) {
		node_t* toExpand = pq.pop();
		// Nodes in pq are no longer worth expanding on
		if ((goal_node->rhs == CNode::get_cost(goal_node)) &&
			(min(toExpand->rhs, CNode::get_cost(toExpand)) + toExpand->heuristic >= CNode::get_cost(goal_node))) {
			break;
		}


		if (toExpand->rhs < CNode::get_cost(toExpand)) {
			CNode::set_cost(toExpand, toExpand->rhs);
		}
		else if (toExpand->rhs > CNode::get_cost(toExpand)) {
			CNode::set_cost(toExpand, max_cost);
		}

		vector<node_t*> neighbors = NodeMap::get_neighbors(node_map, toExpand->x, toExpand->y, rows, cols);
		for (node_t* neighbor : neighbors) {
			toExpand->rhs = CNode::get_cost(toExpand) + COST;
			pq.push(toExpand);
		}
	}
}