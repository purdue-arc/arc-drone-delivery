#pragma once

#include "occupancy.hpp"
#include "../maps2/maps2_display.hpp"

// Displays occupancy tiles and streams
namespace ocpncy {
	// Prints a tile
	template <unsigned int log2_w>
	std::ostream& operator << (std::ostream& os, const otile<log2_w>& tile) {
		for (int y = (1 << log2_w) - 1; y >= 0; y--) {
			for (int x = 0; x < (1 << log2_w); x++) {
				os << (get_occ(x, y, tile) ? "@" : ".") << ' ';
			}
			os << std::endl;
		}
		return os;
	}

	// Draws a tile at a position on an ASCII image
	template <unsigned int log2_w>
	ascii_dsp::ascii_image& operator << (ascii_dsp::ascii_image& img, const maps2::located_tile<otile<log2_w>>& tile) {
		for (int y = 0; y < (1 << log2_w); y++)
			for (int x = 0; x < (1 << log2_w); x++)
				if (get_occ(x, y, *tile.tile)) img(gmtry2i::vector2i(x, y) + tile.origin) = '@';
		return img;
	}

	// Makes an ASCII image fitted to the stream's output
	template <unsigned int log2_w>
	inline ascii_dsp::ascii_image make_fitted_image(maps2::tile_istream<otile<log2_w>>* stream, unsigned int max_line_length) {
		return maps2::make_tile_image(log2_w, stream->get_bounds().min, stream->get_bounds(), max_line_length);
	}

	// Draws tile stream to ASCII image and then prints it
	template <unsigned int log2_w>
	std::ostream& operator << (std::ostream& os, maps2::tile_istream<otile<log2_w>>* stream) {
		ascii_dsp::ascii_image img = maps2::make_tile_image(log2_w, stream->get_bounds().min, stream->get_bounds());
		return os << (img << stream);
	}
	template <unsigned int log2_w>
	std::ostream& operator << (std::ostream& os, maps2::map_istream<otile<log2_w>>* map) {
		ascii_dsp::ascii_image img = maps2::make_tile_image(log2_w, map->get_bounds().min, map->get_bounds());
		return os << (img << map);
	}
}