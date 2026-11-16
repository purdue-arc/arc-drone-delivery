#pragma once

#include "../util/geometry.hpp"

#include <stdint.h>

/*
* Describes different types of occupancy tiles and operations/functions for manipulating and accessing them.
* An occupancy tile is a square of occupancy states, with width w = 2 ^ log2(w), where log2(w) is a natural number.
* An occupancy state is a value that tells whether a small square region is occupied, and may also store the certainty
* with which it is believed to be occupied.
*/
namespace ocpncy {
	// MINI: 8x8 tile of binary occupancy states; used to build bigger binary tiles
	// log2(mini width)
	const unsigned int LOG2_MINIW = 3;
	// width of an mini
	const unsigned int MINI_WIDTH = 1 << LOG2_MINIW;
	// area of an mini
	const unsigned int MINI_AREA = 1 << (2 * LOG2_MINIW);
	// mask to apply to the index of a state to return a coordinate of the state relative to the mini
	const unsigned int MINI_COORD_MASK = MINI_WIDTH - 1;

	// WxW square of occupancy states (W = width of a mini = 2 ^ LOG2_MINIW)
	typedef std::uint64_t omini;

	inline omini mini_sum(const omini& m1, const omini& m2) {
		return m1 | m2;
	}
	inline omini mini_minus(const omini& m1, const omini& m2) {
		return m1 & ~m2;
	}
	inline omini mini_diff(const omini& m1, const omini& m2) {
		return m1 ^ m2;
	}

	// Returns the width of a tile in units of minis
	constexpr unsigned int get_tile_width_minis(unsigned int log2_tile_w) {
		return 1 << (log2_tile_w - LOG2_MINIW);
	}
	// Returns the area of a tile in units of minis
	constexpr unsigned int get_tile_area_minis(unsigned int log2_tile_w) {
		return 1 << ((log2_tile_w - LOG2_MINIW) * 2);
	}
	// Returns a mask for sub-tile coordinates
	constexpr unsigned int get_tile_coord_mask(unsigned int log2_tile_w) {
		return (1 << log2_tile_w) - 1;
	}

	template <typename T>
	concept observable_otile = requires (unsigned int x, unsigned int y, const T& tile, bool b) {
		b = get_occ(x, y, tile);
	};

	/*
	* Tile of binary occupancy states (represented as individual bits) with a width of 2 ^ n 
		(this is so the tile can be evenly cut in half all the way down to the bit level, 
		which makes certain opertions faster)
	* log2_w: base-2 logarithm of tile width; tile width = w = 2 ^ log2_w = 1 << log2_w
	* log2_w MUST BE GREATER THAN OR EQUAL TO LOG2_MINIW (= 3 at the time of writing this)!!!
	*/
	template <unsigned int log2_w>
		requires (log2_w >= 3)
	struct otile {
		omini minis[get_tile_area_minis(log2_w)];
		otile() = default;
		inline otile& operator +=(const otile& tile) {
			for (int i = 0; i < get_tile_area_minis(log2_w); i++)
				minis[i] |= tile.minis[i];
			return *this;
		}
		inline otile& operator -=(const otile& tile) {
			for (int i = 0; i < get_tile_area_minis(log2_w); i++)
				minis[i] &= ~tile.minis[i];
			return *this;
		}
	};

	// Tests whether a tile has any occupancies
	template <unsigned int log2_w>
	bool is_occupied(const otile<log2_w>& t) {
		bool occupied = false;
		for (int i = 0; i < get_tile_area_minis(log2_w); i++)
			occupied |= t.minis[i];
		return occupied;
	}

	// Returned tile contains all occupancies present in t1 or t2
	template <unsigned int log2_w>
	otile<log2_w> operator +(const otile<log2_w>& t1, const otile<log2_w>& t2) {
		otile<log2_w> sum;
		for (int i = 0; i < get_tile_area_minis(log2_w); i++)
			sum.minis[i] = t1.minis[i] | t2.minis[i];
		return sum;
	}

