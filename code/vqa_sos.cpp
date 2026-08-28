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

// Portable replacement for the inherited vqa_sos.asm, VQA movie audio's own HMI SOS
// ADPCM decoder. Same algorithm and tables as soscodec.cpp's sosCODECDecompressData
// (see sos_adpcm_tables.h), but this variant additionally implements 16-bit stereo:
// dwUnCompSize is the combined interleaved byte count for both channels, each channel
// decodes dwUnCompSize/4 samples from its own non-interleaved source block (the right
// channel's compressed nybbles start dwUnCompSize/8 bytes into the source, right after
// the left channel's), and writes into every other 16-bit slot of the interleaved
// destination (left at +0, right at +2). _VQA_SOS_COMPRESS_INFO's wIndex/wIndex2 store
// the step index pre-multiplied by 32, the same on-disk convention as the general
// _SOS_COMPRESS_INFO struct's wIndex.
//
// Verified bit-exact against the original assembly (compiled and run side by side)
// across randomized multi-chunk trials in both mono and stereo before this file
// replaced it.

#include "always.h"

#include "vqalib/cmp.h"

#include "sos_adpcm_tables.h"

#include <cstddef>

namespace {

inline short Clamp16(int v)
{
	if (v > 32767) return 32767;
	if (v < -32768) return -32768;
	return (short)v;
}

// Decodes sampleCount samples starting at predicted/index, writing 16-bit samples into
// dst at destStride bytes apart (2 for mono, 4 for interleaved stereo).
void DecodeChannel(unsigned char const * src, unsigned char * dst, int destStride,
	unsigned long sampleCount, int & predicted, int & index)
{
	for (unsigned long i = 0; i < sampleCount; ++i) {
		unsigned char byte = src[i / 2];
		int nybble = (i & 1) ? (byte >> 4) : (byte & 0xF);
		int flat = index * 16 + nybble;
		predicted = Clamp16(predicted + SosDiffTable[flat]);
		index = SosNextIndexTable[flat];
		*(short *)(dst + (size_t)i * destStride) = (short)predicted;
	}
}

}


/// <summary>
/// Initializes a compression stream for decompression, resetting both channels' running
/// predicted sample and step index.
/// </summary>
/// <param name="info">Compression information structure.</param>
extern "C" void __cdecl VQA_sosCODECInitStream(_VQA_SOS_COMPRESS_INFO * info)
{
	info->wIndex = 0;
	info->dwPredicted = 0;
	info->wIndex2 = 0;
	info->dwPredicted2 = 0;
}


/// <summary>
/// Decompresses 16-bit mono or stereo data from a 4:1 ADPCM compressed stream.
/// </summary>
/// <param name="src">Pointer to compressed source data.</param>
/// <param name="dst">Pointer to the decompression destination.</param>
/// <param name="wBitSize">Sample bit depth; only 16 is implemented.</param>
/// <param name="wChannels">Channel count, 1 or 2.</param>
/// <param name="dwUnCompSize">Combined destination byte count across all channels.</param>
/// <param name="info">Compression information structure.</param>
extern "C" void __cdecl VQA_sosCODECDecompressData(void * src, void * dst, unsigned short wBitSize,
	unsigned short wChannels, unsigned long dwUnCompSize, _VQA_SOS_COMPRESS_INFO * info)
{
	if (wBitSize != 16) {
		return;
	}

	unsigned char const * source = (unsigned char const *)src;
	unsigned char * dest = (unsigned char *)dst;

	if (wChannels == 1) {
		int predicted = (int)info->dwPredicted;
		int index = (unsigned short)info->wIndex / 32;
		unsigned long sampleCount = dwUnCompSize / 2;
		DecodeChannel(source, dest, 2, sampleCount, predicted, index);
		info->dwPredicted = predicted;
		info->wIndex = (short)(index * 32);
		return;
	}

	if (wChannels != 2) {
		return;
	}

	unsigned long perChannelSamples = dwUnCompSize / 4;
	unsigned long perChannelSrcBytes = dwUnCompSize / 8;

	int predL = (int)info->dwPredicted;
	int idxL = (unsigned short)info->wIndex / 32;
	DecodeChannel(source, dest, 4, perChannelSamples, predL, idxL);
	info->dwPredicted = predL;
	info->wIndex = (short)(idxL * 32);

	int predR = (int)info->dwPredicted2;
	int idxR = (unsigned short)info->wIndex2 / 32;
	DecodeChannel(source + perChannelSrcBytes, dest + 2, 4, perChannelSamples, predR, idxR);
	info->dwPredicted2 = predR;
	info->wIndex2 = (short)(idxR * 32);
}
