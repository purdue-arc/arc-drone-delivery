#include "geometry.hpp"

#include <string>
#include <cmath>

// 3D floating-point geometry
namespace gmtry3 {
	const int basis_bivectors[3][2] = { {1, 2}, {2, 0}, {0, 1} };

	vector3::vector3(float i, float j, float k) {
		x = i;
		y = j;
		z = k;
	}
	float& vector3::operator [](int i) {
		return (&x)[i];
	}
	const float& vector3::operator [](int i) const {
		return (&x)[i];
	}
	void vector3::operator +=(const vector3& v) {
		x += v.x;
		y += v.y;
		z += v.z;
	}
	void vector3::operator -=(const vector3& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
	}

	vector3 operator +(const vector3& a, const vector3& b) {
		return vector3(a.x + b.x, a.y + b.y, a.z + b.z);
	}

	vector3 operator -(const vector3& a, const vector3& b) {
		return vector3(a.x - b.x, a.y - b.y, a.z - b.z);
	}

	vector3 operator -(const vector3& v) {
		return vector3(-v.x, -v.y, -v.z);
	}

	vector3 operator *(const vector3& v, float s) {
		return vector3(v.x * s, v.y * s, v.z * s);
	}

	vector3 operator /(const vector3& v, float s) {
		return v * (1.0F / s);
	}

