// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Imports
#include <chrono>
#include <iostream>
#include <math.h>

// Local Imports
#include "src/node.hpp"
#include "src/util.hpp"

// Declaring Namespace
using namespace std;
using namespace chrono;

// Private Define
#define MAX_MAP_BIT_VAL (2)
#define CLUSTER_SIZE (9)


// Interfacing functions to build mazes
bool** create_maze(int nrows, int ncols, double density);
bool** create_clustered_maze(int nrows, int ncols, double density);
bool** create_maze_file(const char* file_name, int nrows, int ncols, double density);
bool** create_clustered_maze_file(const char* file_name, int nrows, int ncols, double density);

// IO functions to read or write existing mazes
bool** read_maze_file(FILE*);
void write_maze_sol(FILE*, node_t**, vector<tuple<int, int>>, int, int);