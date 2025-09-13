#include "InstructionSet.h"

namespace cpl
{
	#ifndef CPL_WINDOWS
	//  GCC Inline Assembly
	// creds: http://stackoverflow.com/questions/6121792/how-to-check-if-a-cpu-supports-the-sse3-instruction-set
	void cpuid(int CPUInfo[4], int InfoType)
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

	void cpuidex(int CPUInfo[4], int InfoType, int SubFunctionID)
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
	#else

	void cpuid(int CPUInfo[4], int InfoType)
	{
		return __cpuid(CPUInfo, InfoType);
	};

	void cpuidex(int CPUInfo[4], int InfoType, int SubFunctionID)
	{
		return __cpuidex(CPUInfo, InfoType, SubFunctionID);
	};

	#endif
	namespace msdn
	{
		InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;
		static std::once_flag cpu_rep_initialized;

		InstructionSet::InstructionSet_Internal& InstructionSet::GetCPU_Rep()
		{
			std::call_once(cpu_rep_initialized, []() {
				CPU_Rep = InstructionSet_Internal();
			});
			return CPU_Rep;
		}
	};

}