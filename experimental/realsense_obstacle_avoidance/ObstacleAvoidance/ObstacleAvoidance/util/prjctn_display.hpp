#pragma once

#include "ascii_display.hpp"
#include "projection.hpp"

namespace prjctn {
	class point_observer2 : public gmtry2i::point_ostream2i {
	public:
		virtual void set_perspective(const gmtry3::transform3& pose) = 0;
	};

	// Literally just draws the deprojected points and lines going to them
	class observed_point_drawer2 : public point_observer2, public ascii_dsp::command_listener {
		ascii_dsp::ascii_image occ_img, base_img;
		gmtry2i::vector2i viewport_dims, viewport_origin;
		bool sync_viewport;
		gmtry2::vector2 cam_origin;
		void reset_images();
	protected:
		void print_manual(std::ostream& os);
		bool attempt_execute(const std::string& args, std::ostream& os);
	public:
		observed_point_drawer2(unsigned int width, unsigned int height);
		void write(const gmtry2i::vector2i& p);
		void set_perspective(const gmtry3::transform3& pose);
		std::string get_name();
	};

	/*
	* Command line interface for exploring a virtual world and viewing it in the first person
	* POV can be moved around and rotated
	* A point observer can be attached to process the renderings
	*/
	class world_explorer : public ascii_dsp::command_listener {
		cam_info config;
		ray_collider* renderer;
		point_observer2* observer;
		float* depths;
		unsigned int brightness;
	protected:
		void print_manual(std::ostream& os);
		bool attempt_execute(const std::string& args, std::ostream& os);
	public:
		world_explorer(const cam_info& cam_config, ray_collider* new_renderer, point_observer2* new_observer);
		std::string get_name();
		~world_explorer();
	};
}