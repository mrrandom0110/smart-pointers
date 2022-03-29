#include "impl.h"
#include <iostream>
#include <memory>
#include <cassert>

struct X {
	int a;
	int b;
};

int main() {
	shared_pointer<X> ptr(new X({1,2}));

	weak_pointer<X> wptr(ptr);

	std::cout << std::boolalpha << "is expired = " << wptr.expired()
	<< '\n' << "use_count = " <<
	ptr.use_count() << '\n' << "is unique = " << ptr.unique() << '\n';

	{
		shared_pointer ptr2(ptr);

		std::cout << std::boolalpha << "is expired = " << wptr.expired()
		          << '\n' << "use_count = " <<
		          ptr.use_count() << '\n' << "is unique = " << ptr.unique() << '\n';

		auto shared_ptr_from_weak = wptr.lock();

		std::cout << std::boolalpha << "is expired = " << wptr.expired()
		          << '\n' << "use_count = " <<
		          ptr.use_count() << '\n' << "is unique = " << ptr.unique() << '\n';

	}

	ptr.reset();

	std::cout << std::boolalpha << "is expired = " << wptr.expired()
	          << '\n' << "use_count = " <<
	          ptr.use_count() << '\n' << "is unique = " << ptr.unique() << '\n';


	unique_pointer<X> uptr(new X({1,2}));

	if (uptr)
	{
		std::cout << "uptr is not null\n" << uptr->a << ' ' << uptr->b << '\n';
	}

	// unique_pointer uptr_try_copy = uptr; compile-error
	{
		/*uptr.release();
		std::cout << uptr->a << ' ' << uptr->b << '\n'; segfault*/
	}

	// template specialization T[] for unique_pointer

	unique_pointer<int[]> arr(new int[100]);

	for (int i = 0; i < 100; ++i)
	{
		arr[i] = i;
	}

	for (int i = 0; i < 100; ++i)
	{
		assert(arr[i] == i);
	}

	unique_pointer<X[]> mas(new X[100]);

	for (int i = 0; i < 100; ++i)
	{
		mas[i].a = i;
		mas[i].b = i + 1;
	}

	for (int i = 0; i < 100; ++i)
	{
		assert(mas[i].a == i);
		assert(mas[i].b == i + 1);
	}

}
