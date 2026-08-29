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

/***************************************************************************
 **      C O N F I D E N T I A L --- W E S T W O O D   S T U D I O S      **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Westwood 32 bit Library                  *
 *                                                                         *
 *                    File Name : AUDIO.H                                  *
 *                                                                         *
 *                   Programmer : Phil W. Gorrow                           *
 *                                                                         *
 *                   Start Date : March 10, 1995                           *
 *                                                                         *
 *                  Last Update : March 10, 1995   [PWG]                   *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#pragma once

#include "win.h"

/*=========================================================================*/
/* AUD file header type																		*/
/*=========================================================================*/
#define	AUD_FLAG_STEREO	1
#define	AUD_FLAG_16BIT		2

// PWG 3-14-95: This structure used to have bit fields defined for Stereo
//   and Bits.  These were removed because watcom packs them into a 32 bit
//   flag entry even though they could have fit in a 8 bit entry.
#pragma pack(push,1)
struct AUDHeaderType {
	unsigned short int	Rate;   // Playback rate (hertz).
	int	Size;               // Size of data (bytes).
	int	UncompSize;         // Size of data (bytes).
	unsigned char Flags;        // Holds flags for info
								//  1: Is the sample stereo?
								//  2: Is the sample 16 bits?
	unsigned char Compression;	// What kind of compression for this sample?
};
#pragma pack(pop)

/*
**	Function to call if we detect focus loss
*/
extern	void (*Audio_Focus_Loss_Function)(void);


extern int Misc;

extern int StreamLowImpact;
