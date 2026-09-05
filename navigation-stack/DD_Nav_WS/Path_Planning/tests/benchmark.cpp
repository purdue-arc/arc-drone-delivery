#include "benchmark.hpp"

#include <chrono>

// For testing performance functions and stuff
namespace bnchmk {
	unsigned long long stopwatch::get_running_duration() {
		return last_time ? (clock.now().time_since_epoch().count() - last_time) : 0;
	}
	stopwatch::stopwatch() : clock() {
		accumulated_time = 0;
		last_time = 0;
	}
	void stopwatch::start() {
		accumulated_time += get_running_duration();
		last_time = clock.now().time_since_epoch().count();
	}
	void stopwatch::pause() {
		accumulated_time += get_running_duration();
		last_time = 0;
	}
	void stopwatch::reset() {
		accumulated_time = 0;
		last_time = 0;
	}
	unsigned long long stopwatch::read() { // returns nanoseconds
		return accumulated_time + get_running_duration();
	}
	unsigned long long stopwatch::read_micro() {
		return read() / 1000ULL;
	}
	unsigned long long stopwatch::read_milli() {
		return read() / 1000000ULL;
	}
}