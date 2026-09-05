#pragma once

#include <chrono>

// For testing performance functions and stuff
namespace bnchmk {
	class stopwatch {
		std::chrono::steady_clock clock;
		unsigned long long accumulated_time, last_time;

		unsigned long long get_running_duration();
	public:
		stopwatch();
		void start();
		void pause();
		void reset();
		unsigned long long read();
		unsigned long long read_micro();
		unsigned long long read_milli();
	};
}