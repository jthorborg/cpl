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

	file:Protected.cpp

		Implementation of Protected.h

*************************************************************************************/

#include "Protected.h"
#include "PlatformSpecific.h"
#include "lib/StackBuffer.h"
#include <memory>
#include <sstream>
#ifdef CPL_MSVC
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#elif defined(CPL_UNIXC)
#include <execinfo.h>
#include <cxxabi.h>
#endif

namespace cpl
{
	CPL_THREAD_LOCAL CProtected::ThreadData CProtected::threadData;

	CProtected & CProtected::instance()
	{
		static CProtected internalInstance;
		return internalInstance;
	}

	CProtected::CProtected()
	{

	}

	CProtected::~CProtected()
	{

	}

	/// <summary>
	/// Includes 0x
	/// </summary>
	static std::string formatDifferenceAddress(const void * base, const void * site)
	{
		char sign = '+';

		const void * deltaAddress = nullptr;

		if (site > base)
		{
			sign = '+';
			deltaAddress = (const void*)((const char*)site - (const char *)base);
		}
		else
		{
			sign = '-';
			deltaAddress = (const void*)((const char *)base - (const char*)site);
		}

		char buf[100];

		sprintfs(buf, "0x%p %c 0x%p", base, sign, deltaAddress);

		return buf;
	}

#ifndef CPL_WINDOWS
	XWORD StatusCodeToXWORD(CProtected::CSystemException::Status code)
	{
		return (XWORD)code;
	}
#else
	XWORD StatusCodeToXWORD(CProtected::CSystemException::Status code)
	{
		switch (code)
		{
		case CProtected::CSystemException::Status::access_violation: return EXCEPTION_ACCESS_VIOLATION;
		case CProtected::CSystemException::Status::intdiv_zero: return EXCEPTION_INT_DIVIDE_BY_ZERO;
		case CProtected::CSystemException::Status::fdiv_zero: return EXCEPTION_FLT_DIVIDE_BY_ZERO;
		case CProtected::CSystemException::Status::finvalid: return EXCEPTION_FLT_INVALID_OPERATION;
		case CProtected::CSystemException::Status::fdenormal: return EXCEPTION_FLT_DENORMAL_OPERAND;
		case CProtected::CSystemException::Status::finexact: return EXCEPTION_FLT_INEXACT_RESULT;
		case CProtected::CSystemException::Status::foverflow: return EXCEPTION_FLT_OVERFLOW;
		case CProtected::CSystemException::Status::funderflow: return EXCEPTION_FLT_UNDERFLOW;
		case CProtected::CSystemException::Status::intoverflow: return EXCEPTION_INT_OVERFLOW;
		case CProtected::CSystemException::Status::undefined_behaviour: return EXCEPTION_ILLEGAL_INSTRUCTION;
		}

		CPL_UNREACHABLE();

		return -1;
	}

	CProtected::CSystemException::Status XWORDExceptionToStatusCode(XWORD code)
	{
		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION: return CProtected::CSystemException::Status::access_violation;
		case EXCEPTION_INT_DIVIDE_BY_ZERO: return CProtected::CSystemException::Status::intdiv_zero;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO: return CProtected::CSystemException::Status::fdiv_zero;
		case EXCEPTION_FLT_INVALID_OPERATION: return CProtected::CSystemException::Status::finvalid;
		case EXCEPTION_FLT_DENORMAL_OPERAND: return CProtected::CSystemException::Status::fdenormal;
		case EXCEPTION_FLT_INEXACT_RESULT: return CProtected::CSystemException::Status::finexact;
		case EXCEPTION_FLT_OVERFLOW: return CProtected::CSystemException::Status::foverflow;
		case EXCEPTION_FLT_UNDERFLOW: return CProtected::CSystemException::Status::funderflow;
		case EXCEPTION_INT_OVERFLOW: return CProtected::CSystemException::Status::intoverflow;
		case EXCEPTION_ILLEGAL_INSTRUCTION: return CProtected::CSystemException::Status::undefined_behaviour;
		}

		CPL_UNREACHABLE();

		return CProtected::CSystemException::Status::access_violation;
	}

