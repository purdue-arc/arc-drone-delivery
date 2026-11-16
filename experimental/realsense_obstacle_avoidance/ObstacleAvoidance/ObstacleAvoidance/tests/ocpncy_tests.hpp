#pragma once 

//#define DEBUG
#include "../ocpncy/ocpncy_streams.hpp"
#include "../ocpncy/ocpncy_display.hpp"
#include "../util/prjctn_display.hpp"

#include <vector>

#ifndef OUTPUT_FILEPATH 
#	define OUTPUT_FILEPATH (std::string(""))
#endif
#ifndef RESOURCES_FILEPATH
#	define RESOURCES_FILEPATH (std::string(""))
#endif

namespace oc_tests {
	ocpncy::otile<3> smileytile, frownytile;

	ocpncy::otile<4> cattile = ocpncy::otile<4>();
	gmtry2i::vector2i cattileorigin(-70, 30);
	ocpncy::otile<4> dogtile = ocpncy::otile<4>();
	gmtry2i::vector2i dogtileorigin(-25, 5);
	bool forgy[300] = { 0 };
	unsigned int forgydims[2] = { 20, 15 };
	gmtry2i::vector2i forgyorigin = gmtry2i::vector2i(-69, 9); // (5, 5)

	int render_faces() {
		smileytile.minis[0] =
			(0b00000000ULL << 48) +
			(0b00100100ULL << 40) +
			(0b00100100ULL << 32) +
			(0b00000000ULL << 24) +
			(0b01000010ULL << 16) +
			(0b00111100ULL << 8) +
			(0b00000000ULL << 0);
		frownytile.minis[0] =
			(0b00000000ULL << 48) +
			(0b00100100ULL << 40) +
			(0b00100100ULL << 32) +
			(0b00000000ULL << 24) +
			(0b00111100ULL << 16) +
			(0b01000010ULL << 8) +
			(0b00000000ULL << 0);
		return 0;
	}

	int render_cat() {
		cattile = ocpncy::otile<4>();
		int circlex = 4;
		int circley = 9;
		int circler = 2;
		for (int x = 0; x < 16; x++) for (int y = 0; y < 16; y++)
			if (((x - circlex) * (x - circlex) + (y - circley) * (y - circley) - circler * circler) >> 1 == 0)
				ocpncy::set_occ(x, y, cattile, true);
		for (int x = circlex + circler; x < 15; x++) ocpncy::set_occ(x, circley, cattile, true);
		for (int leg = 0; leg < 4; leg++) {
			int x = (((10 - circlex + circler) * leg) / 4) + circlex + circler;
			for (int y = circley; y > 0; y--) ocpncy::set_occ(x, y, cattile, true);
		}
		ocpncy::set_occ(3, 12, cattile, true);
		ocpncy::set_occ(5, 12, cattile, true);
		//PrintTile(cattile);
		return 0;
	}

	int render_dog() {
		dogtile = ocpncy::otile<4>();
		int circlex = 4;
		int circley = 9;
		int circler = 2;
		for (int x = 0; x < 16; x++) for (int y = 0; y < 16; y++)
			if (((x - circlex) * (x - circlex) + (y - circley) * (y - circley) - circler * circler) >> 1 == 0)
				ocpncy::set_occ(x, y, dogtile, true);
		for (int x = circlex + circler; x < 15; x++) ocpncy::set_occ(x, circley, dogtile, true);
		for (int leg = 0; leg < 4; leg++) {
			int x = (((10 - circlex + circler) * leg) / 4) + circlex + circler;
			for (int y = circley; y > 0; y--) ocpncy::set_occ(x, y, dogtile, true);
		}
		ocpncy::set_occ(3, 12, dogtile, true); ocpncy::set_occ(3, 13, dogtile, true);
		ocpncy::set_occ(5, 12, dogtile, true); ocpncy::set_occ(5, 13, dogtile, true);
		for (int x = circlex - circler; x >= 0; x--) {
			ocpncy::set_occ(x, circley, dogtile, true);
			ocpncy::set_occ(x, circley - 1, dogtile, true);
		}
		//std::cout << dogtile;
		return 0;
	}

