#include "ascii_display.hpp"
#include "geometry.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <type_traits>

namespace ascii_dsp {
	char& ascii_image::operator ()(unsigned int x, unsigned int y) {
		if (x < width && y < height) return lines[y][horizontal_multiplier * x];
		else return out_of_bounds_wpixel;
	}
	char& ascii_image::operator ()(const gmtry2i::vector2i& p) {
		if (gmtry2i::contains(bounds, p)) {
			gmtry2i::vector2i pixel_p = (p - bounds.min) >> log2_pw;
			return lines[pixel_p.y][horizontal_multiplier * (pixel_p.x)];
		}
		else return out_of_bounds_wpixel;
	}
	void ascii_image::write(const char* text, unsigned int x, unsigned int y) {
		x *= horizontal_multiplier;
		unsigned int c = 0;
		if (y < height) {
			char* line = lines[y];
			while (line[x] && text[c]) line[x++] = text[c++];
		}
	}
	void ascii_image::write(const char* text, const gmtry2i::vector2i& p) {
		if (gmtry2i::contains(bounds, p)) {
			gmtry2i::vector2i pixel_p = (p - bounds.min) >> log2_pw;
			write(text, pixel_p.x, pixel_p.y);
		}
	}
	void ascii_image::overwrite(ascii_image& img) {
		if (!gmtry2i::intersects(bounds, img.bounds)) return;
		gmtry2i::aligned_box2i intersection = gmtry2i::intersection(bounds, img.bounds);
		for (int x = intersection.min.x; x < intersection.max.x; x++)
			for (int y = intersection.min.y; y < intersection.max.y; y++) {
				char img_char = img({ x, y });
				if (img_char != '.') (*this)({ x, y }) = img_char;
			}
	}
	const char* ascii_image::get_line(int i) const {
		if (i < height) return lines[height - (i + 1)];
		else return &out_of_bounds_rpixel;
	}
	int ascii_image::get_num_lines() const {
		return height;
	}
	void ascii_image::set_caption(const std::string& new_caption) {
		caption = new_caption;
	}
	const char* ascii_image::get_caption() const {
		return caption.c_str();
	}
	gmtry2i::aligned_box2i ascii_image::get_bounds() const {
		return bounds;
	}
	unsigned int ascii_image::get_width() {
		return width;
	}
	unsigned int ascii_image::get_height() {
		return height;
	}
	ascii_image::ascii_image(const gmtry2i::aligned_box2i& viewport, unsigned int max_line_length) {
		bounds = viewport;
		log2_pw = find_log2_pixel_width(bounds.max.x - bounds.min.x, max_line_length);
		width = (bounds.max.x - bounds.min.x) >> log2_pw;
		height = (bounds.max.y - bounds.min.y) >> log2_pw;
		lines = new char* [height];
		for (unsigned int y = 0; y < height; y++) {
			lines[y] = new char[horizontal_multiplier * width + 1];
			for (unsigned int x = 0; x < width; x++)
				for (unsigned int x = 0; x < width; x++) {
					lines[y][x * horizontal_multiplier] = '.';
					for (unsigned int fillerx = 1; fillerx < horizontal_multiplier; fillerx++)
						lines[y][x * horizontal_multiplier + fillerx] = ' ';
				}
			lines[y][horizontal_multiplier * width] = '\0';
		}
		(*this)(gmtry2i::vector2i()) = 'O';
		caption = "Image Bounds: " + gmtry2i::to_string(bounds);
	}
	ascii_image::ascii_image(const gmtry2i::aligned_box2i& viewport) : ascii_image(viewport, DEFAULT_MAX_LINE_LENGTH) {}
	ascii_image::ascii_image(ascii_image&& img) {
		log2_pw = img.log2_pw;
		width = img.width;
		img.width = 0;
		height = img.height;
		img.height = 0;
		lines = img.lines;
		img.lines = 0;
		caption = std::move(img.caption);
		bounds = img.bounds;
	}
	ascii_image::ascii_image(const ascii_image& img) {
		log2_pw = img.log2_pw;
		width = img.width;
		height = img.height;
		lines = new char* [height];
		for (int y = 0; y < height; y++) {
			lines[y] = new char[horizontal_multiplier * width + 1];
			for (unsigned int x = 0; x < width * horizontal_multiplier + 1; x++)
				lines[y][x] = img.lines[y][x];
		}
		caption = img.caption;
		bounds = img.bounds;
	}
	ascii_image& ascii_image::operator =(const ascii_image& img) {
		this->~ascii_image();
		new(this) ascii_image(img);
		return *this;
	}
	ascii_image& ascii_image::operator = (ascii_image&& img) {
		this->~ascii_image();
		new(this) ascii_image(std::move(img));
		return *this;
	}
	ascii_image::~ascii_image() {
		for (int y = height - 1; y >= 0; y--) delete[] lines[y];
		delete[] lines;
	}