	// Returned tile contains only the occupancies which are present in either t1 or t2, but not both
	template <unsigned int log2_w>
	otile<log2_w> operator ^(const otile<log2_w>& t1, const otile<log2_w>& t2) {
		otile<log2_w> dif;
		for (int i = 0; i < get_tile_area_minis(log2_w); i++)
			dif.minis[i] = t1.minis[i] ^ t2.minis[i];
		return dif;
	}

	// Returned tile contains only the occupancies from t1 which weren't present in t2
	template <unsigned int log2_w>
	otile<log2_w> operator -(const otile<log2_w>& t1, const otile<log2_w>& t2) {
		otile<log2_w> dif;
		for (int i = 0; i < get_tile_area_minis(log2_w); i++)
			dif.minis[i] = t1.minis[i] & ~t2.minis[i];
		return dif;
	}

	// Returns the index of a mini that contains the point (x, y), relative to the respective tile's origin
	constexpr inline unsigned int get_mini_idx(unsigned int x, unsigned int y, unsigned int log2_w) {
		return (x >> LOG2_MINIW) | ((y >> LOG2_MINIW) << (log2_w - LOG2_MINIW));
	}
	// Returns the index, relative to the respective mini, of a bit at the point (x, y), relative to the tile's origin
	constexpr inline unsigned int get_bit_idx(unsigned int x, unsigned int y) {
		return (x & MINI_COORD_MASK) | ((y & MINI_COORD_MASK) << LOG2_MINIW);
	}

	// Returns the offset of a mini from its tile
	inline gmtry2i::vector2i get_mini_offset(unsigned int mini_idx, unsigned int log2_w) {
		return gmtry2i::vector2i((mini_idx & MINI_COORD_MASK) << LOG2_MINIW, 
		                         (mini_idx >> (log2_w - LOG2_MINIW)) << LOG2_MINIW);
	}
	// Returns the offset of a single occupancy state from its mini
	inline gmtry2i::vector2i get_bit_offset(unsigned int bit_idx) {
		return gmtry2i::vector2i(bit_idx & MINI_COORD_MASK, bit_idx >> LOG2_MINIW);
	}

	// Accessors for individual occupancy-states in a tile
	template <unsigned int log2_w>
	inline bool get_occ(unsigned int x, unsigned int y, const otile<log2_w>& ot) {
		return (ot.minis[get_mini_idx(x, y, log2_w)] >> get_bit_idx(x, y)) & 1;
	}
	template <unsigned int log2_w>
	inline void set_occ(unsigned int x, unsigned int y, otile<log2_w>& ot, bool value) {
		const omini new_occ = static_cast<omini>(value) << get_bit_idx(x, y);
		omini& ot_occ = ot.minis[get_mini_idx(x, y, log2_w)];
		ot_occ = (ot_occ & ~new_occ) | new_occ;
	}
	template <unsigned int log2_w>
	inline void put_occ(unsigned int x, unsigned int y, otile<log2_w>& ot) {
		ot.minis[get_mini_idx(x, y, log2_w)] |= static_cast<omini>(1) << get_bit_idx(x, y);
	}

	// Converts between 2D tile-space coordinates and their compressed integer representation
	inline unsigned int compress_coords2(const gmtry2i::vector2i& p, unsigned int log2_w) {
		return p.x | (p.y << log2_w);
	}
	inline gmtry2i::vector2i decompress_coords2(unsigned int idx, unsigned int log2_w) {
		return gmtry2i::vector2i(idx & MINI_COORD_MASK, idx >> log2_w);
	}

	// Tile with both required (non-modifiable) and temporary (can be observed or forgotten) occupancies
	template <unsigned int log2_w>
	struct separated_otile {
		// Purely observed part
		otile<log2_w> tmp;
		// Purely required part
		otile<log2_w> req;
	};

	/*
	* Tile with gradient occupancies ranging in certainty.
	* States that cannot be unoccupied have a certainty of 255;
	* Normal modifiable states have certainty less than or equal to gradient_otile::MAX_CERTAINTY.
	*/
	template <unsigned int log2_w>
	struct gradient_otile {
		// Maximum certainty for a temporary occupancy; equal to some positive 2^n - 1
		static const unsigned char MAX_CERTAINTY = (1 << 6) - 1;

