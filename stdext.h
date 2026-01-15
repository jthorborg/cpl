/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2016 Janus Lynggaard Thorborg (www.jthorborg.com)

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

	See \licenses\ for additional details on licenses associated with this program.

**************************************************************************************

	file:stdext.h

		Extensions to existing types/algorithms of the std namespace.
		Also functions as creating newer C++ version support.

*************************************************************************************/

#ifndef CPL_STDEXT_H
#define CPL_STDEXT_H

#include <cmath>
#include <functional>
#include <algorithm>
#include <type_traits>
#include <cstring>

namespace cpl
{
	extern const char * newl;
	const char tab = '\t';
	extern const std::string empty_string;

	typedef std::make_signed<std::size_t>::type ssize_t;

	template<class C, typename T>
	std::pair<std::size_t, bool> index_of(const C & c, const T & t)
	{
		auto it = begin(c);
		std::size_t count = 0;
		for (; it != end(c); it++)
		{
			count++;
		}
		return it == end(c) ? std::make_pair(0ul, false) : std::make_pair(count, true);
	}

	template<class C, class T>
	inline auto _contains_impl(const C& c, const T& x, int)
		-> decltype(c.find(x), true)
	{
		return end(c) != c.find(x);
	}

	template<class C, class T>
	inline bool _contains_impl(const C& v, const T& x, long)
	{
		return end(v) != std::find(begin(v), end(v), x);
	}

	template<class C, class T>
	inline auto contains(const C& c, const T& x)
		-> decltype(end(c), true)
	{
		return _contains_impl(c, x, 0);
	}

	template <class C>
	constexpr auto data(C& c) -> decltype(c.data())
	{
		return c.data();
	}
	template <class C>
	constexpr auto data(const C& c) -> decltype(c.data())
	{
		return c.data();
	}

	template <class T, std::size_t N>
	constexpr T* data(T(&arr)[N]) noexcept
	{
		return arr;
	}

	template <class T>
	constexpr T * data(T * arr) noexcept
	{
		return arr;
	}

	template<typename T, typename U>
	T reinterpret_noub_cast(const U & origin)
	{
		T ret;
		std::memcpy(&ret, &origin, sizeof(T));
		return ret;
	}
};



namespace std
{
	template <>
	struct modulus < double >
	{
		typedef double T;
		T operator() (const T& x, const T& y) const { return std::fmod(x, y); }
		typedef T first_argument_type;
		typedef T second_argument_type;
		typedef T result_type;
	};


	/*
		fine piece of code, originally posted here:
			http://codereview.stackexchange.com/a/59999/37465
	*/



	#if defined(__LLVM__) && !defined(__CPP14__)
	// http://isocpp.org/files/papers/N3656.txt

	template<class T> struct _Unique_if {
		typedef unique_ptr<T> _Single_object;
	};

	template<class T> struct _Unique_if<T[]> {
		typedef unique_ptr<T[]> _Unknown_bound;
	};

	template<class T, size_t N> struct _Unique_if<T[N]> {
		typedef void _Known_bound;
	};

	template<class T, class... Args>
	typename _Unique_if<T>::_Single_object
		make_unique(Args&&... args) {
		return unique_ptr<T>(new T(std::forward<Args>(args)...));
	}

	template<class T>
	typename _Unique_if<T>::_Unknown_bound
		make_unique(size_t n) {
		typedef typename remove_extent<T>::type U;
		return unique_ptr<T>(new U[n]());
	}

	template<class T, class... Args>
	typename _Unique_if<T>::_Known_bound
		make_unique(Args&&...) = delete;
	#endif

	template<typename T>
	std::string to_string(T * ptr_type)
	{
		char buf[100];
		sprintf(buf, "0x%p", ptr_type);
		return buf;
	}



}; // std
#endif
