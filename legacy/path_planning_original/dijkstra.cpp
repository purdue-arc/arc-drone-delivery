// Imports
#include "header.hh"


// Constructors
Dijkstra::Dijkstra(bool** occ_map, int rows, int cols) {
	this->rows = rows;
	this->cols = cols;

	this->start_x = 0;
	this->start_y = 0;
	this->goal_x = rows - 1;
	this->goal_y = cols - 1;

	this->node_map = (node_t**) NodeMap::initialize_node_map(rows, cols, occ_map, start_x, start_y);
	this->pq = NodePriorityQueue();
	this->explored = unordered_set<node_t *>();
}

Dijkstra::Dijkstra(bool** occ_map, int rows, int cols, int start_x, int start_y, int goal_x, int goal_y) {
	this->rows = rows;
	this->cols = cols;
	this->pq = NodePriorityQueue();

	this->start_x = start_x;
	this->start_y = start_y;
	this->goal_x = goal_x;
	this->goal_y = goal_y;

	this->node_map = (node_t**)NodeMap::initialize_node_map(rows, cols, occ_map, start_x, start_y);
	this->pq = NodePriorityQueue();
}


// Computes the min distance matrix
bool Dijkstra::compute() {
	// Initializing
	CNode::set_cost(&node_map[start_x][start_y], 0);
	pq.push(&node_map[start_x][start_y]);

	// Recursively expanding "cloud"
	do {
		node_t* of_interest = pq.pop();
		if ((of_interest->x == goal_x) && (of_interest->y == goal_y)) {
			// Reached the goal
			return true;
		}

		this->explored.insert(of_interest);
		vector<node_t*> neighbors = NodeMap::get_neighbors(node_map, of_interest->x, of_interest->y, rows, cols);
		for (int index = 0; index < neighbors.size(); index++) {
			node_t* neighbor = neighbors[index];
			// Not part of explored cloud
			if (explored.find(neighbor) == explored.end()) {
				if (pq.find(neighbor) == -1) {
					CNode::set_cost(neighbor, CNode::get_cost(of_interest) + COST);
					pq.push(neighbor);
				}
				else {
					if (CNode::get_cost(of_interest) + COST < CNode::get_cost(neighbor)) {
						pq.update_node_cost(neighbor, CNode::get_cost(of_interest) + COST);
					}
				}
			}
		}
	} while (!pq.isEmpty());

	return false;
}

// Returns the node map assosiated with this instance
node_t** Dijkstra::get_node_map() {
	return this->node_map;
}

// Finds whether a path exists and if so, traces it
vector<tuple<int, int>> Dijkstra::generate_path() {
	bool path_exists = compute();
	vector<tuple<int, int>> path;
	if (path_exists) {
		path = NodeMap::trace_path(this->node_map, goal_x, goal_y, rows, cols);
	}
	return path;
}