#include "geometry.hpp"
#include "projection.hpp"

#include <vector>

// Handles simple geometric projection/deprojection between a virtual world and depth matrices
namespace prjctn {
	void cam_info::set_pose(const gmtry3::transform3& pose) {
		cam_to_world = pose;
		world_to_cam = pose.T();
	}
	gmtry3::transform3 cam_info::get_pose() {
		return cam_to_world;
	}
	cam_info::cam_info(float fov, unsigned int res_width, unsigned int res_height, const gmtry3::transform3& pose) {
		tan_fov = std::tan(fov * 0.5F) * 2.0F;
		width = res_width;
		height = res_height;
		set_pose(pose);
	}

	void project(float* depths, cam_info config, ray_collider* collider) {
		const float img_scale = config.tan_fov / std::max(config.width, config.height);
		const float pxl_shiftx = -0.5F * config.width + 0.5F;
		const float pxl_shifty = -0.5F * config.height + 0.5F;
		for (int pxl_x = 0; pxl_x < config.width; pxl_x++) for (int pxl_y = 0; pxl_y < config.height; pxl_y++) {
			depths[pxl_x + pxl_y * config.width] = (config.world_to_cam * collider->collide({ config.cam_to_world.t,
				config.cam_to_world.R * gmtry3::vector3((pxl_x + pxl_shiftx) * img_scale, 1,
														(pxl_y + pxl_shifty) * img_scale) })).y;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    PROJECTION IMPLEMENTATIONS                                                                  //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	float ray_marcher::get_min_distance(const gmtry3::ray3& r) {
		int num_objects = measurables.size();
		float min_dst = MAX_DISTANCE;
		for (int i = 0; i < num_objects; i++) {
			min_dst = std::min(min_dst, measurables[i]->get_distance(r.p));
		}
		num_objects = collidables.size();
		for (int i = 0; i < num_objects; i++) {
			min_dst = std::min(min_dst, collidables[i]->get_distance(r, MAX_DISTANCE));
		}
		return std::max(0.0F, min_dst);
	}
	ray_marcher::ray_marcher() : measurables() {};
	void ray_marcher::add_object(const measurable_object* object) {
		measurables.push_back(object);
	}
	void ray_marcher::add_object(const collidable_object* object) {
		collidables.push_back(object);
	}
	gmtry3::vector3 ray_marcher::collide(const gmtry3::ray3& r) {
		gmtry3::vector3 p = r.p;
		gmtry3::vector3 direction = gmtry3::normalize(r.d);
		float step_size = get_min_distance({ p, direction });
		while (MIN_DISTANCE < step_size && step_size < MAX_DISTANCE) {
			p += direction * step_size;
			step_size = get_min_distance({ p, direction });
		}
		if (step_size == MAX_DISTANCE) return p + direction * MAX_DISTANCE;
		return p + direction * 0x1p-6F;
	}

	sphere::sphere(const gmtry3::ball3& new_sphere) {
		ball = new_sphere;
	}
	float sphere::get_distance(const gmtry3::vector3& p) const {
		return gmtry3::magnitude(ball.c - p) - ball.r;
	}

	circle_cylinder::circle_cylinder(const gmtry2::ball2& new_circle) {
		circle = new_circle;
	}
	float circle_cylinder::get_distance(const gmtry3::vector3& p) const {
		return gmtry2::magnitude(circle.c - p) - circle.r;
	}

	rect_cylinder::rect_cylinder(const gmtry2i::aligned_box2i& new_rect) {
		rect = new_rect;
	}
	float rect_cylinder::get_distance(const gmtry3::ray3& r, float max_distance) const {
		bool no_intersection = false;
		gmtry2::vector2 direction = gmtry2::normalize(gmtry2::vector2(r.d.x, r.d.y));
		gmtry2::line_segment2 rayline(r.p, r.p + direction * max_distance);
		gmtry2::line_segment2 collision = gmtry2i::intersection(rayline, rect, no_intersection);
		if (!no_intersection)
			return std::min(gmtry2::dot(collision.a - rayline.a, direction),
				gmtry2::dot(collision.b - rayline.a, direction));
		else return max_distance;
	}
}