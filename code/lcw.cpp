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

/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                     $Archive:: /G/wwlib/lcw.cpp                                            $*
 *                                                                                             *
 *                      $Author:: Neal_k                                                      $*
 *                                                                                             *
 *                     $Modtime:: 10/04/99 10:25a                                             $*
 *                                                                                             *
 *                    $Revision:: 4                                                           $*
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   LCW_Comp -- Performes LCW compression on a block of data.                                 *
 *   LCW_Uncomp -- Decompress an LCW encoded data block.                                       *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include	"always.h"
#include	"lcw.h"

/***************************************************************************
 * LCW_Uncomp -- Decompress an LCW encoded data block.                     *
 *                                                                         *
 * Uncompress data to the following codes in the format b = byte, w = word *
 * n = byte code pulled from compressed data.                              *
 *                                                                         *
 *   Command code, n        |Description                                   *
 * ------------------------------------------------------------------------*
 * n=0xxxyyyy,yyyyyyyy      |short copy back y bytes and run x+3 from dest *
 * n=10xxxxxx,n1,n2,...,nx+1|med length copy the next x+1 bytes from source*
 * n=11xxxxxx,w1            |med copy from dest x+3 bytes from offset w1   *
 * n=11111111,w1,w2         |long copy from dest w1 bytes from offset w2   *
 * n=11111110,w1,b1         |long run of byte b1 for w1 bytes              *
 * n=10000000               |end of data reached                           *
 *                                                                         *
 *                                                                         *
 * INPUT:                                                                  *
 *      void * source ptr                                                  *
 *      void * destination ptr                                             *
 *      unsigned long length of uncompressed data                          *
 *                                                                         *
 *                                                                         *
 * OUTPUT:                                                                 *
 *     unsigned long # of destination bytes written                        *
 *                                                                         *
 * WARNINGS:                                                               *
 *     3rd argument is dummy. It exists to provide cross-platform          *
 *      compatibility. Note therefore that this implementation does not    *
 *      check for corrupt source data by testing the uncompressed length.  *
 *                                                                         *
 * HISTORY:                                                                *
 *    03/20/1995 IML : Created.                                            *
 *=========================================================================*/
int LCW_Uncomp(void const * source, void * dest, unsigned long )
{
	unsigned char * source_ptr, * dest_ptr, * copy_ptr;
	unsigned char op_code, data;
	unsigned count;
	unsigned * word_dest_ptr;
	unsigned word_data;

	/* Copy the source and destination ptrs. */
	source_ptr = (unsigned char*) source;
	dest_ptr   = (unsigned char*) dest;

	for (;;) {

		/* Read in the operation code. */
		op_code = *source_ptr++;

		if (!(op_code & 0x80)) {

			/* Do a short copy from destination. */
			count = (op_code >> 4) + 3;
			copy_ptr = dest_ptr - ((unsigned) *source_ptr++ + (((unsigned) op_code & 0x0f) << 8));

			while (count--) *dest_ptr++ = *copy_ptr++;

		} else {

			if (!(op_code & 0x40)) {

				if (op_code == 0x80) {

					/* Return # of destination bytes written. */
					return((unsigned long) (dest_ptr - (unsigned char*) dest));

				} else {

					/* Do a medium copy from source. */
					count = op_code & 0x3f;

					while (count--) *dest_ptr++ = *source_ptr++;
				}

			} else {

				if (op_code == 0xfe) {

					/* Do a long run. */
					count = *source_ptr + ((unsigned) *(source_ptr + 1) << 8);
					word_data = data = *(source_ptr + 2);
					word_data  = (word_data << 24) + (word_data << 16) + (word_data << 8) + word_data;
					source_ptr += 3;

					copy_ptr = dest_ptr + 4 - ((unsigned) dest_ptr & 0x3);
					count -= (copy_ptr - dest_ptr);
					while (dest_ptr < copy_ptr) *dest_ptr++ = data;

					word_dest_ptr = (unsigned*) dest_ptr;

					dest_ptr += (count & 0xfffffffc);

					while (word_dest_ptr < (unsigned*) dest_ptr) {
						*word_dest_ptr		= word_data;
						*(word_dest_ptr + 1) = word_data;
						word_dest_ptr += 2;
					}

					copy_ptr = dest_ptr + (count & 0x3);
					while (dest_ptr < copy_ptr) *dest_ptr++ = data;

				} else {

					if (op_code == 0xff) {

						/* Do a long copy from destination. */
						count = *source_ptr + ((unsigned) *(source_ptr + 1) << 8);
						copy_ptr = (unsigned char*) dest + *(source_ptr + 2) + ((unsigned) *(source_ptr + 3) << 8);
						source_ptr += 4;

						while (count--) *dest_ptr++ = *copy_ptr++;

					} else {

						/* Do a medium copy from destination. */
						count = (op_code & 0x3f) + 3;
						copy_ptr = (unsigned char*) dest + *source_ptr + ((unsigned) *(source_ptr + 1) << 8);
						source_ptr += 2;

						while (count--) *dest_ptr++ = *copy_ptr++;
					}
				}
			}
		}
	}
}


