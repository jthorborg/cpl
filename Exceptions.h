/*************************************************************************************

	 cpl - cross platform library - v. 0.1.0.

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

	file:Exceptions.h

		Runtime exceptions, assertions, logging

*************************************************************************************/

#ifndef CPL_EXCEPTIONS_H
#define CPL_EXCEPTIONS_H

#include <string>
#include <string_view>
#include <exception>
#include <stdexcept>
#include <string>
#include <cerrno>

#include "MacroConstants.h"
#include "ProgramInfo.h"
#include "Core.h"
#include "lib/string_ref.h"

#ifdef CPL_WINDOWS
#include <Windows.h> // macros for IsDebuggerPresent, OutputDebugString, DebugBreak
#endif

namespace cpl
{
	int GetLastOSError();
	std::string GetLastOSErrorMessage();
	std::string GetLastOSErrorMessage(int errorToPrint);

	const std::string& GetExceptionLogFilePath();
	void CheckPruneExceptionLogFile();
	void LogException(const string_ref errorMessage);
	void CrashIfUserDoesntDebug(const string_ref errorMessage);

    namespace Misc
    {
        bool IsBeingDebugged();
    };

	bool IsDebuggerAttached();

	class CPLRuntimeException : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;

	};

	class CPLNotImplementedException : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;

	};


	class CPLAssertionException : public CPLRuntimeException
	{
	public:
		using CPLRuntimeException::CPLRuntimeException;

	};

	#define CPL_EXPANDED_MESSAGE message


	#define CPL_INTERNAL_EXCEPTION(msg, file, line, funcname, isassert, exceptionT, exceptionExpression) \
			do \
			{ \
				std::string message = std::string("Runtime exception (" #exceptionT ") in ") + ::cpl::programInfo.name + " (" + ::cpl::programInfo.version.toString() + "): \"" + msg + "\" in " + file + ":" + ::std::to_string(line) + " -> " + funcname; \
				auto e = exceptionExpression;\
				CPL_DEBUGOUT((message + "\n").c_str()); \
				cpl::LogException(message); \
				if(cpl::IsDebuggerAttached()) DBG_BREAK(); \
				bool doAbort = isassert; \
				if(doAbort) \
					std::abort(); \
				else \
					throw e; \
			} while(0)

	#define CPL_EVAL(a, b) a b

	#define CPL_RUNTIME_EXCEPTION_SPECIFIC(msg, exceptionT) \
			CPL_INTERNAL_EXCEPTION(msg, __FILE__, __LINE__, __func__, false, exceptionT, exceptionT (CPL_EXPANDED_MESSAGE))

	#define CPL_RUNTIME_EXCEPTION(msg) \
			CPL_RUNTIME_EXCEPTION_SPECIFIC(msg, cpl::CPLRuntimeException)

	#define CPL_RUNTIME_EXCEPTION_SPECIFIC_ARGS(msg, exceptionT, args) \
			CPL_INTERNAL_EXCEPTION(msg, \
			__FILE__, \
			__LINE__, \
			__func__, \
			false, \
			exceptionT, \
			CPL_EVAL(exceptionT,args) \
        )\

	/// <summary>
	/// Throws a suitable system_error from errno with a what() message
	/// </summary>
	#define CPL_POSIX_EXCEPTION(msg) CPL_RUNTIME_EXCEPTION_SPECIFIC_ARGS(msg, std::system_error, (errno, std::generic_category(), CPL_EXPANDED_MESSAGE))

	#ifdef CPL_WINDOWS
	#define CPL_SYSTEM_EXCEPTION(msg) CPL_RUNTIME_EXCEPTION_SPECIFIC_ARGS(msg, \
            std::system_error, (cpl::GetLastOSError(), std::system_category(), CPL_EXPANDED_MESSAGE))
	#else
	#define CPL_SYSTEM_EXCEPTION(msg) CPL_RUNTIME_EXCEPTION_SPECIFIC_ARGS(msg, \
            std::system_error, (errno, std::system_category(), CPL_EXPANDED_MESSAGE))
	#endif
	#define CPL_RUNTIME_ASSERTION(expression) \
			if(!(expression)) \
				CPL_INTERNAL_EXCEPTION("Runtime assertion failed: " #expression, \
				__FILE__, __LINE__, __func__, true, cpl::CPLAssertionException, cpl::CPLAssertionException (CPL_EXPANDED_MESSAGE))

	#define CPL_NOTIMPLEMENTED_EXCEPTION() \
			CPL_RUNTIME_EXCEPTION_SPECIFIC("The requested behaviour is not implemented (yet)", cpl::CPLNotImplementedException)

	#ifdef CPL_MSVC
		#define CPL_UNREACHABLE() __assume(0)
	#else
		#define CPL_UNREACHABLE() __builtin_unreachable()
	#endif

}
#endif
