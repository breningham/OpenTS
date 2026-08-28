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

/* $Header: /CounterStrike/INTERPAL.CPP 1     3/03/97 10:24a Joe_bostic $ */
/***********************************************************************************************
 ***              C O N F I D E N T I A L  ---  W E S T W O O D  S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : INTERPAL.CPP                                                 *
 *                                                                                             *
 *                   Programmer : Steve Tall                                                   *
 *                                                                                             *
 *                   Start Date : December 7th 1995                                            *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Overview:                                                                                   *
 *  This module contains functions to allow use of old 320x200 animations on a 640x400 screen  *
 *                                                                                             *
 * Functions:                                                                                  *
 *  Read_Interpolation_Palette -- reads an interpolation palette table from disk               *
 *  Write_Interpolation_Palette -- writes an interpolation palette to disk                     *
 *  Create_Palette_Interpolation_Table -- build the palette interpolation table                *
 *  Increase_Palette_Luminance -- increase the contrast of a palette                           *
 *  Interpolate_2X_Scale -- Stretch a 320x200 graphic buffer into 640x400                      *
 *                                                                                             *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "always.h"

#include "interpal.h"

#include "ccfile.h"
#include "hsv.h"
#include "misc.h"
#include "palette.h"
#include "surface.h"

#include <algorithm>
#include <cstring>
#include <vector>

bool	InterpolationPaletteChanged = false;

extern "C"{
	unsigned char PaletteInterpolationTable[SIZE_OF_PALETTE][SIZE_OF_PALETTE];
	unsigned char * InterpolationPalette;
}


/***********************************************************************************************
 * Read_Interpolation_Palette -- reads an interpolation palette table from disk                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    name of palette file                                                              *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    12/12/95 12:15PM ST : Created                                                            *
 *=============================================================================================*/

void Read_Interpolation_Palette (char const * palette_file_name)
{
	CCFileClass palette_file(palette_file_name);

	if (palette_file.Is_Available()) {
		palette_file.Open(FileClass::READ);
		palette_file.Read(&PaletteInterpolationTable[0][0], SIZE_OF_PALETTE * SIZE_OF_PALETTE);
		palette_file.Close();
		InterpolationPaletteChanged = FALSE;
	}
}


/***********************************************************************************************
 * Write_Interpolation_Palette -- writes an interpolation palette table to disk                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    name of palette file                                                              *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    12/12/95 12:15PM ST : Created                                                            *
 *=============================================================================================*/

void Write_Interpolation_Palette (char const * palette_file_name)
{
	CCFileClass palette_file(palette_file_name);

	if (!palette_file.Is_Available()) {
		palette_file.Open(FileClass::WRITE);
		palette_file.Write(&PaletteInterpolationTable[0][0], SIZE_OF_PALETTE * SIZE_OF_PALETTE);
		palette_file.Close();
	}
}





/***************************************************************************
 * CREATE_PALETTE_INTERPOLATION_TABLE                                      *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   12/06/1995  MG : Created.                                             *
 *=========================================================================*/
void Create_Palette_Interpolation_Table( void )
{
	int 				i;
	int 				j;
	int 				p;
	unsigned char	* first_palette_ptr;
	unsigned char	* second_palette_ptr;
	unsigned char	* match_pal_ptr;
	int				first_r;
	int				first_g;
	int				first_b;
	int				second_r;
	int				second_g;
	int				second_b;
	int				diff_r;
	int				diff_g;
	int				diff_b;
	int				dest_r;
	int				dest_g;
	int				dest_b;
	int 				distance;
	int 				closest_distance;
	int 				index_of_closest_color;

	//
	// Create an interpolation table for the current palette.
	//
	first_palette_ptr = (unsigned char *) InterpolationPalette;
	for ( i = 0; i < SIZE_OF_PALETTE; i ++ ) {

		//
		// Get the first palette entry's RGB.
		//
		first_r = *first_palette_ptr;
		first_palette_ptr ++;
		first_g = *first_palette_ptr;
		first_palette_ptr ++;
		first_b = *first_palette_ptr;
		first_palette_ptr ++;

		second_palette_ptr = (unsigned char *) InterpolationPalette;
		for  ( j = 0; j < SIZE_OF_PALETTE; j ++ ) {
			//
			// Get the second palette entry's RGB.
			//
			second_r = *second_palette_ptr;
			second_palette_ptr ++;
			second_g = *second_palette_ptr;
			second_palette_ptr ++;
			second_b = *second_palette_ptr;
			second_palette_ptr ++;

			//
			// Now calculate the RGB halfway between the first and second colors.
			//
			dest_r = ( first_r + second_r ) >> 1;
			dest_g = ( first_g + second_g ) >> 1;
			dest_b = ( first_b + second_b ) >> 1;

			//
			// Now find the color in the palette that most closely matches the interpolated color.
			//
			index_of_closest_color = 0;
//			closest_distance = (256 * 256) * 3;
			closest_distance = 500000;
			match_pal_ptr = (unsigned char *) InterpolationPalette;
			for ( p = 0; p < SIZE_OF_PALETTE; p ++ ) {
				diff_r = ( ((int) (*match_pal_ptr)) - dest_r );
				match_pal_ptr ++;
				diff_g = ( ((int) (*match_pal_ptr)) - dest_g );
				match_pal_ptr ++;
				diff_b = ( ((int) (*match_pal_ptr)) - dest_b );
				match_pal_ptr ++;

				distance = ( diff_r * diff_r ) + ( diff_g * diff_g ) + ( diff_b * diff_b );
				if ( distance < closest_distance ) {
					closest_distance = distance;
					index_of_closest_color = p;
				}
			}

			PaletteInterpolationTable[ i ][ j ] = (unsigned char) index_of_closest_color;
		}
	}

	InterpolationPaletteChanged = FALSE;
	return;

}


