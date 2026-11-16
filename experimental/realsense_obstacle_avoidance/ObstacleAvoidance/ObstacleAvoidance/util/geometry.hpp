#pragma once

#include <string>
#include <cmath>

const float PI = 3.14159265F;

// 3D floating-point geometry
namespace gmtry3 {
	struct vector3 {
		float x, y, z;
		vector3() = default;
		vector3(float i, float j, float k);
		float& operator [](int i);
		const float& operator [](int i) const;
		void operator +=(const vector3& v);
		void operator -=(const vector3& v);
	};

	vector3 operator +(const vector3& a, const vector3& b);
	vector3 operator -(const vector3& a, const vector3& b);
	vector3 operator -(const vector3& v);
	vector3 operator *(const vector3& v, float s);
	vector3 operator /(const vector3& v, float s);

	float magnitude(const vector3& v);
	vector3 normalize(const vector3& v);

	float dot(const vector3& a, const vector3& b);
	vector3 unit_project(const vector3& v, const vector3& n);
	vector3 unit_reject(const vector3& v, const vector3& n);
	vector3 project(const vector3& a, const vector3& b);
	vector3 reject(const vector3& a, const vector3& b);
	vector3 cross(const vector3& a, const vector3& b);

	std::string to_string(const vector3& v);

	template <typename T>
	concept writes_vector3 = requires(T w, const vector3 & v) {
		w.write(v);
	};

	class point_ostream3 {
	public:
		virtual void write(const gmtry3::vector3& p) = 0;
		virtual void flush() {}
	};

	struct matrix3 {
		float n[3][3];
		matrix3();
		matrix3(float, float, float, float, float, float, float, float, float);
		matrix3(const vector3& a, const vector3& b, const vector3& c);
		float& operator ()(int i, int j);
		const float& operator ()(int i, int j) const;
		vector3& operator ()(int i);
		const vector3& operator ()(int i) const;
		// Inversion if column vectors are normalized and orthogonal
		matrix3 T() const;
	};

	matrix3 make_rotation(int axis_idx, float theta);

	matrix3 operator *(const matrix3& A, const matrix3& B);

	vector3 operator *(const matrix3& M, const vector3& v);

	vector3 dot(const matrix3& M, const vector3& v);

	struct transform3 {
		matrix3 R;
		vector3 t;
		transform3();
		transform3(float, float, float, float, float, float, float, float, float, float, float, float);
		transform3(const vector3& i, const vector3& j, const vector3& k, const vector3& d);
		transform3(const matrix3& M, const vector3& v);
		// Inversion if column vectors of rotation matrix are normalized and orthogonal
		transform3 T() const;
	};

	vector3 operator *(const transform3& T, const vector3& p);
	transform3 operator *(const transform3& T, const matrix3& M);
	transform3 operator *(const matrix3& M, const transform3& T);
	transform3 operator *(const transform3& A, const transform3& B);
	transform3 operator +(const transform3& T, const vector3& v);
	transform3 operator -(const transform3& T, const vector3& v);

	struct rotor3 {
		float a;
		vector3 B;
		rotor3(float inner, const vector3& outer);
	};

	// unit_bivector has to be normalized
	rotor3 unit_make_rotor(const vector3& unit_bivector, float theta);

	// bivector doesn't have to be normalized
	rotor3 make_rotor(const vector3& bivector, float theta);

	rotor3 normalize(const rotor3& r);

	vector3 operator *(const rotor3& r, const vector3& v);
	rotor3 operator *(const rotor3& r1, const rotor3& r2);

	rotor3 invert(const rotor3& r);

	struct ray3 {
		vector3 p, d;
		ray3(const vector3& point, const vector3& direction);
	};

	struct triangle3 {
		vector3 a, b, c;
		triangle3() = default;
		triangle3(const vector3& p, const vector3& q, const vector3& r);
		vector3& operator [](int i);
		const vector3& operator [](int i) const;
	};

	struct ball3 {
		vector3 c;
		float r;
		ball3() = default;
		ball3(const vector3& center, float radius);
	};

	ball3 operator *(const ball3& c, float s);
	ball3 operator /(const ball3& c, float s);
	ball3 operator +(const ball3& c, const vector3& v);
	ball3 operator -(const ball3& c, const vector3& v);
}

namespace gmtry2 {
	const float EPSILON = 0.00001; // one hundred-thousandth

	struct vector2 {
		float x, y;
		vector2() = default;
		vector2(float i, float j);
		vector2(const gmtry3::vector3& v);
		float& operator [](int i);
		const float& operator [](int i) const;
		void operator +=(const vector2& v);
		void operator -=(const vector2& v);
		vector2& operator =(const gmtry3::vector3& v);
	};

