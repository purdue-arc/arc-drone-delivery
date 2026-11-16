// Standard Libraries
#include <algorithm>
#include <iostream>
#include <math.h>
#include <float.h>
#include <iomanip>
#include <chrono>
#include <thread>
#include <mutex> 
#include <malloc.h>
#include <vector>
#include <tuple>
#include <limits>
#include <chrono>
#include <unordered_set>
#include <queue> 

// Namespaces
using namespace std;
using namespace std::chrono;

// Constants
#define WIDTH (640)
#define HEIGHT (480)

#define COST (2)
const int max_cost = numeric_limits<int>::max();
const float max_heuristic = numeric_limits<float>::max();
const float max_rhs = numeric_limits<float>::max();


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
	static void set_cost_and_occupancy(node_t*, int, bool);
	static float get_heuristic(node_t*);
	static void set_heuristic(node_t*, float);
	static bool nodeCompare(node_t*, node_t*);
};


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


// For general priority queue syntax purposes
struct nodeComp {
	bool operator()(const node_t* n1, const node_t* n2) {
		return CNode::nodeCompare((node_t*)n1, (node_t*)n2);
	}
};

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

// Custom priority queue
class NodePriorityQueue {
private:
	// Attributes
	int size, insertIndex;
	node_t** nodes;

	// Helper functions
	inline void swap(int, int);
	inline int getNextSize();
	inline int getPrevSize();
	inline int getParentIndex(int);
	inline int getLeftChildIndex(int);
	inline int getRightChildIndex(int);
	inline int getLargerElement(int, int);
	inline int getSmallerElement(int, int);
	inline int getSmallerChild(int);
	inline int getLargerChild(int);
	void bubbleUp(int);
	void bubbleDown(int);
	void heapify();
	void heapify_helper(int);

public:
	// Constructors
	NodePriorityQueue();
	NodePriorityQueue(int, node_t**);

	// API Functions
	bool isEmpty();
	void push(node_t*);
	node_t* pop();
	int find(node_t*);
	void update_node_cost(node_t*, int);
	int get_size();
	void print_heap();
};


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


// Function Prototypes
// Util
void** allocate_2d_arr(int, int, int);
void free_2d_arr(void**);