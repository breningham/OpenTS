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

// Portable replacement for six of the ten VQ full-frame decoders inherited in
// unvq_asm.asm; ASM_UnVQ_6/8/9/10 are dropped, since nothing calls them (only
// VQA_SetUnVQ's call sites in vqa.cpp/drawer.cpp reach the other six).
//
// All six share one pointer format: a "pointers" stream holds blocksperrow*numrows
// low-index bytes, immediately followed by the same count of high-index bytes (two
// parallel arrays back to back, not interleaved). Each block reads one low/high byte
// pair; if the block is a "one color" block (checked either as a full 0xFF high byte,
// for the 8-bit ColorMode-0 variants, or the high bit of the 15-bit combined index, for
// the 16-bit ColorMode-1 variants), the fill color comes from the low byte directly (or,
// for the "TABLE" variants, a HicolorTable[] lookup) instead of the codebook.

#include "always.h"

#include "_vqa.h"

#include <cstddef>
#include <cstdint>

namespace {

// Decodes blocksperrow*numrows blocks of one-byte-per-pixel (ColorMode 0) blocks,
// blockW x blockH pixels each, reading a codebook entry of codewordStride bytes per
// block (rowBytes bytes per row, consecutive rows), or filling with a duplicated byte
// for "one color" blocks (signalled by a 0xFF high index byte).
template <int blockW, int blockH, int codewordStride>
void DecodeC0Block(unsigned char const * codebook, unsigned char const * pointers,
	unsigned char * buffer, unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	unsigned long entries = blocksperrow * numrows;
	unsigned char const * lo = pointers;
	unsigned char const * hi = pointers + entries;

	unsigned char * rowBase = buffer;
	unsigned long rowStride = bufwidth * blockH;

	for (unsigned long r = 0; r < numrows; ++r) {
		unsigned char * dst = rowBase;
		for (unsigned long c = 0; c < blocksperrow; ++c) {
			unsigned char l = *lo++;
			unsigned char h = *hi++;

			if (h == 0xFF) {
				for (int y = 0; y < blockH; ++y) {
					for (int x = 0; x < blockW; ++x) {
						dst[(size_t)y * bufwidth + x] = l;
					}
				}
			} else {
				unsigned int index = ((unsigned int)h << 8) | l;
				unsigned char const * cb = codebook + (size_t)index * codewordStride;
				for (int y = 0; y < blockH; ++y) {
					for (int x = 0; x < blockW; ++x) {
						dst[(size_t)y * bufwidth + x] = cb[(size_t)y * blockW + x];
					}
				}
			}

			dst += blockW;
		}
		rowBase += rowStride;
	}
}

}


/// <summary>
/// Draws a VQ-compressed frame into a 16-bit buffer, 4x4 pixels per block, using
/// HicolorTable to resolve solid-color blocks.
/// </summary>
extern "C" void __cdecl ASM_UnVQ1_C1_TABLE(unsigned char * codebook, unsigned char * pointers,
	unsigned char * buffer, unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	unsigned long bufwidthBytes = bufwidth * 2;
	unsigned long entries = blocksperrow * numrows;
	unsigned char const * lo = pointers;
	unsigned char const * hi = pointers + entries;

	unsigned char * rowBase = buffer;
	unsigned long rowStride = bufwidthBytes * 4;

	for (unsigned long r = 0; r < numrows; ++r) {
		unsigned char * dst = rowBase;
		for (unsigned long c = 0; c < blocksperrow; ++c) {
			unsigned char l = *lo++;
			unsigned char h = *hi++;

			if (h & 0x80) {
				unsigned int index = (((unsigned int)h << 8) | l) & 0x7FFF;
				uint16_t color = HicolorTable[index];
				for (int y = 0; y < 4; ++y) {
					uint16_t * p = (uint16_t *)(dst + (size_t)y * bufwidthBytes);
					p[0] = color; p[1] = color; p[2] = color; p[3] = color;
				}
			} else {
				unsigned int index = ((unsigned int)h << 8) | l;
				uint16_t const * cb = (uint16_t const *)(codebook + (size_t)index * 32);
				for (int y = 0; y < 4; ++y) {
					uint16_t * p = (uint16_t *)(dst + (size_t)y * bufwidthBytes);
					uint16_t const * s = cb + (size_t)y * 4;
					p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = s[3];
				}
			}

			dst += 8;
		}
		rowBase += rowStride;
	}
}


