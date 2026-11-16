// Imports
#include "header.hh"


// Given number of rows, cols, and occupancy map, initialized nodes per path finding algo.
node_t** NodeMap::initialize_node_map(int rows, int cols, bool** occ_map, int start_x, int start_y) {
	node_t** node_map = (node_t**)allocate_2d_arr(rows, cols, sizeof(node_t));
	if (node_map == NULL) {
		fprintf(stderr, "Problem allocating memory for the maze.\n");
		return NULL;
	}

	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {
			// storing x,y location
			node_map[row][col].x = row;
			node_map[row][col].y = col;

			/*
			 * Floating points support infinity and math involving infinity
			 * acccording to IEEE 754.
			 */
			CNode::set_cost_and_occupancy(&node_map[row][col], max_cost, occ_map[row][col]);
			node_map[row][col].heuristic = max_heuristic;
			node_map[row][col].rhs = max_rhs;
		}
	}

	node_map[start_x][start_y].cost = 0;

	return node_map;
}

void NodeMap::free_node_map(node_t **node_map) {
	free_2d_arr((void **) node_map);
	node_map = NULL;
}

// Checks whether node is out of bounds
bool NodeMap::outOfBounds(node_t **node_map, int row, int col, int rows, int cols) {
	if ((row < 0) || (col < 0) ||
		(row >= rows) || (col >= cols)) {
		return true;
	}
	return false;
}

// Returns vector of neighbors for a given node
vector<node*> NodeMap::get_neighbors(node_t **node_map, int row, int col, int rows, int cols) {
	vector<node*> neighbors;

	// adding non-diag neighbors
	if ((!outOfBounds(node_map, row - 1, col, rows, cols)) && (!CNode::get_occupancy(&node_map[row - 1][col]))) {
		neighbors.push_back(&node_map[row - 1][col]);
	}
	if ((!outOfBounds(node_map, row, col - 1, rows, cols)) && (!CNode::get_occupancy(&node_map[row][col - 1]))) {
		neighbors.push_back(&node_map[row][col - 1]);
	}
	if ((!outOfBounds(node_map, row + 1, col, rows, cols)) && (!CNode::get_occupancy(&node_map[row + 1][col]))) {
		neighbors.push_back(&node_map[row + 1][col]);
	}
	if ((!outOfBounds(node_map, row, col + 1, rows, cols)) && (!CNode::get_occupancy(&node_map[row][col + 1]))) {
		neighbors.push_back(&node_map[row][col + 1]);
	}

	// adding diag neighbors
	if ((!outOfBounds(node_map, row - 1, col - 1, rows, cols)) && (!CNode::get_occupancy(&node_map[row - 1][col - 1]))) {
		neighbors.push_back(&node_map[row - 1][col - 1]);
	}
	if ((!outOfBounds(node_map, row + 1, col - 1, rows, cols)) && (!CNode::get_occupancy(&node_map[row + 1][col - 1]))) {
		neighbors.push_back(&node_map[row + 1][col - 1]);
	}
	if ((!outOfBounds(node_map, row + 1, col + 1, rows, cols)) && (!CNode::get_occupancy(&node_map[row + 1][col + 1]))) {
		neighbors.push_back(&node_map[row + 1][col + 1]);
	}
	if ((!outOfBounds(node_map, row - 1, col + 1, rows, cols)) && (!CNode::get_occupancy(&node_map[row - 1][col + 1]))) {
		neighbors.push_back(&node_map[row - 1][col + 1]);
	}

	return neighbors;
}

// Prints out the cost matrix given a double pointer of nodes
void NodeMap::print_cost_matrix(node_t** nodes, int rows, int cols) {
	printf("\n\nCost Matrix:\n");
	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {
			printf("%d, ", CNode::get_cost(&nodes[row][col]));
		}
		printf("\n");
	}
	printf("\n");
}

// Prints out the occupancy matrix given a double pointer of nodes
void NodeMap::print_occupancy_matrix(node_t** nodes, int rows, int cols) {
	printf("\n\nOccupancy Matrix:\n");
	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {
			printf("%d, ", CNode::get_occupancy(&nodes[row][col]));
		}
		printf("\n");
	}
	printf("\n");
}

// Prints out the heuristic matrix given a double pointer of nodes
void NodeMap::print_heuristic_matrix(node_t** nodes, int rows, int cols) {
	printf("\n\nHeuristic Matrix:\n");
	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {
			printf("%d, ", CNode::get_heuristic(&nodes[row][col]));
		}
		printf("\n");
	}
	printf("\n");
}

/*
 * Generates a path, which is returned as a list of tuples from start to finish.
 * To do this, it traverse backwards from goal by finding neighbor with lowest
 * cost and continue till cost = 0, in which case, it is the start node. Before
 * returning, list is reverse so that it goes from the start --> finish rather
 * than finish --> start. Ensure that path exists before calling this function.
 */
vector<tuple<int, int>> NodeMap::trace_path(node_t **node_map, int goal_x, int goal_y, int rows, int cols) {
	int x = goal_x, y = goal_y;
	vector<tuple<int, int>> path;

	while (CNode::get_cost(&node_map[x][y]) != 0) {
		path.push_back(make_tuple(x, y));
		vector<node*> neighbors = NodeMap::get_neighbors(node_map, x, y, rows, cols);
		node_t* min = &node_map[x][y];
		for (int index = 0; index < neighbors.size(); index++) {
			if (CNode::get_cost(neighbors[index]) < CNode::get_cost(&node_map[x][y])) {
				min = neighbors[index];
			}
		}
		x = min->x;
		y = min->y;
	}

	reverse(path.begin(), path.end());
	return path;
}

// Given a path and assosiated node map, prints out visually the path.
void NodeMap::print_generated_path(node_t** node_map, vector<tuple<int, int>> path, int rows, int cols) {
	printf("Reminder:\n\"0\" --> unoccupied nodes\n\"1\" --> occupied nodes\n\"2\" --> generated path\n");
	printf("Final Matrix:\n");
	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {
			bool inPath = false;
			for (tuple<int, int> node_in_path : path) {
				if ((get<0>(node_in_path) == row) && (get<1>(node_in_path))) {
					inPath = true;
					break;
				}
			}
			if (inPath) {
				printf("2, ");
			}
			else {
				printf("%d, ", CNode::get_occupancy(&node_map[row][col]));
			}
		}
		printf("\n");
	}
}