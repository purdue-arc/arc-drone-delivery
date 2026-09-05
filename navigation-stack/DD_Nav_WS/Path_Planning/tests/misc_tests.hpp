#pragma once

#include <iostream>

// tests to see whats going on with c++ when im confused
namespace misc_tests {
	template <int i>
	class A {
	protected:
		const char* hidden_message = "Class a's hidden message";
	public:
		A() = default;
		virtual const char* message() { return "Class a's message"; }
		void print_message() { std::cout << message() << std::endl; }
	};

	template <int i>
	class B : public A<i> {
	public:
		B() = default;
		virtual const char* message() { return A<i>::hidden_message; }
	};

	int template_inheritance_test() {
		A<4> a = A<4>();
		a.print_message();
		B<4> b = B<4>();
		b.print_message();

		A<4>* undercoverB = new B<4>();
		undercoverB->print_message();
		delete undercoverB;

		return 0;
	}

	class C {
	public:
		virtual void print_message() = 0;
	};

	class D : public C {
	protected:
		const char* get_message() {
			return "hello";
		}
	public:
		void print_message() {
			std::cout << get_message() << std::endl;
		}
	};

	class E : public D {
	protected:
		const char* get_message() {
			return "goodbye";
		}
	};

	int inheritance_test2() {
		D d = D();
		E e = E();

		C* c_ptr = &d;
		c_ptr->print_message();
		c_ptr = &e;
		c_ptr->print_message();

		e.print_message();

		return 0;
	}

	struct POD1 {
		float some_floats[8][8];
	};
	struct POD2 {
		int some_ints[8][8];
	};
	struct POD_combo : public POD1, public POD2 {
		char some_chars[8][8];
	};

	int inheritance_test3() {
		POD_combo a = POD_combo();
		a.some_floats[5][5] = 69.420F;
		a.some_ints[5][5] = 3000;
		a.some_chars[5][5] = 'e';

		std::cout << &a << std::endl;
		POD1* b = &a;
		std::cout << b << std::endl;
		std::cout << b->some_floats[5][5] << std::endl;
		POD2* c = &a;
		std::cout << c << std::endl;
		std::cout << c->some_ints[5][5] << std::endl;
		POD_combo* d = static_cast<POD_combo*>(c);
		std::cout << d << std::endl;
		std::cout << d->some_chars[5][5] << std::endl;

		return 0;
	}
}