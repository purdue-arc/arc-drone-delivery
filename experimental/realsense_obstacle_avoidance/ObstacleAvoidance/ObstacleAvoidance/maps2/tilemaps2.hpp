#pragma once

#include "../util/geometry.hpp"

#include <iostream>
#include <memory>

#ifdef DEBUG
#	define DEBUG_PRINT(s) std::cout << s << std::endl;
#else 
#	define DEBUG_PRINT(s)
#endif

/*
* Tile Map (2D) Terminology
*	tile: A data-structure that describes a square region of space with a width of 2 ^ n
*	log2_w/log2_tile_w: The base-2 logarithm of the tile width (equal to n from the above expression)
*	spatial tree: A quad-tree (like a binary tree along each dimension) which holds tiles at the bottom layer
*	branch: A tree's branch is a spatial object pointed to by the tree, which will either be another tree or a tile
*	spatial item: Either a spatial tree or a tile; has an origin and is square-shaped
*	depth: Describes how many steps it takes to get from a spatial item down to the tile level
*		For instance, the depth of a tile item is 0 because it is already on the tile level.
*		The depth of a spatial tree which points directly to four tiles is 1 because only one pointer must be 
*			traversed to reach tile level.
*		The width of a spatial item, in UNITS OF TILES, is 2 ^ depth, or 2 ^ (log2_w + depth) in normal units
*	map: A structure that stores tiles, wherein any one point is only described by one tile, and to which tiles may
*		be added or supplemented. Information cannot be forgotten from a map.
*/
namespace maps2 {

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    TILES & TILE ALIGNMENT                                                                      //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Tiles that can be inclusively combined, removed from each other, or totally overwritten
	template <typename T>
	concept writable_tile = requires (T a, T b) {
		a = a + b;
		a = a - b;
		a += b;
		a -= b;
		a = b;
		a = T();
	};

	enum tile_write_mode {
		TILE_REMOVE_MODE = -1,
		TILE_OVERWRITE_MODE = 0,
		TILE_ADD_MODE = 1
	};

	// Preferred way to write tile
	template<writable_tile tile>
	inline void write_tile_to_tile(const tile* new_tile, tile* old_tile, tile_write_mode wmode) {
		if (wmode == TILE_OVERWRITE_MODE) {
			*old_tile = *new_tile;
		}
		else if (wmode == TILE_ADD_MODE) {
			*old_tile += *new_tile;
		}
		else if (wmode == TILE_REMOVE_MODE) {
			*old_tile -= *new_tile;
		}
	}

	// Rounds each coordinate of p down such that p lies on the nearest tile_origin
	inline gmtry2i::vector2i align_down(const gmtry2i::vector2i& p, const gmtry2i::vector2i any_tile_origin, 
		                                unsigned int log2_w) {
		return (((p - any_tile_origin) >> log2_w) << log2_w) + any_tile_origin;
	}

	// Rounds each coordinate of p up such that p lies on the nearest tile_origin
	inline gmtry2i::vector2i align_up(const gmtry2i::vector2i& p, const gmtry2i::vector2i any_tile_origin, 
		                              unsigned int log2_w) {
		gmtry2i::vector2i dif = p - any_tile_origin;
		gmtry2i::vector2i shifted_dif = dif >> log2_w;
		return ((shifted_dif + (dif - (shifted_dif << log2_w) > 0)) << log2_w) + any_tile_origin;
	}

