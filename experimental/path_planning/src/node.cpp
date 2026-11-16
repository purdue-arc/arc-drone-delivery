// Includes
#include "node.hpp"


// Initializes a node with given values
void CNode::initialize_node(node_t* node, int cost, bool occupied, int heuristic) {
	set_cost(node, cost);
	set_occupancy(node, occupied);
	set_heuristic(node, heuristic);
}


// Returns occupancy of the given node from cost var.
bool CNode::get_occupancy(node_t* node) {
	return (node->cost & (unsigned int) 1);
}


// Sets occupancy of given node inside cost through bit manipulation
void CNode::set_occupancy(node_t* node, bool occupied) {
	// Because of opposite notation of pbm
	if (occupied) {
		node->cost |= 1;
	}
	else {
		node->cost &= (~1);
	}
}


// Returns actual cost value from given node.
int CNode::get_cost(node_t* node) {
	return (node->cost & (~1));
}


// Given a node, this function sets the cost.
void CNode::set_cost(node_t* node, int cost) {
	node->cost = (cost & (~1)) | (node->cost & 1);
}


// Gets the heuristic of a node
float CNode::get_heuristic(node_t* node) {
	return node->heuristic;
}


// Sets the heuristic of a node
void CNode::set_heuristic(node_t* node, float heuristic) {
	node->heuristic = heuristic;
}


// Compare function determines which node is a better choice to expand upon
bool CNode::nodeCompare(node_t* n1, node_t* n2) {
	if ((n1->heuristic + (float) get_cost(n1)) > (n2->heuristic + (float) get_cost(n2))) {
		return true;
	}
	return false;
}