	int render_forgy() {
		int ovalx = forgydims[0] / 2 - 1;
		int ovaly = 1 + forgydims[1] / 2;
		int ovalrx = 5;
		int ovalry = 3;
		for (int x = 0; x < forgydims[0]; x++) for (int y = 0; y < forgydims[1]; y++)
			if (((x - ovalx) * (x - ovalx) * ovalry * ovalry + (y - ovaly) * (y - ovaly) * ovalrx * ovalrx
				- ovalrx * ovalrx * ovalry * ovalry) >> 7 == 0)
				forgy[x + y * forgydims[0]] = true;
		for (int leg = 0; leg < 4; leg++) {
			int x = ovalx - ovalrx / 2 + (leg * ovalrx) / 2;
			for (int y = ovaly - ovalry; y > ovaly - ovalry * 2; y--) forgy[x + y * forgydims[0]] = true;
		}
		forgy[7 + (ovaly + 1) * forgydims[0]] = true;
		forgy[6 + (ovaly - 1) * forgydims[0]] = true;
		forgy[7 + (ovaly - 1) * forgydims[0]] = true;
		forgy[8 + (ovaly - 1) * forgydims[0]] = true;

		ocpncy::mat_tile_stream<4, bool> iterator(forgy, forgydims[0], forgydims[1], forgyorigin, gmtry2i::vector2i(42, 35));
		ascii_dsp::ascii_image img = maps2::make_tile_image(4, gmtry2i::vector2i(42, 35), 
		                                                    gmtry2i::aligned_box2i(forgyorigin, 1 << (4 + 2)), -1);
		std::cout << (img << ascii_dsp::decorated_rect(iterator.get_bounds(), 0, 0, 'x') << &iterator);
		//

		/* Prints just the matrix (no tiles)
		maps2::ascii_image img({ {0, 0}, gmtry2i::vector2i(forgydims[0], forgydims[1])});
		for (int x = 0; x < forgydims[0]; x++) for (int y = 0; y < forgydims[1]; y++)
			if (forgy[x + y * forgydims[0]]) img(x, y) = '@';
		std::cout << img;
		*/

		return 0;
	}

	// writes the smileytile to the mymap file at (14, 14), reads it back, and prints it
	// writes the frowneytile to the mymap file at (5, 4), reads it back, and prints it
	int occupancy_test0() { // PASSED!
		std::cout << "OCCUPANCY TEST 0" << std::endl;
		render_faces();
		maps2::map_fstream<3, ocpncy::otile<3>> mapstream(OUTPUT_FILEPATH + "mymap", gmtry2i::vector2i(4, 5));
		const ocpncy::otile<3>* mytile = 0;

		// Running the program with enable_write = true allows it to write the smiley and frowny tiles
		// You can run it with enable_write = false and get the same final result if it the tile has already been written
		bool enable_write = true;
		if (enable_write) {
			std::cout << "Tile to Write: " << std::endl << smileytile;
			mapstream.write(gmtry2i::vector2i(14, 14), &smileytile);

			mytile = mapstream.read(gmtry2i::vector2i(14, 14));
			std::cout << "Reading Success: " << static_cast<bool>(mytile) << std::endl;
			if (mytile) std::cout << "Tile Retrieved from File: " << std::endl << *mytile;

			std::cout << "Tile to Write: " << std::endl << frownytile;
			mapstream.write(gmtry2i::vector2i(5, 4), &frownytile);
		}

		mytile = mapstream.read(gmtry2i::vector2i(5, 4));
		std::cout << "Reading Success: " << static_cast<bool>(mytile) << std::endl;
		if (mytile) std::cout << "Tile Retrieved from File: " << std::endl << *mytile;

		std::cout << "Final Map Bounds: " << gmtry2i::to_string(mapstream.get_bounds()) << std::endl;
		std::cout << &mapstream;

		return 0;
	}

	typedef maps2::map_fstream<4, ocpncy::otile<4>> bmap_fstream4;
	typedef maps2::map_buffer<4, ocpncy::otile<4>> bmap_buffer4;
	typedef maps2::spatial_item<4, void> bmap_item4;
	typedef std::unique_ptr < maps2::lim_tile_istream<ocpncy::otile<4>>> btile_lim_stream4;
	typedef std::unique_ptr<maps2::tile_istream<ocpncy::otile<4>>> btile_stream4;

