Simple compilation files (general .o rule suffices in Makefile):
* None

Advanced dependency-based compilation files (specified target necessary in Makefile):
* path_planning_tests.cpp/hpp: maze_generator.hpp dijkstra.hpp, a_star.hpp, d_star.hpp
* maze_generator.cpp/hpp: node.hpp, util.hpp
* pq_tests.cpp/hpp: node.hpp, priority_queue.hpp
