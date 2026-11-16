#include "header.hh"

#include <iostream>
#include <vector>

struct point {
    int x, y;
};

int main() {
    int rows, cols;
    std::cin >> rows >> cols;

    bool **maze = (bool **) allocate_2d_arr(rows, cols, sizeof(bool));
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int status;
            std::cin >> status;
            maze[i][j] = status;
        }
    }

    // run A*/D* to find path
    // DStar d(maze, rows, cols);
    AStar a(maze, rows, cols);
    // vector<tuple<int, int>> path = d.generate_path();
    vector<tuple<int, int>> path = a.generate_path();
    
    if (path.empty()) {
        std::cout << "No possible path" << std::endl;
        return 0;
    }

    path.insert(path.begin(), make_tuple(0, 0));
    // flush output for gui
    for (tuple<int, int> point : path) {
        std::cout << get<0>(point) << " " << get<1>(point) << std::endl;
    }

    // free_2d_arr((void **) maze);
}