	console_command::console_command(const std::string& command_line) {
		const char* command_str = command_line.c_str();
		int name_len = 0;
		while (command_str[name_len] != ' ' && command_str[name_len] != '\0')
			name_len++;
		name = command_line.substr(0, name_len);
		if (name_len < command_line.length())
			args = command_line.substr(name_len + 1);
		else args = std::string();
	}

	void command_listener::pass_down(const console_command& command, std::ostream& os) {
		int num_listeners = listeners.size();
		for (int i = 0; i < num_listeners; i++) {
			if (listeners[i]->get_name() == command.name) {
				listeners[i]->execute(command.args, os);
				return;
			}
		}
	}
	void command_listener::add_listener(command_listener* listener) {
		listeners.push_back(listener);
	}
	void command_listener::execute(const std::string& command_args, std::ostream& os) {
		if (command_args == "man") {
			print_manual(os);
			int num_listeners = listeners.size();
			if (num_listeners) {
				os << "Sub-listeners: " << std::endl;
				for (int i = 0; i < num_listeners; i++)
					os << ". . " << listeners[i]->get_name() << std::endl;
			}
		}
		else if (attempt_execute(command_args, os)) return;
		else pass_down(console_command(command_args), os);
	}

	ascii_console::ascii_console(command_listener* new_listener, std::ostream& output) : os(output) {
		listener = new_listener;
	}
	void ascii_console::execute_commands(std::istream& is) {
		char command_buf[MAX_COMMAND_LENGTH] = {};
		bool running = true;
		while (running) try {
			is.getline(command_buf, MAX_COMMAND_LENGTH);
			std::string command_str = std::string(command_buf);
			if (command_str == std::string("exit"))
				running = false;
			listener->execute(std::string(command_buf), os);
		}
		catch (const std::exception& e) {
			os << e.what() << std::endl;
		}
	}

	std::ostream& operator << (std::ostream& os, const ascii_image& img) {
		for (int y = 0; y < img.get_num_lines(); y++) os << img.get_line(y) << std::endl;
		os << img.get_caption() << std::endl;
		return os;
	}

	decorated_point::decorated_point(const gmtry2i::vector2i& new_point, char new_decoration) {
		point = new_point;
		decoration = new_decoration;
	}

	ascii_image& operator << (ascii_image& img, const decorated_point& p) {
		img(p.point) = p.decoration;
		return img;
	}
	ascii_image& operator << (ascii_image& img, const gmtry2i::vector2i& p) {
		return img << decorated_point(p, '@');
	}

	decorated_point named_point(const gmtry2i::vector2i& point, char name) {
		return decorated_point(point, name);
	}

	decorated_point gradient_point(const gmtry2i::vector2i& point, const gmtry2i::vector2i& gradient) {
		char slope;
		if (!gradient.x && !gradient.y)
			slope = '*';
		else if (2.414 * std::abs(gradient.x) < std::abs(gradient.y))
			slope = '|';
		else if (2.414 * std::abs(gradient.y) < std::abs(gradient.x))
			slope = '-';
		else slope = ((gradient.x > 0) == (gradient.y > 0)) ? '/' : '\\';
		return decorated_point(point, slope);
	}

	decorated_point shaded_point(const gmtry2i::vector2i& point, float normalized_shade) {
		char shade = char_shade_set1[static_cast<int>(std::min(1.0F, std::max(0.0F, normalized_shade)) * 68)];
		return decorated_point(point, shade);
	}