	// Writes cattile to the mymap4 file at (-70, 30), reads the tile back, and prints it
	int occupancy_test3() { // PASSED!
		std::cout << "OCCUPANCY TEST 3" << std::endl;
		render_cat();

		bmap_fstream4 mapstream(OUTPUT_FILEPATH + "mymap4", gmtry2i::vector2i(42, 35));
		const ocpncy::otile<4>* mytile = 0;

		std::cout << "Tile to Write: " << std::endl << cattile;
		mapstream.write(gmtry2i::vector2i(-70, 30), &cattile);

		mytile = mapstream.read(gmtry2i::vector2i(-70, 30));
		std::cout << "Reading Success: " << static_cast<bool>(mytile) << std::endl;
		if (mytile) std::cout << "Tile Retrieved from File: " << std::endl << *mytile;

		std::cout << "Final Map Bounds: " << gmtry2i::to_string(mapstream.get_bounds()) << std::endl;
		std::cout << &mapstream;

		return 0;
	}

	// RUN AFTER TEST 3
	// Reads the mymap4 file and prints part of its contents
	int occupancy_test4() { // PASSED!
		std::cout << "OCCUPANCY TEST 4" << std::endl;
		bmap_fstream4 mapstream(OUTPUT_FILEPATH + "mymap4", gmtry2i::vector2i());
		bmap_buffer4 map(mapstream.get_bounds().min);

		btile_stream4 file_tile_stream = mapstream.read();
		//file_tile_stream->set_bounds(gmtry2i::aligned_box2i(gmtry2i::vector2i(-20, 0), 1 << (4 + 2)));
		map.set_wmode(maps2::TILE_ADD_MODE);
		map.write(file_tile_stream.get());
		std::cout << &map;
		return 0;
	}

	// RUN AFTER TEST 3
	// Writes dogtile to a map at (-25, 5), writes the map to the mymap4 file, reads the file, and prints part of its contents
	int occupancy_test5() { // PASSED!
		std::cout << "OCCUPANCY TEST 5" << std::endl;
		render_dog();

		bmap_fstream4 mapstream(OUTPUT_FILEPATH + "mymap4", gmtry2i::vector2i());

		bmap_buffer4 map(mapstream.get_bounds().min);
		map.write(dogtileorigin, &dogtile);
		dogtileorigin = maps2::align_down(dogtileorigin, map.get_bounds().min, 4);
		std::cout << "New Dogtile Origin: " << gmtry2i::to_string(dogtileorigin) << std::endl;
		btile_lim_stream4 iterator = map.read(gmtry2i::aligned_box2i(dogtileorigin + gmtry2i::vector2i(15, 15),
			dogtileorigin + gmtry2i::vector2i(1 << 4, 1 << 4)));
		std::cout << "New Map Iterator Bounds: " << gmtry2i::to_string(iterator->get_bounds()) << std::endl;

		std::cout << "Tile to Write: " << std::endl << dogtile;
		mapstream.write(iterator.get());
		std::cout << "Dogtile written to file successfully!" << std::endl;

		bmap_buffer4 map2(mapstream.get_bounds().min);
		btile_lim_stream4 file_tile_stream = mapstream.read(gmtry2i::aligned_box2i(dogtileorigin, 1 << (4 + 2)));
		map2.set_wmode(maps2::TILE_ADD_MODE);
		map2.write(file_tile_stream.get());
		std::cout << "Dogtile written to file successfully!" << std::endl;
		std::cout << &map2;

		std::cout << &mapstream;

		return 0;
	}

	// RUN AFTER TEST 3 OR 3 AND 5
	// Uses a tile stream to read tiles out from the boolean occupancy array (a matrix stored in row-major order)
	// Writes the tiles to the file as they are read out from the array
	// Uses a stream to read back all of the file's tiles straight to the image before it's printed
	int occupancy_test7() {
		std::cout << "OCCUPANCY TEST 7" << std::endl;

		bmap_fstream4 mapstream(OUTPUT_FILEPATH + "mymap4", gmtry2i::vector2i());
		mapstream.set_wmode(maps2::TILE_ADD_MODE);

		std::cout << "Tiles to Write: " << std::endl;
		render_forgy();

		ocpncy::mat_tile_stream<4, bool> iterator(forgy, forgydims[0], forgydims[1], forgyorigin, mapstream.get_bounds().min);
		mapstream.write(&iterator);

		btile_lim_stream4 file_tile_stream = mapstream.read(gmtry2i::aligned_box2i(forgyorigin, 1 << (6)));
		std::cout << file_tile_stream;

		return 0;
	}

