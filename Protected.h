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

	file:Protected.h

		Defines a wrapper call that catches system-level exceptions through a platform
		independant interface.

	options:
		#define CPL_UNSAFE_USE_SIGACTION
		uses sigaction() instead of signal() on unix systems. Highly recommended

*************************************************************************************/

#ifndef CPL_PROTECTED_H
#define CPL_PROTECTED_H

#include "LibraryOptions.h"
#include "PlatformSpecific.h"
#include <vector>
#include <signal.h>
#include <thread>
#include <map>
#include <sstream>
#include "Utility.h"
#include <memory>
#include "Exceptions.h"
#include "Misc.h"

#define CPL_TRACEGUARD_START \
	cpl::CProtected::instance().topLevelTraceGuardedCode([&]() {

#define CPL_TRACEGUARD_STOP(traceGuardName) \
	}, traceGuardName)


namespace cpl
{

	class CProtected final : Utility::CNoncopyable
	{
		static constexpr int OSCustomRaiseCode = 0xBEEF;

		struct Throwable
		{
			virtual ~Throwable() {};
			virtual void throwImpl() = 0;
		};

		struct PendingException
		{
			bool isPending() const noexcept
			{
				return throwable.get() != nullptr;
			}

			void throwException()
			{
				if (!isPending())
					CPL_RUNTIME_EXCEPTION_SPECIFIC("not pending", std::logic_error);

				auto local = std::move(throwable);

				local->throwImpl();
			}
			
			void reset(std::unique_ptr<Throwable> newThrowable)
			{
				throwable = std::move(newThrowable);
			}

			void reset(Throwable* newThrowable)
			{
				throwable.reset(newThrowable);
			}

		private:

			std::unique_ptr<Throwable> throwable;
		};

		template<typename Exception>
		struct ThrowableException : std::enable_if<std::is_base_of<std::exception, Exception>::value, Throwable>::type
		{
			ThrowableException(Exception&& e) : storage(std::move(e)) {}

			void throwImpl() override
			{
				throw storage;
			}

			Exception storage;
		};

		#ifndef CPL_MSVC

		struct ScopedThreadSignalHandler
		{
			ScopedThreadSignalHandler()
			{
				struct sigaction handler {};

				handler.sa_sigaction = &CProtected::signalActionHandler;
				handler.sa_flags = SA_SIGINFO;
				sigemptyset(&handler.sa_mask);

				sigaction(SIGILL, &handler, &oldSigIll);
				sigaction(SIGSEGV, &handler, &oldSigSegv);
				sigaction(SIGFPE, &handler, &oldSigFPE);
				sigaction(SIGBUS, &handler, &oldSigBus);

			}

			~ScopedThreadSignalHandler()
			{
				sigaction(SIGILL, &oldSigIll, nullptr);
				sigaction(SIGSEGV, &oldSigSegv, nullptr);
				sigaction(SIGFPE, &oldSigFPE, nullptr);
				sigaction(SIGBUS, &oldSigBus, nullptr);
			}

			struct sigaction
				oldSigIll,
				oldSigSegv,
				oldSigFPE,
				oldSigBus;
		};

		#endif

	public:

		struct CSystemException;

		struct PreembeddedFormatter
		{
			PreembeddedFormatter(const char * prefixToEmbed)
				: prefix(prefixToEmbed)
			{

			}

			std::stringstream & get()
			{
				if (!hasBeenConstructed)
				{
					stream.reference() << "Handler: " << prefix << '\n';
					hasBeenConstructed = true;
				}
				return stream.reference();
			}

		private:
			const char * prefix;
			bool hasBeenConstructed = false;
			Utility::LazyStackPointer<std::stringstream> stream;
		};

		static std::string formatExceptionMessage(const CSystemException &);

		static CProtected & instance();

