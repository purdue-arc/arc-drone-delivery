// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Imports
#include <chrono>
#include <iostream>
#include <vector>
#include <tuple>

// Local Imports
#include "tests/maze_generator.hpp"
#include "src/path_planning/dijkstra.hpp"
#include "src/path_planning/a_star.hpp"
#include "src/path_planning/d_star.hpp"

// Declaring Namespace
using namespace std;
using namespace chrono;


// Testing
void test_pq();
void test_AStar(const char* out_file_name, bool input_file_exists,
	const char* input_file_name, bool create_input_file,
	const char* new_input_file_name, int nrows, int ncols,
	double density);
void test_DStar(const char* out_file_name, bool input_file_exists,
	const char* input_file_name, bool create_input_file,
	const char* new_input_file_name, int nrows, int ncols,
	double density);