/***********************************************************************************************
 * Increase_Palette_Luminance -- increase contrast of colours in a palette                     *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    ptr to palette                                                                    *
 *           percentage increase of red                                                        *
 *           percentage increase of green                                                      *
 *           percentage increase of blue                                                       *
 *           cap value for colours                                                             *
 *                                                                                             *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    12/12/95 12:16PM ST : Created                                                            *
 *=============================================================================================*/

void Increase_Palette_Luminance (PaletteClass & palette , double percentage)
{
	for (int i=0 ; i<SIZE_OF_PALETTE; i++) {

		HSVClass hsv = palette[i];

		int value = int(hsv.Get_Value() * percentage);
		value = std::min(value, 255);
		value = std::max(value, 0);
		hsv.Set_Value(value);
		palette[i] = hsv;

	}
}


int	CopyType	=0;

namespace {

// Horizontally interpolates one source line to double width: out[2k] is the
// source pixel itself, out[2k+1] is its blend with the next pixel (0 past the
// last pixel, which has no next).
void Interpolate_Single_Line(unsigned char const * source, unsigned char * dest, int source_width)
{
	for (int k = 0; k < source_width; k++) {
		dest[2 * k] = source[k];
		dest[2 * k + 1] = (k + 1 < source_width) ? PaletteInterpolationTable[source[k + 1]][source[k]] : 0;
	}
}

// Blends two already horizontally-interpolated (double-width) lines pixel by pixel.
void Interpolate_Between_Lines(unsigned char const * line1, unsigned char const * line2, unsigned char * dest, int source_width)
{
	int count = source_width * 2;
	for (int i = 0; i < count; i++) {
		dest[i] = PaletteInterpolationTable[line2[i]][line1[i]];
	}
}

}


/// <summary>
/// Stretches a buffer horizontally to double width by interpolating each line, without any
/// vertical interpolation between lines.
/// </summary>
void Asm_Interpolate(unsigned char * src_ptr, unsigned char * dest_ptr, int lines, int src_width, int dest_width)
{
	for (int y = 0; y < lines; y++) {
		Interpolate_Single_Line(src_ptr, dest_ptr, src_width);
		src_ptr += src_width;
		dest_ptr += dest_width;
	}
}


/// <summary>
/// Stretches a buffer to double width and double height, duplicating each horizontally
/// interpolated line into two consecutive destination rows.
/// </summary>
void Asm_Interpolate_Line_Double(unsigned char * src_ptr, unsigned char * dest_ptr, int lines, int src_width, int dest_width)
{
	int row_stride = dest_width / 2;
	for (int y = 0; y < lines; y++) {
		Interpolate_Single_Line(src_ptr, dest_ptr, src_width);
		memcpy(dest_ptr + row_stride, dest_ptr, (size_t)2 * src_width);
		src_ptr += src_width;
		dest_ptr += dest_width;
	}
}


