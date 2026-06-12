#ifndef INSTRUCTIONSET_H
#define INSTRUCTIONSET_H
#include <cmath>
#include <vector>
#include <bitset>
#include <array>
#include <string>
#include <mutex>
#include "../PlatformSpecific.h"
#include <string_view>

namespace cpl
{
	namespace msdn
	{
		// Source: msdn
		// http://msdn.microsoft.com/en-us/library/hskdteyh.aspx

		class InstructionSet
		{
#if CPL_M_ARM
        public:
			// getters
			static constexpr std::string_view Vendor() { return "ARM"; }
			static constexpr std::string_view Brand() { return "ARM64 Processor"; }

			static constexpr bool isIntel() { return false; }
			static constexpr bool isAMD() { return false; }
			static constexpr bool isARM() { return true; }

			static constexpr bool SSE3() { return true; }
			static constexpr bool PCLMULQDQ() { return false; }
			static constexpr bool MONITOR() { return false; }
			static constexpr bool SSSE3() { return true; }
			static constexpr bool FMA() { return true; }
			static constexpr bool CMPXCHG16B() { return false; }
			static constexpr bool SSE41() { return true; }
			static constexpr bool SSE42() { return true; }
			static constexpr bool MOVBE() { return false; }
			static constexpr bool POPCNT() { return true; }
			static constexpr bool AES() { return false; }
			static constexpr bool XSAVE() { return false; }
			static constexpr bool OSXSAVE() { return false; }
			static constexpr bool AVX() { return false; }
			static constexpr bool F16C() { return false; }
			static constexpr bool RDRAND() { return false; }

			static constexpr bool MSR() { return false; }
			static constexpr bool CX8() { return false; }
			static constexpr bool SEP() { return false; }
			static constexpr bool CMOV() { return true; }
			static constexpr bool CLFSH() { return false; }
			static constexpr bool MMX() { return true; }
			static constexpr bool FXSR() { return false; }
			static constexpr bool SSE() { return true; }
			static constexpr bool SSE2() { return true; }

			static constexpr bool FSGSBASE() { return false; }
			static constexpr bool BMI1() { return false; }
			static constexpr bool HLE() { return false; }
			static constexpr bool AVX2() { return false; }
			static constexpr bool BMI2() { return false; }
			static constexpr bool ERMS() { return false; }
			static constexpr bool INVPCID() { return false; }
			static constexpr bool RTM() { return false; }
			static constexpr bool AVX512F() { return false; }
			static constexpr bool RDSEED() { return false; }
			static constexpr bool ADX() { return false; }
			static constexpr bool AVX512PF() { return false; }
			static constexpr bool AVX512ER() { return false; }
			static constexpr bool AVX512CD() { return false; }
			static constexpr bool SHA() { return false; }

			static constexpr bool PREFETCHWT1() { return true; }

			static constexpr bool LAHF() { return false; }
			static constexpr bool LZCNT() { return true; }
			static constexpr bool ABM() { return false; }
			static constexpr bool SSE4a() { return false; }
			static constexpr bool XOP() { return false; }
			static constexpr bool TBM() { return false; }

			static constexpr bool SYSCALL() { return false; }
			static constexpr bool MMXEXT() { return true; }
			static constexpr bool RDTSCP() { return true; }
			static constexpr bool _3DNOWEXT() { return false; }
			static constexpr bool _3DNOW() { return false; }

#else
			// forward declarations
			class InstructionSet_Internal;
			static void cpuid(int CPUInfo[4], int InfoType);
			static void cpuidex(int CPUInfo[4], int InfoType, int SubFunctionID);
		public:
			// getters
			static std::string Vendor() { return CPU_Rep.vendor_; }
			static std::string Brand() { return CPU_Rep.brand_; }

			static bool isIntel() { return CPU_Rep.isIntel_; }
			static bool isAMD() { return CPU_Rep.isAMD_; }
			static constexpr bool isARM() { return false; }

