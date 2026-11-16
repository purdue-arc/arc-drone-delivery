// Imports
#include "header.hh"


// Allocated a 2d array properly given proper entry size
void** allocate_2d_arr(int rows, int cols, int entry_size) {
	void** arr = (void **) malloc(sizeof(void *) * rows);
	if (arr == NULL) {
		return NULL;
	}

	for (int row_index = 0; row_index < rows; row_index++) {
		arr[row_index] = malloc(entry_size * cols);
		if (arr[row_index] == NULL) {
			while (--row_index) {
				free(arr[row_index]);
				arr[row_index] = NULL;
			}
			free(arr);
			arr = NULL;
		}
	}

	return arr;
}

// Frees a given 2d array
void free_2d_arr(void **arr) {
	for (int row_index = 0; row_index < sizeof(arr) / sizeof(arr[0]); row_index++) {
		free(arr[row_index]);
		arr[row_index] = NULL;
	}
	free(arr);
	arr = NULL;
}