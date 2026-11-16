#pragma once

#include "occupancy.hpp"
#include "../maps2/maps2_streams.hpp"

#include <vector>

namespace ocpncy {
	// Produces tiles from a matrix of occupancy values (a state is occupied if it's occupancy value is not 0)
	template <unsigned int log2_w, typename T>
	class mat_tile_stream : public maps2::lim_tile_istream<otile<log2_w>> {
		T* mat;
		gmtry2i::vector2i origin;
		otile<log2_w> last_tile;
		// all geometric objects below are positioned relative to origin
		gmtry2i::aligned_box2i mat_bounds;
		gmtry2i::aligned_box2i bounds;
		gmtry2i::vector2i tilewise_origin;
		gmtry2i::vector2i tile_origin;
		gmtry2i::vector2i last_tile_origin;
	public:
		void reset() {
			tile_origin = tilewise_origin;
		}
		mat_tile_stream(T* T_mat, unsigned int width, unsigned int height,
			const gmtry2i::vector2i& mat_origin, const gmtry2i::vector2i& any_tile_origin) {
			mat = T_mat;
			origin = mat_origin;
			mat_bounds = gmtry2i::aligned_box2i(gmtry2i::vector2i(), gmtry2i::vector2i(width, height));
			bounds = mat_bounds;
			tilewise_origin = maps2::align_down(mat_origin, any_tile_origin, log2_w) - mat_origin;
			reset();
		}
		const otile<log2_w>* next() {
			if (tile_origin.y >= bounds.max.y) return 0;
			otile<log2_w> tile = otile<log2_w>();
			gmtry2i::aligned_box2i readbox = 
				gmtry2i::intersection(bounds, gmtry2i::aligned_box2i(tile_origin, 1 << log2_w));
			for (int x = readbox.min.x; x < readbox.max.x; x++)
				for (int y = readbox.min.y; y < readbox.max.y; y++)
					if (mat[x + y * mat_bounds.max.x])
						put_occ(x - tile_origin.x, y - tile_origin.y, tile);
			last_tile_origin = tile_origin;
			tile_origin.x += 1 << log2_w;
			if (tile_origin.x >= bounds.max.x) {
				tile_origin.x = tilewise_origin.x;
				tile_origin.y += 1 << log2_w;
			}
			last_tile = tile;
			return &last_tile;
		}
		gmtry2i::vector2i last_origin() {
			return last_tile_origin + origin;
		}
		gmtry2i::aligned_box2i get_bounds() const {
			return maps2::align_out(bounds, tilewise_origin, log2_w) + origin;
		}
		void set_bounds(const gmtry2i::aligned_box2i& new_bounds) {
			gmtry2i::aligned_box2i local_new_bounds = new_bounds - origin;
			if (gmtry2i::intersects(mat_bounds, local_new_bounds)) 
				bounds = gmtry2i::intersection(mat_bounds, local_new_bounds);
			else bounds = { mat_bounds.min, 0 };
			tilewise_origin = maps2::align_down(bounds.min, tilewise_origin, log2_w);
			reset();
		}
		~mat_tile_stream() { }
	};

	// Cuts each dimension of the matrix in half
	template <typename T>
	T* halve_matrix(const T* mat, unsigned int width, unsigned int height) {
		unsigned int new_width = width >> 1;
		unsigned int new_height = height >> 1;
		T* half_mat = new T[new_width * new_height];
		for (int i = 0; i < new_width * new_height; i++) half_mat[i] = 0;
		for (unsigned int y = 0, hy = 0; y < height && hy < new_height; y++, hy = y >> 1)
			for (unsigned int x = 0, hx = 0; x < width && hx < new_width; x++, hx = x >> 1)
				half_mat[hx + new_width * hy] |= mat[x + width * y];
		return half_mat;
	}