	decorated_point simple_shaded_point(const gmtry2i::vector2i& point, float normalized_shade) {
		char shade = ".:-=+*#%@"[static_cast<int>(std::min(1.0F, std::max(0.0F, normalized_shade)) * 9)];
		return decorated_point(point, shade);
	}

	decorated_point faded_point(const gmtry2i::vector2i& point, float fade_factor, float init_fade_rate) {
		return shaded_point(point, 1 / (1 + fade_factor * init_fade_rate));
	}

	decorated_point steep_faded_point(const gmtry2i::vector2i& point, float fade_factor, float init_fade_rate) {
		float sqrt_denominator = fade_factor * init_fade_rate + 2.0F;
		return shaded_point(point, 4 / (sqrt_denominator * sqrt_denominator));
	}

	decorated_rect::decorated_rect(const gmtry2i::aligned_box2i& new_box, char new_fill, bool has_outline, char new_name) {
		box = new_box;
		fill = new_fill;
		is_outlined = has_outline;
		name = new_name;
	}

	ascii_image& operator << (ascii_image& img, const decorated_rect& box) {
		if (box.fill)
			for (int x = box.box.min.x; x < box.box.max.x; x++)
				for (int y = box.box.min.y; y < box.box.max.y; y++)
					img({ x, y }) = box.fill;
		if (box.is_outlined) {
			for (int y = box.box.min.y; y < box.box.max.y; y++) {
				img({ box.box.min.x, y }) = '|';
				img({ box.box.max.x, y }) = '|';
			}
			for (int x = box.box.min.x; x < box.box.max.x; x++) {
				img({ x, box.box.min.y }) = '-';
				img({ x, box.box.max.y }) = '-';
			}
		}
		char name = box.name;
		if (name) {
			if ('A' <= name && name <= 'Z') name = (name + 'a') - 'A';
			img(box.box.min) = name;
			img(box.box.max) = (name + 'A') - 'a';
		}
		return img;
	}

	decorated_rect named_rect(const gmtry2i::aligned_box2i& box, char name) {
		return decorated_rect(box, '@', false, name);
	}

	ascii_image& operator << (ascii_image& img, const gmtry2i::aligned_box2i& box) {
		return img << decorated_rect(box, '@', false, 0);
	}

	static class line_drawer : public gmtry2i::point_ostream2i {
		ascii_image& img;
		gmtry2i::vector2i pt_buf[3];
	public:
		line_drawer(ascii_image& new_img, gmtry2i::vector2i pre_point) : img(new_img) {
			pt_buf[0] = pre_point;
			pt_buf[1] = pre_point;
		}
		inline void write(const gmtry2i::vector2i& p) {
			pt_buf[2] = p;
			img << gradient_point(pt_buf[2], pt_buf[2] - pt_buf[0]);
			pt_buf[0] = pt_buf[1];
			pt_buf[1] = pt_buf[2];
		}
	};

	ascii_image& operator << (ascii_image& img, const gmtry2::line_segment2& l) {
		gmtry2::line_stepper2 stepper(l, 1.0F);
		gmtry2i::vector2i pt_buf[3] = { l.b, l.b, l.a };
		for (int t = 0; t < stepper.waypoints; t++) {
			pt_buf[2] = stepper.p;
			// Direction displayed is based on average of the two most recent displacements
			//		This is to make the line slopes both continuous and smooth
			img << gradient_point(pt_buf[2], pt_buf[2] - pt_buf[0]);
			pt_buf[0] = pt_buf[1];
			pt_buf[1] = pt_buf[2];
			stepper.step();
		}
		img << gradient_point(l.b, l.b - pt_buf[0]);
		/* Alternate line drawing method - fills every pixel intersected but looks uggo mode

		line_drawer ld(img, l.b);
		gmtry2i::rasterize(l, &ld);

		*/
		return img;
	}

	ascii_image& operator <<(ascii_image& img, const gmtry2::ball2& b) {
		gmtry2i::aligned_box2i bounds(gmtry2i::boundsof(b));
		for (int y = bounds.min.y; y < bounds.max.y; y++)
			for (int x = bounds.min.x; x < bounds.max.x; x++)
				if (gmtry2::intersects(gmtry2i::to_point2({ x, y }), b))
					img({ x, y }) = '@';
		return img;
	}
}