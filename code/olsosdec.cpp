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

// Portable replacement for the inherited olsosdec.asm's HMI "general" ADPCM decoder,
// which handles every wBitSize/wChannels combination sosCODECDecompressData
// (soscodec.asm/soscodec.cpp) does not -- dsaudio.cpp routes to this one whenever
// wBitSize/wChannels isn't 16-bit mono.
//
// This is classic DVI-style ADPCM (a different, simpler formula than the SOS codec's
// precomputed tables): diff = (step>>3) + (code&4 ? step : 0) + (code&2 ? step>>1 : 0) +
// (code&1 ? step>>2 : 0), negated if code&8, then predicted = clamp16(predicted + diff);
// the step index is adjusted by CodecIndexTab[code] and clamped to [0,88], and the new
// step comes from CodecStepTab[index]. Each source byte yields two samples, low nybble
// first then high nybble (tracked via the odd/even parity of the running sample index, as
// the original did). Stereo channels each own every other source byte and write into
// alternating destination slots; 8-bit output takes the high byte of the 16-bit predicted
// sample XORed with the sign bit to convert to unsigned PCM8, matching the original.
//
// Verified bit-exact against the original assembly (compiled and run side by side) across
// randomized multi-chunk streaming decode trials in all four format combinations before
// this file replaced it.

#include "always.h"

#include "soscomp.h"

namespace {

const short CodecIndexTab[16] = { -1,-1,-1,-1,2,4,6,8, -1,-1,-1,-1,2,4,6,8 };

const short CodecStepTab[89] = {
	7,	8,	9,	10,	11,	12,	13,	14,
	16,	17,	19,	21,	23,	25,	28,	31,
	34,	37,	41,	45,	50,	55,	60,	66,
	73,	80,	88,	97,	107,	118,	130,	143,
	157,	173,	190,	209,	230,	253,	279,	307,
	337,	371,	408,	449,	494,	544,	598,	658,
	724,	796,	876,	963,	1060,	1166,	1282,	1411,
	1552,	1707,	1878,	2066,	2272,	2499,	2749,	3024,
	3327,	3660,	4026,	4428,	4871,	5358,	5894,	6484,
	7132,	7845,	8630,	9493,	10442,	11487,	12635,	13899,
	15289,	16818,	18500,	20350,	22385,	24623,	27086,	29794,
	32767,
};

struct ChannelState {
	int predicted;
	int step;
	int index;
	unsigned long sampleIndex;
	int codeBuf;
};

inline int DecodeNybble(int code, int step)
{
	int diff = step >> 3;
	if (code & 4) diff += step;
	if (code & 2) diff += step >> 1;
	if (code & 1) diff += step >> 2;
	if (code & 8) diff = -diff;
	return diff;
}

inline void AdvanceState(ChannelState & ch, int code)
{
	int predicted = ch.predicted + DecodeNybble(code, ch.step);
	if (predicted > 0x7FFF) predicted = 0x7FFF;
	if (predicted < (short)0x8000) predicted = (short)0x8000;
	ch.predicted = predicted;

	int index = ch.index + CodecIndexTab[code];
	index = (index < 0) ? 0 : (index > 88 ? 88 : index);
	ch.index = index;
	ch.step = CodecStepTab[index];
	ch.sampleIndex++;
}

// Fetches the code for the next sample: a new source byte's low nybble on an even sample
// index, or the previously fetched byte's high nybble on an odd one. srcStride is how far
// to advance the source pointer on a fetch (1 for mono, 2 for stereo, since each stereo
// channel only owns every other source byte).
inline int NextCode(ChannelState & ch, unsigned char const *& src, int srcStride)
{
	if (ch.sampleIndex & 1) {
		return (ch.codeBuf >> 4) & 0xF;
	}
	ch.codeBuf = *src;
	src += srcStride;
	return ch.codeBuf & 0xF;
}

}


/// <summary>
/// Initializes a compression stream for decompression, resetting both channels' running
/// predicted sample, step, step index, and sample index.
/// </summary>
/// <param name="info">Compression information structure.</param>
extern "C" void __cdecl General_sosCODECInitStream(_SOS_COMPRESS_INFO * info)
{
	info->wIndex = 0;
	info->wStep = 7;
	info->dwPredicted = 0;
	info->dwSampleIndex = 0;
	info->wIndex2 = 0;
	info->wStep2 = 7;
	info->dwPredicted2 = 0;
	info->dwSampleIndex2 = 0;
}


