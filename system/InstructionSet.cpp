#include "InstructionSet.h"
#if CPL_M_X86

namespace cpl
{
	#ifndef CPL_WINDOWS
	//  GCC Inline Assembly
	// creds: http://stackoverflow.com/questions/6121792/how-to-check-if-a-cpu-supports-the-sse3-instruction-set
	void __cpuid(int CPUInfo[4], int InfoType)
	{
		#if defined(__x86_64__) || defined(__i386__)
			__asm__ __volatile__(
				"cpuid":
			"=a" (CPUInfo[0]),
				"=b" (CPUInfo[1]),
				"=c" (CPUInfo[2]),
				"=d" (CPUInfo[3]) :
				"a" (InfoType), "c" (0)
				);
		#elif defined(__aarch64__) || defined(__arm64__)
			// ARM64 doesn't have cpuid, return dummy values
			// InfoType 0 returns max function and vendor string
			if (InfoType == 0) {
				CPUInfo[0] = 0; // Max function number
				CPUInfo[1] = 0x41524D20; // "ARM "
				CPUInfo[2] = 0x41524D20; // "ARM "
				CPUInfo[3] = 0x41524D20; // "ARM "
			} else {
				// Return zeros for other queries
				CPUInfo[0] = CPUInfo[1] = CPUInfo[2] = CPUInfo[3] = 0;
			}
		#else
			#error "Unknown platform for CPUID"
		#endif
	}

	void __cpuidex(int CPUInfo[4], int InfoType, int SubFunctionID)
	{
		#if defined(__x86_64__) || defined(__i386__)
			__asm__	__volatile__("movl $0, %%ecx" : "=g" (SubFunctionID));
			__asm__ __volatile__(
				"cpuid":
			"=a" (CPUInfo[0]),
				"=b" (CPUInfo[1]),
				"=c" (CPUInfo[2]),
				"=d" (CPUInfo[3]) :
				"a" (InfoType), "c" (0)
				);
		#else
			// For non-x86 architectures, just call cpuid
			(void)SubFunctionID;
			cpuid(CPUInfo, InfoType);
		#endif
	}
	#endif

	namespace msdn
	{
		const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;

		void InstructionSet::cpuid(int CPUInfo[4], int InfoType)
		{
			return __cpuid(CPUInfo, InfoType);
		};

		void InstructionSet::cpuidex(int CPUInfo[4], int InfoType, int SubFunctionID)
		{
			return __cpuidex(CPUInfo, InfoType, SubFunctionID);
		};
	};

}

#endif