	unsigned char* get_map_image(std::uint32_t* width, std::uint32_t* height) {
		std::fstream file;
		file.open(RESOURCES_FILEPATH + "map.b");
		if (!file.is_open()) return 0;
		file.read(reinterpret_cast<char*>(width), 4);
		file.read(reinterpret_cast<char*>(height), 4);
		unsigned int area = (*width) * (*height);
		unsigned char* pixels = new unsigned char[area];
		file.read(reinterpret_cast<char*>(pixels), area);
		file.close();
		return pixels;
	}

	std::string get_map_file_name(unsigned int log2_w) {
		std::string width_str = std::to_string(1 << log2_w);
		return OUTPUT_FILEPATH + "map" + width_str + "x" + width_str;
	}

	template <unsigned int log2_w>
	using tile_istream_ptr = std::unique_ptr<maps2::tile_istream<ocpncy::otile<log2_w>>>;
	template <unsigned int log2_w>
	using lim_tile_istream_ptr = std::unique_ptr<maps2::lim_tile_istream<ocpncy::otile<log2_w>>>;

	template <unsigned int log2_w>
	void generate_map_file(const gmtry2i::vector2i& map_origin, const gmtry2i::vector2i& any_tile_origin) {
		std::uint32_t og_width, og_height;
		unsigned char* og_pixels = get_map_image(&og_width, &og_height);
		if (og_pixels == 0) return;
		unsigned int width = og_width >> 1;
		unsigned int height = og_height >> 1;
		unsigned char* pixels = ocpncy::halve_matrix<unsigned char>(og_pixels, og_width, og_height);
		delete[] og_pixels;

		ocpncy::mat_tile_stream<log2_w, unsigned char> tiles_in(pixels, width, height, map_origin, any_tile_origin);
		std::cout << "Original map file bounds: " << gmtry2i::to_string(tiles_in.get_bounds()) << std::endl;
		std::string width_str = std::to_string(1 << log2_w);
		maps2::map_fstream<log2_w, ocpncy::otile<log2_w>> map_file(get_map_file_name(log2_w), any_tile_origin);
		map_file.write(&tiles_in);

		delete[] pixels;
	}

	template <unsigned int log2_w>
	void print_map_file_item(const gmtry2i::vector2i& item_to_print_origin, unsigned int item_to_print_depth) {
		maps2::map_fstream<log2_w, ocpncy::otile<log2_w>> map_file(get_map_file_name(log2_w), gmtry2i::vector2i());
		lim_tile_istream_ptr<log2_w> tiles_out =
			map_file.read(gmtry2i::aligned_box2i(item_to_print_origin, 1 << (log2_w + item_to_print_depth)));
		std::cout << "Bounds of region to print: " << gmtry2i::to_string(tiles_out->get_bounds()) << std::endl;
		std::cout << tiles_out;
	}

	template <unsigned int log2_w>
	void print_map_file() {
		maps2::map_fstream<log2_w, ocpncy::otile<log2_w>> map_file(get_map_file_name(log2_w), {});
		tile_istream_ptr<log2_w> tiles_out = map_file.read();
		std::cout << tiles_out;
	}

	template <unsigned int log2_w>
	void print_map_file_tiles() {
		maps2::map_fstream<log2_w, ocpncy::otile<log2_w>> map_file(get_map_file_name(log2_w), {});
		tile_istream_ptr<log2_w> tiles_out = map_file.read();
		int num_tiles_read = 0;
		const ocpncy::otile<log2_w>* tile;
		while (tile = tiles_out->next()) {
			std::cout << *tile;
			std::cout << "Tile origin: " << gmtry2i::to_string(tiles_out->last_origin()) << std::endl;
			num_tiles_read++;
		}
		std::cout << "Total number of tiles read: " << num_tiles_read << std::endl;

		delete tiles_out;
	}

	template <unsigned int log2_w>
	class collidable_otile : public prjctn::collidable_object {
		ocpncy::gradient_otile<log2_w> tile;
		gmtry2i::aligned_box2i bounds;

