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

	file:CDataBuffer.h

		A generic contigious buffer buffer, specialized for large growable buffers,
		taking advantage of realloc(). As of such, it only works for types that satisfy
		the following concepts:

			TriviallyCopyable - enforced through static_assert

		Note: It is not guaranteed to use realloc, and may in fact be a wrapper
		around std::vector. This can be checked through CDataBuffer<T>::is_std_vector > 0

*************************************************************************************/

#ifndef _CDATABUFFER_H
#define _CDATABUFFER_H
#include "../MacroConstants.h"
#include <vector>
#include <cstdlib>
#include <type_traits>
#include <algorithm>

namespace cpl
{

	template<typename T, std::size_t requiredAlignment = alignof(T)>
	class CDataBuffer
	{
	public:

		static_assert(std::is_trivially_copyable<T>::value, "CDataBuffer needs its argument to be trivially copyable!");

		static const std::size_t is_std_vector = 0;
		static const std::size_t alignment = requiredAlignment;
		CDataBuffer()
			: buffer(nullptr), bufSize(0)
		{

		}

		CDataBuffer(const std::size_t initialSize)
			: buffer(nullptr), bufSize(0)
		{
			resize(initialSize);
		}

		CDataBuffer(const std::size_t initialSize, const T & initializer)
			: buffer(nullptr), bufSize(0)
		{
			resize(initialSize, initializer);
		}

		template<class InputIterator>
		CDataBuffer(InputIterator first, InputIterator last)
			: buffer(nullptr), bufSize(0)
		{
			#ifdef _DEBUG
			if (first > last || !first || !last)
				CPL_RUNTIME_EXCEPTION("Corrupt range arguments");
			#endif
			resize(last - first);
			std::copy(first, last, begin());
		}

		CDataBuffer(const T * first, const T * last)
			: buffer(nullptr), bufSize(0)
		{
			#ifdef _DEBUG
			if (first > last || !first || !last)
				CPL_RUNTIME_EXCEPTION("Corrupt range arguments");
			#endif
			resize(last - first);
			std::memcpy(buffer, first, (last - first) * sizeof(T));
		}

		CDataBuffer(const CDataBuffer<T, requiredAlignment> & other)
			: buffer(nullptr), bufSize(0)
		{
			resize(other.size());
			std::memcpy(buffer, other.buffer, other.size() * sizeof(T));
		}

		CDataBuffer(CDataBuffer<T> && other)
		{
			bufSize = other.bufSize;
			buffer = other.buffer;
			other.bufSize = 0;
			other.buffer = nullptr;
		}

		CDataBuffer & operator = (const CDataBuffer<T, requiredAlignment> & other)
		{
			clear();
			resize(other.size());
			std::memcpy(buffer, other.buffer, other.size() * sizeof(T));
			return *this;
		}

		CDataBuffer & operator = (CDataBuffer<T, requiredAlignment> && other)
		{
			bufSize = other.bufSize;
			buffer = other.buffer;
			other.bufSize = 0;
			other.buffer = nullptr;
			return *this;
		}

		~CDataBuffer()
		{
			clear();
		}

		void clear()
		{
			if (buffer)
			{
				Misc::alignedFree(buffer);
			}
			buffer = nullptr;
			bufSize = 0;
		}

		std::size_t size() const CPL_NOEXCEPT_IF_RELEASE { return bufSize; }
		std::size_t capacity() const CPL_NOEXCEPT_IF_RELEASE { return bufSize; }

		void resize(std::size_t newSize)
		{
			if (newSize != bufSize)
			{
				T * newBlock = Misc::alignedRealloc<T, alignment>(buffer, newSize);
				if (!newBlock && newSize != 0)
				{
					// error allocating memory.
					CPL_RUNTIME_EXCEPTION("Error at CDataBuffer::resize::realloc()");
				}
				else
				{
					buffer = newBlock;
					bufSize = newSize;
				}
			}
		}
		/// <summary>
		/// Notice that 0 is a valid argument, in which the value of
		/// begin()/data() is implementation defined
		/// </summary>
		void resize(std::size_t newSize, const T & initializer)
		{
			std::size_t oldSize = bufSize;
			resize(newSize);

			if (newSize > oldSize)
			{
				// fill remaining elements
				std::fill(buffer + oldSize, buffer + bufSize, initializer);
			}
		}

		inline T * begin() CPL_NOEXCEPT_IF_RELEASE
		{
			return buffer;
		}

		inline T * end() CPL_NOEXCEPT_IF_RELEASE
		{
			return buffer + bufSize;
		}

		inline const T * begin() const CPL_NOEXCEPT_IF_RELEASE
		{
			return buffer;
		}

		inline const T * end() const CPL_NOEXCEPT_IF_RELEASE
		{

			return buffer + bufSize;
		}

		inline const T & operator [] (std::size_t index) const CPL_NOEXCEPT_IF_RELEASE
		{
			#ifdef _DEBUG
			if (index >= bufSize)
				CPL_RUNTIME_EXCEPTION("Index out of bounds");
			#endif
			return buffer[index];
		}

		inline T & operator [] (std::size_t index) CPL_NOEXCEPT_IF_RELEASE
		{
			#ifdef _DEBUG
			if (index >= bufSize)
				CPL_RUNTIME_EXCEPTION("Index out of bounds");
			#endif
			return buffer[index];
		}

		inline T & at(std::size_t index)
		{
			if (!buffer)
				CPL_RUNTIME_EXCEPTION("No valid elements to return!");
			if (index >= bufSize)
				CPL_RUNTIME_EXCEPTION("Index out of bounds");
			return buffer[index];
		}

		inline const T & at(std::size_t index) const
		{
			if (!buffer)
				CPL_RUNTIME_EXCEPTION("No valid elements to return!");
			if (index >= bufSize)
				CPL_RUNTIME_EXCEPTION("Index out of bounds");
			return buffer[index];
		}

		inline T * data() CPL_NOEXCEPT_IF_RELEASE
		{
			return begin();
		}

		inline const T * data() const CPL_NOEXCEPT_IF_RELEASE
		{
			return begin();
		}

	private:
		T * buffer;
		std::size_t bufSize;
	};

};
#endif