		/// <summary>
		/// calls lambda inside 'safe wrappers', catches OS errors and filters them.
		/// Throws CSystemException on errors, crashes on unrecoverable errors.
		///
		/// Does NOT catch software exceptions, but it IS C++ exception safe.
		/// Additionally, if you want to catch C++ exceptions across external
		/// code, you have to use this function (and Proctected::throwException{T}()).
		/// 
		/// Is NOT guaranteed to capture all system hardware exceptions/signals,
		/// in general only those that are synchronous.
		/// Guaranteed handled exceptions:
		///		segmentation violations, bus errors, floating point exceptions, illegal instructions.
		/// 
		/// This function is reentrant (with expected behaviour), and safe in multithreaded programs.
		///
		/// Note that the stack may NOT (until this point) be unwound,
		/// so consider your program to be in an UNDEFINED state; write some info to a file and crash gracefully
		/// afterwards!
		/// </summary>
		template <class func>
		void runProtectedCode(func && function)
		{
			auto oldThreadData = std::move(threadData);
			threadData.isInStack = true;

			auto scopeRelease = [&]()
			{
				threadData = std::move(oldThreadData);
			};

			Utility::OnScopeExit<decltype(scopeRelease)> releaser(
				scopeRelease
			);

			#ifdef CPL_MSVC
				[&]() {
					bool exception_caught = false;

					CSystemException::Storage exceptionData;

					__try {

						function();
					}
					__except (structuredExceptionHandler(GetExceptionCode(), exceptionData, GetExceptionInformation()))
					{
						// this is a way of leaving the SEH block before we throw a C++ software exception
						exception_caught = true;
					}
					if (exception_caught)
					{
						throw CSystemException(exceptionData);
					}

				} ();
			#else

				ScopedThreadSignalHandler h;
				// set the jump in case a signal gets raised
				if (sigsetjmp(threadData.threadJumpBuffer, 1))
				{
					if (threadData.pendingException.isPending())
						threadData.pendingException.throwException();

					/*
						return from exception handler.
						current exception is in CState::currentException
					*/
					throw CSystemException(threadData.currentExceptionData);
				}
				// run the potentially bad code
				function();

			#endif
		}

		template <class func>
		auto topLevelTraceGuardedCode(
			func && function,
			const char * levelDescription = "Top-level exception/signal handler")
			-> decltype(function())
		{

			PreembeddedFormatter debugOutput(levelDescription);
			auto oldThreadData = std::move(threadData);
			threadData.isInStack = true;
			threadData.traceIntercept = true;
			threadData.propagate = true;
			threadData.debugTraceBuffer = &debugOutput;

			auto scopeRelease = [&]()
			{
				threadData = std::move(oldThreadData);
			};

			Utility::OnScopeExit<decltype(scopeRelease)> releaser(
				scopeRelease
			);

			// in this frame we catch signals and SEH exceptions.
			#ifdef CPL_MSVC
			return internalSEHTraceInterceptor(debugOutput, function);
			#else
			// async signals will be captured further up
			return internalSignalTraceInterceptor(debugOutput, function);
			#endif
		}

		template <class func>
		static void runProtectedCodeErrorHandling(func && function)
		{
			try
			{
				CProtected::instance().runProtectedCode
				(
					[&]()
					{
						function();
					}
				);
			}
			catch (CProtected::CSystemException & cs)
			{
				auto error = CProtected::formatExceptionMessage(cs);
				LogException(error);
				CrashIfUserDoesntDebug(error);
				cs.reraise();
			}
			catch (std::exception & e)
			{
				auto error = e.what();
				LogException(error);
				CrashIfUserDoesntDebug(error);
				throw;
			}

		}

		/*
			Base exception for all critical exceptions thrown in this program.
			Derives from std::exception, but has no meaningful .what()
			- See Protected::formatExceptionMessage
		*/
		struct CSystemException : public std::exception
		{
		public:
			// this is kinda ugly, but needed to create a simple interface, abstract to seh and signals

			// the retarted create() pattern is used because clang STILL doesn't support thread_local,
			// thus we must use __thread, which DOESN'T support non-trivial destruction (implied by use of constructors).
			// TODO: fix the upper messages.
			struct Storage
			{
				const void * faultAddr; // the address the exception occured
				const void * attemptedAddr; // if exception is a memory violation, this is the attempted address
				XWORD exceptCode; // the exception code
				int extraInfoCode; // addition, exception-specific code
				int actualCode; // what signal it was

				union {
					// hack hack union
					bool safeToContinue; // exception not critical or can be handled
					bool aVInProtectedMemory; // whether an access violation happened in our protected memory
				};

				static Storage create(XWORD exp, bool resolved = true, const void * faultAddress = nullptr,
					const void * attemptedAddress = nullptr, int extraCode = 0, int actualCode = 0)
				{
					return {faultAddress, attemptedAddress, exp, extraCode, actualCode, resolved};
				}

				static Storage create()
				{
					return {};
				}
			} data;

			enum status : XWORD {
				nullptr_from_plugin = 1,
				#ifdef CPL_WINDOWS
					// this is not good: these are not crossplatform constants.
					access_violation = EXCEPTION_ACCESS_VIOLATION,
					undefined_behaviour = EXCEPTION_ILLEGAL_INSTRUCTION,
					intdiv_zero = EXCEPTION_INT_DIVIDE_BY_ZERO,
					fdiv_zero = EXCEPTION_FLT_DIVIDE_BY_ZERO,
					finvalid = EXCEPTION_FLT_INVALID_OPERATION,
					fdenormal = EXCEPTION_FLT_DENORMAL_OPERAND,
					finexact = EXCEPTION_FLT_INEXACT_RESULT,
					foverflow = EXCEPTION_FLT_OVERFLOW,
					funderflow = EXCEPTION_FLT_UNDERFLOW
				#elif defined(CPL_MAC) || defined(CPL_UNIXC)
					access_violation = SIGSEGV,
					intdiv_zero,
					fdiv_zero,
					finvalid,
					fdenormal,
					finexact,
					foverflow,
					funderflow,
					intsubscript,
					intoverflow,
					undefined_behaviour

