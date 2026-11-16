#pragma once

#include "geometry.hpp"

#include <vector>

// Handles simple geometric projection/deprojection between a virtual world and depth matrices
namespace prjctn {
	/*
	* Holds information about a camera, which can be used to project virtual geometry into the camera
	*	for it to be displayed, or to deproject perceived geometry into a virtual world.
	* tan_fov isn't actually tangent of field of view angle
	* referring to the diagram below, if a = fov angle, then tan_fov = t
	*      t
	*   \|---|/
	*    \   / ---
	*     \a/   | 1
	*      V -----
	* tan_fov is twice the tangent of half the fov angle
	* fov is defined to be the field of fiew for the longest dimension of the image
	* so if widith > height, then fov is the horizontal field of view
	*/
	struct cam_info {
		float tan_fov;
		unsigned int width, height;
		gmtry3::transform3 cam_to_world, world_to_cam;
		void set_pose(const gmtry3::transform3& pose);
		gmtry3::transform3 get_pose();
		cam_info() = default;
		cam_info(float fov, unsigned int res_width, unsigned int res_height, const gmtry3::transform3& pose);
	};

	// Collides a ray with a virtual world and returns the point of collision
	class ray_collider {
	public:
		virtual gmtry3::vector3 collide(const gmtry3::ray3& ray) = 0;
	};

	// Generates a depth matrix by sending a ray out through each pixel and colliding it with the virtual world
	void project(float* depths, cam_info config, ray_collider* collider);

	// Deprojects each depth from a depth matrix into a 2D point
	template <gmtry2i::writes_vector2i T>
	void deproject(const float* depths, cam_info config, T points_ostream) {
		gmtry3::vector3 cam_space_point;
		gmtry2i::vector2i projected_point;
		float pt_scale;
		// half width and half height
		const float img_scale = config.tan_fov / std::max(config.width, config.height);
		const float pxl_shiftx = -0.5F * config.width + 0.5F;
		const float pxl_shifty = -0.5F * config.height + 0.5F;
		for (int pxl_y = 0; pxl_y < config.height; pxl_y++) for (int pxl_x = 0; pxl_x < config.width; pxl_x++) {
			cam_space_point.y = depths[pxl_x + pxl_y * config.width];
			pt_scale = img_scale * cam_space_point.y;
			cam_space_point.x = (pxl_x + pxl_shiftx) * pt_scale;
			cam_space_point.z = (pxl_y + pxl_shifty) * pt_scale;
			projected_point.x = config.cam_to_world.R.n[0][0] * cam_space_point.x +
								config.cam_to_world.R.n[1][0] * cam_space_point.y +
								config.cam_to_world.R.n[2][0] * cam_space_point.z + config.cam_to_world.t.x;
			projected_point.y = config.cam_to_world.R.n[0][1] * cam_space_point.x +
								config.cam_to_world.R.n[1][1] * cam_space_point.y +
								config.cam_to_world.R.n[2][1] * cam_space_point.z + config.cam_to_world.t.y;
			points_ostream.write(projected_point);
		}
	}

	// Deprojects each depth from a depth matrix into a 3D point
	template <gmtry3::writes_vector3 T>
	void deproject(const float* depths, cam_info config, T points_ostream) {
		gmtry3::vector3 cam_space_point;
		float pt_scale;
		int hwidth = config.width / 2;
		int hheight = config.height / 2;
		float img_scale = config.tan_fov / std::max(hwidth, hheight);
		for (int y = 0; y < config.height; y++) for (int x = 0; x < config.width; x++) {
			cam_space_point.y = depths[x + y * config.width];
			pt_scale = img_scale * cam_space_point.y;
			cam_space_point.x = (x - hwidth) * pt_scale;
			cam_space_point.z = (y - hheight) * pt_scale;
			points_ostream.write(config.cam_to_world * cam_space_point);
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    PROJECTION IMPLEMENTATIONS                                                                  //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	class measurable_object {
	public:
		// Returns the minimum distance between the given point and any point on the object
		virtual float get_distance(const gmtry3::vector3& p) const = 0;
	};

	class collidable_object {
	public:
		// Returns the distance traveled by the ray before colliding with the object (or max_distance for no collision)
		virtual float get_distance(const gmtry3::ray3& r, float max_distance) const = 0;
	};

	// Marches a point from the start of a ray out until it collides with something
	class ray_marcher : public ray_collider {
		const float MIN_DISTANCE = 0x1p-8; // 1 / 256
		const float MAX_DISTANCE = 0x1p10; // 1024
		std::vector<const measurable_object*> measurables;
		std::vector<const collidable_object*> collidables;

		float get_min_distance(const gmtry3::ray3& r);
	public:
		ray_marcher();
		void add_object(const measurable_object* object);
		void add_object(const collidable_object* object);
		gmtry3::vector3 collide(const gmtry3::ray3& r);
	};

	class sphere : public measurable_object {
		gmtry3::ball3 ball;
	public:
		sphere(const gmtry3::ball3& new_sphere);
		float get_distance(const gmtry3::vector3& p) const;
	};

	struct circle_cylinder : public measurable_object {
		gmtry2::ball2 circle;
	public:
		circle_cylinder(const gmtry2::ball2& new_circle);
		float get_distance(const gmtry3::vector3& p) const;
	};

	struct rect_cylinder : public collidable_object {
		gmtry2i::aligned_box2i rect;
	public:
		rect_cylinder(const gmtry2i::aligned_box2i& new_rect);
		float get_distance(const gmtry3::ray3& r, float max_distance) const;
	};
}