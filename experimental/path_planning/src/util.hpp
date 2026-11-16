// Prevents multiple definitions
#pragma once

// Prevents warnings for depricated libraries
#define _CRT_SECURE_NO_DEPRECATE

// Imports
#include <cstddef>
#include <malloc.h>

// Declaring namespaces
using namespace std;


// Function Prototypes
void** allocate_2d_arr(int, int, int);
void free_2d_arr(void**);