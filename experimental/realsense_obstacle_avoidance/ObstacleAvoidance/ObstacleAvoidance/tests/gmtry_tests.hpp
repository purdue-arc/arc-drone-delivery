#pragma once

#include "benchmark.hpp"
#include "../util/prjctn_display.hpp"

#include <iostream>

// Tests to evaulate the consistency of geometric operators/functions
namespace gmtry_tests {
	const float PI = 3.14159265F;

	int geometry_test0() {
		float theta = PI / 6;
		gmtry3::rotor3 r = gmtry3::unit_make_rotor(gmtry3::vector3(0, 1, 0), theta) *
			gmtry3::unit_make_rotor(gmtry3::vector3(0, 0, 1), 2 * theta);
		gmtry3::matrix3 M = gmtry3::make_rotation(1, theta) * gmtry3::make_rotation(2, 2 * theta);
		std::cout << gmtry3::to_string(r * gmtry3::vector3(1.5, 2, 1)) << std::endl;
		std::cout << gmtry3::to_string(M * gmtry3::vector3(1.5, 2, 1)) << std::endl;

		return 0;
	}

	int geometry_test1() {
		ascii_dsp::ascii_image img(gmtry2i::aligned_box2i(-gmtry2i::vector2i(1, 1) << 5, 1 << 6), -1);
		gmtry2i::aligned_box2i box1({ 5, 10 }, 5);
		gmtry2i::aligned_box2i box2({ 15, 0 }, 5);
		img << ascii_dsp::named_rect(box1,        'a')
			<< ascii_dsp::named_rect(box2,        'b')
			<< ascii_dsp::named_rect(box1 - box2, 'c');
		std::cout << img;
		return 0;
	}

	int geometry_test2() {
		ascii_dsp::ascii_image img(gmtry2i::aligned_box2i(gmtry2i::vector2i(), 100));
		img.write("hello", { 99, 50 });
		gmtry2i::vector2i p1(20, 15), p2(105, 40), q1(76, -50), q2(5, 90);
		gmtry2i::line_segment2i l1(p1, p2), l2(q1, q2);
		img << l1 << l2;
		img << p1 << p2 << q1 << q2;
		img(gmtry2i::intersection(l1, l2)) = 'I';
		std::cout << img;
		return 0;
	}

	gmtry2i::vector2i random_vector(const gmtry2i::aligned_box2i& bounds) {
		gmtry2i::vector2i disp = bounds.max - bounds.min - gmtry2i::vector2i(1, 1);
		return (gmtry2i::vector2i(std::rand() * disp.x, std::rand() * disp.y) / RAND_MAX) + bounds.min;
	}

	gmtry2i::aligned_box2i random_rectangle(const gmtry2i::aligned_box2i& bounds) {
		// Use this one for a bigger rectangle
		//return gmtry2i::boundsof(gmtry2i::boundsof(random_vector(bounds), random_vector(bounds)), random_vector(bounds));
		return gmtry2i::boundsof(random_vector(bounds), random_vector(bounds));
	}

	// tests line-box intersections
	int geometry_test3() { // PASSED
		const int num_examples = 30;
		gmtry2i::aligned_box2i img_bounds({ 0, 0 }, 32);
		ascii_dsp::ascii_image* results[2][num_examples];
		int result_idcs[2] = { 0, 0 };
		for (int i = 0; i < num_examples; i++) {
			ascii_dsp::ascii_image& img = *new ascii_dsp::ascii_image(img_bounds);
			gmtry2i::aligned_box2i rect(random_rectangle(img_bounds));
			gmtry2i::vector2i a(random_vector(img_bounds)), b(random_vector(img_bounds));
			gmtry2i::line_segment2i ab(a, b), ab_int;
			gmtry2::line_segment2 abf(ab), abf_int;
			bool no_intersection = false;
			abf_int = gmtry2i::intersection(abf, rect, no_intersection);
			bool they_intersect;
			//they_intersect = gmtry2i::intersects(ab, rect);
			they_intersect = !no_intersection;
			if (they_intersect) {
				//ab_int = gmtry2i::intersection(ab, rect);
				img << abf_int;
			}
			else img << ab;
			img << ascii_dsp::named_point(a, 'a') << ascii_dsp::named_point(b, 'b');
			if (they_intersect) {
				img.write("a'", ab_int.a);
				img.write("b'", ab_int.b);
			}
			img << ascii_dsp::decorated_rect(rect, 0, 0, 'x');
			results[they_intersect][result_idcs[they_intersect]++] = &img;
		}
		for (int i = 0; i < 2; i++) {
			std::cout << "INTERSECTING: " << i << "\n\n";
			for (int j = 0; j < result_idcs[i]; j++) {
				std::cout << *results[i][j] << std::endl;
				delete results[i][j];
			}
			std::cout << "==============================================\n\n\n";
		}
		return 0;
	}

