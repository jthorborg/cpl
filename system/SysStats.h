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

	file:SysStats.h

		Gives access to stats of the system, like supported cpu instruction sets
		and such.

*************************************************************************************/


#ifndef CPL_SYSSTATS_H
#define CPL_SYSSTATS_H

#include "../MacroConstants.h"
#include "InstructionSet.h"

#ifdef __CPP11__
#include <thread>
#endif

#include "../PlatformSpecific.h"
#include "../LexicalConversion.h"

namespace cpl
{
	namespace system
	{
		/// <summary>
		/// Information about the central processor
		/// </summary>
		class CProcessor
		{
		public:

			enum Archs
			{
				MMX = 1,
				SSE = 1 << 1,
				SSE2 = 1 << 2,
				SSE3 = 1 << 3,
				SSE4 = 1 << 4,
				AVX = 1 << 5,
				AVX2 = 1 << 6,
				FMA = 1 << 7
			};

			/*
				Certain processors like Intel use hyperthreading
				to actually increase performance, when using more threads
				than available cores.
			*/
			static std::size_t getNumOptimalThreads()
			{
				auto numCores = getNumCores();
				#if 0
				if (msdn::InstructionSet::isIntel)
					return numCores + numCores - 1;
				else
					return numCores;
				#else
				return numCores > 1 ? numCores - 1 : numCores;
				#endif
			}

			/*
				http://stackoverflow.com/a/150971/1287254
			*/
			static std::size_t getNumCores()
			{
				#ifdef __CPP11__
				return std::thread::hardware_concurrency();
				#else
				#ifdef CPL_WINDOWS
				SYSTEM_INFO sysinfo;
				GetSystemInfo(&sysinfo);
				return sysinfo.dwNumberOfProcessors;
				#else
				return sysconf(_SC_NPROCESSORS_ONLN);
				#endif
				#endif
			}

			static double getMHz()
			{
				return instance().frequency;
			}

			static const std::string getName()
			{
				return std::string(msdn::InstructionSet::Vendor()) + "/" + std::string(msdn::InstructionSet::Brand());
			}

			static bool test(Archs arch)
			{
				return (instance().narchs & arch) ? true : false;
			}

		private:

			static const CProcessor & instance()
			{
				static const CProcessor _this;
				return _this;
			}

			CProcessor()
				: narchs(0), frequency(0)
			{
				collectInfo();
			}


			void collectInfo()
			{
				narchs = 0;
				if (msdn::InstructionSet::AVX())
					narchs |= Archs::AVX;
				if (msdn::InstructionSet::AVX2())
					narchs |= Archs::AVX2;
				if (msdn::InstructionSet::FMA())
					narchs |= Archs::FMA;
				if (msdn::InstructionSet::SSE())
					narchs |= Archs::SSE;
				if (msdn::InstructionSet::SSE2())
					narchs |= Archs::SSE2;
				if (msdn::InstructionSet::SSE3())
					narchs |= Archs::SSE3;
				if (msdn::InstructionSet::SSE41())
					narchs |= Archs::SSE4;
				if (msdn::InstructionSet::MMX())
					narchs |= Archs::MMX;


				#ifdef CPL_WINDOWS
				HKEY hKey;
				DWORD dwMHz;
				DWORD dwSize = sizeof(DWORD);

				RegOpenKeyEx(HKEY_LOCAL_MACHINE,
					_T("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"),
					0,
					KEY_READ,
					&hKey);

				RegQueryValueEx(hKey, _T("~MHz"), NULL, NULL, (LPBYTE)&dwMHz, &dwSize);
				RegCloseKey(hKey);
				frequency = dwMHz;
				#elif defined(CPL_MAC)

				auto contents = Misc::ExecCommand("sysctl hw.cpufrequency");
				if (contents.first == 0 && contents.second.size() > 0)
				{
					auto pos = contents.second.find(": ");
					if (pos != std::string::npos)
					{
						auto number = contents.second.c_str() + pos + 2;
						frequency = std::strtod(number, nullptr);
						if (frequency != 0)
							frequency /= 1000000;
					}

				}

				#else
				auto contents = Misc::ExecCommand("grep 'cpu MHz' /proc/cpuinfo");
				if (contents.first == 0 && contents.second.size() > 0)
				{
					auto pos = contents.second.find(": ");
					if (pos != std::string::npos)
					{
						auto number = contents.second.c_str() + pos + 2;
						frequency = std::strtod(number, nullptr);
					}

				}
				#endif
			}

			std::size_t narchs;
			double frequency;

		};

	};
};

#endif