/// <summary>
/// Stretches a buffer to double width and double height, blending each pair of adjacent
/// horizontally interpolated source lines into an extra vertically interpolated row between
/// them, so a run of N source lines yields 2N-1 destination rows.
/// </summary>
/// <remarks>The inherited assembly wrote its final row by re-interpolating one source line
/// past the end of the buffer instead of reusing the already-computed last line; fixed here
/// rather than preserved, since it read past the caller's buffer.</remarks>
void Asm_Interpolate_Line_Interpolate(unsigned char * src_ptr, unsigned char * dest_ptr, int lines, int src_width, int dest_width)
{
	if (lines <= 0) {
		return;
	}

	int row_stride = dest_width / 2;
	int row_bytes = src_width * 2;

	std::vector<unsigned char> top_line(row_bytes), bottom_line(row_bytes), blend_line(row_bytes);
	unsigned char * next_line = top_line.data();
	unsigned char * last_line = bottom_line.data();

	Interpolate_Single_Line(src_ptr, next_line, src_width);
	std::swap(next_line, last_line);
	src_ptr += src_width;

	for (int remaining = lines - 1; remaining > 0; remaining--) {
		Interpolate_Single_Line(src_ptr, next_line, src_width);
		Interpolate_Between_Lines(last_line, next_line, blend_line.data(), src_width);

		memcpy(dest_ptr, last_line, row_bytes);
		dest_ptr += row_stride;
		memcpy(dest_ptr, blend_line.data(), row_bytes);
		dest_ptr += row_stride;

		src_ptr += src_width;
		std::swap(next_line, last_line);
	}

	memcpy(dest_ptr, last_line, row_bytes);
}


/***************************************************************************
 * INTERPOLATE_2X_SCALE                                                    *
 *                                                                         *
 * INPUT:                                                                  *
 *                                                                         *
 * OUTPUT:                                                                 *
 *                                                                         *
 * WARNINGS:                                                               *
 *                                                                         *
 * HISTORY:                                                                *
 *   12/06/1995  MG : Created.                                             *
 *=========================================================================*/
void Interpolate_2X_Scale( Surface * source, Surface * dest , char const * palette_file_name)
{
	unsigned char	* src_ptr;
	unsigned char	* dest_ptr;
//	unsigned char	* last_dest_ptr;
//	unsigned char	* end_of_source;
	int	src_width;
	int	dest_width;
//	int	width_counter;
	bool	source_locked = FALSE;
	bool	dest_locked = FALSE;


	/*
	**	If a palette table exists on disk then read it in otherwise create it
	*/
	if (InterpolationPaletteChanged) {
		if (palette_file_name) {
			Read_Interpolation_Palette(palette_file_name);
		}
		if (InterpolationPaletteChanged) {
			Create_Palette_Interpolation_Table();
		}
	}

	/*
	**	Write the palette table to disk so we don't have to create it again next time
	*/
	if (palette_file_name) {
		Write_Interpolation_Palette(palette_file_name);
	}


	/*
	 * Lock video surfaces if required.
	 * Get pointers to the source and destination buffers.
	 */
	src_ptr = (unsigned char *)source->Lock();
	if (src_ptr != NULL) {
		source_locked = true;
	}

	dest_ptr = (unsigned char *)dest->Lock();
	if (dest_ptr != NULL) {
		dest_locked = true;
	}

	if (dest_locked && source_locked) {

		//
		// Get width of source and dest buffers.
		//
		src_width = source->Get_Width();
		dest_width = 2*(dest->Stride());

		/*
		**	Call the appropriate assembly language copy routine
		*/
#if (1)
		switch (CopyType) {
			case 0:
				Asm_Interpolate ( src_ptr , dest_ptr , source->Get_Height() , src_width , dest_width);
				break;

			case 1:
				Asm_Interpolate_Line_Double( src_ptr , dest_ptr , source->Get_Height() , src_width , dest_width);
				break;

			case 2:
				Asm_Interpolate_Line_Interpolate( src_ptr , dest_ptr , source->Get_Height() , src_width , dest_width);
				break;
		}
#endif

#if (0)
		//
		// Copy over the first pixel (upper left).
		//
		*dest_ptr = *src_ptr;

		src_ptr ++;
		dest_ptr ++;

		//
		// Scale copy.
		//
		width_counter = 0;
		while ( src_ptr < end_of_source ) {

			//
			// Blend this pixel with the one to the left and place this new color in the dest buffer.
			//
			*dest_ptr = PaletteInterpolationTable[ (*src_ptr) ][ (*( src_ptr - 1 )) ];
			dest_ptr ++;

			//
			// Now place the source pixel into the dest buffer.
			//
			*dest_ptr = *src_ptr;

			src_ptr ++;
			dest_ptr ++;

			width_counter ++;
			if ( width_counter == src_width ) {
				width_counter = 0;
				last_dest_ptr += dest_width;
				dest_ptr = last_dest_ptr;
			}
		}
#endif
	}

	if (source_locked) source->Unlock();
	if (dest_locked) dest->Unlock();
}