	// Tests line-line intersections
	int geometry_test4() { // PASSED
		const int num_examples = 30;
		gmtry2i::aligned_box2i img_bounds({ 0, 0 }, 32);
		for (int i = 0; i < num_examples; i++) {
			ascii_dsp::ascii_image img(img_bounds);
			gmtry2i::vector2i a(random_vector(img_bounds)), b(random_vector(img_bounds));
			gmtry2i::vector2i c(random_vector(img_bounds)), d(random_vector(img_bounds));
			gmtry2i::line_segment2i ab(a, b), cd(c, d);
			img << ab << ascii_dsp::named_point(a, 'a') << ascii_dsp::named_point(b, 'b');
			img << cd << ascii_dsp::named_point(c, 'c') << ascii_dsp::named_point(d, 'd');
			img << ascii_dsp::named_point(gmtry2i::intersection(ab, cd), 'I');
			std::cout << img << "Intersects: " << gmtry2i::intersects(ab, cd) << "\n\n";
		}
		return 0;
	}

	// Tests whether lines are properly extending all the way to each endpoint
	int geometry_test5() {
		const int num_examples = 30;
		gmtry2i::aligned_box2i img_bounds({ 0, 0 }, 16);
		for (int i = 0; i < num_examples; i++) {
			ascii_dsp::ascii_image img(img_bounds);
			gmtry2i::vector2i a(random_vector(img_bounds)), b(random_vector(img_bounds));
			gmtry2i::vector2i shift(1, 0);
			img << gmtry2i::line_segment2i(a, b) << ascii_dsp::named_point(a + shift, 'a') << ascii_dsp::named_point(b + shift, 'b');
			std::cout << img << "\n\n";
		}
		return 0;
	}

	float field_example(const gmtry2i::vector2i& p) {
		return std::sqrt(gmtry2i::squared(p - 
			gmtry2i::vector2i(std::cos(p.x - 5) + 5, std::sin(p.y * p.y - 5*p.y + 5) + 13)));
	}

	// Prints gradient lines of a field
	int geometry_test6() {
		gmtry2i::aligned_box2i img_bounds({ 0, 0 }, 16);
		ascii_dsp::ascii_image img(img_bounds);
		std::cout << (img << field_example);
		return 0;
	}

	// Tests line-point intersections
	int geometry_test7() { // PASSED
		const int num_examples = 30;
		gmtry2i::aligned_box2i img_bounds({ 0, 0 }, 16);
		for (int i = 0; i < num_examples; i++) {
			ascii_dsp::ascii_image img(img_bounds);
			gmtry2i::vector2i a(random_vector(img_bounds)), b(random_vector(img_bounds));
			gmtry2i::line_segment2i ab(a, b);
			gmtry2::line_segment2 abf(ab);
			img << abf;
			bool intersects = false;
			while (!intersects) {
				gmtry2i::vector2i p(random_vector(img_bounds));
				intersects = gmtry2i::intersects(abf, gmtry2i::boundsof(p));
				img << ascii_dsp::named_point(p, intersects ? '@' : 'o');
			}
			std::cout << (img << ascii_dsp::named_point(a, 'a') << ascii_dsp::named_point(b, 'b')) << std::endl;
		}
		return 0;
	}

