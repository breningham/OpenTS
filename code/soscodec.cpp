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

// Portable replacement for the inherited soscodec.asm's HMI SOS ADPCM decoder. Only
// 16-bit mono was ever functionally implemented there -- the 8-bit and stereo code paths
// jumped straight to the exit without decoding, and dsaudio.cpp's only call site already
// gates on wBitSize==16 && wChannels==1, routing everything else to
// General_sosCODECDecompressData (olsosdec.asm) instead, so those paths stay unimplemented
// here too.
//
// This is standard 4:1 IMA-style ADPCM: dwPredicted is the running 16-bit sample,
// advanced by SosDiffTable[stepIndex * 16 + nybble] and clamped; stepIndex is advanced by
// SosNextIndexTable[stepIndex * 16 + nybble]. Each source byte yields two samples, low
// nybble first then high nybble. sCompInfo's on-disk wIndex/wIndex2 fields store the step
// index pre-multiplied by 32 (a hand-optimized addressing scheme from the original
// assembly); that representation is preserved here for compatibility with anything else
// that reads or writes them.
//
// Verified bit-exact against the original assembly (compiled and run side by side) across
// randomized multi-chunk streaming decode trials before this file replaced it.

#include "always.h"

#include "soscomp.h"
#include "sos_adpcm_tables.h"

namespace {

inline short Clamp16(int value)
{
	if (value > 32767) {
		return 32767;
	}
	if (value < -32768) {
		return -32768;
	}
	return (short)value;
}

}


/// <summary>
/// Initializes a compression stream for decompression, resetting both channels' running
/// predicted sample and step index.
/// </summary>
/// <param name="info">Compression information structure.</param>
extern "C" void __cdecl sosCODECInitStream(_SOS_COMPRESS_INFO * info)
{
	info->wIndex = 0;
	info->dwPredicted = 0;
	info->wIndex2 = 0;
	info->dwPredicted2 = 0;
}


/// <summary>
/// Decompresses data from a 4:1 ADPCM compressed stream.
/// </summary>
/// <param name="info">Compression information structure.</param>
/// <param name="numBytes">Number of destination bytes to decompress.</param>
/// <returns>unsigned long; The number of bytes decompressed.</returns>
extern "C" unsigned long __cdecl sosCODECDecompressData(_SOS_COMPRESS_INFO * info, unsigned long numBytes)
{
	if (info->wBitSize != 16 || info->wChannels != 1) {
		return 0;
	}

	int predicted = (int)info->dwPredicted;
	int index = (unsigned short)info->wIndex / 32;

	unsigned char const * src = (unsigned char const *)info->lpSource;
	short * dst = (short *)info->lpDest;
	unsigned long sampleCount = numBytes / 2;

	for (unsigned long i = 0; i < sampleCount; ++i) {
		unsigned char byte = src[i / 2];
		int nybble = (i & 1) ? (byte >> 4) : (byte & 0xF);
		int flat = index * 16 + nybble;
		predicted = Clamp16(predicted + SosDiffTable[flat]);
		index = SosNextIndexTable[flat];
		dst[i] = (short)predicted;
	}

	info->dwPredicted = predicted;
	info->wIndex = (short)(index * 32);
	return numBytes;
}