	// Returns a box that completely and exclusively contains all tiles intersected by box b
	inline gmtry2i::aligned_box2i align_out(const gmtry2i::aligned_box2i& b, const gmtry2i::vector2i any_tile_origin, 
		                                    unsigned int log2_w) {
		return gmtry2i::aligned_box2i(align_down(b.min, any_tile_origin, log2_w), 
									  align_up(b.max, any_tile_origin, log2_w));
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    REGIONS AND TILE STREAMS                                                                    //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Tile-aligned region bounded by an aligned_box2i
	class bounded_region {
	public:
		// Returns a tile-aligned rectangular outer bounds on all encompassed area
		virtual gmtry2i::aligned_box2i get_bounds() const = 0;
	};

	// Bounded region that can be iteratively stretched in a direction to expand it to fit points or areas
	template <unsigned int log2_w>
	class stretchable_region : bounded_region {
	public:
		// Stretches the region in the given direction
		virtual void stretch(const gmtry2i::vector2i& direction) = 0;
		// Fits the point into the region by iteratively stretching it as necessary
		void fit(const gmtry2i::vector2i& p) {
			gmtry2i::aligned_box2i alloc_box = this->get_bounds();
			while (!gmtry2i::contains(alloc_box, p)) {
				stretch(p - gmtry2i::center(alloc_box));
				alloc_box = this->get_bounds();
				DEBUG_PRINT("Expanded Map Bounds: " <<
					alloc_box.min.x << ", " << alloc_box.min.y << "; " <<
					alloc_box.max.x << ", " << alloc_box.max.y); //test
			}
		}
		// Fits the box into the region by iteratively stretching it as necessary
		void fit(const gmtry2i::aligned_box2i& box) {
			gmtry2i::aligned_box2i alloc_box = this->get_bounds();
			gmtry2i::vector2i box_center = gmtry2i::center(box);
			while (!gmtry2i::contains(alloc_box, box)) {
				stretch(box_center - gmtry2i::center(alloc_box));
				alloc_box = this->get_bounds();
				DEBUG_PRINT("Expanded Map Bounds: " <<
					alloc_box.min.x << ", " << alloc_box.min.y << "; " <<
					alloc_box.max.x << ", " << alloc_box.max.y); //test
			}
		}
	};

	// Stream for receiving tiles in no particular order
	template <typename tile>
	class tile_istream : public bounded_region {
	public:
		// Resets the stream's reading position
		virtual void reset() = 0;
		// Returns the next tile in the stream, or 0 if all tiles have been retrieved
		virtual const tile* next() = 0;
		// Returns the origin of the last tile retrieved from next()
		virtual gmtry2i::vector2i last_origin() = 0;
		virtual ~tile_istream() {}
	};

	// Stream for receiving only tiles that intersect the given bounds (bounds can be set manually)
	template <typename tile, gmtry2i::intersects_box2i limiter_type = gmtry2i::aligned_box2i>
	class lim_tile_istream : public tile_istream<tile> {
	public:
		// Sets a bounds on the outgoing tiles. 
		virtual void set_bounds(const limiter_type& new_bounds) = 0;
	};

	// Stream for sending tiles in an ordered or unordered manner
	template <typename tile>
	class tile_ostream {
	public:
		// Writes a tile at the given position to the stream
		virtual void write(const gmtry2i::vector2i& p, const tile* src) = 0;
		// Writes each tile from the tile_istream to the stream, assuming only tile-level allignment between streams
		virtual void write(tile_istream<tile>* src) {
			const tile* next_tile;
			while (next_tile = src->next())
				write(src->last_origin(), next_tile);
		}
		// Saves any unsaved changes
		virtual void flush() {}
		virtual ~tile_ostream() {};
	};

	// Stream for reading tiles from a map in an ordered or unordered manner
	template <typename tile>
	class map_istream : public bounded_region {
	public:
		/*
		* Reads tile at given position to the destination
		* Returns 0 if no tile contains the position
		*/
		virtual const tile* read(const gmtry2i::vector2i& p) = 0;
		/*
		* Returns a stream that can be used to stream out all tiles from a desired rectangular region of the map
		* TILE STREAM MUST BE DELETED MANUALLY
		*/
		virtual std::unique_ptr<tile_istream<tile>> read() = 0;
		virtual ~map_istream() {}
	};

	template <typename tile, gmtry2i::intersects_box2i limiter_type = gmtry2i::aligned_box2i>
	class lim_tile_istream_vendor {
	public:
		// Returns a limitable tile input stream, which starts off limited by the limit parameter
		virtual std::unique_ptr<lim_tile_istream<tile, limiter_type>> read(const limiter_type& limit) = 0;
	};

	// Stream for writing tiles to a map with options for specifying how new tiles will be written to existing ones
	template <writable_tile tile>
	class map_ostream : public tile_ostream<tile> {
	public:
		// Accessors for writing mode (determines how a written tile will be combined with an existing one)
		virtual tile_write_mode get_wmode() = 0;
		virtual void set_wmode(tile_write_mode new_write_mode) = 0;
	};

	// Stream for reading and writing tiles to/from a map
	template <typename tile>
	class map_iostream : public map_istream<tile>, public map_ostream<tile> {};

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    SPACIAL TREES                                                                               //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// tree used to represent a square region of space whose width is 2^n for some positive integer n
	template <typename T>
	struct spatial_tree {
		T* branch[4];
	};

	// tree whose branches may either be all tiles or all trees, depending on its depth
	using mixed_tree = spatial_tree<void>;

	/*
	* item: is a mixed_tree* if depth > 0 or a tile* if depth == 0
	* depth: number of layers below layer of the item parameter (depth at root of tree = #layers - 1;
																 depth at base of tree = 0)
	*/
	template <typename tile>
	void delete_mixed_tree(void* item, unsigned int depth) {
		if (depth > 0 && item) {
			for (int i = 0; i < 4; i++)
				delete_mixed_tree<tile>(static_cast<mixed_tree*>(item)->branch[i], depth - 1);
			delete static_cast<mixed_tree*>(item);
		}
		else delete static_cast<tile*>(item);
	}

	// tree that holds the data of T while also branching off to similar trees
	template <typename T>
	struct homogeneous_tree : public spatial_tree<homogeneous_tree<T>>, public T {};

	template <typename T>
	void delete_homogeneous_tree(homogeneous_tree<T>* tree, unsigned int depth) {
		if (depth >= 0 && tree) {
			for (int i = 0; i < 4; i++)
				delete_homogeneous_tree(tree->branch[i], depth - 1);
			delete tree;
		}
	}

	inline gmtry2i::vector2i get_next_branch_disp(unsigned int branch_idx, unsigned long branch_width) {
		return gmtry2i::vector2i(branch_idx & 1, branch_idx >> 1) * branch_width;
	}

	/*
	* Holds information about a spatial tree
	* depth: depth of the tree (# of layers in the tree above the tile layer)
	* origin: origin (southeasternmost position) of tree in world-space
	*/
	template <unsigned int log2_tile_w>
	struct tree_info {
		gmtry2i::vector2i origin;
		unsigned int depth;
		tree_info() = default;
		tree_info(const gmtry2i::vector2i tree_origin, unsigned int tree_depth) {
			origin = tree_origin;
			depth = tree_depth;
		}
		inline unsigned int get_width() const {
			return 1 << (depth + log2_tile_w);
		}
		inline gmtry2i::aligned_box2i get_bounds() const {
			return gmtry2i::aligned_box2i(origin, get_width());
		}
		inline unsigned int get_branch_width() const {
			return get_width() >> 1;
		}
		inline unsigned int get_branch_idx(const gmtry2i::vector2i& branch_origin, unsigned int branch_width) const {
			return (branch_origin.x - origin.x >= branch_width) + 2 * (branch_origin.y - origin.y >= branch_width);
		}
		inline unsigned int get_branch_idx(const gmtry2i::vector2i& branch_origin) const {
			return get_branch_idx(branch_origin, get_branch_width());
		}
		inline tree_info get_branch_info(unsigned int branch_idx, unsigned int branch_width) const {
			return tree_info(origin + get_next_branch_disp(branch_idx, branch_width), depth - 1);
		}
		inline tree_info get_branch_info(const gmtry2i::vector2i& branch_origin) const {
			unsigned int branch_width = get_branch_width();
			return get_branch_info(get_branch_idx(branch_origin, branch_width), branch_width);
		}
		inline tree_info get_parent_info(const gmtry2i::vector2i& parent_direction) const {
			return tree_info(origin - gmtry2i::vector2i(parent_direction.x < 0, parent_direction.y < 0) * get_width(),
			                 depth + 1);
		}
	};

	/*
	* Temporarily holds information about a part of a spatial tree (either another tree or a tile)
	* Does not free tree memory
	* Ptr is assumed to be non-null; it points to a tree if depth > 0 or tile if depth = 0
	* T must either be void or an extension of homogeneous_tree
	*/
	template <unsigned int log2_tile_w, typename T>
	struct spatial_item {
		tree_info<log2_tile_w> info;
		T* ptr;
		spatial_item() = default;
		spatial_item(T* item_ptr, gmtry2i::vector2i item_origin, unsigned int item_depth) {
			info = tree_info<log2_tile_w>(item_origin, item_depth);
			ptr = item_ptr;
		}
		spatial_item(T* item_ptr, const tree_info<log2_tile_w>& item_info) {
			info = item_info;
			ptr = item_ptr;
		}
		inline spatial_item get_branch_item(unsigned int branch_idx, unsigned int branch_width) const {
			return spatial_item(static_cast<spatial_tree<T>*>(ptr)->branch[branch_idx],
				                info.get_branch_info(branch_idx, branch_width));
		}
		inline spatial_item get_branch_item(unsigned int branch_idx) const {
			return get_branch_item(branch_idx, info.get_branch_width());
		}
		inline spatial_item get_branch_item(const gmtry2i::vector2i& branch_origin) const {
			unsigned int branch_width = info.get_branch_width();
			return get_branch_item(info.get_branch_idx(branch_origin, branch_width), branch_width);
		}
		inline spatial_item create_branch_item(const gmtry2i::vector2i& branch_origin, T* branch_ptr) {
			unsigned int branch_width = info.get_branch_width();
			unsigned int branch_idx = info.get_branch_idx(branch_origin, branch_width);
			return spatial_item(static_cast<spatial_tree<T>*>(ptr)->branch[branch_idx] = branch_ptr,
				                info.get_branch_info(branch_idx, branch_width));
		}
		inline spatial_item create_parent_item(const gmtry2i::vector2i& parent_direction) {
			unsigned int my_index = (parent_direction.x < 0) + 2 * (parent_direction.y < 0);
			spatial_item new_parent(new mixed_tree(), info.get_parent_info(parent_direction));
			static_cast<spatial_tree<T>*>(new_parent.ptr)->branch[my_index] = ptr;
			return new_parent;
		}
	};

	// spatial item for representing some component of a mixed tree
	template <unsigned int log2_w>
	using mixed_item = spatial_item<log2_w, void>;

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    TREE ACCESS                                                                                 //
	////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/*
	* Walks through the bottom layer (depth = 0) of a tree, returning each tile found along the way
	* Returns 0 when all tiles have been read; can be reset to start from the first tile again
	* Makes no assumptions about how the branches of an item/tree are formatted and accessed 
	*	(default implementation, which may be overridden, assumes items are standard mixed_trees)
	*/
	template <unsigned int log2_w, typename tile, gmtry2i::intersects_box2i limiter_type = gmtry2i::aligned_box2i>
		requires std::copyable<limiter_type>
	class tree_walker : public lim_tile_istream<tile, limiter_type> {
		tree_info<log2_w> info;
		void** items;
		gmtry2i::vector2i* origins;
		unsigned int* branch_indices;
		unsigned int current_level;
		limiter_type limiter;
	protected:
		inline virtual void* get_next_item(void* current_item, unsigned int branch_index) {
			return static_cast<mixed_tree*>(current_item)->branch[branch_index];
		}
		inline virtual tile* get_tile(void* item) {
			return static_cast<tile*>(item);
		}
	public:
		void reset() {
			origins[0] = info.origin;
			branch_indices[0] = 0;
			current_level = 0;
		}
		tree_walker(const mixed_item<log2_w>& item, const limiter_type& default_limiter) {
			info = item.info;
			items = new void* [info.depth + 1];
			items[0] = item.ptr;
			origins = new gmtry2i::vector2i[info.depth + 1];
			branch_indices = new unsigned int[info.depth];
			limiter = default_limiter; // gmtry2i::intersection(default_limiter, info.get_bounds());
			reset();
		}
		tile* next_tile() {
			// if the top item yields a tile, don't explore it the same as a tree-yielding item would be explored
			if (info.depth == 0) {
				// return 0 if it has already been read
				if (branch_indices[0]) return 0;
				// return the tile if it hasn't been read yet
				else {
					branch_indices[0]++;
					return get_tile(items[0]);
				}
			}
			// if top item (the whole tree basically) is not fully explored, continue exploring
			while (branch_indices[0] < 4) {

				// if current item has been fully explored, step up into parent item and move on to next branch
				if (branch_indices[current_level] > 3) branch_indices[--current_level]++;
				else {
					unsigned int current_depth = info.depth - current_level;
					unsigned int half_width = 1 << (log2_w + current_depth - 1);
					origins[current_level + 1] = origins[current_level] + 
						get_next_branch_disp(branch_indices[current_level], half_width);

					// test if next item is in the designated search bounds
					if (gmtry2i::intersects(limiter, gmtry2i::aligned_box2i(origins[current_level + 1], half_width))) {
						DEBUG_PRINT("Reading item from depth " << (current_depth + 1) <<
						            " and position " << gmtry2i::to_string(origins[current_level + 1])); //test
						items[current_level + 1] = get_next_item(items[current_level], branch_indices[current_level]);

						// test if the next item was successfully loaded (exists)
						if (items[current_level + 1]) {

							// if next item yields a tile, get and return the tile
							if (current_depth == 1) {
								DEBUG_PRINT("Reading tile from position " <<
								            gmtry2i::to_string(origins[current_level + 1])); //test
								// get tile from next item
								tile* next_tile = get_tile(items[current_level + 1]);
								// move on to next branch of current item (next item is fully explored)
								branch_indices[current_level]++;
								return next_tile;
							}
							// if next item yields a tree, step into it
							else {
								branch_indices[current_level + 1] = 0;
								current_level++;
								DEBUG_PRINT("Stepping into tree at depth " << (current_depth) <<
								            " and position " << gmtry2i::to_string(origins[current_level])); //test
							}
						}
						// next item is decidedly irrelevent; move on to next branch of current item
						else branch_indices[current_level]++;
					}
					// next item is decidedly irrelevent; move on to next branch of current item
					else branch_indices[current_level]++;
				}
			}
			// the whole tree is explored, return null ptr to denote 
			return 0;
		}
		const tile* next() {
			return next_tile();
		}
		gmtry2i::vector2i last_origin() {
			return origins[current_level + 1];
		}
		gmtry2i::aligned_box2i get_bounds() const {
			gmtry2i::aligned_box2i limiter_bounds = gmtry2i::boundsof(limiter);
			gmtry2i::aligned_box2i tree_bounds = info.get_bounds();
			if (gmtry2i::intersects(limiter_bounds, tree_bounds))
				return gmtry2i::intersection(align_out(limiter_bounds, tree_bounds.min, log2_w), tree_bounds);
			else return gmtry2i::aligned_box2i(tree_bounds.min, 0);
		}
		void set_bounds(const limiter_type& new_bounds) {
			limiter = new_bounds;
			reset();
		}
		~tree_walker() {
			for (int i = 0; i < info.depth; i++) items[i] = 0;
			delete[] items;
			delete[] origins;
			delete[] branch_indices;
		}
	};

	/*
	* Returns a tree_info describing the sub-tree which most tightly fits the given bounds inside of its span
	* If the tree's span does not completely contain the bounds, returns item_info parameter
	* TREE MUST CONTAIN THE GIVEN POINT
	*/
	template <unsigned int log2_w>
	tree_info<log2_w> get_fitted_item_info(const tree_info<log2_w>& item_info, const gmtry2i::aligned_box2i& bounds) {
		tree_info<log2_w> matching_item(item_info.origin, item_info.depth);
		tree_info<log2_w> next_matching_item(item_info.origin, item_info.depth);
		while (gmtry2i::contains(next_matching_item.get_bounds(), bounds) && matching_item.depth) {
			matching_item = next_matching_item;
			next_matching_item = matching_item.get_branch_info(bounds.min);
		}
		return matching_item;
	}

	/*
	* Retrieves the spatial item closest to the desired depth which contains the given point in its span
	* Returned item will contain the given point if the starting item contained it
	* Returned item depth will be less than or equal to the starting item depth
	* TREE MUST CONTAIN THE GIVEN POINT
	*/
	template <unsigned int log2_w>
	mixed_item<log2_w> seek_mixed_item(const mixed_item<log2_w>& start, const gmtry2i::vector2i& p, unsigned int depth) {
		mixed_item<log2_w> item = mixed_item<log2_w>();
		mixed_item<log2_w> next_item = start;
		while (next_item.ptr && next_item.info.depth > depth) {
			item = next_item;
			next_item = item.get_branch_item(p);
		}
		return next_item.ptr ? next_item : item;
	}

	/*
	* Allocates an item in the tree at the desired position and depth and returns it
	* Returns the existing item if one existed
	* Extends tree branches as necessary to get down to the desired depth
	* TREE MUST CONTAIN THE GIVEN POINT
	*/
	template <unsigned int log2_w, typename tile>
	mixed_item<log2_w> alloc_mixed_item(const mixed_item<log2_w>& top, const gmtry2i::vector2i& p, unsigned int depth) {
		mixed_item<log2_w> item = mixed_item<log2_w>();
		mixed_item<log2_w> next_item = seek_mixed_item(top, p, depth);
		while (next_item.info.depth > depth) {
			item = next_item;
			next_item = item.create_branch_item(p, new mixed_tree());
		}
		// Assuming top contained p, next_item.info.depth equals depth
		// The last time the loop iterated, next_item.ptr was written to item.ptr 
		//	and a new tree was created and written to next_item.ptr
		// Iff the loop was never entered, item.ptr is null
		// If next_item.info.depth == 0, next_item.ptr should point to a tile
		// If the loop was entered (item.ptr is not null) and next_item.info.depth = 0, 
		//	then next_item.ptr points to a new tree, which must be deleted and replaced with a tile
		if (depth == 0 && item.ptr) {
			delete static_cast<mixed_tree*>(next_item.ptr);
			next_item.ptr = static_cast<mixed_tree*>(item.ptr)->branch[item.info.get_branch_idx(p)] = new tile();
		}
		return next_item;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    NEIGHBORING TILES                                                                           //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	// Tile that can be traversed to its adjacent and diagonal neighbors
	template <typename base_tile>
	struct nbrng_tile {
		base_tile tile;
		/*
		* Neighbors configured as shown below (X is position of this tile, n is position of nbrs[n])
		* 5 6 7
		* 3 X 4
		* 0 1 2
		*/
		nbrng_tile<base_tile>* nbrs[8];
		nbrng_tile() = default;
		nbrng_tile(const base_tile& t) : nbrs {} {
			tile = t;
		}
	};

	inline gmtry2i::vector2i get_nbrhd_origin(const gmtry2i::vector2i& center_origin, unsigned int log2_w) {
		return center_origin - gmtry2i::vector2i(1 << log2_w, 1 << log2_w);
	}

	// Returns bounds of a nbrng_tile's neighborhood (the region including itself and its neighbors)
	inline gmtry2i::aligned_box2i get_nbrhd_bounds(const gmtry2i::vector2i& center_origin, unsigned int log2_w) {
		gmtry2i::vector2i corners_disp(1 << log2_w, 1 << log2_w);
		return gmtry2i::aligned_box2i(center_origin - corners_disp, center_origin + corners_disp * 2);
	}

	// Returns neighbor's non-negative coordinates relative to the neighborhood origin, in units of tiles
	inline gmtry2i::vector2i get_nbrhd_coords(const gmtry2i::vector2i& nbr_origin, 
		                                        const gmtry2i::vector2i& nbrhood_origin, unsigned int log2_w) {
		return (nbr_origin - nbrhood_origin) >> log2_w;
	}

	// Returns index (for a nbrng_tile's nbrs array) of a neighbor
	inline unsigned int get_nbr_idx(const gmtry2i::vector2i& nbr_nbrhood_coords) {
		unsigned int compressed_coords = nbr_nbrhood_coords.x + 3 * nbr_nbrhood_coords.y;
		return compressed_coords - (compressed_coords > 4);
	}

	// Returns index (for a nbrng_tile's nbrs array) of neighbor in the direction of center_nbr_disp
	inline unsigned int get_nbr_idx(const gmtry2i::vector2i& center_nbr_disp, unsigned int log2_w) {
		unsigned char compressed_coords = (center_nbr_disp.x >= 0) + (center_nbr_disp.x >= (1 << log2_w)) + 
		                             3 * ((center_nbr_disp.y >= 0) + (center_nbr_disp.y >= (1 << log2_w)));
		return compressed_coords - (compressed_coords > 4);
	}

	template <unsigned int log2_w, typename tile>
	struct tile_nbrhd {
		gmtry2i::vector2i origin;
		tile* tiles[3][3];
		tile_nbrhd() = default;
		tile_nbrhd(const gmtry2i::vector2i& center_origin, nbrng_tile<tile>* center) : tiles {
			&(center->nbrs[5]->tile), &(center->nbrs[6]->tile), &(center->nbrs[7]->tile),
			&(center->nbrs[3]->tile), &(center->tile)         , &(center->nbrs[4]->tile),
			&(center->nbrs[0]->tile), &(center->nbrs[1]->tile), &(center->nbrs[2]->tile)
		} {
			origin = center_origin - gmtry2i::vector2i(1 << log2_w, 1 << log2_w);
		}
		inline tile* operator [](int i) {
			return (*tiles)[i];
		}
		inline tile* operator ()(int nbr_x, int nbr_y) {
			return tiles[nbr_y][nbr_x];
		}
		inline const tile* operator ()(int nbr_x, int nbr_y) const {
			return tiles[nbr_y][nbr_x];
		}
	};

	/*
	* Links the new tile located at the given position with its neighbors.
	* Doesn't insert the new tile into the spacial tree.
	* If a tile was there before, it will be unlinked 
	*	(so the caller should either have another reference to what's there already or know that nothing's there).
	*/
	template <unsigned int log2_w, typename tile>
	void link_nbrng_tile(const mixed_item<log2_w>& top, const gmtry2i::vector2i& p, nbrng_tile<tile>* new_tile) {
		gmtry2i::aligned_box2i neighborhood_bounds = get_nbrhd_bounds(align_down(p, top.info.origin, log2_w), log2_w);
		tree_walker<log2_w, nbrng_tile<tile>> nbr_retriever(top, neighborhood_bounds);
		nbrng_tile<tile>* next_nbr;
		gmtry2i::vector2i local_coords;
		int compact_coords;
		while (next_nbr = nbr_retriever.next_tile()) {
			// Local coordinates of neighbor, relative to neighborhood around tile
			local_coords = (nbr_retriever.last_origin() - neighborhood_bounds.min) >> log2_w;
			compact_coords = local_coords.x + 3 * local_coords.y;
			if (compact_coords != 4) {
				// Link tile to neighbor
				new_tile->nbrs[compact_coords - (local_coords.x > 4)] = next_nbr;
				// Local coordinates of tile, relative to neighbor
				local_coords = gmtry2i::vector2i(2, 2) - local_coords;
				compact_coords = local_coords.x + 3 * local_coords.y;
				// Link neighbor to tile
				next_nbr->nbrs[compact_coords - (local_coords.x > 4)] = new_tile;
			}
		}
	}

	/*
	* Allocates a neighboring tile in a tree, links it bidirectionally with its neighbors, and returns it.
	* Returns the existing item if one existing, skipping the linking process.
	* Extends tree branches as necessary to get down to the desired depth.
	* TREE MUST CONTAIN THE GIVEN POINT
	*/
	template <unsigned int log2_w, typename tile>
	mixed_item<log2_w> alloc_nbrng_tile(const mixed_item<log2_w>& top, const gmtry2i::vector2i& p) {
		gmtry2i::aligned_box2i neighborhood_bounds = 
			get_nbrhd_bounds(align_down(p, top.info.origin, log2_w), log2_w);
		mixed_item<log2_w> item = seek_mixed_item(top, p, 0);
		// If tile was already allocated and connected to neighbors, return
		if (item.info.depth == 0) return item;
		item = alloc_mixed_item<log2_w, nbrng_tile<tile>>(item, p, 0);
		link_nbrng_tile<log2_w, tile>(top, p, static_cast<nbrng_tile<tile>*>(item.ptr));
		return item;
	}
}