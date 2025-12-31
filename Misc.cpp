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

	file:Misc.cpp

		Implementation of Misc.h

*************************************************************************************/

#include "MacroConstants.h"
#include "Misc.h"
#include <ctime>
#include "PlatformSpecific.h"
#include "CThread.h"
#include <cstdarg>
#include "Types.h"
#include "stdext.h"
#include <atomic>
#include "CExclusiveFile.h"
#include <typeinfo>
#include <future>
#include "Exceptions.h"
#include "Core.h"
#include <queue>
#include "filesystem.h"

#ifdef __GNUG__
#include <cstdlib>
#include <memory>
#include <cxxabi.h>
#endif

namespace cpl
{
	namespace Misc
	{

		std::mutex idMutex;
		std::queue<int> freeIDList;
		int instanceCounter;

		int addHandlers();

		static std::string GetDirectoryPath();
		static int __unusedInitialization = addHandlers();
		static std::atomic<std::terminate_handler> oldTerminate;

		static std::thread::id MainThreadID = std::this_thread::get_id();

		#ifdef __GNUG__

		std::string DemangleRawName(const string_ref name) {
			//http://stackoverflow.com/questions/281818/unmangling-the-result-of-stdtype-infoname
			int status = -4; // some arbitrary value to eliminate the compiler warning

			// enable c++11 by passing the flag -std=c++11 to g++
			std::unique_ptr<char, void(*)(void*)> res {
				abi::__cxa_demangle(name.c_str(), NULL, NULL, &status),
				std::free
			};

            return (status == 0) ? res.get() : name.c_str();
		}

		#else

		std::string DemangleRawName(const string_ref name) 
		{
			return name.string();
		}

		#endif

		void __cdecl _purescall(void)
		{
			auto except = "Pure virtual function called. This is a programming error, usually happening if a freed object is used again, "
				"or calling virtual functions inside destructors/constructors.";
			LogException(except);
			cpl::CrashIfUserDoesntDebug(except);
		}

		void terminateHook()
		{
			// see if we terminated because of an exception
			if (std::current_exception())
			{
				try
				{
					// as there is an exception, retrow it
					throw;
				}
				catch (const std::exception & e)
				{
					auto what = std::string("Software exception at std::terminate_hook: ") + e.what();
					LogException(what);
					MsgBox(what, cpl::programInfo.name + ": Software exception", MsgStyle::sOk | MsgIcon::iStop);
				}
			}
			auto oldHandler = oldTerminate.load();
			if (oldHandler)
				oldHandler();
			else
				std::abort();
		}

		int addHandlers()
		{
			static bool hasBeenAdded = false;
			if (!hasBeenAdded)
			{
				oldTerminate.store(std::set_terminate(terminateHook));
				#ifdef CPL_MSVC
				hasBeenAdded = true;
				_set_purecall_handler(_purescall);
				#ifdef _DEBUG
				_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
				#endif
				#endif

			}
			return 0;
		}

		std::pair<int, std::string> ExecCommand(const string_ref cmd)
		{
			// http://stackoverflow.com/a/478960/1287254
			char buffer[1024];
			std::string result;

			auto PipeOpen = [](const string_ref cmd)
			{
				#ifdef CPL_WINDOWS
				return ::_popen(cmd.c_str(), "r");
				#else
				return ::popen(cmd.c_str(), "r");
				#endif
			};

			auto PipeClose = [](const auto & file)
			{
				#ifdef CPL_WINDOWS
				return ::_pclose(file);
				#else
				return ::pclose(file);
				#endif
			};

			auto pipe = PipeOpen(cmd);

			if (!pipe)
				CPL_RUNTIME_EXCEPTION(format("Error executing commandline: \"%s\"", cmd.c_str()));


			while (!std::feof(pipe))
			{
				if (std::fgets(buffer, sizeof(buffer), pipe) != NULL)
					result += buffer;
			}

			return { PipeClose(pipe), result };
		}

		std::pair<bool, std::string> ReadFile(const fs::path& path) noexcept
		{
			char buffer[1024];
			std::string result;
			std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path.string().c_str(), "r"), &std::fclose);

			if (!file)
				return { false, result };