	float magnitude(const vector3& v) {
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	vector3 normalize(const vector3& v) {
		return v / magnitude(v);
	}

	float dot(const vector3& a, const vector3& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	vector3 unit_project(const vector3& v, const vector3& n) {
		return n * dot(v, n);
	}

	vector3 unit_reject(const vector3& v, const vector3& n) {
		return v - unit_project(v, n);
	}

	vector3 project(const vector3& a, const vector3& b) {
		return b * dot(a, b) / dot(b, b);
	}

	vector3 reject(const vector3& a, const vector3& b) {
		return a - project(a, b);
	}

	vector3 cross(const vector3& a, const vector3& b) {
		return vector3(a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x);
	}

	std::string to_string(const vector3& v) {
		return std::to_string(v.x) + std::string(", ") + std::to_string(v.y) + std::string(", ") + std::to_string(v.z);
	}

	matrix3::matrix3() {
		n[0][0] = 1; n[1][0] = 0; n[2][0] = 0;
		n[0][1] = 0; n[1][1] = 1; n[2][1] = 0;
		n[0][2] = 0; n[1][2] = 0; n[2][2] = 1;
	}
	matrix3::matrix3(float n00, float n01, float n02,
		float n10, float n11, float n12,
		float n20, float n21, float n22) {
		n[0][0] = n00; n[1][0] = n10; n[2][0] = n20;
		n[0][1] = n01; n[1][1] = n11; n[2][1] = n21;
		n[0][2] = n02; n[1][2] = n12; n[2][2] = n22;
	}
	matrix3::matrix3(const vector3& a, const vector3& b, const vector3& c) {
		n[0][0] = a.x; n[1][0] = b.x; n[2][0] = c.x;
		n[0][1] = a.y; n[1][1] = b.y; n[2][1] = c.y;
		n[0][2] = a.z; n[1][2] = b.z; n[2][2] = c.z;
	}
	float& matrix3::operator ()(int i, int j) {
		return n[i][j];
	}
	const float& matrix3::operator ()(int i, int j) const {
		return n[i][j];
	}
	vector3& matrix3::operator ()(int i) {
		return *(reinterpret_cast<vector3*>(n[i]));
	}
	const vector3& matrix3::operator ()(int i) const {
		return *(reinterpret_cast<const vector3*>(n[i]));
	}
	matrix3 matrix3::T() const {
		return matrix3(n[0][0], n[1][0], n[2][0],
			n[0][1], n[1][1], n[2][1],
			n[0][2], n[1][2], n[2][2]);
	}

	matrix3 make_rotation(int axis_idx, float theta) {
		float sin_theta = std::sin(theta);
		float cos_theta = std::cos(theta);
		matrix3 mat(0, 0, 0, 0, 0, 0, 0, 0, 0);
		mat(axis_idx, axis_idx) = 1;
		mat(basis_bivectors[axis_idx][0], basis_bivectors[axis_idx][0]) = cos_theta;
		mat(basis_bivectors[axis_idx][0], basis_bivectors[axis_idx][1]) = sin_theta;
		mat(basis_bivectors[axis_idx][1], basis_bivectors[axis_idx][0]) = -sin_theta;
		mat(basis_bivectors[axis_idx][1], basis_bivectors[axis_idx][1]) = cos_theta;
		return mat;
	}

	matrix3 operator *(const matrix3& A, const matrix3& B) {
		return matrix3(A.n[0][0] * B.n[0][0] + A.n[1][0] * B.n[0][1] + A.n[2][0] * B.n[0][2],
			A.n[0][1] * B.n[0][0] + A.n[1][1] * B.n[0][1] + A.n[2][1] * B.n[0][2],
			A.n[0][2] * B.n[0][0] + A.n[1][2] * B.n[0][1] + A.n[2][2] * B.n[0][2],
			A.n[0][0] * B.n[1][0] + A.n[1][0] * B.n[1][1] + A.n[2][0] * B.n[1][2],
			A.n[0][1] * B.n[1][0] + A.n[1][1] * B.n[1][1] + A.n[2][1] * B.n[1][2],
			A.n[0][2] * B.n[1][0] + A.n[1][2] * B.n[1][1] + A.n[2][2] * B.n[1][2],
			A.n[0][0] * B.n[2][0] + A.n[1][0] * B.n[2][1] + A.n[2][0] * B.n[2][2],
			A.n[0][1] * B.n[2][0] + A.n[1][1] * B.n[2][1] + A.n[2][1] * B.n[2][2],
			A.n[0][2] * B.n[2][0] + A.n[1][2] * B.n[2][1] + A.n[2][2] * B.n[2][2]);
	}

	vector3 operator *(const matrix3& M, const vector3& v) {
		return vector3(M.n[0][0] * v.x + M.n[1][0] * v.y + M.n[2][0] * v.z,
			M.n[0][1] * v.x + M.n[1][1] * v.y + M.n[2][1] * v.z,
			M.n[0][2] * v.x + M.n[1][2] * v.y + M.n[2][2] * v.z);
	}

	vector3 dot(const matrix3& M, const vector3& v) {
		return vector3(M.n[0][0] * v.x + M.n[0][1] * v.y + M.n[0][2] * v.z,
			M.n[1][0] * v.x + M.n[1][1] * v.y + M.n[1][2] * v.z,
			M.n[2][0] * v.x + M.n[2][1] * v.y + M.n[2][2] * v.z);
	}

	transform3::transform3() {
		R = matrix3();
		t = vector3();
	}
	transform3::transform3(float n00, float n01, float n02,
		float n10, float n11, float n12,
		float n20, float n21, float n22,
		float n30, float n31, float n32) {
		R = matrix3(n00, n01, n02, n10, n11, n12, n20, n21, n22);
		t = vector3(n30, n31, n32);
	}
	transform3::transform3(const vector3& i, const vector3& j, const vector3& k, const vector3& d) {
		R = matrix3(i, j, k);
		t = d;
	}
	transform3::transform3(const matrix3& M, const vector3& v) {
		R = M;
		t = v;
	}
	transform3 transform3::T() const {
		matrix3 RT = R.T();
		return transform3(RT, RT * -t);
	}

	vector3 operator *(const transform3& T, const vector3& p) {
		return T.R * p + T.t;
	}

	transform3 operator *(const transform3& T, const matrix3& M) {
		return transform3(T.R * M, T.t);
	}

	transform3 operator *(const matrix3& M, const transform3& T) {
		return transform3(M * T.R, M * T.t);
	}

	transform3 operator *(const transform3& A, const transform3& B) {
		return transform3(A.R * B.R, A.R * B.t + A.t);
	}

	transform3 operator +(const transform3& T, const vector3& v) {
		return transform3(T.R, T.t + v);
	}

	transform3 operator -(const transform3& T, const vector3& v) {
		return transform3(T.R, T.t - v);
	}

	rotor3::rotor3(float inner, const vector3& outer) {
		a = inner;
		B = outer;
	}

	// unit_bivector has to be normalized
	rotor3 unit_make_rotor(const vector3& unit_bivector, float theta) {
		return rotor3(std::cos(theta / 2), -unit_bivector * std::sin(theta / 2));
	}

	// bivector doesn't have to be normalized
	rotor3 make_rotor(const vector3& bivector, float theta) {
		return rotor3(std::cos(theta / 2), -normalize(bivector) * std::sin(theta / 2));
	}

	rotor3 normalize(const rotor3& r) {
		float inv_norm = 1.0F / std::sqrt(r.a * r.a + dot(r.B, r.B));
		return rotor3(r.a * inv_norm, r.B * inv_norm);
	}

	vector3 operator *(const rotor3& r, const vector3& v) {
		vector3 bcrossv = cross(r.B, v);
		return v * (r.a * r.a) - bcrossv * (2 * r.a) + cross(r.B, bcrossv) + r.B * dot(r.B, v);
	}

	rotor3 operator *(const rotor3& r1, const rotor3& r2) {
		return rotor3(r1.a * r2.a - dot(r1.B, r2.B), r2.B * r1.a + r1.B * r2.a - cross(r1.B, r2.B));
	}

	rotor3 invert(const rotor3& r) {
		return rotor3(r.a, -r.B);
	}

	ray3::ray3(const vector3& point, const vector3& direction) {
		p = point;
		d = direction;
	}

	triangle3::triangle3(const vector3& p, const vector3& q, const vector3& r) {
		a = p;
		b = q;
		c = r;
	}
	vector3& triangle3::operator [](int i) {
		return (&a)[i];
	}
	const vector3& triangle3::operator [](int i) const {
		return (&a)[i];
	}

	ball3::ball3(const vector3& center, float radius) {
		c = center;
		r = radius;
	}

	ball3 operator *(const ball3& c, float s) {
		return { c.c, c.r * s };
	}

	ball3 operator /(const ball3& c, float s) {
		return { c.c, c.r / s };
	}

	ball3 operator +(const ball3& c, const vector3& v) {
		return { c.c + v, c.r };
	}

	ball3 operator -(const ball3& c, const vector3& v) {
		return { c.c - v, c.r };
	}
}

namespace gmtry2 {
	vector2::vector2(float i, float j) {
		x = i;
		y = j;
	}
	vector2::vector2(const gmtry3::vector3& v) {
		x = v.x;
		y = v.y;
	}
	float& vector2::operator [](int i) {
		return (&x)[i];
	}
	const float& vector2::operator [](int i) const {
		return (&x)[i];
	}
	void vector2::operator +=(const vector2& v) {
		x += v.x;
		y += v.y;
	}
	void vector2::operator -=(const vector2& v) {
		x -= v.x;
		y -= v.y;
	}
	vector2& vector2::operator =(const gmtry3::vector3& v) {
		x = v.x;
		y = v.y;
		return *this;
	}

	vector2 operator +(const vector2& a, const vector2& b) {
		return vector2(a.x + b.x, a.y + b.y);
	}

	vector2 operator -(const vector2& a, const vector2& b) {
		return vector2(a.x - b.x, a.y - b.y);
	}

	vector2 operator -(const vector2& v) {
		return vector2(-v.x, -v.y);
	}

	vector2 operator *(const vector2& v, float s) {
		return vector2(v.x * s, v.y * s);
	}

	vector2 operator /(const vector2& v, float s) {
		return v * (1.0F / s);
	}

	float magnitude(const vector2& v) {
		return std::sqrt(v.x * v.x + v.y * v.y);
	}

	vector2 normalize(const vector2& v) {
		return v / magnitude(v);
	}

	float dot(const vector2& a, const vector2& b) {
		return a.x * b.x + a.y * b.y;
	}

	float wedge(const vector2& a, const vector2& b) {
		return a.x * b.y - a.y * b.x;
	}

	float squared(const vector2& v) {
		return dot(v, v);
	}

	std::string to_string(const vector2& v) {
		return std::to_string(v.x) + std::string(", ") + std::to_string(v.y);
	}

	line_segment2::line_segment2(const vector2& pointA, const vector2& pointB) {
		a = pointA;
		b = pointB;
	}

	bool intersects(const line_segment2& l1, const line_segment2& l2) {
		vector2 disp1 = l1.b - l1.a; // from l1.a to l1.b
		vector2 disp2 = l2.b - l2.a; // from l2.a to l2.b
		vector2 adisp = l1.a - l2.a; // from l2.a to l1.a
		float wedge1 = wedge(disp2, adisp);
		float wedge2 = wedge(disp1, adisp);
		float wedge3 = wedge(disp2, disp1);
		// test for parallelity
		if (abs(wedge3) < gmtry2::EPSILON) return false;
		// see intersection(line_segment2, line_segment2) for first half of following derivation
		// if t >= 0 and abs(t) <= 1, then l1 and l2 intersect
		//		[ t >= 0 ] = XOR(wedge(disp2, adisp) < 0, wedge(disp2, disp1) < 0)
		//		[ abs(t) <= 1 ] = [ abs(wedge(disp2, adisp)) / abs(wedge(disp2, disp1)) <= 1 ]
		//						= [ abs(wedge(disp2, adisp)) <= abs(wedge(disp2, disp1)) ]
		// Note: this test really has to be implemented from the perspective of each line segment,
		//       as it only tests whether l1 intersects the infinite line on which l2 lies
		return ((wedge1 < 0) != (wedge3 < 0)) && ((wedge2 < 0) != (wedge3 < 0)) &&
			(abs(wedge1) <= abs(wedge3)) && (abs(wedge2) <= abs(wedge3));
	}

	// result invalid if l1 and l2 do not intersect
	// can be used to find line (not segments) intersections if they are first checked for parallelity
	vector2 intersection(const line_segment2& l1, const line_segment2& l2) {
		vector2 disp1 = l1.b - l1.a; // from l1.a to l1.b
		vector2 disp2 = l2.b - l2.a; // from l2.a to l2.b
		vector2 adisp = l1.a - l2.a; // from l2.a to l1.a
		// wedge(disp2, adisp + t*disp1) = 0
		// = wedge(disp2, adisp) + t*wedge(disp2, disp1)
		// t = - wedge(disp2, adisp) / wedge(disp2, disp1)
		return l1.a - disp1 * (wedge(disp2, adisp) / wedge(disp2, disp1));
	}

	// if no intersection exists, returns origin and sets no_intersection to true
	vector2 intersection(const line_segment2& l1, const line_segment2& l2, bool& no_intersection) {
		vector2 disp1 = l1.b - l1.a;
		vector2 disp2 = l2.b - l2.a;
		vector2 adisp = l1.a - l2.a;
		float wedge1 = wedge(disp2, adisp);
		float wedge2 = wedge(disp1, adisp);
		float wedge3 = wedge(disp2, disp1);
		if (abs(wedge3) >= gmtry2::EPSILON) {
			if (((wedge1 < 0) != (wedge3 < 0)) && ((wedge2 < 0) != (wedge3 < 0)) &&
				(abs(wedge1) <= abs(wedge3)) && (abs(wedge2) <= abs(wedge3)))
				return l1.a - disp1 * (wedge1 / wedge3);
		}
		no_intersection = true;
		return {};
	}

	line_segment2 operator +(const line_segment2& l, const vector2& v) {
		return { l.a + v, l.b + v };
	}

	line_segment2 operator -(const line_segment2& l, const vector2& v) {
		return { l.a - v, l.b - v };
	}

	std::string to_string(const line_segment2& l) {
		return to_string(l.a) + std::string("; ") + to_string(l.b);
	}

	line_stepper2::line_stepper2(const line_segment2& l, float step_size) {
		vector2 disp = l.b - l.a;
		float lengthf = std::sqrt(squared(disp));
		waypoints = (lengthf / step_size) + 1;
		step_p = disp * (step_size / lengthf);
		p = l.a;
	}
	void line_stepper2::step() {
		p += step_p;
	}

	ball2::ball2(const vector2& center, float radius) {
		c = center;
		r = radius;
	}
	ball2& ball2::operator =(const gmtry3::ball3& sphere) {
		c = sphere.c;
		r = sphere.r;
		return *this;
	}

	ball2 operator *(const ball2& b, float s) {
		return { b.c, b.r * s };
	}

	ball2 operator /(const ball2& b, float s) {
		return { b.c, b.r / s };
	}

	ball2 operator +(const ball2& b, const vector2& v) {
		return { b.c + v, b.r };
	}

	ball2 operator -(const ball2& b, const vector2& v) {
		return { b.c - v, b.r };
	}

	bool intersects(const vector2& p, const ball2& b) {
		return squared(p - b.c) <= b.r * b.r;
	}
}

// 2D integer geometry
namespace gmtry2i {
	long floor(float f) {
		long l = f;
		return l - (l > f);
	}

	long ceil(float f) {
		long l = f;
		return l + (l < f);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    INTEGER_PRECISION VECTORS & POINTS                                                          //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	vector2i::vector2i(long i, long j) {
		x = i;
		y = j;
	}
	vector2i::vector2i(const gmtry2::vector2& p) {
		x = floor(p.x);
		y = floor(p.y);
	}
	long& vector2i::operator [](int i) {
		return (&x)[i];
	}
	const long& vector2i::operator [](int i) const {
		return (&x)[i];
	}
	vector2i& vector2i::operator +=(const vector2i& v) {
		x += v.x;
		y += v.y;
		return *this;
	}
	vector2i& vector2i::operator -=(const vector2i& v) {
		x -= v.x;
		y -= v.y;
		return *this;
	}
	vector2i& vector2i::operator =(const gmtry2::vector2& p) {
		x = floor(p.x);
		y = floor(p.y);
		return *this;
	}

	vector2i operator +(const vector2i& a, const vector2i& b) {
		return vector2i(a.x + b.x, a.y + b.y);
	}

	vector2i operator -(const vector2i& a, const vector2i& b) {
		return vector2i(a.x - b.x, a.y - b.y);
	}

	vector2i operator |(const vector2i& a, const vector2i& b) {
		return vector2i(a.x | b.x, a.y | b.y);
	}

	vector2i operator -(const vector2i& v) {
		return vector2i(-v.x, -v.y);
	}

	vector2i operator *(const vector2i& v, long s) {
		return vector2i(v.x * s, v.y * s);
	}

	vector2i operator /(const vector2i& v, long s) {
		return vector2i(v.x / s, v.y / s);
	}

	vector2i operator <<(const vector2i& v, unsigned int i) {
		return vector2i(v.x << i, v.y << i);
	}

	vector2i operator >>(const vector2i& v, unsigned int i) {
		return vector2i(v.x >> i, v.y >> i);
	}

	vector2i operator &(const vector2i& v, long s) {
		return vector2i(v.x & s, v.y & s);
	}

	vector2i operator |(const vector2i& v, long s) {
		return vector2i(v.x | s, v.y | s);
	}

	vector2i operator >(const vector2i& v, long s) {
		return vector2i(v.x > s, v.y > s);
	}

	vector2i operator <(const vector2i& v, long s) {
		return vector2i(v.x < s, v.y < s);
	}

	vector2i operator >=(const vector2i& v, long s) {
		return vector2i(v.x >= s, v.y >= s);
	}

	vector2i operator <=(const vector2i& v, long s) {
		return vector2i(v.x <= s, v.y <= s);
	}

	bool operator ==(const vector2i& u, const vector2i& v) {
		return u.x == v.x && u.y == v.y;
	}

	long dot(const vector2i& u, const vector2i& v) {
		return u.x * v.x + u.y * v.y;
	}

	long squared(const vector2i& v) {
		return dot(v, v);
	}

	// returns pseudoscalar (<u v>_2) / (e_1 e_2)
	long wedge(const vector2i& u, const vector2i& v) {
		return u.x * v.y - u.y * v.x;
	}

	std::string to_string(const vector2i& v) {
		return std::to_string(v.x) + std::string(", ") + std::to_string(v.y);
	}

	gmtry2::vector2 to_vector2(const vector2i& v) {
		return gmtry2::vector2(v.x, v.y);
	}

	gmtry2::vector2 to_point2(const vector2i& p) {
		return { p.x + 0.5F, p.y + 0.5F };
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    AXIS-ALIGNED INTEGER-PRECISION BOXES                                                        //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	aligned_box2i::aligned_box2i(const vector2i& min_v, const vector2i& max_v) {
		min = min_v;
		max = max_v;
	}
	aligned_box2i::aligned_box2i(const vector2i& origin, long width) {
		min = origin;
		max = vector2i(origin.x + width, origin.y + width);
	}
	vector2i& aligned_box2i::operator [](int i) {
		return (&min)[i];
	}
	const vector2i& aligned_box2i::operator [](int i) const {
		return (&min)[i];
	}

	vector2i center(const aligned_box2i& b1) {
		return (b1.min + b1.max) >> 1;
	}

	long area(const aligned_box2i& b) {
		return (b.max.x - b.min.x) * (b.max.y - b.min.y);
	}

	aligned_box2i boundsof(const vector2i& p) {
		return aligned_box2i(p, 1);
	}

	aligned_box2i boundsof(const aligned_box2i& b) {
		return b;
	}

	aligned_box2i boundsof(const vector2i& p1, const vector2i& p2) {
		return aligned_box2i({ std::min(p1.x, p2.x),     std::min(p1.y, p2.y) },
			{ std::max(p1.x, p2.x) + 1, std::max(p1.y, p2.y) + 1 });
	}

	aligned_box2i boundsof(const aligned_box2i& b, const vector2i& p) {
		return aligned_box2i({ std::min(b.min.x, p.x),     std::min(b.min.y, p.y) },
			{ std::max(b.max.x, p.x + 1), std::max(b.max.y, p.y + 1) });
	}

	aligned_box2i boundsof(const gmtry2::line_segment2& l) {
		return aligned_box2i({ floor(std::min(l.a.x, l.b.x)), floor(std::min(l.a.y, l.b.y)) },
			{ ceil(std::max(l.a.x, l.b.x)),  ceil(std::max(l.a.y, l.b.y)) });
	}

	aligned_box2i boundsof(const gmtry2::ball2& b) {
		gmtry2::vector2 corner_disp(b.r, b.r);
		return { b.c - corner_disp, b.c + corner_disp };
	}

	bool contains(const aligned_box2i& b, const vector2i& p) {
		return b.min.x <= p.x && b.min.y <= p.y && b.max.x > p.x && b.max.y > p.y;
	}

	bool contains(const aligned_box2i& b1, const aligned_box2i& b2) {
		return b1.min.x <= b2.min.x && b1.min.y <= b2.min.y && b1.max.x >= b2.max.x && b1.max.y >= b2.max.y;
	}

	bool intersects(const vector2i& p, const aligned_box2i& b) {
		return contains(b, p);
	}

	vector2i intersection(const vector2i& p, const aligned_box2i& b) {
		return p;
	}

	// Returns minkowski sum
	aligned_box2i operator +(const aligned_box2i& b1, const aligned_box2i& b2) {
		return aligned_box2i(b1.min + b2.min, b1.max + b2.max);
	}

	// Returns minkowski difference
	aligned_box2i operator -(const aligned_box2i& b1, const aligned_box2i& b2) {
		vector2i dif1 = b1.min - b2.max;
		vector2i dif2 = b1.max - b2.min;
		// sign(dif1.x - dif2.x) = sign(dif1.y - dif2.y) ? Sure hope so
		bool dif1_is_min = dif1.x < dif2.x || dif1.y < dif2.y;
		return aligned_box2i(dif1_is_min ? dif1 : dif2, dif1_is_min ? dif2 : dif1);
	}

	aligned_box2i operator +(const aligned_box2i& b, const vector2i& v) {
		return aligned_box2i(b.min + v, b.max + v);
	}

	aligned_box2i operator -(const aligned_box2i& b, const vector2i& v) {
		return aligned_box2i(b.min - v, b.max - v);
	}

	aligned_box2i operator -(const vector2i& v, const aligned_box2i& b) {
		return aligned_box2i(v - b.max, v - b.min);
	}

	bool intersects(const aligned_box2i& b1, const aligned_box2i& b2) {
		return contains(b1 - b2, vector2i());
	}

	aligned_box2i intersection(const aligned_box2i& b1, const aligned_box2i& b2) {
		return { { std::max(b1.min.x, b2.min.x), std::max(b1.min.y, b2.min.y) },
				 { std::min(b1.max.x, b2.max.x), std::min(b1.max.y, b2.max.y) } };
	}

	// if no intersection exists, sets no_intersection to true
	aligned_box2i intersection(const aligned_box2i& b1, const aligned_box2i& b2, bool& no_intersection) {
		aligned_box2i b3({ std::max(b1.min.x, b2.min.x), std::max(b1.min.y, b2.min.y) },
			{ std::min(b1.max.x, b2.max.x), std::min(b1.max.y, b2.max.y) });
		if ((b3.min.x < b3.max.x) && (b3.min.y < b3.max.y)) return b3;
		no_intersection = true;
		return aligned_box2i();
	}

	bool intersects(const gmtry2::line_segment2 l, const aligned_box2i& box) {
		// test if box contains either endpoint
		if (contains(box, l.a) || contains(box, l.b)) return true;
		gmtry2::vector2 disp = l.b - l.a;
		for (int dim = 0; dim < 2; dim++) if (abs(disp[dim]) > gmtry2::EPSILON)
			for (int extrema = 0; extrema < 2; extrema++) {
				// a[dim] + t*disp[dim] = box[extrema][dim]
				// t = (box[extrema][dim] - a[dim]) / disp[dim]
				// if 0 <= t <= 1 then l intersects the infinite line on which this edge of the box lies
				float value1 = box[extrema][dim] - l.a[dim];
				float value2 = disp[dim];
				// if t is not negative AND t is less than 1
				if (((value1 < 0) == (value2 < 0)) && (abs(value1) <= abs(value2))) {
					int other_dim = 1 & ~dim;
					// the other dimension of l's intersection with this edge
					float other_intersection = l.a[other_dim] + disp[other_dim] * (value1 / value2);
					if (box.min[other_dim] <= other_intersection && other_intersection < box.max[other_dim])
						return true;
				}
			}
		return false;
	}

	// Returns the intersection of a line segment and a box
	// Parameter no_intersection is set to true if there is no intersection
	gmtry2::line_segment2 intersection(const gmtry2::line_segment2 l, const aligned_box2i& box,
		bool& no_intersection) {
		// new points will either be intersections with box edges or existing endpoints contained in the box
		gmtry2::vector2 new_pts[2];
		int pt_idx = 0;
		if (contains(box, l.a)) new_pts[pt_idx++] = l.a;
		if (contains(box, l.b)) new_pts[pt_idx++] = l.b;
		if (pt_idx > 1) return { new_pts[0], new_pts[1] };
		gmtry2::vector2 disp = l.b - l.a;
		for (int dim = 0; dim < 2; dim++) if (abs(disp[dim]) > gmtry2::EPSILON) {
			// explanation for the following meth is in intersects(line_segment2i, aligned_box2i)
			float value2 = disp[dim];
			for (int extrema = 0; extrema < 2; extrema++) {
				float value1 = box[extrema][dim] - l.a[dim];
				if (((value1 < 0) == (value2 < 0)) && (abs(value1) <= abs(value2)) && (value1 != 0)) {
					int other_dim = 1 & ~dim;
					// position along direction parallel to box edge of intersection with box_pts area
					float other_intersection = l.a[other_dim] + disp[other_dim] * (value1 / value2);
					if (box.min[other_dim] <= other_intersection && other_intersection < box.max[other_dim]) {
						gmtry2::vector2 intersection;
						intersection[dim] = extrema ? (box[extrema][dim] - gmtry2::EPSILON) : (box[extrema][dim]);
						intersection[other_dim] = other_intersection;
						new_pts[pt_idx++] = intersection;
					}
					if (pt_idx > 1) return { new_pts[0], new_pts[1] };
				}
			}
		}
		no_intersection = true;
		return {};
	}

	std::string to_string(const aligned_box2i& b) {
		return to_string(b.min) + std::string("; ") + to_string(b.max);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    INTEGER-PRECISION LINE SEGMENTS                                                             //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	line_segment2i::line_segment2i(const vector2i& point_A, const vector2i& point_B) {
		a = point_A;
		b = point_B;
	}
	line_segment2i::line_segment2i(const gmtry2::line_segment2& l) {
		a = l.a;
		b = l.b;
	}
	line_segment2i::operator gmtry2::line_segment2() const {
		return { to_point2(a), to_point2(b) };
	}

	aligned_box2i boundsof(const line_segment2i& l) {
		return boundsof(l.a, l.b);
	}

	bool contains(const gmtry2i::aligned_box2i& b, const line_segment2i& l) {
		return contains(b, l.a) && contains(b, l.b);
	}

	line_segment2i operator +(const line_segment2i& l, const vector2i& v) {
		return { l.a + v, l.b + v };
	}

	line_segment2i operator -(const line_segment2i& l, const vector2i& v) {
		return { l.a - v, l.b - v };
	}

	bool intersects(const line_segment2i& l1, const line_segment2i& l2) {
		vector2i disp1 = l1.b - l1.a; // from l1.a to l1.b
		vector2i disp2 = l2.b - l2.a; // from l2.a to l2.b
		vector2i adisp = l1.a - l2.a; // from l2.a to l1.a
		long wedge1 = wedge(disp2, adisp);
		long wedge2 = wedge(disp1, adisp);
		long wedge3 = wedge(disp2, disp1);
		// test for parallelity
		if (wedge3 == 0) return false;
		// see intersection(line_segment2i, line_segment2i) for first half of following derivation
		// if t >= 0 and abs(t) <= 1, then l1 and l2 intersect
		//		[ t >= 0 ] = XOR(wedge(disp2, adisp) < 0, wedge(disp2, disp1) < 0)
		//		[ abs(t) <= 1 ] = [ abs(wedge(disp2, adisp)) / abs(wedge(disp2, disp1)) <= 1 ]
		//						= [ abs(wedge(disp2, adisp)) <= abs(wedge(disp2, disp1)) ]
		return ((wedge1 < 0) != (wedge3 < 0)) && ((wedge2 < 0) != (wedge3 < 0)) &&
			(abs(wedge1) <= abs(wedge3)) && (abs(wedge2) <= abs(wedge3));
	}

	// result invalid if l1 and l2 do not intersect
	vector2i intersection(const line_segment2i& l1, const line_segment2i& l2) {
		vector2i disp1 = l1.b - l1.a; // from l1.a to l1.b
		vector2i disp2 = l2.b - l2.a; // from l2.a to l2.b
		vector2i adisp = l1.a - l2.a; // from l2.a to l1.a
		// wedge(disp2, adisp + t*disp1) = 0
		// = wedge(disp2, adisp) + t*wedge(disp2, disp1)
		// t = - wedge(disp2, adisp) / wedge(disp2, disp1)
		float disp_scale = static_cast<float>(wedge(disp2, adisp)) / wedge(disp2, disp1);
		return l1.a + gmtry2i::vector2i(floor(0.5F - disp1.x * disp_scale), floor(0.5F - disp1.y * disp_scale));
	}

	bool intersects(const line_segment2i& l, const aligned_box2i& box) {
		// test if box contains either endpoint
		if (contains(box, l.a) || contains(box, l.b)) return true;
		vector2i disp = l.b - l.a;
		for (int dim = 0; dim < 2; dim++) if (disp[dim])
			for (int extrema = 0; extrema < 2; extrema++) {
				// a[dim] + t*disp[dim] = box[extrema][dim]
				// t = (box[extrema][dim] - a[dim]) / disp[dim]
				// if 0 <= t <= 1 then they intersect
				float value1 = box[extrema][dim] - (l.a[dim] + 0.5F);
				int value2 = disp[dim];
				// if t is not negative AND t is less than 1
				if (((value1 < 0) == (value2 < 0)) && (abs(value1) <= abs(value2))) {
					int other_dim = 1 & ~dim;
					float other_intersection = l.a[other_dim] + 0.5F + disp[other_dim] * (value1 / value2);
					if (box.min[other_dim] <= other_intersection && other_intersection < box.max[other_dim])
						return true;
				}
			}
		return false;
	}

	// Current implementation is slow
	bool intersects(const line_segment2i& l, const vector2i& p) {
		gmtry2i::aligned_box2i box(boundsof(p));
		// test if box contains either endpoint
		if (contains(box, l.a) || contains(box, l.b)) return true;
		vector2i disp = l.b - l.a;
		for (int dim = 0; dim < 2; dim++) if (disp[dim])
			for (int extrema = 0; extrema < 2; extrema++) {
				// a[dim] + t*disp[dim] = box[extrema][dim]
				// t = (box[extrema][dim] - a[dim]) / disp[dim]
				// if 0 <= t <= 1 then they intersect
				float value1 = box[extrema][dim] - (l.a[dim] + 0.5F);
				int value2 = disp[dim];
				// if t is not negative AND t is less than 1
				if (((value1 < 0) == (value2 < 0)) && (abs(value1) <= abs(value2))) {
					int other_dim = 1 & ~dim;
					float other_intersection = l.a[other_dim] + 0.5F + disp[other_dim] * (value1 / value2);
					if (box.min[other_dim] <= other_intersection && other_intersection < box.max[other_dim])
						return true;
				}
			}
		return false;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    LINE SEGMENT RASTERIZATION                                                                  //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	line_stepper2i::line_stepper2i(const line_segment2i& l, float step_size) {
		vector2i disp = l.b - l.a;
		float length = std::sqrt(squared(disp));
		waypoints = (length / step_size) + 1;
		float step_scale = step_size / length;
		step_x = disp.x * step_scale;
		step_y = disp.y * step_scale;
		x = static_cast<float>(l.a.x) + 0.5F;
		y = static_cast<float>(l.a.y) + 0.5F;
		p = l.a;
	}
	line_stepper2i::line_stepper2i(const gmtry2::line_segment2& l, float step_size) {
		gmtry2::vector2 disp = l.b - l.a;
		float length = std::sqrt(gmtry2::squared(disp));
		waypoints = (length / step_size) + 1;
		float step_scale = step_size / length;
		step_x = disp.x * step_scale;
		step_y = disp.y * step_scale;
		x = l.a.x;
		y = l.a.y;
		p = l.a;
	}
	void line_stepper2i::step() {
		x += step_x; y += step_y;
		p = vector2i(floor(x), floor(y));
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////
	//    BOX INTERSECTION ABSTRACTION                                                                //
	////////////////////////////////////////////////////////////////////////////////////////////////////

	box_intersectable2i::box_intersectable2i() {
		intersector = new intersectable_box_intersector2i<aligned_box2i>(aligned_box2i());
	}
	box_intersectable2i::box_intersectable2i(box_intersector2i* new_intersector) {
		intersector = new_intersector->clone();
	}
	box_intersectable2i::box_intersectable2i(const box_intersectable2i& object) {
		intersector = object.intersector->clone();
	}
	bool box_intersectable2i::intersects(const aligned_box2i& box) const {
		return intersector->intersects(box);
	}
	//box_intersectable2i intersection(const aligned_box2i& box) const {
	//	return box_intersectable2i(intersector->intersection(box));
	//}
	aligned_box2i box_intersectable2i::get_bounds() const {
		return intersector->get_bounds();
	}
	box_intersectable2i& box_intersectable2i::operator =(const box_intersectable2i& object) {
		if (intersector) delete intersector;
		intersector = object.intersector->clone();
		return *this;
	}
	box_intersectable2i::~box_intersectable2i() {
		delete intersector;
	}

	bool intersects(const box_intersectable2i& object, const aligned_box2i& box) {
		return object.intersects(box);
	}

	//box_intersectable2i intersection(const box_intersectable2i& object, const aligned_box2i& box) {
	//	return object.intersection(box);
	//}

	aligned_box2i boundsof(const box_intersectable2i& object) {
		return object.get_bounds();
	}
}