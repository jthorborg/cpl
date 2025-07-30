#ifndef INSTRUCTIONSET_H
#define INSTRUCTIONSET_H
#include <cmath>
#include <vector>
#include <bitset>
#include <array>
#include <string>
#include <mutex>
#include "../PlatformSpecific.h"

namespace cpl
{
	void cpuid(int *, int);
	void cpuidex(int *, int, int);

	namespace msdn
	{
		// Source: msdn
		// http://msdn.microsoft.com/en-us/library/hskdteyh.aspx

		class InstructionSet
		{
			// forward declarations
			class InstructionSet_Internal;

		public:
			// getters
			static std::string Vendor(void) { return GetCPU_Rep().vendor_; }
			static std::string Brand(void) { return GetCPU_Rep().brand_; }

			static bool isIntel(void) { return GetCPU_Rep().isIntel_; }
			static bool isAMD(void) { return GetCPU_Rep().isAMD_; }

			static bool SSE3(void) { return GetCPU_Rep().f_1_ECX_[0]; }
			static bool PCLMULQDQ(void) { return GetCPU_Rep().f_1_ECX_[1]; }
			static bool MONITOR(void) { return GetCPU_Rep().f_1_ECX_[3]; }
			static bool SSSE3(void) { return GetCPU_Rep().f_1_ECX_[9]; }
			static bool FMA(void) { return GetCPU_Rep().f_1_ECX_[12]; }
			static bool CMPXCHG16B(void) { return GetCPU_Rep().f_1_ECX_[13]; }
			static bool SSE41(void) { return GetCPU_Rep().f_1_ECX_[19]; }
			static bool SSE42(void) { return GetCPU_Rep().f_1_ECX_[20]; }
			static bool MOVBE(void) { return GetCPU_Rep().f_1_ECX_[22]; }
			static bool POPCNT(void) { return GetCPU_Rep().f_1_ECX_[23]; }
			static bool AES(void) { return GetCPU_Rep().f_1_ECX_[25]; }
			static bool XSAVE(void) { return GetCPU_Rep().f_1_ECX_[26]; }
			static bool OSXSAVE(void) { return GetCPU_Rep().f_1_ECX_[27]; }
			static bool AVX(void) { return GetCPU_Rep().f_1_ECX_[28]; }
			static bool F16C(void) { return GetCPU_Rep().f_1_ECX_[29]; }
			static bool RDRAND(void) { return GetCPU_Rep().f_1_ECX_[30]; }

			static bool MSR(void) { return GetCPU_Rep().f_1_EDX_[5]; }
			static bool CX8(void) { return GetCPU_Rep().f_1_EDX_[8]; }
			static bool SEP(void) { return GetCPU_Rep().f_1_EDX_[11]; }
			static bool CMOV(void) { return GetCPU_Rep().f_1_EDX_[15]; }
			static bool CLFSH(void) { return GetCPU_Rep().f_1_EDX_[19]; }
			static bool MMX(void) { return GetCPU_Rep().f_1_EDX_[23]; }
			static bool FXSR(void) { return GetCPU_Rep().f_1_EDX_[24]; }
			static bool SSE(void) { return GetCPU_Rep().f_1_EDX_[25]; }
			static bool SSE2(void) { return GetCPU_Rep().f_1_EDX_[26]; }

			static bool FSGSBASE(void) { return GetCPU_Rep().f_7_EBX_[0]; }
			static bool BMI1(void) { return GetCPU_Rep().f_7_EBX_[3]; }
			static bool HLE(void) { return GetCPU_Rep().isIntel_ && GetCPU_Rep().f_7_EBX_[4]; }
			static bool AVX2(void) { return GetCPU_Rep().f_7_EBX_[5]; }
			static bool BMI2(void) { return GetCPU_Rep().f_7_EBX_[8]; }
			static bool ERMS(void) { return GetCPU_Rep().f_7_EBX_[9]; }
			static bool INVPCID(void) { return GetCPU_Rep().f_7_EBX_[10]; }
			static bool RTM(void) { return GetCPU_Rep().isIntel_ && GetCPU_Rep().f_7_EBX_[11]; }
			static bool AVX512F(void) { return GetCPU_Rep().f_7_EBX_[16]; }
			static bool RDSEED(void) { return GetCPU_Rep().f_7_EBX_[18]; }
			static bool ADX(void) { return GetCPU_Rep().f_7_EBX_[19]; }
			static bool AVX512PF(void) { return GetCPU_Rep().f_7_EBX_[26]; }
			static bool AVX512ER(void) { return GetCPU_Rep().f_7_EBX_[27]; }
			static bool AVX512CD(void) { return GetCPU_Rep().f_7_EBX_[28]; }
			static bool SHA(void) { return GetCPU_Rep().f_7_EBX_[29]; }

			static bool PREFETCHWT1(void) { return GetCPU_Rep().f_7_ECX_[0]; }

			static bool LAHF(void) { return GetCPU_Rep().f_81_ECX_[0]; }
			static bool LZCNT(void) { return GetCPU_Rep().isIntel_ && GetCPU_Rep().f_81_ECX_[5]; }
			static bool ABM(void) { return GetCPU_Rep().isAMD_ && GetCPU_Rep().f_81_ECX_[5]; }
			static bool SSE4a(void) { return GetCPU_Rep().isAMD_ && GetCPU_Rep().f_81_ECX_[6]; }
			static bool XOP(void) { return GetCPU_Rep().isAMD_ && GetCPU_Rep().f_81_ECX_[11]; }
			static bool TBM(void) { return GetCPU_Rep().isAMD_ && GetCPU_Rep().f_81_ECX_[21]; }

			static bool SYSCALL(void) { return GetCPU_Rep().isIntel_ && GetCPU_Rep().f_81_EDX_[11]; }
			static bool MMXEXT(void) { return GetCPU_Rep().isAMD_ && GetCPU_Rep().f_81_EDX_[22]; }
			static bool RDTSCP(void) { return GetCPU_Rep().isIntel_ && GetCPU_Rep().f_81_EDX_[27]; }
			static bool _3DNOWEXT(void) { return GetCPU_Rep().isAMD_ && GetCPU_Rep().f_81_EDX_[30]; }
			static bool _3DNOW(void) { return GetCPU_Rep().isAMD_ && GetCPU_Rep().f_81_EDX_[31]; }

		private:
			static InstructionSet_Internal& GetCPU_Rep();
			static InstructionSet_Internal CPU_Rep;

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
					cpl::cpuid(cpui.data(), 0);
					nIds_ = cpui[0];

					for (int i = 0; i <= nIds_; ++i)
					{
						cpl::cpuidex(cpui.data(), i, 0);
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
					cpl::cpuid(cpui.data(), (int)0x80000000);
					nExIds_ = cpui[0];

					char brand[0x40];
					memset(brand, 0, sizeof(brand));

					for (int i = (int)0x80000000; i <= nExIds_; ++i)
					{
						cpl::cpuidex(cpui.data(), i, 0);
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
		};
	};
};
#endif