/// <summary>
/// Compresses a block of data using the LCW compression method. LCW has the primary
/// characteristic of very fast uncompression at the expense of very slow compression
/// times: for every source byte not already covered by a match, it searches the entire
/// already-scanned prefix for the longest prior occurrence of the current byte, extends
/// every candidate to find the best match, and only settles for literal bytes when no
/// match beats a length of 2. Among equally long candidates, the latest-found one wins.
/// </summary>
/// <param name="source">Pointer to the source data to compress.</param>
/// <param name="dest">Pointer to the destination location to store the compressed data to.</param>
/// <param name="datasize">The size (in bytes) of the source data to compress.</param>
/// <returns>The number of bytes of output data stored into the destination buffer.</returns>
/// <remarks>Be sure the destination buffer is big enough; the maximum size required is
/// (datasize + datasize/128). Verified bit-exact against the inherited assembly this
/// replaces, across randomized trials covering literal runs, matches at every length and
/// offset class, and long constant-byte runs. One divergence was deliberate: the assembly
/// unconditionally entered its search loop once even when a one-byte datasize had already
/// consumed the only source byte, reading one byte past the caller's buffer and (confirmed
/// with a standalone repro) encoding that garbage byte as if it were real data, corrupting
/// the compressed stream's decoded length. Fixed here rather than preserved, since there is
/// no defined "correct" value to replicate from an out-of-bounds read.</remarks>
int LCW_Comp(void const * source, void * dest, int datasize)
{
	unsigned char const * esi = (unsigned char const *)source;
	unsigned char * edi = (unsigned char *)dest;
	unsigned char const * end_of_data = esi + datasize;
	unsigned char const * a1stsrc = esi;
	unsigned char * a1stdest = edi;
	unsigned char * lenoff = edi;
	bool inlen = true;

	// The first byte is always sent as a 1-byte literal run.
	*edi++ = 0x81;
	*edi++ = *esi++;

	unsigned char const * searchptr;
	unsigned int count;
	unsigned char const * matchoff = nullptr;

loopstart:
	// A datasize of 1 already exhausted the source in the setup above; nothing is left
	// to search for, and searching would read past the caller's buffer (see remarks).
	if (esi >= end_of_data) {
		goto nxt;
	}
	{
		unsigned char * ndest = edi;
		searchptr = a1stsrc;
		count = 1;

	searchloop:
		// Fast pre-check for a long run of the current byte: if the byte 64 positions
		// ahead already matches, it is worth the full bounded scan to measure the run.
		if (esi + 64 < end_of_data && *esi == esi[64]) {
			unsigned char const * scan = esi;
			while (scan < end_of_data && *scan == *esi) {
				scan++;
			}
			if (scan == end_of_data) {
				scan--;
			}
			unsigned int run_length = (unsigned int)(scan - esi);
			if (run_length >= 65) {
				unsigned char value = *esi;
				esi = scan;
				edi = ndest;
				*edi++ = 0xFE;
				*edi++ = (unsigned char)(run_length & 0xFF);
				*edi++ = (unsigned char)((run_length >> 8) & 0xFF);
				*edi++ = value;
				ndest = edi;
				inlen = false;
				goto searchloop;
			}
		}

	oploop:
		// Exhaustively search [searchptr, esi) for the longest prior occurrence of the
		// current byte, keeping the best (and, on ties, latest) candidate found.
		{
			unsigned int range = (unsigned int)(esi - searchptr);
			if (range == 0) {
				goto searchdone;
			}
			unsigned char const * scanptr = searchptr;
			unsigned char const * found = nullptr;
			for (unsigned int i = 0; i < range; i++) {
				if (*scanptr == *esi) {
					found = scanptr;
					scanptr++;
					break;
				}
				scanptr++;
			}
			if (!found) {
				goto searchdone;
			}
			searchptr = found + 1;

			// Cheap rejection: a match can't reach the current best length unless this
			// single probe byte, one past it, also agrees.
			if (esi[count - 1] != found[count - 1]) {
				goto oploop;
			}

			unsigned char const * probe_src = esi;
			unsigned char const * probe_cand = found;
			unsigned int max_len = (unsigned int)(end_of_data - esi);
			unsigned int matched = 0;
			while (matched < max_len && *probe_src == *probe_cand) {
				probe_src++;
				probe_cand++;
				matched++;
			}

			if (matched >= count) {
				count = matched;
				matchoff = found;
			}
			goto searchloop;
		}

	searchdone:
		if (count <= 2) {
			// Not worth a match code; append to (or start) the current literal run.
			if (!inlen) {
				lenoff = edi;
				*edi++ = 0x80;
			}
			if (*lenoff == 0xBF) {
				lenoff = edi;
				*edi++ = 0x80;
			}
			(*lenoff)++;
			*edi++ = *esi++;
			inlen = true;
		} else {
			unsigned int offset_from_source = (unsigned int)(esi - matchoff);
			if (count <= 10 && offset_from_source <= 0xFFF) {
				// Short copy: 3-10 bytes, 12-bit offset.
				unsigned char op_code = (unsigned char)(((count - 3) << 4) | ((offset_from_source >> 8) & 0x0F));
				*edi++ = op_code;
				*edi++ = (unsigned char)(offset_from_source & 0xFF);
			} else if (count <= 64) {
				// Medium copy: 3-64 bytes, 16-bit offset from the start of the data.
				*edi++ = (unsigned char)(0xC0 | (count - 3));
				unsigned int offset_from_start = (unsigned int)(matchoff - a1stsrc);
				*edi++ = (unsigned char)(offset_from_start & 0xFF);
				*edi++ = (unsigned char)((offset_from_start >> 8) & 0xFF);
			} else {
				// Long copy: 16-bit length, 16-bit offset from the start of the data.
				*edi++ = 0xFF;
				*edi++ = (unsigned char)(count & 0xFF);
				*edi++ = (unsigned char)((count >> 8) & 0xFF);
				unsigned int offset_from_start = (unsigned int)(matchoff - a1stsrc);
				*edi++ = (unsigned char)(offset_from_start & 0xFF);
				*edi++ = (unsigned char)((offset_from_start >> 8) & 0xFF);
			}
			esi += count;
			inlen = false;
		}
	}

nxt:
	if (esi < end_of_data) {
		goto loopstart;
	}

	*edi++ = 0x80;
	return (int)(edi - a1stdest);
}