/// <summary>
/// Decompresses data from a 4:1 DVI-style ADPCM compressed stream, for any bit size or
/// channel count.
/// </summary>
/// <param name="info">Compression information structure.</param>
/// <param name="numBytes">Number of destination bytes to decompress.</param>
/// <returns>unsigned long; The number of bytes decompressed.</returns>
extern "C" unsigned long __cdecl General_sosCODECDecompressData(_SOS_COMPRESS_INFO * info, unsigned long numBytes)
{
	info->dwSampleIndex = 0;
	info->dwSampleIndex2 = 0;

	bool is16 = info->wBitSize == 16;
	bool stereo = info->wChannels == 2;

	if (!stereo) {
		ChannelState ch;
		ch.predicted = info->dwPredicted;
		ch.step = info->wStep;
		ch.index = info->wIndex;
		ch.sampleIndex = info->dwSampleIndex;
		ch.codeBuf = info->wCodeBuf;

		unsigned char const * src = (unsigned char const *)info->lpSource;
		unsigned char * dst = (unsigned char *)info->lpDest;
		unsigned long sampleCount = is16 ? numBytes / 2 : numBytes;

		for (unsigned long i = 0; i < sampleCount; ++i) {
			int code = NextCode(ch, src, 1);
			AdvanceState(ch, code);
			if (is16) {
				*(short *)dst = (short)ch.predicted;
				dst += 2;
			} else {
				*dst = (unsigned char)(((ch.predicted >> 8) & 0xFF) ^ 0x80);
				dst += 1;
			}
		}

		info->dwPredicted = ch.predicted;
		info->wStep = (short)ch.step;
		info->wIndex = (short)ch.index;
		info->dwSampleIndex = ch.sampleIndex;
		info->wCodeBuf = (short)ch.codeBuf;
	} else {
		unsigned long total = is16 ? numBytes / 2 : numBytes;
		unsigned long perChannel = total / 2;

		ChannelState left;
		left.predicted = info->dwPredicted;
		left.step = info->wStep;
		left.index = info->wIndex;
		left.sampleIndex = info->dwSampleIndex;
		left.codeBuf = info->wCodeBuf;

		unsigned char const * srcL = (unsigned char const *)info->lpSource;
		unsigned char * dstL = (unsigned char *)info->lpDest;

		for (unsigned long i = 0; i < perChannel; ++i) {
			int code = NextCode(left, srcL, 2);
			AdvanceState(left, code);
			if (is16) {
				*(short *)dstL = (short)left.predicted;
				dstL += 4;
			} else {
				*dstL = (unsigned char)(((left.predicted >> 8) & 0xFF) ^ 0x80);
				dstL += 2;
			}
		}

		info->dwPredicted = left.predicted;
		info->wStep = (short)left.step;
		info->wIndex = (short)left.index;
		info->dwSampleIndex = left.sampleIndex;
		info->wCodeBuf = (short)left.codeBuf;

		ChannelState right;
		right.predicted = info->dwPredicted2;
		right.step = info->wStep2;
		right.index = info->wIndex2;
		right.sampleIndex = info->dwSampleIndex2;
		right.codeBuf = info->wCodeBuf2;

		unsigned char const * srcR = (unsigned char const *)info->lpSource + 1;
		unsigned char * dstR = (unsigned char *)info->lpDest + (is16 ? 2 : 1);

		for (unsigned long i = 0; i < perChannel; ++i) {
			int code = NextCode(right, srcR, 2);
			AdvanceState(right, code);
			if (is16) {
				*(short *)dstR = (short)right.predicted;
				dstR += 4;
			} else {
				*dstR = (unsigned char)(((right.predicted >> 8) & 0xFF) ^ 0x80);
				dstR += 2;
			}
		}

		info->dwPredicted2 = right.predicted;
		info->wStep2 = (short)right.step;
		info->wIndex2 = (short)right.index;
		info->dwSampleIndex2 = right.sampleIndex;
		info->wCodeBuf2 = (short)right.codeBuf;
	}

	return numBytes;
}
