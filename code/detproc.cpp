/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2025 Electronic Arts Inc.
 * Copyright 2026 OpenTS contributors
 *
 * Contains material derived from Electronic Arts source code.
 * Modified by OpenTS contributors, 2026.
 * EA's GPLv3 Section 7 additional terms and supplemental warranty
 * disclaimers apply; see LICENSE.md.
 ******************************************************************************/

// Portable replacement for the inherited detproc.asm. That file's CPUID-availability
// probe (toggling the EFLAGS AC/ID bits to tell a 386 or 486 from a CPUID-capable chip)
// is unreachable on any build this project supports: docs/BUILDING.md requires SSE2,
// which no CPU predating CPUID ever implements, so CPUID is always available here. The
// value-producing logic below reproduces the original CPUID-leaf math exactly, since
// CPUType is read directly by stats.cpp and reported as network stats field
// FIELD_CPU_TYPE.

#include "always.h"

#include "getcpu.h"
#include "mpu.h"

#include <cstring>

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#include <x86intrin.h>
#endif

namespace {

void CPUID(int leaf, unsigned int & eax, unsigned int & ebx, unsigned int & ecx, unsigned int & edx)
{
#if defined(_MSC_VER)
	int regs[4];
	__cpuid(regs, leaf);
	eax = (unsigned int)regs[0];
	ebx = (unsigned int)regs[1];
	ecx = (unsigned int)regs[2];
	edx = (unsigned int)regs[3];
#else
	__get_cpuid(leaf, &eax, &ebx, &ecx, &edx);
#endif
}

}


extern "C" {
	char CPUType = 0;
	char VendorID[20] = "Not available";
	char UseMMX = 0;
	char UseCMOV = 0;
	char HasCMOV = 0;
}


/// <summary>
/// Detects the presence of MMX technology, and along the way records the CPU's family
/// nibble in CPUType and its vendor ID string in VendorID.
/// </summary>
/// <returns>bool; Is MMX technology available?</returns>
bool __cdecl Detect_MMX_Availability(void)
{
	unsigned int eax, ebx, ecx, edx;

	CPUID(0, eax, ebx, ecx, edx);
	std::memcpy(VendorID + 0, &ebx, sizeof(ebx));
	std::memcpy(VendorID + 4, &edx, sizeof(edx));
	std::memcpy(VendorID + 8, &ecx, sizeof(ecx));
	std::memset(VendorID + 12, ' ', 4);

	CPUID(1, eax, ebx, ecx, edx);
	CPUType = (char)((eax >> 8) & 0xF);

	if ((unsigned char)CPUType < 5 || !(edx & 0x00800000u)) {
		UseMMX = 0;
		return(false);
	}

	UseMMX = 1;
	return(true);
}


/// <summary>
/// Detects the presence of CMOV, and whether the engine should use it. CPUType must
/// already be set by a prior call to Detect_MMX_Availability.
/// </summary>
/// <returns>bool; Is CMOV supported?</returns>
bool __cdecl Detect_CMOV_Availability(void)
{
	if ((unsigned char)CPUType < 5) {
		UseCMOV = 0;
		HasCMOV = 0;
		return(false);
	}

	unsigned int eax, ebx, ecx, edx;
	CPUID(1, eax, ebx, ecx, edx);
	if (!(edx & 0x00008000u)) {
		UseCMOV = 0;
		HasCMOV = 0;
		return(false);
	}

	HasCMOV = 1;
	UseCMOV = (unsigned char)CPUType > 5 ? 1 : 0;
	return(true);
}


/// <summary>
/// Fetches the current CPU clock time. This is the internal Pentium-or-later clock
/// accumulator, incremented every clock tick; the low half is returned directly, the
/// high half is stored in the location specified.
/// </summary>
/// <param name="high">Reference to the high value of the 64 bit clock number.</param>
/// <returns>unsigned int; The low half of the CPU clock value.</returns>
unsigned int __cdecl Get_CPU_Clock(unsigned int & high)
{
	unsigned long long const clock = __rdtsc();
	high = (unsigned int)(clock >> 32);
	return((unsigned int)clock);
}


/// <summary>
/// Reports which class of processor family this build recognizes. Callers treat a
/// return of 2 or more as "CPUID-capable", which every build of this project satisfies.
/// </summary>
/// <returns>unsigned short; PROC_PENTIUM (2), matching the original's CPUID-capable class.</returns>
extern "C" unsigned short __cdecl Processor(void)
{
	return(PROC_PENTIUM);
}
