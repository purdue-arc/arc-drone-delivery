#pragma once

#include "tilemaps2.hpp"
#include "../util/ascii_display.hpp"

// Displays generic tiles and tile streams
namespace maps2 {
	ascii_dsp::ascii_image make_tile_image(unsigned int log2_tile_width, const gmtry2i::vector2i& any_tile_origin,
	                                       const gmtry2i::aligned_box2i& viewport, unsigned int max_line_length) {
		gmtry2i::aligned_box2i bounds = align_out(viewport, any_tile_origin, log2_tile_width);
		ascii_dsp::ascii_image img(bounds, max_line_length);
		// Draw horizontal lines
		for (long y = bounds.min.y; y < bounds.max.y; y += (1 << log2_tile_width))
			for (long x = bounds.min.x; x < bounds.max.x; x++)
				img({ x, y }) = '-';
		// Draw vertical lines
		for (long x = bounds.min.x; x < bounds.max.x; x += (1 << log2_tile_width))
			for (long y = bounds.min.y; y < bounds.max.y; y++)
				img({ x, y }) = '|';
		return img;
	}
	ascii_dsp::ascii_image make_tile_image(unsigned int log2_tile_width, const gmtry2i::vector2i& any_tile_origin,
	                                       const gmtry2i::aligned_box2i& viewport) {
		return make_tile_image(log2_tile_width, any_tile_origin, viewport, ascii_dsp::DEFAULT_MAX_LINE_LENGTH);
	}

	template <typename tile_type>
	struct located_tile {
		const tile_type* tile;
		gmtry2i::vector2i origin;
		located_tile(const tile_type* new_tile, const gmtry2i::vector2i& new_origin) {
			tile = new_tile;
			origin = new_origin;
		}
	};

	template <typename T>
	concept drawable_tile = requires (const T * a, const gmtry2i::vector2i & p, ascii_dsp::ascii_image & img) {
		img = img << located_tile(a, p);
	};

	template <drawable_tile tile>
	ascii_dsp::ascii_image& operator << (ascii_dsp::ascii_image& img, tile_istream<tile>* tiles) {
		const tile* next_tile;
		while (next_tile = tiles->next())
			if (gmtry2i::contains(img.get_bounds(), tiles->last_origin()))
				img << located_tile<tile>(next_tile, tiles->last_origin());
		return img;
	}

	template <drawable_tile tile>
	ascii_dsp::ascii_image& operator << (ascii_dsp::ascii_image& img, map_istream<tile>* map) {
		std::unique_ptr<tile_istream<tile>> tiles = map->read();
		img << tiles.get();
		return img;
	}
}