	vector2 operator +(const vector2& a, const vector2& b);
	vector2 operator -(const vector2& a, const vector2& b);
	vector2 operator -(const vector2& v);
	vector2 operator *(const vector2& v, float s);
	vector2 operator /(const vector2& v, float s);

	float magnitude(const vector2& v);
	vector2 normalize(const vector2& v);

	float dot(const vector2& a, const vector2& b);

	float wedge(const vector2& a, const vector2& b);

	float squared(const vector2& v);

	std::string to_string(const vector2& v);

	struct line_segment2 {
		vector2 a, b;
		line_segment2() = default;
		line_segment2(const vector2& pointA, const vector2& pointB);
	};

	bool intersects(const line_segment2& l1, const line_segment2& l2);

	// result invalid if l1 and l2 do not intersect
	// can be used to find line (not segments) intersections if they are first checked for parallelity
	vector2 intersection(const line_segment2& l1, const line_segment2& l2);

	// if no intersection exists, returns origin and sets no_intersection to true
	vector2 intersection(const line_segment2& l1, const line_segment2& l2, bool& no_intersection);

	line_segment2 operator +(const line_segment2& l, const vector2& v);
	line_segment2 operator -(const line_segment2& l, const vector2& v);

	std::string to_string(const line_segment2& l);

	struct line_stepper2 {
		vector2 step_p, p;
		unsigned int waypoints;
		line_stepper2(const line_segment2& l, float step_size);
		void step();
	};

	struct ball2 {
		vector2 c;
		float r;
		ball2() = default;
		ball2(const vector2& center, float radius);
		ball2& operator =(const gmtry3::ball3& sphere);
	};

	ball2 operator *(const ball2& b, float s);
	ball2 operator /(const ball2& b, float s);
	ball2 operator +(const ball2& b, const vector2& v);
	ball2 operator -(const ball2& b, const vector2& v);

	bool intersects(const vector2& p, const ball2& b);
}