		unsigned char certainties[1 << (log2_w * 2)];
		gradient_otile() = default;
		gradient_otile(const otile<log2_w>& t) {
			for (int y = 0; y < (1 << log2_w); y++)
				for (int x = 0; x < (1 << log2_w); x++)
					certainties[x | (y << log2_w)] = get_occ(x, y, t) ? MAX_CERTAINTY : 0;
		}
		gradient_otile(const separated_otile<log2_w>& t) {
			for (int y = 0; y < (1 << log2_w); y++)
				for (int x = 0; x < (1 << log2_w); x++) {
					if (get_occ(x, y, t.req)) certainties[x | (y << log2_w)] = -1;
					else if (get_occ(x, y, t.tmp)) certainties[x | (y << log2_w)] = MAX_CERTAINTY;
					else certainties[x | (y << log2_w)] = 0;
				}
		}
		operator otile<log2_w>() const {
			otile<log2_w> t = otile<log2_w>();
			for (int y = 0; y < (1 << log2_w); y++)
				for (int x = 0; x < (1 << log2_w); x++)
					if (certainties[x | (y << log2_w)])
						put_occ(x, y, t);
			return t;
		}
		operator separated_otile<log2_w>() const {
			separated_otile<log2_w> t;
			for (int y = 0; y < (1 << log2_w); y++)
				for (int x = 0; x < (1 << log2_w); x++) {
					unsigned char certainty = certainties[x | (y << log2_w)];
					if (certainty) put_occ(x, y, ~certainty ? t.tmp : t.req);
				}
			return t;
		}
	};

	// Returns the occupancy-state of the given position within the tile
	template <unsigned int log2_w>
	inline bool get_occ(unsigned int x, unsigned int y, const gradient_otile<log2_w>& t) {
		return t.certainties[x | (y << log2_w)];
	}

	// 3D tile of binary occupancy states formed by layers of 2D binary occupancy tiles
	template <unsigned int log2_w, unsigned int num_layers>
	struct otile3 {
		otile<log2_w> layers[num_layers];
		otile3() = default;
		inline otile3& operator +=(const otile3& tile) {
			for (int z = 0; z < num_layers; z++)
				layers[z] += tile.layers[z];
			return *this;
		}
		inline otile3& operator +=(const otile3* tile) {
			for (int z = 0; z < num_layers; z++)
				layers[z] += tile->layers[z];
			return *this;
		}
		inline otile3& operator -=(const otile3& tile) {
			for (int z = 0; z < num_layers; z++)
				layers[z] -= tile.layers[z];
			return *this;
		}
		inline otile3& operator -=(const otile3* tile) {
			for (int z = 0; z < num_layers; z++)
				layers[z] -= tile->layers[z];
			return *this;
		}
		inline otile3 operator =(const otile<log2_w>& tile) {
			for (int z = 0; z < num_layers; z++)
				layers[z] = tile;
			return *this;
		}
	};

	template <unsigned int log2_w, unsigned int num_layers>
	inline bool get_bit(int x, int y, int z, const otile3<log2_w, num_layers>& ot) {
		return get_bit(x, y, ot.layers[z]);
	}

	template <unsigned int log2_w, unsigned int num_layers>
	inline void set_bit(int x, int y, int z, otile3<log2_w, num_layers>& ot, bool value) {
		set_bit(x, y, ot.layers[z], value);
	}

	template <unsigned int log2_w>
	inline unsigned int compress_coords3(const gmtry3::vector3& p) {
		return static_cast<unsigned int>(p.x) + 
			 ((static_cast<unsigned int>(p.y) +
			  (static_cast<unsigned int>(p.z) << log2_w)) << log2_w);
	}

	template <unsigned int log2_w>
	inline gmtry3::vector3 decompress_coords3(unsigned int idx) {
		unsigned int mask = (1 << log2_w) - 1;
		return gmtry3::vector3(idx & mask, (idx >> log2_w) & mask, idx >> (2 * log2_w));
	}
}