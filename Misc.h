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
#include "PlatformSpecific.h"
#include "Types.h"
#include "Core.h"
#include "filesystem.h"

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
		bool IsBeingDebugged();

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

		/// <summary>
		/// Returns uninitialized memory aligned to alignment boundary.
		/// Has same behaviour as std::malloc
		/// </summary>
		template<class Type, size_t alignment = 32>
		Type * alignedMalloc(std::size_t numObjects)
		{
			void * ptr = nullptr;
			std::size_t size = sizeof(Type) * numObjects;
			#ifdef CPL_WINDOWS
			ptr = _aligned_malloc(size, alignment);
			#else
			// : http://stackoverflow.com/questions/196329/osx-lacks-memalign

			#ifdef CPL_MAC
				// all allocations on OS X are aligned to 16-byte boundaries
				// NOTE: removed, as alignedFree doesn't account for this
				// if(alignment <= 16)
				//	return reinterpret_cast<Type *>(std::malloc(numObjects * sizeof(Type)));
			#endif

			void *mem = malloc(size + (alignment - 1) + sizeof(void*));

			char *amem = ((char*)mem) + sizeof(void*);
			amem += (alignment - ((uintptr_t)amem & (alignment - 1)) & (alignment - 1));

			((void**)amem)[-1] = mem;
			ptr = amem;
			#endif

			return reinterpret_cast<Type *>(ptr);
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
			#ifdef CPL_WINDOWS
			return reinterpret_cast<Type *>(_aligned_realloc(ptr, numObjects * sizeof(Type), alignment));
			#else
			#ifdef CPL_MAC
			// all allocations on OS X are aligned to 16-byte boundaries
			// NOTE: removed, as alignedFree doesn't account for this
			//if(alignment <= 16)
			//	return reinterpret_cast<Type *>(std::realloc(ptr, numObjects * sizeof(Type)));
			#endif
			// https://github.com/numpy/numpy/issues/5312
			void *p1, **p2, *base;
			std::size_t
				old_offs,
				offs = alignment - 1 + sizeof(void*),
				n = sizeof(Type) * numObjects;

			if (ptr != nullptr)
			{
				base = *(((void**)ptr) - 1);
				if ((p1 = std::realloc(base, n + offs)) == nullptr)
					return nullptr;
				if (p1 == base)
					return ptr;
				p2 = (void**)(((std::uintptr_t)(p1) + offs) & ~(alignment - 1));
				old_offs = (size_t)((std::uintptr_t)ptr - (std::uintptr_t)base);
				std::memmove(p2, (char*)p1 + old_offs, n);
			}
			else
			{
				if ((p1 = std::malloc(n + offs)) == nullptr)
					return nullptr;
				p2 = (void**)(((std::uintptr_t)(p1) + offs) & ~(alignment - 1));
			}
			*(p2 - 1) = p1;
			return reinterpret_cast<Type *>(p2);

			#endif
		}

		/// <summary>
		/// Returns uninitialized memory aligned to alignment boundary.
		/// Has same behaviour as std::malloc
		/// </summary>
		inline void * alignedBytesMalloc(std::size_t size, std::size_t alignment)
		{

			void * ptr = nullptr;
			#ifdef CPL_WINDOWS
			ptr = _aligned_malloc(size, alignment);
			#else
			// : http://stackoverflow.com/questions/196329/osx-lacks-memalign
			void *mem = malloc(size + (alignment - 1) + sizeof(void*));

			char *amem = ((char*)mem) + sizeof(void*);
			amem += ((alignment - ((uintptr_t)amem & (alignment - 1))) & (alignment - 1));

			((void**)amem)[-1] = mem;
			ptr = amem;;
			#endif

			return reinterpret_cast<void *>(ptr);
		}

		template<class Type>
		void alignedFree(Type & obj)
		{
			#ifdef CPL_WINDOWS
			_aligned_free(obj);
			#else
			if (obj)
			{
				free(((void**)obj)[-1]);
			}
			#endif
			obj = nullptr;
		}

		enum MsgButton : int
		{
			#ifdef CPL_WINDOWS
			bYes = IDYES,
			bNo = IDNO,
			bRetry = IDRETRY,
			bTryAgain = IDTRYAGAIN,
			bContinue = IDCONTINUE,
			bCancel = IDCANCEL,
			bError = -1
			#elif defined(CPL_MAC)
			bError = -1,
			bYes = 6,
			bNo = 7,
			bRetry = 4,
			bTryAgain = 10,
			bContinue = 11,
			bCancel = 2,
			bOk = bYes
			#else
			bError = -1,
			bYes = 1,
			bNo = 2,
			bRetry = 3,
			bTryAgain = 4,
			bContinue = 5,
			bCancel = 6,
			bOk = bYes
			#endif
		};
		enum MsgStyle : int
		{
			#ifdef CPL_WINDOWS
			sOk = MB_OK,
			sYesNo = MB_YESNO,
			sYesNoCancel = MB_YESNOCANCEL,
			sConTryCancel = MB_CANCELTRYCONTINUE
			#elif defined(CPL_MAC)
			sOk = 0,
			sYesNo = 3,
			sYesNoCancel = 6,
			sConTryCancel = 9
			#else
			sOk = 0,
			sYesNo = 1,
			sYesNoCancel = 2,
			sConTryCancel = 3
			#endif
		};
		enum MsgIcon : int
		{
			#ifdef CPL_WINDOWS
			iStop = MB_ICONSTOP,
			iQuestion = MB_ICONQUESTION,
			iInfo = MB_ICONINFORMATION,
			iWarning = MB_ICONWARNING
			#elif defined(APE_IPLUG)
			iStop = MB_ICONSTOP,
			iQuestion = MB_ICONINFORMATION,
			iInfo = MB_ICONINFORMATION,
			iWarning = MB_ICONSTOP
			#elif defined(CPL_MAC)
			iStop = 0x10,
			iWarning = iStop,
			iInfo = 0x40,
			iQuestion = 0x20
			#else
			iInfo = 0 << 8,
			iWarning = 1 << 8,
			iStop = 2 << 8,
			iQuestion = 3 << 8
			#endif
		};

		int MsgBox(const string_ref text,
			const string_ref title = "",
			int nStyle = MsgStyle::sOk,
			void * parent = NULL,
			const bool bBlocking = true);


		/// <summary>
		/// Waits on some boolean flag to become false. Be careful with using this in case of deadlocks.
		/// </summary>
		/// <param name="ms"></param>
		/// <param name="bVal"></param>
		/// <returns></returns>
		template<typename T>
		bool SpinLock(unsigned int ms, T & bVal) {
			unsigned start;
			int ret;
		loop:
			start = QuickTime();
			while (!!bVal) {
				if ((QuickTime() - start) > ms)
					goto time_out;
				Delay(0);
			}
			// normal exitpoint
			return true;
			// deadlock occurs

		time_out:
			// TODO: refactor to separate file.. so we access the name of the program without cyclic dependencies.
			ret = MsgBox("Deadlock detected in spinlock: Protected resource is not released after max interval. "
				"Wait again (try again), release resource (continue) - can create async issues - or exit (cancel)?",
				"cpl sync error!",
				sConTryCancel | iStop);
			switch (ret)
			{
				case MsgButton::bTryAgain:
					goto loop;
				case MsgButton::bContinue:
					bVal = !bVal; // flipping val, and it's a reference so should release resource.
					return false;
				case MsgButton::bCancel:
					//TODO: Maybe find a better way to exit?
					exit(-1);
			}
			// not needed (except for warns)
			return false;
		}

		/// <summary>
		/// Waits on the functor to return true for at least ms miliseconds.
		/// If condition has not returned true yet, it prompts the user to
		/// either continue anyway, wait some more, or exit the application.
		/// </summary>
		/// <param name="ms">How many miliseconds to wait</param>
		/// <param name="cond">Functor returning true if condition is met</param>
		/// <param name="delay">Optional delay between invocations (0, default, will yield the thread)</param>
		/// <returns>True if functor returned true, false if user chose to continue anyway.</returns>
		template<typename Condition, bool presentUserOption = true>
		bool WaitOnCondition(unsigned int ms, Condition cond, unsigned int delay = 0)
		{
			unsigned start;
			int ret;
		loop:
			start = QuickTime();
			while (!cond()) {
				if ((QuickTime() - start) > ms)
					goto time_out;
				Delay(delay);
			}
			// normal exitpoint
			return true;
			// deadlock occurs

		time_out:
			if (!presentUserOption)
			{
				return false;
			}
			ret = MsgBox("Deadlock detected in conditional wait: Protected resource is not released after max interval. "
				"Wait again (try again, breaks if debugged), continue anyway (continue) - can create async issues - or exit (cancel)?",
				"cpl conditional wait error!",
				sConTryCancel | iStop);
			switch (ret)
			{
				case MsgButton::bTryAgain:
					CPL_BREAKIFDEBUGGED();
					goto loop;
				case MsgButton::bCancel:
					// TODO: exit another way?
					exit(-1);
			}
			// not needed (except for warns)
			return false;
		}

	}; // Misc
}; // APE
#endif