	// delete? THIS CLASS MIGHT BE DELETED
	template <unsigned int radius_minis>
	class occupancy_accumulator : public gmtry2i::point_ostream2i, public maps2::tile_istream<omini> {
	protected:
		omini minis[1 + 2 * radius_minis][1 + 2 * radius_minis] = {};
		gmtry2i::vector2i origin;
		// Geometric objects below are defined relative to origin
		gmtry2i::vector2i next_mini_origin, last_mini_origin;
		gmtry2i::aligned_box2i minis_bounds, read_bounds;
	public:
		void reset() {
			next_mini_origin = read_bounds.min;
		}
		occupancy_accumulator(const gmtry2i::vector2i& center, const gmtry2i::vector2i& any_mini_origin) {
			origin = maps2::align_down(center, any_mini_origin, 3) -
				(gmtry2i::vector2i(radius_minis, radius_minis) << 3);
			minis_bounds = { {}, (1 + 2 * radius_minis) << 3 };
			read_bounds = minis_bounds;
			reset();
		}
		inline void write(const gmtry2i::vector2i& p) {
			gmtry2i::vector2i local_p = p - origin;
			if (gmtry2i::contains(minis_bounds, local_p))
				minis[local_p.x >> LOG2_MINIW][local_p.y >> LOG2_MINIW] |=
				((omini)0b1) << ((local_p.x & MINI_COORD_MASK) + ((local_p.y & MINI_COORD_MASK) << LOG2_MINIW));
		}
		const omini* next() {
			while (next_mini_origin.y < read_bounds.max.y) {
				last_mini_origin = next_mini_origin;
				omini* next_mini_ptr = &(minis[last_mini_origin.y >> LOG2_MINIW][last_mini_origin.x >> LOG2_MINIW]);
				if ((next_mini_origin.x += MINI_WIDTH) > read_bounds.max.x) {
					next_mini_origin.x = read_bounds.min.x;
					next_mini_origin.y += MINI_WIDTH;
				}
				if (next_mini_ptr) return next_mini_ptr;
			}
			return 0;
		}
		gmtry2i::vector2i last_origin() {
			return last_mini_origin + origin;
		}
	};

	// Is fed updates on changed-occupancy states from an occupancy map
	template <unsigned int log2_w>
	class occmap_monitor {
	public:
		virtual void write(gradient_otile<log2_w>* tile_ptr, unsigned int occupancy_idx) = 0;
	};

	/*
	* Observes new nearby occupancies, compares them with recorded occupancies, 
	*	updates records, tracks which recorded tiles have been changed, 
	*	and feeds newly discovered occupancies to an occmap_monitor.
	* Only cares about occupancies of observer's current tile and its neighbors.
	* Does not manage the map; cannot add new tiles or load in existing ones to the map.
	* Has to request for missing tiles to be added
	*/
	template <unsigned int log2_w, unsigned int observation_radius>
	class occupancy_observer : public gmtry2i::point_ostream2i {
		static const unsigned int radius_minis = std::min((observation_radius >> LOG2_MINIW), 
		                                                  get_tile_width_minis(log2_w));
		static const unsigned int accumulator_width_minis = 1 + 2 * radius_minis;

		typedef gradient_otile<log2_w> gradient_tile;

		class forgetter : public gmtry2i::point_ostream2i {
			gradient_tile& tile;
		public:
			forgetter(gradient_tile& target_tile) : tile(target_tile) {}
			inline void write(const gmtry2i::vector2i& p) {
				unsigned char& intrsctd_char = tile.certainties[p.x + (p.y << log2_w)];
				if (intrsctd_char && ~intrsctd_char) intrsctd_char--;
			}
		};

		gmtry2i::vector2i position, tile_origin;
		maps2::nbrng_tile<gradient_tile>* current_tile;
		// Tiles used as the control group; compared with map tiles after map is updated to find changed states
		gradient_tile* control_tiles[3][3];
		/*
		* When the observer needs a neighboring tile that hasn't been added to the map yet, it submits a request
		* through the tile_requestee. If the tile exists, it will be added to the map by the requestee. Otherwise,
		* and exception will be thrown.
		*/
		gmtry2i::point_ostream2i* tile_requestee;
		// For reporting changed-state occupancies
		occmap_monitor<log2_w>& changes_listener;
		// Aggregates occupancies from one wave of observed points (accumulator is completely contained in neighborhood)
		omini accumulator[accumulator_width_minis][accumulator_width_minis] = {};
		gmtry2i::aligned_box2i accumulator_bounds;