			static bool SSE3() { return CPU_Rep.f_1_ECX_[0]; }
			static bool PCLMULQDQ() { return CPU_Rep.f_1_ECX_[1]; }
			static bool MONITOR() { return CPU_Rep.f_1_ECX_[3]; }
			static bool SSSE3() { return CPU_Rep.f_1_ECX_[9]; }
			static bool FMA() { return CPU_Rep.f_1_ECX_[12]; }
			static bool CMPXCHG16B() { return CPU_Rep.f_1_ECX_[13]; }
			static bool SSE41() { return CPU_Rep.f_1_ECX_[19]; }
			static bool SSE42() { return CPU_Rep.f_1_ECX_[20]; }
			static bool MOVBE() { return CPU_Rep.f_1_ECX_[22]; }
			static bool POPCNT() { return CPU_Rep.f_1_ECX_[23]; }
			static bool AES() { return CPU_Rep.f_1_ECX_[25]; }
			static bool XSAVE() { return CPU_Rep.f_1_ECX_[26]; }
			static bool OSXSAVE() { return CPU_Rep.f_1_ECX_[27]; }
			static bool AVX() { return CPU_Rep.f_1_ECX_[28]; }
			static bool F16C() { return CPU_Rep.f_1_ECX_[29]; }
			static bool RDRAND() { return CPU_Rep.f_1_ECX_[30]; }

			static bool MSR() { return CPU_Rep.f_1_EDX_[5]; }
			static bool CX8() { return CPU_Rep.f_1_EDX_[8]; }
			static bool SEP() { return CPU_Rep.f_1_EDX_[11]; }
			static bool CMOV() { return CPU_Rep.f_1_EDX_[15]; }
			static bool CLFSH() { return CPU_Rep.f_1_EDX_[19]; }
			static bool MMX() { return CPU_Rep.f_1_EDX_[23]; }
			static bool FXSR() { return CPU_Rep.f_1_EDX_[24]; }
			static bool SSE() { return CPU_Rep.f_1_EDX_[25]; }
			static bool SSE2() { return CPU_Rep.f_1_EDX_[26]; }

			static bool FSGSBASE() { return CPU_Rep.f_7_EBX_[0]; }
			static bool BMI1() { return CPU_Rep.f_7_EBX_[3]; }
			static bool HLE() { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[4]; }
			static bool AVX2() { return CPU_Rep.f_7_EBX_[5]; }
			static bool BMI2() { return CPU_Rep.f_7_EBX_[8]; }
			static bool ERMS() { return CPU_Rep.f_7_EBX_[9]; }
			static bool INVPCID() { return CPU_Rep.f_7_EBX_[10]; }
			static bool RTM() { return CPU_Rep.isIntel_ && CPU_Rep.f_7_EBX_[11]; }
			static bool AVX512F() { return CPU_Rep.f_7_EBX_[16]; }
			static bool RDSEED() { return CPU_Rep.f_7_EBX_[18]; }
			static bool ADX() { return CPU_Rep.f_7_EBX_[19]; }
			static bool AVX512PF() { return CPU_Rep.f_7_EBX_[26]; }
			static bool AVX512ER() { return CPU_Rep.f_7_EBX_[27]; }
			static bool AVX512CD() { return CPU_Rep.f_7_EBX_[28]; }
			static bool SHA() { return CPU_Rep.f_7_EBX_[29]; }

			static bool PREFETCHWT1() { return CPU_Rep.f_7_ECX_[0]; }

			static bool LAHF() { return CPU_Rep.f_81_ECX_[0]; }
			static bool LZCNT() { return CPU_Rep.isIntel_ && CPU_Rep.f_81_ECX_[5]; }
			static bool ABM() { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[5]; }
			static bool SSE4a() { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[6]; }
			static bool XOP() { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[11]; }
			static bool TBM() { return CPU_Rep.isAMD_ && CPU_Rep.f_81_ECX_[21]; }

			static bool SYSCALL() { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[11]; }
			static bool MMXEXT() { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[22]; }
			static bool RDTSCP() { return CPU_Rep.isIntel_ && CPU_Rep.f_81_EDX_[27]; }
			static bool _3DNOWEXT() { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[30]; }
			static bool _3DNOW() { return CPU_Rep.isAMD_ && CPU_Rep.f_81_EDX_[31]; }

		private:
			static const InstructionSet_Internal CPU_Rep;