/// <summary>
/// Draws a VQ-compressed frame into a 16-bit buffer, 4 pixels wide, sampling codebook
/// rows 0 and 2 into output rows 0 and 2 (rows 1 and 3 of each 4-row macro-block are left
/// untouched), using HicolorTable to resolve solid-color blocks.
/// </summary>
extern "C" void __cdecl ASM_UnVQ1_C1_TABLE_ALT(unsigned char * codebook, unsigned char * pointers,
	unsigned char * buffer, unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	unsigned long bufwidthBytes = bufwidth * 2;
	unsigned long entries = blocksperrow * numrows;
	unsigned char const * lo = pointers;
	unsigned char const * hi = pointers + entries;

	unsigned char * rowBase = buffer;
	unsigned long rowStride = bufwidthBytes * 4;

	for (unsigned long r = 0; r < numrows; ++r) {
		unsigned char * dst = rowBase;
		for (unsigned long c = 0; c < blocksperrow; ++c) {
			unsigned char l = *lo++;
			unsigned char h = *hi++;

			if (h & 0x80) {
				unsigned int index = (((unsigned int)h << 8) | l) & 0x7FFF;
				uint16_t color = HicolorTable[index];
				for (int y = 0; y < 2; ++y) {
					uint16_t * p = (uint16_t *)(dst + (size_t)y * 2 * bufwidthBytes);
					p[0] = color; p[1] = color; p[2] = color; p[3] = color;
				}
			} else {
				unsigned int index = ((unsigned int)h << 8) | l;
				uint16_t const * cb = (uint16_t const *)(codebook + (size_t)index * 32);
				for (int y = 0; y < 2; ++y) {
					uint16_t * p = (uint16_t *)(dst + (size_t)y * 2 * bufwidthBytes);
					uint16_t const * s = cb + (size_t)y * 2 * 4;
					p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = s[3];
				}
			}

			dst += 8;
		}
		rowBase += rowStride;
	}
}


/// <summary>
/// Draws a VQ-compressed frame into an 8-bit buffer, 4x2 pixels per block.
/// </summary>
extern "C" void __cdecl ASM_UnVQ_4x2(unsigned char * codebook, unsigned char * pointers,
	unsigned char * buffer, unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	DecodeC0Block<4, 2, 8>(codebook, pointers, buffer, blocksperrow, numrows, bufwidth);
}


/// <summary>
/// Draws a VQ-compressed frame into an 8-bit buffer, 4x4 pixels per block.
/// </summary>
extern "C" void __cdecl ASM_UnVQ_4x4(unsigned char * codebook, unsigned char * pointers,
	unsigned char * buffer, unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	DecodeC0Block<4, 4, 16>(codebook, pointers, buffer, blocksperrow, numrows, bufwidth);
}


/// <summary>
/// Draws a VQ-compressed frame into an 8-bit buffer, sampling every other pixel of each
/// 4x4 codebook entry (columns and rows 0 and 2) into a dense 2x2 output block.
/// </summary>
extern "C" void __cdecl ASM_UnVQ_4x4_HALF(unsigned char * codebook, unsigned char * pointers,
	unsigned char * buffer, unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	unsigned long entries = blocksperrow * numrows;
	unsigned char const * lo = pointers;
	unsigned char const * hi = pointers + entries;

	unsigned char * rowBase = buffer;
	unsigned long rowStride = bufwidth * 2;

	for (unsigned long r = 0; r < numrows; ++r) {
		unsigned char * dst = rowBase;
		for (unsigned long c = 0; c < blocksperrow; ++c) {
			unsigned char l = *lo++;
			unsigned char h = *hi++;

			if (h == 0xFF) {
				dst[0] = l; dst[1] = l;
				dst[bufwidth] = l; dst[bufwidth + 1] = l;
			} else {
				unsigned int index = ((unsigned int)h << 8) | l;
				unsigned char const * cb = codebook + (size_t)index * 16;
				dst[0] = cb[0]; dst[1] = cb[2];
				dst[bufwidth] = cb[8]; dst[bufwidth + 1] = cb[10];
			}

			dst += 2;
		}
		rowBase += rowStride;
	}
}


/// <summary>
/// Draws a VQ-compressed frame into a 16-bit buffer, 4x4 pixels per block, using the
/// combined index directly as the fill color for solid-color blocks (no HicolorTable
/// lookup).
/// </summary>
extern "C" void __cdecl ASM_UnVQ1_C1_4x4(unsigned char * codebook, unsigned char * pointers,
	unsigned char * buffer, unsigned long blocksperrow, unsigned long numrows, unsigned long bufwidth)
{
	unsigned long bufwidthBytes = bufwidth * 2;
	unsigned long entries = blocksperrow * numrows;
	unsigned char const * lo = pointers;
	unsigned char const * hi = pointers + entries;

	unsigned char * rowBase = buffer;
	unsigned long rowStride = bufwidthBytes * 4;

	for (unsigned long r = 0; r < numrows; ++r) {
		unsigned char * dst = rowBase;
		for (unsigned long c = 0; c < blocksperrow; ++c) {
			unsigned char l = *lo++;
			unsigned char h = *hi++;

			if (h & 0x80) {
				uint16_t color = (uint16_t)((((unsigned int)h << 8) | l) & 0x7FFF);
				for (int y = 0; y < 4; ++y) {
					uint16_t * p = (uint16_t *)(dst + (size_t)y * bufwidthBytes);
					p[0] = color; p[1] = color; p[2] = color; p[3] = color;
				}
			} else {
				unsigned int index = ((unsigned int)h << 8) | l;
				uint16_t const * cb = (uint16_t const *)(codebook + (size_t)index * 32);
				for (int y = 0; y < 4; ++y) {
					uint16_t * p = (uint16_t *)(dst + (size_t)y * bufwidthBytes);
					uint16_t const * s = cb + (size_t)y * 4;
					p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = s[3];
				}
			}

			dst += 8;
		}
		rowBase += rowStride;
	}
}