		class min_dst_finder : public gmtry2i::point_ostream2i {
			collidable_otile& tile;
			gmtry2::vector2 d;
			gmtry2::line_segment2 l, local_l;
			float min_dst;
		public:
			// local_l must be defined relative to tile
			min_dst_finder(collidable_otile target_tile,
			               gmtry2::line_segment2 line,
			               gmtry2::line_segment2 local_line,
			               gmtry2::vector2 direction,
			               float max_distance) : tile(target_tile) {
				l = line;
				local_l = local_line;
				d = direction;
				min_dst = max_distance;
			}
			// p must be defined relative to tile
			void write(const gmtry2i::vector2i& p) {
				if (ocpncy::get_occ(p.x, p.y, tile.tile)) {
					bool no_collision = false;
					// defined in world-space
					gmtry2::line_segment2 state_intrsctd_line =
						gmtry2i::intersection(local_l, boundsof(p), no_collision) + 
						gmtry2i::to_vector2(tile.bounds.min);
					if (!no_collision) {
						min_dst = std::min(min_dst,
						                   std::min(gmtry2::dot(state_intrsctd_line.a - l.a, d),
						                            gmtry2::dot(state_intrsctd_line.b - l.a, d)));
					}
				}
			}
			float get_min_dst() {
				return min_dst;
			}
		};

	public:
		collidable_otile(const ocpncy::gradient_otile<log2_w>& source_tile, const gmtry2i::vector2i& origin) {
			tile = source_tile;
			bounds = gmtry2i::aligned_box2i(origin, 1 << log2_w);
		}
		float get_distance(const gmtry3::ray3& r, float max_distance) const {
			gmtry2::vector2 direction = gmtry2::normalize(r.d);
			gmtry2::line_segment2 l(r.p, r.p + direction * max_distance);

			bool no_intersection = false;
			gmtry2::line_segment2 intrsctd_line = gmtry2i::intersection(l, bounds, no_intersection);
			if (!no_intersection) {
				float dst_to_tile = std::min(gmtry2::dot(intrsctd_line.a - l.a, direction), 
											 gmtry2::dot(intrsctd_line.b - l.a, direction));
				if (dst_to_tile > 1) return dst_to_tile;
				// Convert to tile space
				intrsctd_line = intrsctd_line - gmtry2i::to_vector2(bounds.min);
				min_dst_finder jeremy(*this, l, intrsctd_line, direction, max_distance);
				gmtry2i::rasterize<min_dst_finder&>(intrsctd_line, jeremy);
				return jeremy.get_min_dst();
			}
			return max_distance;
		}
	};

	// Produces an explorable virtual world with the cat and dog tiles (interpretted as infinite vertical columns)
	int occupancy_test8() { // PASSED
		render_cat();
		render_dog();
		//std::cout << cattile << std::endl;
		prjctn::ray_marcher marcher;
		//prjctn::sphere a({ { 0, 10, 0 }, 3 }), b({ { 5, 20, 5 }, 9 });
		//marcher.add_object(&a); marcher.add_object(&b);

		collidable_otile<4> collidable_cattile(cattile, { -10, 10 });
		collidable_otile<4> collidable_dogtile(dogtile, { -26, 10 });
		marcher.add_object(&collidable_cattile);
		marcher.add_object(&collidable_dogtile);

		prjctn::cam_info config(PI / 2, 48, 32, gmtry3::transform3(gmtry3::make_rotation(2, 9 * PI / 12), 
		                                                           {-10 + gmtry2::EPSILON, 26 + gmtry2::EPSILON, 0}));
		prjctn::observed_point_drawer2 drawer(48, 32);
		prjctn::world_explorer explora(config, &marcher, &drawer);
		explora.add_listener(&drawer);
		ascii_dsp::ascii_console console(&explora, std::cout);
		console.execute_commands(std::cin);
		return 0;
	}

	template <unsigned int log2_w>
	class otile_loader : public gmtry2i::point_ostream2i {
		using nbrng_grad_otile = maps2::nbrng_tile<ocpncy::gradient_otile<log2_w>>;