		void clear_accumulator() {
			for (int i = 0; i < accumulator_width_minis * accumulator_width_minis; i++)
				(*accumulator)[i] = omini();
		}
		void update_accumulator_bounds() {
			gmtry2i::vector2i gator_corner_disp(radius_minis << LOG2_MINIW, radius_minis << LOG2_MINIW);
			accumulator_bounds = gmtry2i::aligned_box2i(-gator_corner_disp, (gmtry2i::vector2i(1, 1) << log2_w) + 
			                                            gator_corner_disp) + tile_origin;
		}
		void fill_control() {
			maps2::tile_nbrhd<log2_w, gradient_tile> nbrhd({}, current_tile);
			for (int i = 0; i < 9; i++) if (!((*control_tiles)[i]) && nbrhd[i])
				(*control_tiles)[i] = new gradient_tile(*nbrhd[i]);
		}
	public:
		occupancy_observer(const gmtry2i::vector2i& init_position, maps2::nbrng_tile<gradient_tile>* init_tile,
		                   const gmtry2i::vector2i& any_tile_origin, occmap_monitor<log2_w>* listener) : 
			changes_listener(*listener) {
			position = init_position;
			current_tile = init_tile;
			tile_origin = maps2::align_down(position, any_tile_origin, log2_w);
			tile_requestee = 0;
			clear_accumulator();
			update_accumulator_bounds();
			fill_control();
		}
		// Returns the tile in which the observer is currently contained
		gradient_tile* get_current_tile() {
			return current_tile;
		}
		// Requestee is expected to load requested tiles into the map at the requested positions
		void set_requestee(gmtry2i::point_ostream2i* new_requestee) {
			tile_requestee = new_requestee;
		}
		// Throws std::exception if path to the destination crosses tiles that aren't provided by the tile_requestee
		void move(const gmtry2i::vector2i& new_position) {
			gmtry2i::vector2i new_tile_origin = maps2::align_down(new_position, tile_origin, log2_w);
			// in the loop, position is always tile-aligned
			while (tile_origin != new_tile_origin) {
				gmtry2i::vector2i disp = new_tile_origin - tile_origin;
				gmtry2i::vector2i nbr_nbrhd_coords = (disp >= 0) + (disp >= (1 << log2_w));
				gmtry2i::vector2i nbr_origin = tile_origin + ((nbr_nbrhd_coords - gmtry2i::vector2i(1, 1)) << log2_w);
				unsigned int nbr_compressed_coords = nbr_nbrhd_coords.x + 3 * nbr_nbrhd_coords.y;
				unsigned int nbr_idx = nbr_compressed_coords - (nbr_compressed_coords > 4);
				if (tile_requestee && !current_tile->nbrs[nbr_idx]) {
					tile_requestee->write(nbr_origin);
					tile_requestee->flush();
				}
				current_tile = current_tile->nbrs[nbr_idx];
				if (!current_tile) throw std::exception("Observer fell off the map!");
				tile_origin = nbr_origin;

				// Backup the shared control_tiles and delete the ones that are left behind
				gradient_tile* new_control[3][3] = {};
				for (int x = 1; x < 3; x++) for (int y = 1; y < 3; y++) 
					if (std::abs(x - nbr_nbrhd_coords.x) <= 1 && std::abs(y - nbr_nbrhd_coords.y) <= 1) 
						new_control[y - nbr_nbrhd_coords.y + 1][x - nbr_nbrhd_coords.x + 1] = control_tiles[y][x];
					else delete control_tiles[y][x];
				// Copy over new control_tiles
				for (int i = 0; i < 9; i++) (*control_tiles)[i] = (*new_control)[i];
			}
			position = new_position;
			update_accumulator_bounds();
			fill_control();
		}
		/*
		* Relocates observer to new tile and position
		* Can be used to move observer to an entirely new map
		* Use this if observer falls off the map
		*/
		void relocate(const gmtry2i::vector2i& new_position, maps2::nbrng_tile<gradient_tile>* new_tile) {
			position = new_position;
			current_tile = new_tile;
			update_accumulator_bounds();
			fill_control();
		}
		void write(const gmtry2i::vector2i& p) override {
			if (gmtry2i::contains(accumulator_bounds, p)) {
				gmtry2i::vector2i local_p = p - accumulator_bounds.min;
				accumulator[local_p.x >> LOG2_MINIW][local_p.y >> LOG2_MINIW] |=
					((omini)1) << ((local_p.x & MINI_COORD_MASK) | ((local_p.y & MINI_COORD_MASK) << LOG2_MINIW));
			}
		}
		void flush() override {
			maps2::tile_nbrhd<log2_w, gradient_tile> nbrhd(tile_origin, current_tile);
			// Observer position defined relative to neighborhood origin
			gmtry2i::vector2i nbrhd_position = position - nbrhd.origin;
			// Translation from accumulator to neighborhood
			gmtry2i::vector2i gator_nbrhd_shift = nbrhd.origin - accumulator_bounds.min;
			// Defined relative to neighborhood origin
			gmtry2i::aligned_box2i nbrhd_tile_boxes[3][3];
			for (int nbrhd_x = 0; nbrhd_x < 3; nbrhd_x++) for (int nbrhd_y = 0; nbrhd_y < 3; nbrhd_y++) {
				if (nbrhd.tiles[nbrhd_y][nbrhd_x]) nbrhd_tile_boxes[nbrhd_y][nbrhd_x] = 
					gmtry2i::aligned_box2i(gmtry2i::vector2i(nbrhd_x, nbrhd_y) << log2_w, 1 << log2_w);
			}
			// Tracks whether each member of the neighborhood has been modified
			bool nbrs_modified[3][3] = {};
			// Saves the position of each observed occupancy relative to neighborhood origin
			std::vector<gmtry2i::vector2i> observed_points;
			
			// Step 1: Lower certainties of occupancies that should have been seen, but might not have been.
			//         An occupancy might not have been seen if there exists a line of sight (to another occupancy)
			//         which passes through it.
			//         Therefore, each line from the camera to an observed occupancy can be rasterized to find the 
			//         states that might not have been observed, and the more lines pass through a state, the less 
			//         likely it is [still] occupied.
			for (int ay = 0; ay < accumulator_width_minis; ay++) for (int ax = 0; ax < accumulator_width_minis; ax++) {
				omini observed_mini = accumulator[ay][ax];
				if (observed_mini) {
					// Origin of mini relative to neighborhood
					gmtry2i::vector2i mini_origin = (gmtry2i::vector2i(ax, ay) << LOG2_MINIW) + gator_nbrhd_shift;
					for (int bit_idx = 0; bit_idx < MINI_AREA; bit_idx++) if ((observed_mini >> bit_idx) & 1) {
						// Position of an occupancy state relative to the neighborhood
						gmtry2i::vector2i nbrhd_oc_pos(mini_origin | get_bit_offset(bit_idx));
						// Save the point in a list so we won't have to go through accumulator and find it again later
						observed_points.push_back(nbrhd_oc_pos);
						gmtry2i::line_segment2i nbrhd_oc_line(nbrhd_position, nbrhd_oc_pos);
						for (int nbr_x = 0; nbr_x < 3; nbr_x++) for (int nbr_y = 0; nbr_y < 3; nbr_y++) {
							bool no_intersection = false;
							gmtry2::line_segment2 tile_oc_line = 
								gmtry2i::intersection(nbrhd_oc_line, nbrhd_tile_boxes[nbr_y][nbr_x], no_intersection);
							if (!no_intersection) {
								gradient_tile* intersected_tile = nbrhd(nbr_x, nbr_y);
								if (!intersected_tile) continue;
								gmtry2i::rasterize(tile_oc_line, forgetter(*intersected_tile));
							}
							nbrs_modified[nbr_y][nbr_x] = true;
						}
					}
				}
			}

			// Step 2: Raise certainties of spotted occupancies
			int num_observed_points = observed_points.size();
			for (int i = 0; i < num_observed_points; i++) {
				gmtry2i::vector2i point = observed_points[i];
				gradient_tile* occupied_tile = nbrhd(point.x >> log2_w, point.y >> log2_w);
				if (occupied_tile) occupied_tile->
					certainties[(point.x & get_tile_coord_mask(log2_w)) | 
					           ((point.y & get_tile_coord_mask(log2_w)) << log2_w)] |= gradient_tile::MAX_CERTAINTY;
			}

			// Bounds of aggregator relative to neighborhood
			gmtry2i::aligned_box2i nbrhd_gator_bounds(accumulator_bounds + gator_nbrhd_shift);

			// Step 3: Compare occupancies from control_tiles buffer with the map occupancies.
			//         Identify and report changed occupancy states, then copy them to the map.
			for (int nbr_x = 0; nbr_x < 3; nbr_x++) for (int nbr_y = 0; nbr_y < 3; nbr_y++) {
				if (nbrs_modified[nbr_y][nbr_x] && 
				    gmtry2i::intersects(nbrhd_tile_boxes[nbr_y][nbr_x], nbrhd_gator_bounds)) {
					gradient_tile* old_tile = control_tiles[nbr_y][nbr_x];
					gradient_tile* new_tile = nbrhd(nbr_x, nbr_y);
					gmtry2i::aligned_box2i tile_gator_box = 
						gmtry2i::intersection(nbrhd_tile_boxes[nbr_y][nbr_x], nbrhd_gator_bounds) - 
						nbrhd_tile_boxes[nbr_y][nbr_x].min;
					for (long y = tile_gator_box.min.y; y < tile_gator_box.max.y; y++) {
						unsigned int row_start_idx = y << log2_w;
						for (long x = tile_gator_box.min.x; x < tile_gator_box.max.x; x++) {
							unsigned int oc_idx = x | row_start_idx;
							unsigned char& old_occupancy = old_tile->certainties[oc_idx];
							unsigned char new_occupancy = new_tile->certainties[oc_idx];
							// only copies over
							if (static_cast<bool>(old_occupancy) != static_cast<bool>(new_occupancy)) {
								old_occupancy = new_occupancy;
								changes_listener.write(new_tile, oc_idx);
							}
						}
					}
				}
			}
			clear_accumulator();
		}
		~occupancy_observer() {
			for (int i = 0; i < 9; i++) delete (*control_tiles)[i];
		}
	};
}