/*************************************************************************************

	cpl - cross-platform library - v. 0.1.0.

	Copyright (C) 2026 Janus Lynggaard Thorborg (www.jthorborg.com)

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

	file:PlatformMisc.h

		Whatever doesn't fit other places, depending on system headers.

*************************************************************************************/

#ifndef CPL_PLATFORMMISC_H
#define CPL_PLATFORMMISC_H

#include "PlatformSpecific.h"
#include "lib/string_ref.h"
#include "Exceptions.h"

namespace cpl
{
	namespace Misc
	{
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

        long Delay(int ms);
        unsigned int QuickTime();
    
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
