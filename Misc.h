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

	file:Misc.h

		Whatever doesn't fit other places, globals.
		Helper classes.
		Global functions.

*************************************************************************************/

#ifndef CPL_MISC_H
#define CPL_MISC_H

#include <string>
#include <sstream>
#include <typeinfo>
#include <cstring>
#include <system_error>

#include "Common.h"
#include "Types.h"
#include "Core.h"
#include "filesystem.h"
#include "Exceptions.h"

namespace cpl
{
	template<typename To, typename From>
	constexpr inline typename std::enable_if<std::is_enum<From>::value, To>::type enum_cast(const From & f)
	{
		return static_cast<To>(static_cast<typename std::underlying_type<From>::type>(f));
	}

	template<typename To, typename From>
	constexpr inline typename std::enable_if<std::is_enum<To>::value, To>::type enum_cast(const From & f)
	{
		return static_cast<To>(static_cast<typename std::underlying_type<To>::type>(f));
	}

	template<typename Enum, typename Func>
	typename std::enable_if<std::is_enum<Enum>::value, void>::type foreach_enum(Func f)
	{
		typedef typename std::underlying_type<Enum>::type T;
		for (T i = 0; i < enum_cast<T>(Enum::end); ++i)
			f(enum_cast<Enum>(i));
	}

	/// <summary>
	/// Instead of passing the enum, passes the underlying type of the enum.
	/// </summary>
	template<typename Enum, typename Func>
	typename std::enable_if<std::is_enum<Enum>::value, void>::type foreach_uenum(Func f)
	{
		typedef typename std::underlying_type<Enum>::type T;
		for (T i = 0; i < enum_cast<T>(Enum::end); ++i)
			f(i);
	}

	namespace Misc
	{
		std::pair<int, std::string> ExecCommand(const string_ref arg);
		std::pair<bool, std::string> ReadFile(const fs::path& path) noexcept;
		bool WriteFile(const fs::path& path, const string_ref contents) noexcept;
		std::string GetTime();
		std::string GetDate();
		std::string DemangleRawName(const string_ref name);

		template<class T>
		std::string DemangledTypeName(const T & object)
		{
			return DemangleRawName(typeid(object).name());
		}

		/// <summary>
		/// Returns a system-wide unique ID.
		/// </summary>
		std::int32_t AcquireUniqueInstanceID();
		/// <summary>
		/// Releases a ID previously acquired.
		/// </summary>
		void ReleaseUniqueInstanceID(std::int32_t ID);

		/// <summary>
		/// Returns a pointer to the base of the current image (DLL/DYLIB/SO)
		/// </summary>
		const char * GetImageBase();

		/// <summary>
		/// For Unix & Windows, returns the parent directory of the executable.
		/// Additionally, for OS X, if the executable is inside a bundle, returns "(...) .bundle/Contents/Resources/".
		/// Trailing slash always included.
		/// </summary>
		const std::string & DirectoryPath();
		const fs::path& DirFSPath();

		void TextEditFile(const std::string& textFile);

		long Round(double number);
		long Delay(int ms);
		void PreciseDelay(double msecs);

		unsigned int QuickTime();
		int GetSizeRequiredFormat(const string_ref fmt, va_list pargs);

		std::uint64_t ClockCounter();
		long long TimeCounter();
		double TimeDifference(long long);
        double TimeDifferenceSeconds(long long);
		double TimeToMilisecs(long long);
        double TimeToSeconds(long long);

		/// <summary>
		/// Consumes any key from the console, without requiring enter to be hit.
		/// </summary>
		bool ConsumeAnyKey();
		/// <summary>
		/// Promts the user to press any key in the console to continue
		/// </summary>
		bool PromptAnyKey();

        void _internalAlignedFree(void*);
        void* _internalAlignedRealloc(void* ptr, std::size_t elementSize, std::size_t numObjects, std::size_t alignment);
        void* alignedBytesMalloc(std::size_t size, std::size_t alignment);
    
		/// <summary>
		/// Returns uninitialized memory aligned to alignment boundary.
		/// Has same behaviour as std::malloc
		/// </summary>
		template<class Type, size_t alignment = 32>
		Type * alignedMalloc(std::size_t numObjects)
		{
			return reinterpret_cast<Type *>(alignedBytesMalloc(sizeof(Type) * numObjects, alignment));
		}

		/// <summary>
		/// Returns uninitialized memory aligned to alignment boundary.
		/// Has same behaviour as std::realloc. Alignment between calls
		/// has to be consistent.
		/// </summary>
		template<class Type, size_t alignment = 32>
		inline Type * alignedRealloc(Type * ptr, std::size_t numObjects)
		{
			static_assert(std::is_trivially_copyable<Type>::value, "Reallocations may need to trivially copy buffer");
			return reinterpret_cast<Type *>(_internalAlignedRealloc(ptr, sizeof(Type), numObjects, alignment));
		}

		template<class Type>
		void alignedFree(Type & obj)
		{
            _internalAlignedFree(obj);
			obj = nullptr;
		}
	}; // Misc
}; // APE
#endif