			class InstructionSet_Internal
			{
			public:
				InstructionSet_Internal()
					: nIds_ {0},
					nExIds_ {0},
					isIntel_ {false},
					isAMD_ {false},
					f_1_ECX_ {0},
					f_1_EDX_ {0},
					f_7_EBX_ {0},
					f_7_ECX_ {0},
					f_81_ECX_ {0},
					f_81_EDX_ {0},
					data_ {},
					extdata_ {}
				{
					#if defined(__aarch64__) || defined(__arm64__)
					// ARM64 fast path - skip expensive x86 CPU feature detection
					vendor_ = "ARM";
					brand_ = "ARM64 Processor";
					// Reserve minimal space to avoid reallocations
					data_.reserve(2);
					extdata_.reserve(2);
					// Set basic ARM64/NEON capabilities
					// Enable SSE/SSE2 flags as these map to NEON via SIMDE
					f_1_EDX_[25] = 1; // SSE maps to NEON
					f_1_EDX_[26] = 1; // SSE2 maps to NEON
					// ARM64 always has FPU
					f_1_EDX_[24] = 1; // FXSR
					return;
					#endif

					//int cpuInfo[4] = {-1};
					std::array<int, 4> cpui;

					// Reserve memory upfront to avoid reallocations
					data_.reserve(32);
					extdata_.reserve(16);

					// Calling __cpuid with 0x0 as the function_id argument
					// gets the number of the highest valid function ID.
					cpuid(cpui.data(), 0);
					nIds_ = cpui[0];

					for (int i = 0; i <= nIds_; ++i)
					{
						cpuidex(cpui.data(), i, 0);
						data_.push_back(cpui);
					}

					// Capture vendor string
					char vendor[0x20];
					memset(vendor, 0, sizeof(vendor));
					*reinterpret_cast<int*>(vendor) = data_[0][1];
					*reinterpret_cast<int*>(vendor + 4) = data_[0][3];
					*reinterpret_cast<int*>(vendor + 8) = data_[0][2];
					vendor_ = vendor;
					if (vendor_ == "GenuineIntel")
					{
						isIntel_ = true;
					}
					else if (vendor_ == "AuthenticAMD")
					{
						isAMD_ = true;
					}

					// load bitset with flags for function 0x00000001
					if (nIds_ >= 1)
					{
						f_1_ECX_ = (unsigned)data_[1][2];
						f_1_EDX_ = (unsigned)data_[1][3];
					}

					// load bitset with flags for function 0x00000007
					if (nIds_ >= 7)
					{
						f_7_EBX_ = (unsigned)data_[7][1];
						f_7_ECX_ = (unsigned)data_[7][2];
					}

					// Calling __cpuid with 0x80000000 as the function_id argument
					// gets the number of the highest valid extended ID.
					cpuid(cpui.data(), (int)0x80000000);
					nExIds_ = cpui[0];

					char brand[0x40];
					memset(brand, 0, sizeof(brand));

					for (int i = (int)0x80000000; i <= nExIds_; ++i)
					{
						cpuidex(cpui.data(), i, 0);
						extdata_.push_back(cpui);
					}

					// load bitset with flags for function 0x80000001
					if (nExIds_ >= (int)0x80000001)
					{
						f_81_ECX_ = (unsigned)extdata_[1][2];
						f_81_EDX_ = (unsigned)extdata_[1][3];
					}

					// Interpret CPU brand string if reported
					if (nExIds_ >= (int)0x80000004)
					{
						memcpy(brand, extdata_[2].data(), sizeof(cpui));
						memcpy(brand + 16, extdata_[3].data(), sizeof(cpui));
						memcpy(brand + 32, extdata_[4].data(), sizeof(cpui));
						brand[sizeof(brand) - 1] = '\0';
						brand_ = brand;
					}
				};

				int nIds_;
				int nExIds_;
				std::string vendor_;
				std::string brand_;
				bool isIntel_;
				bool isAMD_;
				std::bitset<32> f_1_ECX_;
				std::bitset<32> f_1_EDX_;
				std::bitset<32> f_7_EBX_;
				std::bitset<32> f_7_ECX_;
				std::bitset<32> f_81_ECX_;
				std::bitset<32> f_81_EDX_;
				std::vector<std::array<int, 4>> data_;
				std::vector<std::array<int, 4>> extdata_;
			};
#endif
		};
	};
};
#endif