		maps2::map_istream<ocpncy::otile<log2_w>>* source;
		maps2::nbrng_tile_linker<log2_w, ocpncy::gradient_otile<log2_w>> linker;
		std::vector<nbrng_grad_otile*> freestyle;
	public:
		otile_loader(maps2::map_istream<ocpncy::otile<log2_w>>* tile_source, 
		             gmtry2i::vector2i map_center) : 
			linker(maps2::align_down(map_center, tile_source->get_bounds().min, log2_w)) {
			source = tile_source;
		}
		maps2::nbrng_tile_linker<log2_w, ocpncy::gradient_otile<log2_w>>* get_linker() {
			return &linker;
		}
		void write(const gmtry2i::vector2i& p) override {
			const ocpncy::otile<log2_w>* source_tile = source->read(p);
			if (source_tile) {
				ocpncy::gradient_otile<log2_w> dest_tile = *source_tile;
				linker.write(p, &dest_tile);
			}
			else {
				nbrng_grad_otile* freestyle_tile = new nbrng_grad_otile();
				freestyle.push_back(freestyle_tile);
				maps2::link_nbrng_tile<log2_w, ocpncy::gradient_otile<log2_w>>(linker.get_top_item(), p, freestyle_tile);
			}
		}
		~otile_loader() {
			int num_freestyle = freestyle.size();
			for (int i = 0; i < num_freestyle; i++) 
				delete freestyle[i];
		}
	};

	template <unsigned int log2_w>
	class monitored_observed_occupancy_drawer : public ascii_dsp::command_listener,
	                                            public prjctn::point_observer2,
	                                            public ocpncy::occmap_monitor<log2_w> {
	protected:
		void print_manual(std::ostream& os) {
			os << "w/a/s/d: move viewport north/west/south/east" << std::endl;
			os << "sp <x> <y>: set viewport southwest corner position to (x, y)" << std::endl;
			os << "sync: synchronize viewport movement with camera" << std::endl;
			os << "drw: draw all plain occupancy states" << std::endl;
			os << "drwc: draw all occupancy certainty states" << std::endl;
		}
		bool attempt_execute(const std::string& command_args, std::ostream& os) {

		}
	public:
		monitored_observed_occupancy_drawer() {

		}
		std::string get_name() override {
			return "mood";
		}
		void write(const gmtry2i::vector2i& p) override {

		}
		void set_perspective(const gmtry3::transform3& pose) override {

		}
		void write(ocpncy::gradient_otile<log2_w>* tile_ptr, unsigned int occupancy_idx) override {

		}
	};

	int occupancy_test9() {
		render_cat();
		// User can move around and observe a virtual environment
		// Whatever occupancies they see in first person will be added into the map
		// Virtual environment may contain tiles from a saved map, as well as totally new 3D shapes
		// User can view contents of a region of the map (including certainties)
		// User should be able to move or maybe remove 3D shapes

		prjctn::cam_info config(PI / 2, 48, 32, gmtry3::transform3(gmtry3::make_rotation(2, 9 * PI / 12), 
		                                                           {-10 + gmtry2::EPSILON, 26 + gmtry2::EPSILON, 0}));
		maps2::map_buffer<4, ocpncy::otile<4>> permatiles({});

		otile_loader<4> loader(&permatiles, gmtry2i::vector2i(config.get_pose().t));
		//ocpncy::occupancy_observer<4, 8> observer(config.get_pose().t, &loader);

		return 0;
	}

	void prep_tests_0to7() {
		std::string filename = OUTPUT_FILEPATH + "mymap";
		std::cout << "Prepping test 0... " << (!remove(filename.c_str()) ? "Success!" : "Failed") << std::endl;
		filename += "4";
		std::cout << "Prepping tests 3-7... " << (!remove(filename.c_str()) ? "Success!" : "Failed") << std::endl;
	}

	template <unsigned int log2_w>
	void prep_real_map_tests() {
		std::string map_name = get_map_file_name(log2_w);
		std::cout << "Prepping real map tests... " << 
			(!remove(map_name.c_str()) ? "Success!" : "Failed") << std::endl;
	}

	int run_all_tests() {
		prep_tests_0to7();
		prep_real_map_tests<6>();

		oc_tests::occupancy_test0();
		oc_tests::occupancy_test3();
		//oc_tests::occupancy_test4();
		oc_tests::occupancy_test5();
		oc_tests::occupancy_test7();

		oc_tests::generate_map_file<6>(gmtry2i::vector2i(), gmtry2i::vector2i());
		//oc_tests::print_map_file_item<6>({ 0, 0 }, 3);
		oc_tests::print_map_file<6>();

		return 0;
	}
}