			if(std::fseek(file.get(), 0, SEEK_END))
				return { false, result };

			result.reserve(std::ftell(file.get()));

			std::rewind(file.get());

			while (!std::feof(file.get()))
			{
				if (std::fgets(buffer, sizeof(buffer), file.get()) != NULL)
					result += buffer;
			}

			return { true, result };
		}

		bool WriteFile(const fs::path& path, const string_ref contents) noexcept
		{
			std::unique_ptr<FILE, decltype(&std::fclose)> file(std::fopen(path.string().c_str(), "w"), &std::fclose);

			if (!file)
				return false;

			return std::fwrite(contents.c_str(), contents.size() + 1, 1, file.get()) == 1;
		}

		/*********************************************************************************************

			Delays the execution for at least msecs. Should have good precision bar context-switches,
			may spin.

		*********************************************************************************************/
		void PreciseDelay(double msecs)
		{
			#ifdef CPL_JUCE
			auto start = juce::Time::getHighResolutionTicks();
			double factor = 1.0 / juce::Time::getHighResolutionTicksPerSecond();
			msecs /= 1000;
			while ((juce::Time::getHighResolutionTicks() - start) * factor < msecs)
				;
			#else
			auto start = std::chrono::high_resolution_clock::now();
			while (true)
			{
				auto now = std::chrono::high_resolution_clock::now();
				auto elapsed = now - start;
				if (elapsed.count() / 1.0e8 > msecs)
				{
					break;
				}
			}
			#endif
		}

		/*********************************************************************************************

			Returns the path of our directory.
			For macs, this is <pathtobundle>/contents/resources/
			for windows, this is the directory of the DLL.

		 *********************************************************************************************/
		const std::string & DirectoryPath()
		{
			static std::string dirPath = GetDirectoryPath();
			return dirPath;
		}

		const fs::path& DirFSPath()
		{
			static fs::path path = GetDirectoryPath();
			return path;
		}

		void TextEditFile(const std::string& path)
		{
#ifdef CPL_WINDOWS
			std::system(("Notepad.exe \"" + path + "\"").c_str());
#elif defined(CPL_MAC)
			std::string cmdLine = "open \"" + path + "\"";
			std::system((cmdLine).c_str());
#else
			std::string cmdLine = "gedit \"" + path + "\"";
			std::system((cmdLine).c_str());
#endif
		}

		std::int32_t AcquireInstanceCounter()
		{
			std::unique_lock<std::mutex> lockGuard(idMutex);
			if (freeIDList.size() > 0)
			{
				auto top = freeIDList.front();
				freeIDList.pop();
				return top;
			}

			return instanceCounter++;
		}

		int AcquireUniqueInstanceID()
		{
			std::int32_t pID;
			#if defined(CPL_WINDOWS)
				pID = static_cast<std::int32_t>(GetProcessId(GetCurrentProcess()));
			#elif defined(CPL_MAC) || defined(CPL_UNIXC)
				pID = static_cast<std::int32_t>(getpid());
			#endif
			if (instanceCounter > std::numeric_limits<unsigned char>::max())
				MsgBox("Warning: You currently have had more than 256 instances open, this may cause a wrap around in instance-id's", programInfo.name, iInfo | sOk, nullptr, true);
			std::int32_t id = ((pID << 8) & 0xFFFFFF00) | AcquireInstanceCounter();
			return id;
		}

		void ReleaseUniqueInstanceID(std::int32_t id)
		{
			std::unique_lock<std::mutex> lockGuard(idMutex);
			freeIDList.push(id);
		}

		/*********************************************************************************************

			Checks whether we are being debugged.

		 *********************************************************************************************/
		bool IsBeingDebugged()
		{
			#ifdef CPL_WINDOWS
			return IsDebuggerPresent() ? true : false;
			#elif defined(CPL_MAC)
			int                 junk;
			int                 mib[4];
			struct kinfo_proc   info;
			size_t              size;

			// Initialize the flags so that, if sysctl fails for some bizarre
			// reason, we get a predictable result.

			info.kp_proc.p_flag = 0;

			// Initialize mib, which tells sysctl the info we want, in this case
			// we're looking for information about a specific process ID.

			mib[0] = CTL_KERN;
			mib[1] = KERN_PROC;
			mib[2] = KERN_PROC_PID;
			mib[3] = getpid();

			// Call sysctl.

			size = sizeof(info);
			junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);


			// We're being debugged if the P_TRACED flag is set.
			if (junk == 0)
				return ((info.kp_proc.p_flag & P_TRACED) != 0);
			else
				return false;
			#elif defined(CPL_UNIXC)

			auto contents = ExecCommand("grep 'TracerPid' /proc/self/status");

			if (contents.first == 0 && contents.second.size() > 0)
			{
				auto pos = contents.second.find(":");
				if (pos != std::string::npos)
				{
					auto number = contents.second.c_str() + pos + 1;
					auto tracerPID = std::strtol(number, nullptr, 10);
					return tracerPID != 0;
				}
			}
			#else
			#error No detection of debugging implementation
			#endif
			return false;
		}


		const char * GetImageBase()
		{
			#ifdef CPL_WINDOWS
			HMODULE hMod;
			if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (const char*)&GetDirectoryPath, &hMod))
			{
				return reinterpret_cast<char *>(hMod);
			}
			#elif defined(CPL_MAC) || defined(CPL_UNIXC)
			Dl_info exeInfo;
			dladdr((void*)GetImageBase, &exeInfo);
			return (const char*)exeInfo.dli_fbase;
			#endif
			return nullptr;
		}

		/*********************************************************************************************

			returns the amount of characters needed to print pargs to the format list,
			EXCLUDING the null character

		 *********************************************************************************************/
		int GetSizeRequiredFormat(const string_ref fmt, va_list pargs)
		{
			#ifdef CPL_WINDOWS
			return _vscprintf(fmt.c_str(), pargs);
			#else
			int retval;
			va_list argcopy;
			va_copy(argcopy, pargs);
			retval = vsnprintf(nullptr, 0, fmt.c_str(), argcopy);
			va_end(argcopy);
			return retval;
			#endif

		}
		/*********************************************************************************************

			Define a symbol for __rdtsc if it doesn't exist

		 *********************************************************************************************/
		#ifndef CPL_MSVC
		__inline__ uint64_t __rdtsc() {
			#if defined(__x86_64__) || defined(__i386__)
				#ifdef CPL_M_64BIT_
					uint64_t a, d;
					__asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
					return (d << 32) | a;
				#else
					uint64_t x;
					__asm__ volatile ("rdtsc" : "=A" (x));
					return x;
				#endif
			#elif defined(__aarch64__) || defined(__arm64__)
				// ARM64: Use the system counter
				uint64_t val;
				__asm__ volatile("mrs %0, cntvct_el0" : "=r" (val));
				return val;
			#else
				#error "Implement rdtsc for your platform"
			#endif
		}
		#endif
		/*********************************************************************************************

			Returns the CPU clock counter

		 *********************************************************************************************/
		std::uint64_t ClockCounter()
		{
			return __rdtsc();
		}
		/*********************************************************************************************

			Returns a precise point in time. Use TimeDifference() to get the difference in miliseconds
			between time points.

		*********************************************************************************************/
		long long TimeCounter()
		{
			long long t = 0;

			#ifdef _WINDOWS_
			::QueryPerformanceCounter((LARGE_INTEGER*)&t);
			#elif defined(CPL_MAC)
			auto t1 = mach_absolute_time();
			*(decltype(t1)*)&t = t1;
			#elif defined(__CPP11__)
			using namespace std::chrono;
			auto tpoint = high_resolution_clock::now().time_since_epoch().count();
			(*(decltype(tpoint)*)&t) = tpoint;
			#endif

			return t;
		}
		double TimeDifference(long long time)
		{
			return TimeToMilisecs(TimeCounter() - time);
		}

		double TimeToMilisecs(long long time)
		{
			double ret = 0.0;

			#ifdef _WINDOWS_
			long long f;

			::QueryPerformanceFrequency((LARGE_INTEGER*)&f);

			//long long t = TimeCounter();
			ret = (time) * (1000.0 / f);
			#elif defined(CPL_MAC)
			auto t1 = *(decltype(mach_absolute_time())*)&time;

			struct mach_timebase_info tinfo;
			if (mach_timebase_info(&tinfo) == KERN_SUCCESS)
			{
				double hTime2nsFactor = (double)tinfo.numer / tinfo.denom;
				ret = (((t1)* hTime2nsFactor) / 1000.0) / 1000.0;
			}

			#elif defined(__CPP11__)
			using namespace std::chrono;

			high_resolution_clock::rep t1;
			t1 = *(high_resolution_clock::rep *)&time;
			milliseconds elapsed(t1);
			ret = elapsed.count();
			#endif

			return ret;

		}

		/*********************************************************************************************

			Get a formatted time string in the format "hh:mm:ss"

		*********************************************************************************************/
		std::string GetTime()
		{
			time_t timeObj;
			tm * ctime;
			time(&timeObj);
			#ifdef CPL_MSVC
			tm pTime;
			gmtime_s(&pTime, &timeObj);
			ctime = &pTime;
			#else
			// consider using a safer alternative here.
			ctime = gmtime(&timeObj);
			#endif
			char buffer[100];
			// not cross platform.
			#ifdef CPL_MSVC

			sprintf_s(buffer, "%01d:%01d:%01d", ctime->tm_hour, ctime->tm_min, ctime->tm_sec);
			#else
			sprintf(buffer, "%01d:%01d:%01d", ctime->tm_hour, ctime->tm_min, ctime->tm_sec);
			#endif
			return buffer;
		}

		std::string GetDate()
		{
			time_t timeObj;
			tm * ctime;
			time(&timeObj);
			#ifdef CPL_MSVC
			tm pTime;
			gmtime_s(&pTime, &timeObj);
			ctime = &pTime;
			#else
			// consider using a safer alternative here.
			ctime = gmtime(&timeObj);
			#endif
			char buffer[100];
			// not cross platform.
			#ifdef CPL_MSVC
			sprintf_s(buffer, "%d/%d/%d", ctime->tm_mday, ctime->tm_mon + 1, ctime->tm_year + 1900);
			#else
			sprintf(buffer, "%d/%d/%d", ctime->tm_mday, ctime->tm_mon + 1, ctime->tm_year + 1900);
			#endif
			return buffer;
		}

		/*********************************************************************************************

			'private' function, initializes the global DirectoryPath

		*********************************************************************************************/

		static std::string GetDirectoryPath()
		{
			if (programInfo.hasCustomDirectory)
				return programInfo.customDirectory();

			#ifdef CPL_WINDOWS
				// change to MAX_PATH on all supported systems
				char path[MAX_PATH + 2];
				unsigned long nLen = 0;
				unsigned long nSize = sizeof path;
				/*
					GetModuleFileName() returns the path to the dll, inclusive of the dll name.
					We shave this off to get the directory
				*/

				HMODULE hMod;
				GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
					(char*)&GetDirectoryPath, &hMod);

				nLen = GetModuleFileNameA(hMod, path, nSize);
				if (!nLen)
					return "";
				while (nLen-- > 0) {
					if (CPL_DIRC_COMP(path[nLen])) {
						path[nLen] = '\0';
						break;
					}
				};
				path[MAX_PATH + 1] = NULL;
				return path;

			#elif defined(CPL_MAC) && defined(__APE_LOCATE_USING_COCOA)

				/*
					GetBundlePath() returns the path to the bundle, inclusive of the bundle name.
					Since everything we have is in the subdir /contents/ we append that.
					NOTE: This is broken
				*/
				// change to MAX_PATH on all supported systems
				char path[MAX_PATH + 2];
				unsigned long nLen = 0;
				unsigned long nSize = sizeof path;
				nLen = GetBundlePath(path, nSize);
				if (!nLen)
					return "";
				path[nLen] = '\0';
				std::string ret = path;
				ret += "/Contents";
				return ret;
				#elif defined(CPL_MAC) && defined(__APE_LOCATE_USING_CF)
				// change to MAX_PATH on all supported systems
				char path[MAX_PATH + 2];
				unsigned long nLen = 0;
				unsigned long nSize = sizeof path;
				CFBundleRef ref = CFBundleGetBundleWithIdentifier(CFSTR("com.Lightbridge.AudioProgrammingEnvironment"));
				if (ref)
				{
					CFURLRef url = CFBundleCopyBundleURL(ref);
					if (url)
					{
						CFURLGetFileSystemRepresentation(url, true, (unsigned char*)path, nSize);
						CFRelease(url);
						return path;
					}
				}

			#elif defined(CPL_UNIXC) || defined(CPL_MAC)

				Dl_info exeInfo;
				dladdr((void*)GetDirectoryPath, &exeInfo);

				fs::path fullPath(exeInfo.dli_fname);

				auto parent = fullPath.parent_path();

				#ifdef CPL_MAC
					if(parent.stem().string() == "MacOS" && parent.parent_path().stem() == "Contents")
						parent = parent.parent_path() / "Resources";

				#endif

				return parent.string() + "/";

			#endif
				
			return "<Error getting directory of executable>";
		}
		/*********************************************************************************************

			Floating point rounding function.

		*********************************************************************************************/
		long Round(double number)
		{
			return (number >= 0.0) ? (long)(number + 0.5) : (long)(number - 0.5);
		}

		/*********************************************************************************************

			Delays execution for a certain amount of time. Sleeping for a time less than (or equal to)
			zero yields the thread instead.

		*********************************************************************************************/
		long Delay(int ms)
		{
			#ifdef __CPP11__
			if (ms <= 0)
				std::this_thread::yield();
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(ms));
			#elif defined (CPL_WINDOWS)
			::Sleep(ms);
			#elif defined(CPL_MAC) || defined(CPL_UNIXC)
			usleep(ms * 1000);
			#endif
			return 0;
		}

		/*********************************************************************************************

			Imprecise, incrementing timer.
			At least 1000/60 seconds precision.

		*********************************************************************************************/
		unsigned int QuickTime()
		{
			#ifdef CPL_WINDOWS
			// TODO: GetTickCount64()
			return GetTickCount();
			#else
			// from sweep/lice
			struct timeval tm = {0,};
			gettimeofday(&tm, NULL);
			// this is not really ideal, but i would really like to keep the counter in a 32-bit int.
			// a simple static_cast<unsigned> does NOT work as expected: result is TRUNCATED instead
			// of following the standard which says unsigned integers wrap around INT_MAX.
			// we do this ourselves, therefore.
			return static_cast<unsigned long long>(tm.tv_sec*1000.0 + tm.tv_usec / 1000.0) % UINT32_MAX;
			#endif
		}

		/*********************************************************************************************

			'private' function, maps to a MessageBox

		*********************************************************************************************/
		inline static int _mbx(void * systemData, const string_ref text, const string_ref title, int nStyle = 0)
		{
			std::promise<int> promise;
			auto future = promise.get_future();

			// this pattern ensures the message box is called on the main thread, no matter what thread we're in.

			auto boxGenerator = [&]()
			{
				#ifdef CPL_WINDOWS
					if (!systemData)
						nStyle |= MB_DEFAULT_DESKTOP_ONLY;
					auto ret = (int)MessageBoxA(reinterpret_cast<HWND>(systemData), text.c_str(), title.c_str(), nStyle);
				#elif defined(CPL_MAC)
					auto ret = MacBox(systemData, text.c_str(), title.c_str(), nStyle);
				#elif defined(CPL_JUCE)
					auto ret = MsgButton::bOk;

					auto iconStyle = (nStyle >> 8) & 0xFF;
					auto buttonStyle = nStyle & 0xFF;
					juce::AlertWindow::AlertIconType iconType;
					switch (iconStyle)
					{
						case iInfo:
							iconType = juce::AlertWindow::AlertIconType::InfoIcon;
							break;
						case iWarning:
						case iStop:
							iconType = juce::AlertWindow::AlertIconType::WarningIcon;
							break;
						case iQuestion:
							iconType = juce::AlertWindow::AlertIconType::QuestionIcon;
							break;
						default:
							iconType = juce::AlertWindow::AlertIconType::NoIcon;
							break;
					}

					switch (buttonStyle)
					{
						case MsgStyle::sOk:
						{
							juce::NativeMessageBox::showMessageBox(iconType, title.c_str(), text.c_str(), nullptr);
							ret = MsgButton::bOk;
							break;
						}
						case MsgStyle::sYesNo:
						{
							bool result = juce::NativeMessageBox::showOkCancelBox(iconType, title.c_str(), text.c_str(), nullptr, nullptr);
							ret = result ? MsgButton::bYes : MsgButton::bNo;
							break;
						}
						case sYesNoCancel:
						case sConTryCancel:
						{
							int result = juce::NativeMessageBox::showYesNoCancelBox(iconType, title.c_str(), text.c_str(), nullptr, nullptr);
							if (result == 0)
								ret = MsgButton::bCancel;
							else if (result == 1)
								ret = MsgButton::bYes;
							else
								ret = MsgButton::bNo;
							break;
						}
					}
				#elif defined(CPL_UNIXC)
				std::string options = "zenity ";

				auto iconStyle = (nStyle >> 8) & 0xFF;
				switch (iconStyle)
				{
					default:
					case iInfo: options += "--info "; break;
					case iStop: options += "--error "; break;
					case iWarning: options += "--warning "; break;
					case iQuestion: options += "--question "; break;
				}

				options += "--text=\"" + text.string() + "\" --title=\"" + title.string() + "\"";

				auto ret = ExecCommand(options).first ? MsgButton::bNo : MsgButton::bYes;

				#endif

				promise.set_value(ret);
			};

			#ifdef CPL_JUCE

			if (juce::MessageManager::getInstance()->isThisTheMessageThread())
			{
				boxGenerator();
			}
			else if (
				// deadlock: we are not the main thread, but we have it locked and we are currently blocking progress
				juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager()
				// deadlock: OS X always finds a weird way to query the context about something random.
				// in this situation, the context is locked, so we also create a deadlock here.
				|| juce::OpenGLContext::getCurrentContext())
			{
				return MsgButton::bError;

			}
			else
			{
				cpl::GUIUtils::MainEvent(boxGenerator);
			}
			#else
			if (std::this_thread::get_id() == MainThreadID)
			{
				boxGenerator();
			}
			else
			{
				throw CPLNotImplementedException("Non-main thread message boxes not implemented for non-GUI builds.");
			}
			#endif
			future.wait();
			return future.get();
		}

		/*********************************************************************************************

			MsgBoxData - used to transfer information about a messagebox through threads

		*********************************************************************************************/
		struct MsgBoxData
		{
			std::string title;
			std::string text;
			int nStyle;
			void * systemWindow;
			MsgBoxData(const string_ref title, const string_ref text, int style, void * window = NULL)
				: title(title.string()), text(text.string()), nStyle(style), systemWindow(window) {};
		};

		/*********************************************************************************************

			Spawns a messagebox in a threaded context. Max of 10 open boxes.

		*********************************************************************************************/
		static void * ThreadedMessageBox(void * imp_data)
		{
			//Global for all instances of this plugin: But we do not want to spam screen with msgboxes
			static std::atomic<int> nOpenMsgBoxes {0};
			const int nMaxBoxes = 10;

			if (nOpenMsgBoxes >= nMaxBoxes)
				return reinterpret_cast<void*>(-1);
			MsgBoxData * data = reinterpret_cast<MsgBoxData*>(imp_data);
			nOpenMsgBoxes++;

			int ret = _mbx(data->systemWindow, data->text.c_str(), data->title.c_str(), data->nStyle);
			delete data;

			nOpenMsgBoxes--;

			return reinterpret_cast<void*>(static_cast<std::intptr_t>(ret));
		}

		/*********************************************************************************************

			MsgBox - Spawns a messagebox, optionally blocking.

		*********************************************************************************************/
		int MsgBox(const string_ref text, const string_ref title, int nStyle, void * parent, const bool bBlocking)
		{
			if (bBlocking)
				return _mbx(parent, text.c_str(), title.c_str(), nStyle);
			else
			{
				// spawn and detach instantly.
				/* NOTE:
					This is a severely deprecated method on both windows and osx
					Implement this as some sort of global stack of messageboxes,
					which gets shown when a global, static RenderMsgBoxes() function
					is called from a gui loop
				*/

				CThread msgThread(ThreadedMessageBox);
				msgThread.run(new MsgBoxData(title, text, nStyle));
				return 0;
			}
		}
	};
};