// 2D integer geometry
namespace gmtry2i {
	long floor(float f);
	long ceil(float f);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    INTEGER_PRECISION VECTORS & POINTS                                                          //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	struct vector2i {
		long x, y;
		vector2i() = default;
		vector2i(long i, long j);
		vector2i(const gmtry2::vector2& p);
		long& operator [](int i);
		const long& operator [](int i) const;
		vector2i& operator +=(const vector2i& v);
		vector2i& operator -=(const vector2i& v);
		vector2i& operator =(const gmtry2::vector2& p);
	};

	vector2i operator +(const vector2i& a, const vector2i& b);
	vector2i operator -(const vector2i& a, const vector2i& b);
	vector2i operator |(const vector2i& a, const vector2i& b);
	vector2i operator -(const vector2i& v);
	vector2i operator *(const vector2i& v, long s);
	vector2i operator /(const vector2i& v, long s);
	vector2i operator <<(const vector2i& v, unsigned int i);
	vector2i operator >>(const vector2i& v, unsigned int i);
	vector2i operator &(const vector2i& v, long s);
	vector2i operator |(const vector2i& v, long s);
	vector2i operator >(const vector2i& v, long s);
	vector2i operator <(const vector2i& v, long s);
	vector2i operator >=(const vector2i& v, long s);
	vector2i operator <=(const vector2i& v, long s);
	bool operator ==(const vector2i& u, const vector2i& v);

	long dot(const vector2i& u, const vector2i& v);
	long squared(const vector2i& v);
	// returns pseudoscalar (<u v>_2) / (e_1 e_2)
	long wedge(const vector2i& u, const vector2i& v);

	std::string to_string(const vector2i& v);
	gmtry2::vector2 to_vector2(const vector2i& v);

	gmtry2::vector2 to_point2(const vector2i& p);

	template <typename T>
	concept writes_vector2i = requires(T w, const vector2i & p) {
		w.write(p);
	};

	class point_ostream2i {
	public:
		virtual void write(const gmtry2i::vector2i& p) = 0;
		virtual void flush() {}
	};

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    AXIS-ALIGNED INTEGER-PRECISION BOXES                                                        //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	struct aligned_box2i {
		// inclusive min, exclusive max
		vector2i min, max;
		aligned_box2i() = default;
		aligned_box2i(const vector2i& min_v, const vector2i& max_v);
		aligned_box2i(const vector2i& origin, long width);
		vector2i& operator [](int i);
		const vector2i& operator [](int i) const;
	};

	vector2i center(const aligned_box2i& b1);
	long area(const aligned_box2i& b);

	aligned_box2i boundsof(const vector2i& p);
	aligned_box2i boundsof(const aligned_box2i& b);
	aligned_box2i boundsof(const vector2i& p1, const vector2i& p2);
	aligned_box2i boundsof(const aligned_box2i& b, const vector2i& p);
	aligned_box2i boundsof(const gmtry2::line_segment2& l);
	aligned_box2i boundsof(const gmtry2::ball2& b);

	bool contains(const aligned_box2i& b, const vector2i& p);
	bool contains(const aligned_box2i& b1, const aligned_box2i& b2);

	bool intersects(const vector2i& p, const aligned_box2i& b);
	vector2i intersection(const vector2i& p, const aligned_box2i& b);

	// Returns minkowski sum
	aligned_box2i operator +(const aligned_box2i& b1, const aligned_box2i& b2);
	// Returns minkowski difference
	aligned_box2i operator -(const aligned_box2i& b1, const aligned_box2i& b2);

	aligned_box2i operator +(const aligned_box2i& b, const vector2i& v);
	aligned_box2i operator -(const aligned_box2i& b, const vector2i& v);
	aligned_box2i operator -(const vector2i& v, const aligned_box2i& b);

	bool intersects(const aligned_box2i& b1, const aligned_box2i& b2);
	aligned_box2i intersection(const aligned_box2i& b1, const aligned_box2i& b2);
	// if no intersection exists, sets no_intersection to true
	aligned_box2i intersection(const aligned_box2i& b1, const aligned_box2i& b2, bool& no_intersection);
	bool intersects(const gmtry2::line_segment2 l, const aligned_box2i& box);
	// Returns the intersection of a line segment and a box
	// Parameter no_intersection is set to true if there is no intersection
	gmtry2::line_segment2 intersection(const gmtry2::line_segment2 l, const aligned_box2i& box,
		bool& no_intersection);

	std::string to_string(const aligned_box2i& b);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    INTEGER-PRECISION LINE SEGMENTS                                                             //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	struct line_segment2i {
		vector2i a, b;
		line_segment2i() = default;
		line_segment2i(const vector2i& point_A, const vector2i& point_B);
		line_segment2i(const gmtry2::line_segment2& l);
		operator gmtry2::line_segment2() const;
	};

	aligned_box2i boundsof(const line_segment2i& l);

	bool contains(const gmtry2i::aligned_box2i& b, const line_segment2i& l);

	line_segment2i operator +(const line_segment2i& l, const vector2i& v);
	line_segment2i operator -(const line_segment2i& l, const vector2i& v);

	bool intersects(const line_segment2i& l1, const line_segment2i& l2);

	// result invalid if l1 and l2 do not intersect
	vector2i intersection(const line_segment2i& l1, const line_segment2i& l2);

	bool intersects(const line_segment2i& l, const aligned_box2i& box);

	// Current implementation is slow
	bool intersects(const line_segment2i& l, const vector2i& p);

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    LINE SEGMENT RASTERIZATION                                                                  //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	struct line_stepper2i {
		float step_x, step_y;
		float x, y;
		unsigned int waypoints;
		vector2i p;
		line_stepper2i(const line_segment2i& l, float step_size);
		line_stepper2i(const gmtry2::line_segment2& l, float step_size);
		void step();
	};

	// l cannot be parallel to the dimension on which the scanlines lie (ln_dim)
	template <bool ln_dim, writes_vector2i T>
	void scanline_rasterize(const gmtry2::line_segment2& l, T out) {
		// offset refers to offset between scanlines
		const bool ofst_dim = !ln_dim;
		const gmtry2::vector2 disp = l.b - l.a;
		const bool neg_slope = (disp[ln_dim] < 0) != (disp[ofst_dim] < 0);
		const float dln_dofst = disp[ln_dim] / disp[ofst_dim];
		// min/max scanlines
		const float min_ofst = std::min(l.a[ofst_dim], l.b[ofst_dim]),
			max_ofst = std::max(l.a[ofst_dim], l.b[ofst_dim]);
		const long min_sl = floor(min_ofst),
			max_sl = ceil(max_ofst);
		// min/max of all positions along all scanlines
		const float abs_min_ln_pos = std::min(l.a[ln_dim], l.b[ln_dim]),
			abs_max_ln_pos = std::max(l.a[ln_dim], l.b[ln_dim]),
			init_ln_pos = !neg_slope ? abs_min_ln_pos : abs_max_ln_pos,
			finl_ln_pos = !neg_slope ? abs_max_ln_pos : abs_min_ln_pos;
		// extrema of positions along current scanline, ordered min, max
		float ln_pos_extrema[2];
		gmtry2i::vector2i next_point;
		// if +slope: current extrema idx is 0 and next extrema idx is 1
		// if -slope: current extrema idx is 1 and next extrema idx is 0
		// so current extrema idx = neg_slope and next extrema idx = !neg_slope
		ln_pos_extrema[!neg_slope] = init_ln_pos;
		for (long cur_sl = min_sl; cur_sl < max_sl - 1; cur_sl++) {
			ln_pos_extrema[neg_slope] = ln_pos_extrema[!neg_slope];
			ln_pos_extrema[!neg_slope] = dln_dofst * (cur_sl + 1 - min_ofst) + init_ln_pos;
			// min/max positions along current scanline
			long min_ln_pos = floor(ln_pos_extrema[0]), max_ln_pos = ceil(ln_pos_extrema[1]);
			for (long ln_pos = min_ln_pos; ln_pos < max_ln_pos; ln_pos++) {
				next_point[ln_dim] = ln_pos; next_point[ofst_dim] = cur_sl;
				out.write(next_point);
			}
		}
		ln_pos_extrema[neg_slope] = ln_pos_extrema[!neg_slope];
		ln_pos_extrema[!neg_slope] = finl_ln_pos;
		long min_ln_pos = floor(ln_pos_extrema[0]), max_ln_pos = ceil(ln_pos_extrema[1]);
		for (long ln_pos = min_ln_pos; ln_pos < max_ln_pos; ln_pos++) {
			next_point[ln_dim] = ln_pos; next_point[ofst_dim] = max_sl - 1;
			out.write(next_point);
		}
	}

	template <writes_vector2i T>
	void rasterize(const gmtry2::line_segment2& l, T out) {
		gmtry2::vector2 disp = l.b - l.a;
		if (std::abs(disp.y) < gmtry2::EPSILON) {
			const long min_ln_pos = floor(std::min(l.a.x, l.b.x)), max_ln_pos = ceil(std::max(l.a.x, l.b.x));
			const long y = l.a.y;
			for (long ln_pos = min_ln_pos; ln_pos < max_ln_pos; ln_pos++)
				out.write({ ln_pos, y });
		}
		else if (std::abs(disp.x) < gmtry2::EPSILON) {
			const long min_ln_pos = floor(std::min(l.a.y, l.b.y)), max_ln_pos = ceil(std::max(l.a.y, l.b.y));
			const long x = l.a.x;
			for (long ln_pos = min_ln_pos; ln_pos < max_ln_pos; ln_pos++)
				out.write({ x, ln_pos });
		}
		else if (std::abs(disp.x) > std::abs(disp.y))
			scanline_rasterize<0, T>(l, out);
		else scanline_rasterize<1, T>(l, out);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    BOX INTERSECTION ABSTRACTION                                                                //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T>
	concept intersects_box2i = requires (T a, aligned_box2i b, bool c) {
		b = boundsof(a);
		c = intersects(a, b);
		//intersects_box2i<typename decltype(intersection(a, b))>;
	};

	// Interface for object with the basic functionality of intersects_box2i without the proper functions
	class box_intersector2i {
	public:
		virtual bool intersects(const aligned_box2i& box) = 0;
		//virtual box_intersector2i* intersection(const aligned_box2i& box) = 0;
		virtual aligned_box2i get_bounds() = 0;
		virtual box_intersector2i* clone() = 0;
	};

	// Used to create a box_intersector2i out of an intersects_box2i
	template <intersects_box2i T>
	class intersectable_box_intersector2i : public box_intersector2i {
		T shape;
	public:
		intersectable_box_intersector2i(T new_shape) {
			shape = new_shape;
		}
		bool intersects(const aligned_box2i& box) {
			return gmtry2i::intersects(shape, box);
		}
		//box_intersector2i* intersection(const aligned_box2i& box) {
		//	auto box_intersection = gmtry2i::intersection(shape, box);
		//	return new intersectable_box_intersector2i<decltype(box_intersection)>(box_intersection);
		//}
		box_intersector2i* clone() {
			return new intersectable_box_intersector2i(shape);
		}
		aligned_box2i get_bounds() {
			return gmtry2i::boundsof(shape);
		}
	};

	//  Memory manager for a box_intersector2i*, through which it satisfies the requirements for intersects_box2i
	class box_intersectable2i {
		box_intersector2i* intersector;
	public:
		box_intersectable2i();
		box_intersectable2i(box_intersector2i* new_intersector);
		box_intersectable2i(const box_intersectable2i& object);
		bool intersects(const aligned_box2i& box) const;
		aligned_box2i get_bounds() const;
		box_intersectable2i& operator =(const box_intersectable2i& object);
		~box_intersectable2i();
	};

	bool intersects(const box_intersectable2i& object, const aligned_box2i& box);

	aligned_box2i boundsof(const box_intersectable2i& object);

	// Creates a box_intersectable2i out of an intersectable
	// Practical for passing an intersects_box2i<aligned_box> to a non-template class or function
	template <intersects_box2i T>
	box_intersectable2i make_box_intersectable(T shape) {
		intersectable_box_intersector2i intersector(shape);
		return box_intersectable2i(&intersector);
	}
}