				#endif
			};

			CSystemException()
				: data(Storage::create())
			{

			}

			CSystemException(const Storage & eData)
			{
				data = eData;
			}

			void reraise() const
			{
				#ifdef CPL_WINDOWS
				RaiseException(static_cast<DWORD>(data.exceptCode), 0, 0, nullptr);
				#else
				raise(static_cast<int>(data.exceptCode));
				#endif
			}

			CSystemException(XWORD exp, bool resolved = true, const void * faultAddress = nullptr, const void * attemptedAddress = nullptr, int extraCode = 0, int actualCode = 0)
			{
				data = Storage::create(exp, resolved, faultAddress, attemptedAddress, extraCode, actualCode);
			}

			const char * what() const noexcept override
			{
				return "OS specific hardware exception";
			}
		};

		template<typename Exception, typename... Args>
		void throwException(Args&&... args)
		{
#ifndef CPL_MSVC
			if (!threadData.isInStack)
				throw Exception(args...);
			else
			{
				threadData.pendingException.reset(new ThrowableException<Exception>(Exception(args...)));
					siglongjmp(threadData.threadJumpBuffer, OSCustomRaiseCode);
			}
#else
			throw Exception(args...);
#endif
		}

		~CProtected();

	protected:

		CProtected();

	private:

		template <class func>
		auto internalCxxTraceInterceptor(PreembeddedFormatter & out, func && function)
			-> decltype(function())
		{
			// in this frame, we catch C++ software exceptions
			try
			{
				return function();
			}
			catch (CProtected::CSystemException & cs)
			{
				out.get() << "Hardware -> " << CProtected::formatExceptionMessage(cs) << '\n';
				out.get() << "what() -> " << cs.what() << '\n';
				LogException(out.get().str());
				CrashIfUserDoesntDebug(out.get().str());
				throw;
			}
			catch (std::exception & e)
			{
				out.get() << "Software -> " << e.what() << '\n';
				LogException(out.get().str());
				CrashIfUserDoesntDebug(out.get().str());
				throw;
			}
			catch (...)
			{
				out.get() << "Unknown software exception";
				LogException(out.get().str());
				CrashIfUserDoesntDebug(out.get().str());
				throw;
			}

		}

		template <class func>
		auto internalSEHTraceInterceptor(PreembeddedFormatter & out, func && function)
			-> decltype(function())
		{

			#ifdef CPL_MSVC
			CSystemException::Storage exceptionInformation;
			__try
			{
				return internalCxxTraceInterceptor(out, function);
			}
			__except (structuredExceptionHandlerTraceInterceptor(
				out,
				GetExceptionCode(),
				exceptionInformation,
				GetExceptionInformation())
				)
			{
				std::terminate();
			}
			#endif
		}

		template <class func>
		auto internalSignalTraceInterceptor(PreembeddedFormatter & out, func && function)
			-> decltype(function())
		{

			#ifndef CPL_MSVC
			ScopedThreadSignalHandler h;
			// set the jump in case a signal gets raised
			if (sigsetjmp(threadData.threadJumpBuffer, 1))
			{
				/*
				return from exception handler.
				current exception is in CState::currentException
				*/
				throw CSystemException(threadData.currentExceptionData);
			}

			return internalCxxTraceInterceptor(out, function);

			#endif
		}

		struct ThreadData
		{
			/// <summary>
			/// Thread local set on entry and exit of protected code stack frames.
			/// </summary>
			bool isInStack;
			/// <summary>
			/// If set, exception will be propagated further in the handler chain
			/// </summary>
			bool propagate;
			/// <summary>
			/// If set, logs debug output, presents a message box with debugging abilities
			/// </summary>
			bool traceIntercept;

			/// <summary>
			/// A pre-allocated buffer that can be used for scratch space
			/// </summary>
			PreembeddedFormatter * debugTraceBuffer;

			#ifndef CPL_MSVC
				sigjmp_buf threadJumpBuffer;
				CSystemException::Storage currentExceptionData;
				PendingException pendingException;
			#endif
			unsigned fpuMask;
		};

		static CPL_THREAD_LOCAL ThreadData threadData;

		XWORD structuredExceptionHandler(XWORD _code, CSystemException::Storage & e, void * _systemInformation);
		XWORD structuredExceptionHandlerTraceInterceptor(PreembeddedFormatter & outputStream, XWORD _code, CSystemException::Storage & e, void * _systemInformation);

		static void signalTraceInterceptor(CSystemException::Storage & e);
		static void signalHandler(int some_number);
		static void signalActionHandler(int signal, siginfo_t * siginfo, void * extraData);
	};
};
#endif
