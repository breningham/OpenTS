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

// Portable replacement for the inherited vqa_uncomp.asm. OLD_VQA_LCW_Uncompress, the
// third function that file defined, had no callers anywhere (superseded by
// VQA_LCW_Uncompress below) and is dropped rather than ported.
//
// AudioUnzap decodes the same "zapped" delta-coded audio format as the removed dead
// auduncmp.asm did, called from vqalib/loader.cpp for VQA movie audio. Each code byte's
// top 2 bits select 2-bit deltas, 4-bit deltas, a raw byte dump or a single signed 5-bit
// delta, or silence (repeat); the bottom 6 bits are a repeat/length count. Like the
// original, a single code's expansion is not clamped to the caller's requested byte
// count -- only checked between codes -- so callers must size the destination buffer with
// headroom past what they asked for, exactly as before.
//
// VQA_LCW_Uncompress decodes the same LCW format documented in lcwuncmp.cpp's
// LCW_Uncompress, but is not a drop-in replacement for it: a leading zero byte in the
// source switches every back-reference in the medium-run/long-copy opcodes (0xC0-0xFD,
// 0xFF) from absolute-from-buffer-start offsets to relative-from-current-position
// offsets, matching whichever encoder produced this particular VQA asset.
//
// Both verified bit-exact against the original assembly (compiled and run side by side)
// across randomized trials before this file replaced it.

#include "always.h"

#include <cstring>

namespace {

const signed char TwoBitDecode[4] = { -2, -1, 0, 1 };
const signed char FourBitDecode[16] = { -9, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8 };

inline unsigned char SatAdd8(unsigned char prev, int delta)
{
	int v = (int)prev + delta;
	if (v < 0) return 0;
	if (v > 0xFF) return 0xFF;
	return (unsigned char)v;
}

}


/// <summary>
/// Decompresses "zapped" delta-coded audio sample data into a buffer.
/// </summary>
/// <param name="source">Pointer to encoded audio data.</param>
/// <param name="dest">Pointer to buffer to decompress into.</param>
/// <param name="count">Maximum size of dest buffer.</param>
/// <returns>long; The number of source bytes consumed.</returns>
extern "C" long __cdecl AudioUnzap(void * source, void * dest, long count)
{
	if (source == nullptr || dest == nullptr || count == 0) {
		return 0;
	}

	unsigned char const * src = (unsigned char const *)source;
	unsigned char * dst = (unsigned char *)dest;
	long remaining = count;
	long incount = 0;
	unsigned char previous = 0x80;

	while (remaining > 0) {
		unsigned char byte = *src++;
		incount++;
		int code = byte >> 6;
		int subcode = byte & 0x3F;

		if (code == 2) { // raw sequence, or a single signed 5-bit delta
			if (subcode & 0x20) {
				signed char delta = (signed char)(subcode << 3) >> 3;
				previous = (unsigned char)(previous + delta);
				*dst++ = previous;
				remaining--;
			} else {
				int rawCount = subcode + 1;
				for (int i = 0; i < rawCount; ++i) {
					*dst++ = *src++;
				}
				previous = dst[-1];
				incount += rawCount;
				remaining -= rawCount;
			}
			continue;
		}

		int repeatCount = subcode + 1;

		if (code == 1) { // 4-bit deltas
			for (int i = 0; i < repeatCount; ++i) {
				unsigned char byte2 = *src++;
				incount++;
				previous = SatAdd8(previous, FourBitDecode[byte2 & 0xF]);
				*dst++ = previous;
				previous = SatAdd8(previous, FourBitDecode[(byte2 >> 4) & 0xF]);
				*dst++ = previous;
			}
			remaining -= repeatCount * 2;
		} else if (code == 0) { // 2-bit deltas
			for (int i = 0; i < repeatCount; ++i) {
				unsigned char byte2 = *src++;
				incount++;
				for (int shift = 0; shift < 8; shift += 2) {
					previous = SatAdd8(previous, TwoBitDecode[(byte2 >> shift) & 0x3]);
					*dst++ = previous;
				}
			}
			remaining -= repeatCount * 4;
		} else { // silence: repeat the previous sample
			memset(dst, previous, repeatCount);
			dst += repeatCount;
			remaining -= repeatCount;
		}
	}

	return incount;
}


/// <summary>
/// Decompresses LCW-encoded data. See lcwuncmp.cpp's LCW_Uncompress for the shared opcode
/// format; a leading zero byte here additionally switches medium-run/long-copy
/// back-references to relative-from-current-position addressing.
/// </summary>
/// <param name="source">Pointer to compressed data.</param>
/// <param name="dest">Pointer to decompression area.</param>
/// <returns>unsigned long; The number of destination bytes written.</returns>
extern "C" unsigned long __cdecl VQA_LCW_Uncompress(char const * source, char * dest, unsigned long)
{
	unsigned char const * src = (unsigned char const *)source;
	unsigned char * destBase = (unsigned char *)dest;
	unsigned char * dst = destBase;

	bool relative = (*src == 0);
	if (relative) {
		src++;
	}

	for (;;) {
		unsigned char op = *src++;

		if (!(op & 0x80)) {
			// Short run: back up (offset) bytes from current dest and copy op>>4 + 3 bytes.
			int count = (op >> 4) + 3;
			unsigned offset = (unsigned)*src++ + (((unsigned)op & 0x0F) << 8);
			unsigned char const * copy = dst - offset;
			while (count--) {
				*dst++ = *copy++;
			}
			continue;
		}

		if (!(op & 0x40)) {
			if (op == 0x80) {
				break;
			}
			// Medium length copy from source: next (op&0x3F) bytes copied verbatim.
			int count = op & 0x3F;
			while (count--) {
				*dst++ = *src++;
			}
			continue;
		}

		if (op == 0xFE) {
			// Long run: word count, single repeated byte.
			unsigned count = (unsigned)src[0] + ((unsigned)src[1] << 8);
			unsigned char value = src[2];
			src += 3;
			while (count--) {
				*dst++ = value;
			}
			continue;
		}

		unsigned count;
		if (op == 0xFF) {
			// Long copy: word count, then word offset.
			count = (unsigned)src[0] + ((unsigned)src[1] << 8);
			src += 2;
		} else {
			// Medium run: (op&0x3F)+3 count, then word offset.
			count = (op & 0x3F) + 3;
		}

		unsigned offset = (unsigned)src[0] + ((unsigned)src[1] << 8);
		src += 2;
		unsigned char const * copy = relative ? (dst - offset) : (destBase + offset);
		while (count--) {
			*dst++ = *copy++;
		}
	}

	return (unsigned long)(dst - destBase);
}
