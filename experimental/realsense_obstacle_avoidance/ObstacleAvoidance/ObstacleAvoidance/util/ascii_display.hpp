#pragma once

#include "geometry.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <type_traits>

namespace ascii_dsp {
	const unsigned int DEFAULT_MAX_LINE_LENGTH = 1536;

	class ascii_image {
	private:
		unsigned int log2_pw;
		char** lines;
		// Chars to be returned when an attempt is made to read/write an out-of-bounds pixel
		char out_of_bounds_wpixel, const out_of_bounds_rpixel = '\0';
		std::string caption;
		// Bounds of region displayed
		gmtry2i::aligned_box2i bounds;
		// Pixel-wise dimensions
		unsigned int width, height;
		// Width of a pixel in units of characters
		const static unsigned int horizontal_multiplier = 2;
		// Finds the pixel width necessary to fit the image on screen
		static unsigned int find_log2_pixel_width(unsigned int bounds_width, unsigned int max_line_length) {
			unsigned int log2_pw = 0;
			while ((bounds_width >> log2_pw) * horizontal_multiplier > max_line_length) log2_pw++;
			return log2_pw;
		}
	public:
		// References the character being displayed for the given pixel position on the image
		char& operator ()(unsigned int x, unsigned int y);
		// References the character being displayed for the given point in space
		char& operator ()(const gmtry2i::vector2i& p);
		void write(const char* text, unsigned int x, unsigned int y);
		void write(const char* text, const gmtry2i::vector2i& p);
		void overwrite(ascii_image& img);
		const char* get_line(int i) const;
		int get_num_lines() const;
		void set_caption(const std::string& new_caption);
		const char* get_caption() const;
		// Returns bounds of the displayed region in space; might not be tile-aligned
		gmtry2i::aligned_box2i get_bounds() const;
		unsigned int get_width();
		unsigned int get_height();
		ascii_image(const gmtry2i::aligned_box2i& viewport, unsigned int max_line_length);
		ascii_image(const gmtry2i::aligned_box2i& viewport);
		ascii_image(ascii_image&& img);
		ascii_image(const ascii_image& img);
		ascii_image& operator =(const ascii_image& img);
		ascii_image& operator = (ascii_image&& img);
		~ascii_image();
	};

	struct console_command {
		std::string name, args;
		console_command(const std::string& command_line);
	};

	class command_listener {
	public:
		virtual std::string get_name() = 0;
		void execute(const std::string&, std::ostream&);
	private:
		std::vector<command_listener*> listeners;
		void pass_down(const console_command& command, std::ostream& os);
	protected:
		virtual void print_manual(std::ostream& os) = 0;
		virtual bool attempt_execute(const std::string& command_args, std::ostream& os) = 0;
	public:
		void add_listener(command_listener* listener);
	};

	class ascii_console {
		static const int MAX_COMMAND_LENGTH = 100;
		command_listener* listener;
		std::ostream& os;
	public:
		ascii_console(command_listener* new_listener, std::ostream& output);
		void execute_commands(std::istream& is);
	};

	std::ostream& operator << (std::ostream& os, const ascii_image& img);

	struct decorated_point {
		gmtry2i::vector2i point;
		char decoration;
		decorated_point(const gmtry2i::vector2i& new_point, char new_decoration);
	};

	ascii_image& operator << (ascii_image& img, const decorated_point& p);
	ascii_image& operator << (ascii_image& img, const gmtry2i::vector2i& p);

	decorated_point named_point(const gmtry2i::vector2i& point, char name);
	decorated_point gradient_point(const gmtry2i::vector2i& point, const gmtry2i::vector2i& gradient);

	const char char_shade_set1[70] = ".'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
	const char char_shade_set2[10] = ".:-=+*#%@";

	decorated_point shaded_point(const gmtry2i::vector2i& point, float normalized_shade);
	decorated_point simple_shaded_point(const gmtry2i::vector2i& point, float normalized_shade);

	// fade_factor and init_fade_rate range between 0 and infinity
	decorated_point faded_point(const gmtry2i::vector2i& point, float fade_factor, float init_fade_rate);
	decorated_point steep_faded_point(const gmtry2i::vector2i& point, float fade_factor, float init_fade_rate);

	struct decorated_rect {
		gmtry2i::aligned_box2i box;
		char fill, name;
		bool is_outlined;
		decorated_rect(const gmtry2i::aligned_box2i& new_box, char new_fill, bool has_outline, char new_name);
	};

	ascii_image& operator << (ascii_image& img, const decorated_rect& box);
	ascii_image& operator << (ascii_image& img, const gmtry2i::aligned_box2i& box);
	decorated_rect named_rect(const gmtry2i::aligned_box2i& box, char name);

	ascii_image& operator << (ascii_image& img, const gmtry2::line_segment2& l);
	ascii_image& operator <<(ascii_image& img, const gmtry2::ball2& b);



	template <typename T>
	using spatial_field2 = T(*)(const gmtry2i::vector2i& p);

	using vector_field2 = spatial_field2<gmtry2i::vector2i>;

	template <typename T>
	concept number_type = std::is_arithmetic<T>::value;

	template <number_type T>
	ascii_image& operator << (ascii_image& img, spatial_field2<T> field) {
		const gmtry2i::vector2i shift1(1, 1);
		gmtry2i::aligned_box2i img_bounds = img.get_bounds();
		gmtry2i::aligned_box2i field_bounds(img_bounds.min - shift1, img_bounds.max + shift1);
		gmtry2i::vector2i field_dims(field_bounds.max - field_bounds.min);
		T** field_values = new T * [field_dims.y];
		for (int y = field_bounds.min.y; y < field_bounds.max.y; y++) {
			field_values[y - field_bounds.min.y] = new T[field_dims.x];
			for (int x = field_bounds.min.x; x < field_bounds.max.x; x++) {
				field_values[y - field_bounds.min.y][x - field_bounds.min.x] = field({ x, y });
			}
		}
		for (int y = img_bounds.min.y; y < img_bounds.max.y; y++) {
			for (int x = img_bounds.min.x; x < img_bounds.max.x; x++) {
				gmtry2i::vector2i gradient = gmtry2i::vector2i();
				for (int ny = -1; ny < 2; ny++) for (int nx = -1; nx < 2; nx++)
					if (ny != 0 || nx != 0)
						gradient += gmtry2i::vector2i(nx, ny) *
						field_values[y + ny - field_bounds.min.y][x + nx - field_bounds.min.x];
				img << gradient_point({ x, y }, gradient);
			}
		}
		for (int y = 0; y < field_dims.y; y++) {
			delete[] field_values[y];
			field_values[y] = 0;
		}
		delete[] field_values;
		return img;
	}
}