// header file
#include "header.hh"

#define OUTPUT_FILEPATH (std::string("resources/output/"))
#define RESOURCES_FILEPATH (std::string("resources/"))
// other files
#include "tests/ocpncy_tests.hpp"
#include "tests/misc_tests.hpp"
#include "tests/gmtry_tests.hpp"

// Main
int main() {
	freopen("./resources/output.txt", "w", stdout);
	test_AStar("./resources/a_star_output.pbm", true, "./resources/a_star_input.pbm", true, "./resources/a_star_input.pbm", 30, 30, .1);
	return 0;
}