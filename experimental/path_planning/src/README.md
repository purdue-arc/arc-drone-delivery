Simple compilation files (general .o rule suffices in Makefile):
* priority_queue.cpp/hpp
* node.cpp/hpp
* util.cpp/hpp

Advanced dependency based compilation files (specified target necessary in Makefile):
* nodemap.cpp/hpp: constants.hpp, node.hpp, util.hpp
* a_star.cpp/hpp: node.hpp, nodemap.hpp
* d_star.cpp/hpp: node.hpp, node_map.hpp, priority_queue.hpp
* dijkstra.cpp/hpp: node.hpp, node_map.hpp, priority_queue.hpp