	// Tests line-point intersections
	int geometry_test8() { // Line-point intersection has maximum 99.44% accuracy
		const int num_lines = 100, tests_per_line = 20;
		int num_differences = 0;
		double num_tests = 0, num_correct = 0;
		gmtry2i::aligned_box2i img_bounds({ 0, 0 }, 16);
		bnchmk::stopwatch timer;
		for (int i = 0; i < num_lines; i++) {
			//ascii_dsp::ascii_image img(img_bounds);
			gmtry2i::vector2i a(random_vector(img_bounds)), b(random_vector(img_bounds));
			gmtry2i::line_segment2i ab(a, b);
			gmtry2::line_segment2 abf(ab);
			//img << ab;

			for (int j = 0; j < tests_per_line; j++) {
				gmtry2i::vector2i p(random_vector(img_bounds));
				bool intersects = false;
				
				// taking tiny steps so every point will be hit
				gmtry2i::line_stepper2i stepper(ab, 0.00001F);
				for (int t = 0; t < stepper.waypoints; t++) {
					if (stepper.p == p) {
						intersects = true;
						break;
					}
					stepper.step();
				}
				if (stepper.p == p) intersects = true;

				num_tests++;
				timer.start();
				bool should_intersect = gmtry2i::intersects(abf, gmtry2i::boundsof(p)); // 99.4%, 4.19ms
				//bool should_intersect = gmtry2i::intersects(ab, p); // 99.15%, 1.996ms
				if (intersects == should_intersect) num_correct++;
				//else img << ascii_dsp::named_point(p, intersects ? '@' : 'o');
				timer.pause();
			}
			std::cout << "Tested " << i << "/" << num_lines << " lines" << std::endl;
			std::cout << "Accuracy: " << 100 * (num_correct / (num_tests)) << "%" << std::endl;
			std::cout << "Intersection latency: " << (timer.read_micro() / num_tests) << " micro sec" << std::endl;
		}
		return 0;
	}

	int projection_test0() { // PASSED
		prjctn::cam_info config(PI / 2, 4, 1, {});
		float* depths = new float[config.width * config.height] {10, 11, 12, 13};
		gmtry3::vector3 cam_origin(40, 6, 0);
		gmtry2i::vector2i cam_origin2(cam_origin.x, cam_origin.y);
		gmtry3::matrix3 cam_orientation = gmtry3::make_rotation(2, PI / 4);		// Yaw left 45deg
		//								* gmtry3::make_rotation(0, -PI / 3);	// Pitch down 60deg
		prjctn::observed_point_drawer2 drawer(64, 64);
		drawer.set_perspective(config.get_pose());
		// Draw camera's local axes
		//img << gmtry2i::line_segment2i(cam_origin2, cam_origin2 + 
		//	   gmtry2i::vector2i(cam_orientation(0).x * 8, cam_orientation(0).y * 8))
		//	<< gmtry2i::line_segment2i(cam_origin2, cam_origin2 + 
		//	   gmtry2i::vector2i(cam_orientation(1).x * 8, cam_orientation(1).y * 8));
		config.cam_to_world = gmtry3::transform3(cam_orientation, cam_origin);
		prjctn::deproject(depths, config, drawer);
		drawer.execute("drw", std::cout);

		delete[] depths;
		return 0;
	}

	int projection_test1() { // PASSED
		prjctn::cam_info config(PI / 3, 3, 2, {});
		float* depths = new float[config.width * config.height] {6, 7, 8, 9, 10, 11};
		gmtry3::vector3 cam_origin(40, 6, 0);
		gmtry2i::vector2i cam_origin2(cam_origin.x, cam_origin.y);
		gmtry3::matrix3 cam_orientation = gmtry3::make_rotation(2, PI / 4)	// Yaw left
			//								* gmtry3::make_rotation(0, PI / 2)	// Pitch up
			* gmtry3::make_rotation(1, -PI / 6);// Roll down to the left
		ascii_dsp::ascii_image img(gmtry2i::aligned_box2i(gmtry2i::vector2i(), 64));
		prjctn::observed_point_drawer2 drawer(64, 64);
		config.set_pose(gmtry3::transform3(cam_orientation, cam_origin));
		prjctn::deproject(depths, config, drawer);
		drawer.execute("drw", std::cout);

		delete[] depths;
		return 0;
	}

	// Projects a generated environment to the camera, then deprojects it back out into the map
	int projection_test2() {
		prjctn::ray_marcher marcher;
		prjctn::sphere a({ { 0, 10, 0 }, 3 }), b({ { 5, 20, 5 }, 9 });
		marcher.add_object(&a);
		marcher.add_object(&b);
		prjctn::circle_cylinder c1({ {-20, 20}, 4 });
		marcher.add_object(&c1);
		prjctn::rect_cylinder c2({ {-22, 16}, {-16, 26} });
		marcher.add_object(&c2);
		prjctn::cam_info config(PI / 2, 48, 32, { gmtry3::make_rotation(2, PI / 8) * gmtry3::make_rotation(1, -PI / 6), 
		                                          gmtry3::vector3(6, 2, -0.5) });
		prjctn::observed_point_drawer2 drawer(48, 32);
		prjctn::world_explorer explora(config, &marcher, &drawer);
		explora.add_listener(&drawer);
		ascii_dsp::ascii_console console(&explora, std::cout);
		console.execute_commands(std::cin);
		return 0;
	}
}