#endif

	/*********************************************************************************************

		Formats a string and returns it. It explains what went wrong.

	 *********************************************************************************************/
	std::string CProtected::formatExceptionMessage(const CSystemException & e)
	{
		auto imageBase = (const void *)Misc::GetImageBase();
		std::stringstream base;

		base << "Non-software exception at 0x" << std::hex << e.data.faultAddr
			<< " (at image base " + formatDifferenceAddress(imageBase, e.data.faultAddr) + ")" << newl;

		base << "Exception code: " << StatusCodeToXWORD(e.data.exceptCode)
			<< ", actual code: " << e.data.actualCode
			<< ", extra info: " << e.data.extraInfoCode << newl;

		base << "Formatted message: ";

		switch (e.data.exceptCode)
		{
			case CSystemException::Status::intdiv_zero:
				return base.str() + "An integral division-by-zero was performed";
			case CSystemException::Status::funderflow:
				return base.str() + "A floating point operation resulted in underflow";
			case CSystemException::Status::foverflow:
				return base.str() + "A floating point operation resulted in overflow";
			case CSystemException::Status::finexact:
				return base.str() + "A floating point operation's result cannot be accurately expressed";
			case CSystemException::Status::finvalid:
				return base.str() + "One of the operands for a floating point operation was invalid (typically negative numbers for sqrt, exp, log)";
			case CSystemException::Status::fdiv_zero:
				return base.str() + "A floating point division-by-zero was performed";
			case CSystemException::Status::fdenormal:
				return base.str() + "One of the operands for a floating point operation was denormal (too small to be represented)";
			case CSystemException::Status::nullptr_from_plugin:
				return base.str() + "An API function was called with 'this' as an null pointer.";
			case CSystemException::Status::undefined_behaviour:
				return base.str() + "Undefined behaviour was executed (unreachable code inserted by compiler)";
			case CSystemException::Status::access_violation:
			{
				std::stringstream fmt;
				#ifndef CPL_MSVC
				switch (e.data.actualCode)
				{
					case SIGSEGV:
					{
						fmt << "Segmentation fault ";
						switch (e.data.extraInfoCode)
						{
							case SEGV_ACCERR:
								fmt << "(invalid permission for object) ";
								break;
							case SEGV_MAPERR:
								fmt << "(address not mapped for object) ";
								break;
						}
						break;
					}
					case SIGBUS:
					{
						fmt << "Bus error ";
						switch (e.data.extraInfoCode)
						{
							case BUS_ADRALN:
								fmt << "(invalid address alignment) ";
								break;
							case BUS_ADRERR:
								fmt << "(non-existant address) ";
								break;
							case BUS_OBJERR:
								fmt << "(object hardware error) ";
						}
						break;

					}

					case SIGILL:
					{
						fmt << "Illegal instruction ";
						switch (e.data.extraInfoCode)
						{

							case ILL_ILLOPC:
								fmt << "(Illegal opcode) "; break;
							case ILL_ILLOPN:
								fmt << "(Illegal operand) "; break;
							case ILL_ILLADR:
								fmt << "(Illegal addressing mode) "; break;
							case ILL_ILLTRP:
								fmt << "(Illegal trap) "; break;
							case ILL_PRVOPC:
								fmt << "(Privileged opcode) "; break;
							case ILL_PRVREG:
								fmt << "(Privileged register) "; break;
							case ILL_COPROC:
								fmt << "(Coprocessor error) "; break;
							case ILL_BADSTK:
								fmt << "(Internal stack error) "; break;
						}
						break;

					}

					default:
						fmt << "Access violation ";
						break;
				}
				#else
				fmt << "Access violation ";

				switch (e.data.actualCode)
				{
					case 0:
						fmt << "reading ";
						break;
					case 1:
						fmt << "writing ";
						break;
					case 8:
						fmt << "executing ";
						break;
					default:
						fmt << "(unknown error?) at ";
						break;
				};


				#endif
				fmt << "address " << std::hex << e.data.attemptedAddr << ".";
				return base.str() + fmt.str();
			}
			default:
				return base.str() + " Unknown exception (BAD!).";
		};
	}

	/*********************************************************************************************

		Structured exception handler for windows

	 *********************************************************************************************/
	XWORD CProtected::structuredExceptionHandler(XWORD _code, CSystemException::Storage & e, void * systemInformation)
	{
		//CPL_BREAKIFDEBUGGED();

		#ifdef CPL_WINDOWS
		auto exceptCode = _code;
		bool safeToContinue(false);
		const void * exceptionAddress = nullptr;
		int additionalCode = 0;
		EXCEPTION_POINTERS * exp = reinterpret_cast<EXCEPTION_POINTERS *>(systemInformation);
		if (exp)
			exceptionAddress = exp->ExceptionRecord->ExceptionAddress;
		switch (_code)
		{
			case EXCEPTION_ILLEGAL_INSTRUCTION:
			{
				if (exceptionAddress)
				{
					const std::uint16_t* assembly = static_cast<const std::uint16_t*>(exceptionAddress);

					if (*assembly == 0x0B0F) // ud2 - #UD undefined behaviour, compiler trigger.
					{
						e = CSystemException::Storage::create(XWORDExceptionToStatusCode(_code), false, exceptionAddress, nullptr, additionalCode);
						return EXCEPTION_EXECUTE_HANDLER;
					}
				}
				break;
			}			
			case EXCEPTION_ACCESS_VIOLATION:
			{
				std::ptrdiff_t addr = 0; // nullptr invalid here?

				if (exp)
				{
					// the address that was attempted
					addr = exp->ExceptionRecord->ExceptionInformation[1];
					// 0 = read violation, 1 = write violation, 8 = dep violation
					additionalCode = static_cast<int>(exp->ExceptionRecord->ExceptionInformation[0]);
				}

				e = CSystemException::Storage::create(XWORDExceptionToStatusCode(_code), safeToContinue, exceptionAddress, (const void *)addr, additionalCode);

				return EXCEPTION_EXECUTE_HANDLER;
			}
			/*
				trap all math errors:
			*/
			case EXCEPTION_INT_DIVIDE_BY_ZERO:
			case EXCEPTION_FLT_UNDERFLOW:
			case EXCEPTION_FLT_OVERFLOW:
			case EXCEPTION_FLT_INEXACT_RESULT:
			case EXCEPTION_FLT_INVALID_OPERATION:
			case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			case EXCEPTION_FLT_DENORMAL_OPERAND:
				_clearfp();
				safeToContinue = true;
				e = CSystemException::Storage::create(XWORDExceptionToStatusCode(_code), safeToContinue, exceptionAddress);

				return EXCEPTION_EXECUTE_HANDLER;

			default:
				return EXCEPTION_CONTINUE_SEARCH;
		};
		return EXCEPTION_CONTINUE_SEARCH;

		#endif
		return 0;
	}

	#ifdef CPL_WINDOWS
	void WindowsBackTrace(std::ostream & f, PEXCEPTION_POINTERS pExceptionInfo)
	{
		// http://stackoverflow.com/questions/28099965/windows-c-how-can-i-get-a-useful-stack-trace-from-a-signal-handler
		HANDLE process = GetCurrentProcess();
		SymInitialize(process, Misc::GetDirectoryPath().c_str(), TRUE);

		// StackWalk64() may modify context record passed to it, so we will
		// use a copy.
		CONTEXT context_record = *pExceptionInfo->ContextRecord;
		// Initialize stack walking.
		STACKFRAME64 stack_frame;
		memset(&stack_frame, 0, sizeof(stack_frame));
		#if defined(_WIN64)
		int machine_type = IMAGE_FILE_MACHINE_AMD64;
		stack_frame.AddrPC.Offset = context_record.Rip;
		stack_frame.AddrFrame.Offset = context_record.Rbp;
		stack_frame.AddrStack.Offset = context_record.Rsp;
		#else
		int machine_type = IMAGE_FILE_MACHINE_I386;
		stack_frame.AddrPC.Offset = context_record.Eip;
		stack_frame.AddrFrame.Offset = context_record.Ebp;
		stack_frame.AddrStack.Offset = context_record.Esp;
		#endif
		stack_frame.AddrPC.Mode = AddrModeFlat;
		stack_frame.AddrFrame.Mode = AddrModeFlat;
		stack_frame.AddrStack.Mode = AddrModeFlat;

		StackBuffer<SYMBOL_INFO, 256> symbol;
		symbol.zero();
		symbol->MaxNameLen = 255;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

		f << std::hex;

		auto imageBase = Misc::GetImageBase();

		while (StackWalk64(machine_type,
			GetCurrentProcess(),
			GetCurrentThread(),
			&stack_frame,
			&context_record,
			NULL,
			&SymFunctionTableAccess64,
			&SymGetModuleBase64,
			NULL)) {

			DWORD64 displacement = 0;

			if (SymFromAddr(process, (DWORD64)stack_frame.AddrPC.Offset, &displacement, &symbol.get()))
			{
				IMAGEHLP_MODULE64 moduleInfo;
				std::memset(&moduleInfo, 0, sizeof(moduleInfo));
				moduleInfo.SizeOfStruct = sizeof(moduleInfo);

				if (::SymGetModuleInfo64(process, symbol->ModBase, &moduleInfo))
					f << moduleInfo.ModuleName << ": ";

				f << formatDifferenceAddress(imageBase, (const void*)symbol->Address) << " + 0x" << displacement << " | " << symbol->Name << newl;
			}
			else
			{
				// no symbols loaded.
				f << formatDifferenceAddress(imageBase, (const void *)stack_frame.AddrPC.Offset) << newl;
			}
		}

		SymCleanup(GetCurrentProcess());

	}
	#endif

	void CProtected::CSystemException::reraise() const
	{
#ifdef CPL_WINDOWS
		RaiseException(static_cast<DWORD>(data.exceptCode), 0, 0, nullptr);
#else
		raise(static_cast<int>(data.exceptCode));
#endif
	}

	XWORD CProtected::structuredExceptionHandlerTraceInterceptor(CProtected::PreembeddedFormatter & output, XWORD code, CSystemException::Storage & e, void * systemInformation)
	{

		XWORD ret = 0;
		#ifdef CPL_WINDOWS
		auto & outputStream = output.get();
		CPL_BREAKIFDEBUGGED();
		CSystemException::Storage exceptionInformation;
		// ignore return and basically use it to fill in the exception information
		structuredExceptionHandler(code, exceptionInformation, systemInformation);

		auto exceptionString = formatExceptionMessage(exceptionInformation);
		CrashIfUserDoesntDebug(exceptionString);
		outputStream << "- SEH exception description: " << newl << exceptionString << newl;
		outputStream << "- Stack backtrace: " << newl;
		WindowsBackTrace(outputStream, (PEXCEPTION_POINTERS)systemInformation);
		ret = EXCEPTION_CONTINUE_SEARCH;

		LogException(outputStream.str());


		#endif

		return ret;
	}

	void CProtected::signalTraceInterceptor(CSystemException::Storage & exceptionInformation)
	{
		CPL_BREAKIFDEBUGGED();
		#ifdef CPL_UNIXC
		if (threadData.debugTraceBuffer == nullptr)
			return;

		std::stringstream & outputStream = threadData.debugTraceBuffer->get();

		auto exceptionString = formatExceptionMessage(exceptionInformation);

		outputStream << "Sigaction exception description: " << exceptionString << newl;

		void *callstack[128];
		const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
		char buf[1024];
		int nFrames = backtrace(callstack, nMaxFrames);
		char **symbols = backtrace_symbols(callstack, nFrames);

		for (int i = 0; i < nFrames; i++) {
			Dl_info info;
			if (dladdr(callstack[i], &info) && info.dli_sname) {
				char *demangled = NULL;
				int status = -1;
				if (info.dli_sname[0] == '_')
					demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
				snprintf(buf, sizeof(buf), "%-3d %*p %s + %zd\n",
					i, int(2 + sizeof(void*) * 2), callstack[i],
					status == 0 ? demangled :
					info.dli_sname == 0 ? symbols[i] : info.dli_sname,
					(char *)callstack[i] - (char *)info.dli_saddr);
				free(demangled);
			}
			else {
				snprintf(buf, sizeof(buf), "%-3d %*p %s\n",
					i, int(2 + sizeof(void*) * 2), callstack[i], symbols[i]);
			}
			outputStream << buf;
		}
		free(symbols);

		LogException(outputStream.str());
		CrashIfUserDoesntDebug(exceptionString);
		#endif
	}

	void CProtected::signalHandler(int some_number)
	{
		throw (XWORD)some_number;
	}

	void CProtected::signalActionHandler(int sig, siginfo_t * siginfo, void * extra)
	{
		#ifndef CPL_WINDOWS
		/*
			Firstly, check if the exception occured at our stack, after runProtectedCode
			which sets activeStateObject to a valid object
		*/
		if (threadData.isInStack)
		{
			/*
				handle the exception here.
			*/

			const void * fault_address = siginfo ? siginfo->si_addr : nullptr;
			auto ecode = siginfo->si_code;
			bool safeToContinue = false;

			// -- this should be handled by siglongjmp
			//sigemptyset (&newAction.sa_mask);
			//sigaddset(&newAction.sa_mask, sig);
			//sigprocmask(SIG_UNBLOCK, &newAction.sa_mask, NULL);

			switch (sig)
			{
				case SIGILL:
                {
                    if (fault_address)
                    {
                        const std::uint16_t* assembly = static_cast<const std::uint16_t*>(fault_address);
                        
                        if (*assembly == 0x0B0F) // ud2 - #UD undefined behaviour, compiler trigger.
                        {
                            threadData.currentExceptionData = CSystemException::Storage::create(
                                CSystemException::Status::undefined_behaviour,
                                safeToContinue,
                                nullptr,
                                fault_address,
                                ecode,
                                sig
                            );

                            if (threadData.traceIntercept)
                                signalTraceInterceptor(threadData.currentExceptionData);

                            // jump back to CState::runProtectedCode. Note, we know that function was called
                            // earlier in the stackframe, because threadData.activeStateObject is non-null
                            // : that field is __only__ set in runProtectedCode. Therefore, the threadJumpBuffer
                            // IS valid.

                            if (!threadData.propagate)
                                siglongjmp(threadData.threadJumpBuffer, 1);
                            break;
                        }
                    }
                }
				case SIGBUS:
				case SIGSEGV:
				{

					threadData.currentExceptionData = CSystemException::Storage::create(
						CSystemException::Status::access_violation,
						safeToContinue,
						nullptr,
						fault_address,
						ecode,
						sig
					);

					if (threadData.traceIntercept)
						signalTraceInterceptor(threadData.currentExceptionData);

					// jump back to CState::runProtectedCode. Note, we know that function was called
					// earlier in the stackframe, because threadData.activeStateObject is non-null
					// : that field is __only__ set in runProtectedCode. Therefore, the threadJumpBuffer
					// IS valid.

					if (!threadData.propagate)
						siglongjmp(threadData.threadJumpBuffer, 1);
					break;
				}
				case SIGFPE:
				{
					// exceptions that happened are still set in the status flags - always clear these,
					// or the exception might throw again
					std::feclearexcept(FE_ALL_EXCEPT);
					CSystemException::Status code_status;
					switch (ecode)
					{
						case FPE_FLTDIV:
							code_status = CSystemException::Status::fdiv_zero;
							break;
						case FPE_FLTOVF:
							code_status = CSystemException::Status::foverflow;
							break;
						case FPE_FLTUND:
							code_status = CSystemException::Status::funderflow;
							break;
						case FPE_FLTRES:
							code_status = CSystemException::Status::finexact;
							break;
						case FPE_FLTINV:
							code_status = CSystemException::Status::finvalid;
							break;
						case FPE_FLTSUB:
							code_status = CSystemException::Status::intsubscript;
							break;
						case FPE_INTDIV:
							code_status = CSystemException::Status::intdiv_zero;
							break;
						case FPE_INTOVF:
							code_status = CSystemException::Status::intoverflow;
							break;
					}

					safeToContinue = true;

					threadData.currentExceptionData = CSystemException::Storage::create(
						code_status,
						safeToContinue,
						fault_address
					);

					if (threadData.traceIntercept)
						signalTraceInterceptor(threadData.currentExceptionData);

					// jump back to CState::runProtectedCode. Note, we know that function was called
					// earlier in the stackframe, because threadData.activeStateObject is non-null
					// : that field is __only__ set in runProtectedCode. Therefore, the threadJumpBuffer
					// IS valid.
					if (!threadData.propagate)
						siglongjmp(threadData.threadJumpBuffer, 1);
					break;
				}
				default:
					goto default_handler;
			} // switch signal
		} // if threadData.activeStateObject

		/*
			Exception happened in some arbitrary place we have no knowledge off, or we are
			propagating the exception.
			First we try to call the old signal handlers
		*/
	default_handler:
		// no handler found, throw exception (that will call terminate)
	die_brutally:
		CPL_RUNTIME_EXCEPTION(programInfo.name + " - CProtected:signalActionHandler called for unregistrered signal; no appropriate signal handler to call.");
		#endif
	}

}
