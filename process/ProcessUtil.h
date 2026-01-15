/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2017 Janus Lynggaard Thorborg (www.jthorborg.com)

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

	file:ProcessUtil.h
		Private detail of Process.h

*************************************************************************************/

#ifndef CPL_PROCESS_UTIL_H
#define CPL_PROCESS_UTIL_H

#include <ostream>
#include <istream>
#include <memory>
#include "../MacroConstants.h"
#include "../PlatformSpecific.h"
#include "../Misc.h"

namespace cpl
{
	namespace detail
	{
		template<typename T, std::ptrdiff_t invalid>
		struct NullableHandle
		{
			static constexpr T null()
			{
				return T(invalid);
			}

			NullableHandle() : handle(null()) {}
			NullableHandle(T h) : handle(h) {}
			NullableHandle(std::nullptr_t) : handle(null()) {}

			operator T() const noexcept { return handle; }

			bool operator ==(const NullableHandle &other) const { return handle == other.handle; }
			bool operator !=(const NullableHandle &other) const { return handle != other.handle; }
			bool operator ==(std::nullptr_t) const { return handle == null(); }
			bool operator !=(std::nullptr_t) const { return handle != null(); }

			T handle;
		};

		#ifdef CPL_WINDOWS
		typedef NullableHandle<HANDLE, -1> Handle;
		#else
		typedef NullableHandle<int, -1> Handle;
		#endif

		class SysDeleter
		{
		public:
			typedef Handle pointer;

			void operator() (pointer p)
			{
				#ifdef CPL_WINDOWS
				::CloseHandle(p);
				#else
				::close(p);
				#endif
			}
		};

		using unique_handle = std::unique_ptr<Handle, SysDeleter>;


		template<typename Buf, typename Stream>
		class FileEdge
		{
		public:

			FileEdge(unique_handle pipe)
				: buf(std::move(pipe)), stream(&buf)
			{

			}

			Buf buf;
			Stream stream;
		};

		class OutputBuffer : public std::streambuf
		{
		public:

			OutputBuffer(unique_handle pipe) : pipe(std::move(pipe)) { }

			~OutputBuffer()
			{
				close();
			}

			void close()
			{
				if (pipe.get() == nullptr)
					return;

				sync();
				pipe.reset(nullptr);
			}

		protected:

			virtual int sync() override
			{
				if (pipe.get() == nullptr)
					return -1;

				auto nc = pptr() && pbase() ? pptr() - pbase() : 0;

				setp(std::begin(buffer), std::end(buffer));

				if (nc <= 0)
					return 0;

				#ifdef CPL_WINDOWS
				DWORD dwWritten;
				if (::WriteFile(pipe.get(), buffer, static_cast<DWORD>(nc), &dwWritten, nullptr) && dwWritten == static_cast<DWORD>(nc))
				{
					return 0;
				}
				#else
				if (::write(pipe.get(), buffer, nc) == nc)
					return 0;
				#endif
				return -1;
			}

			virtual int overflow(int c) override
			{

				if (pipe.get() == nullptr)
					return traits_type::eof();

				sync();

				*pptr() = traits_type::to_char_type(c);
				this->pbump(1);

				return traits_type::not_eof(c);
			}

			unique_handle pipe;
			char buffer[1024];
		};


		class OutputStream : public std::ostream
		{
			template<typename, typename>
			friend class FileEdge;

		public:
			void close()
			{
				buf.close();
			}

		private:
			OutputStream(OutputBuffer* buffer) : std::ostream(buffer), buf(*buffer) {}

			OutputBuffer& buf;
		};

		class InputBuffer : public std::streambuf
		{
		public:
			InputBuffer(unique_handle pipe) : pipe(std::move(pipe)) {}

		protected:

			virtual int underflow() override
			{
				if (pipe.get() == nullptr)
					return traits_type::eof();

				#ifdef CPL_WINDOWS
				DWORD lpRead;
				if (ReadFile(pipe.get(), buf, sizeof(buf), &lpRead, nullptr))
				{
					setg(buf, buf, buf + lpRead);
					return buf[0];
				}
				#else
				auto read = ::read(pipe.get(), buf, sizeof(buf));
				if (read > 0)
				{
					setg(buf, buf, buf + read);
					return buf[0];
				}
				#endif
				return traits_type::eof();
			}

			unique_handle pipe;
			char buf[1024];
		};

	}

}

#endif
