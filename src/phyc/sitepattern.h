/*
 *  sitepattern.c
 *  PhyC
 *
 *  Created by Mathieu Fourment on 2/15/11.
 *  Copyright (C) 2016 Mathieu Fourment. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program; if not,
 *  write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#ifndef _SITEPATTERN_H_
#define _SITEPATTERN_H_

#include "sequence.h"

#include <stdint.h>

#include "mstring.h"

#include "datatype.h"
#include "mjson.h"

/*static char AMINO_ACIDS[20] = "ACDEFGHIKLMNPQRSTVWY";
static int AMINO_ACID_STATES[128] = {
    25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,	// 0-15
    25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,	// 16-31
    //                                 *        -
    25,25,25,25,25,25,25,25,25,25,23,25,25,25,25,25,	// 32-47
    //                                                ?
    25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,24,	// 48-63
    //		A  B  C  D  E  F  G  H  I  j  K  L  M  N  o
    25, 0,20, 1, 2, 3, 4, 5, 6, 7,24, 8, 9,10,11,24,	// 64-79
    //	 P  Q  R  S  T  u  V  W  X  Y  Z
    12,13,14,15,16,24,17,18,22,19,21,25,25,25,25,25,	// 80-95
    //		A  B  C  D  E  F  G  H  I  j  K  L  M  N  o
    25, 0,20, 1, 2, 3, 4, 5, 6, 7,24, 8, 9,10,11,24,	// 96-111
    //	 P  Q  R  S  T  u  V  W  X  Y  Z
    12,13,14,15,16,24,17,18,22,19,21,25,25,25,25,25		// 112-127
};

static const char NUCLEOTIDES[18] = "ACGTUKMRSWYBDHVN?-";
static const int NUCLEOTIDE_STATES[128] = {
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,	// 0-15
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,	// 16-31
	//                                          -
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,	// 32-47
	//                                                ?
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,16,	// 48-63
	//	    A  B  C  D  e  f  G  H  i  j  K  l  M  N  o
    17, 0,11, 1,12,16,16, 2,13,16,16,10,16, 7,15,16,	// 64-79
	//	 p  q  R  S  T  U  V  W  x  Y  z
    16,16, 5, 9, 3, 3,14, 8,16, 6,16,17,17,17,17,17,	// 80-95
	//	    A  B  C  D  e  f  G  H  i  j  K  l  M  N  o
    17, 0,11, 1,12,16,16, 2,13,16,16,10,16, 7,15,16,	// 96-111
	//	 p  q  R  S  T  U  V  W  x  Y  z
    16,16, 5, 9, 3, 3,14, 8,16, 6,16,17,17,17,17,17		// 112-127
};*/


typedef struct SitePattern{
	int id;
	uint8_t **patterns; // [size][count]
	double **partials; // [size][count]
	double *weights; // [count]
	int *indexes;    // from site index of the alignment to pattern index in **patterns
    int size;        // Length of each pattern (number of sequences)
	int count;       // Number of patterns
	char **names;    // ordered as in the alignment [size][?]
	int nsites;      // sum weights
	int nstate;
    DataType *datatype;
	void (*get_partials)(const struct SitePattern*, size_t, double*);
	int ref_count;
} SitePattern;

SitePattern * new_SitePattern( const Sequences *aln );

SitePattern * new_SitePattern2( const Sequences *aln, int start, int length, int every );

SitePattern ** SitePattern_split( const SitePattern *sitePattern, const int count );

SitePattern* SitePattern_merge( const SitePattern* sitePattern1, const SitePattern* sitePattern2  );

void free_SitePattern( SitePattern *sp );

SitePattern * clone_SitePattern( const SitePattern *sp );


Sequences * SitePattern_to_Sequences( const SitePattern *sp  );

int get_sequence_index( const SitePattern *sp, const char *name );

void compare_sitepattern( const SitePattern *sp1, const SitePattern *sp2 );

void SitePattern_print( const SitePattern *sp );

void SitePattern_save( const SitePattern *sitepattern, const char *output );

double unconstrained_lk( const SitePattern *sp, const unsigned int count);


void SitePattern_sort( SitePattern *sitepattern, int *ordering );

double ** SitePattern_rates( const SitePattern *sp );

unsigned SitePattern_polymorphic_count(SitePattern *sp);

SitePattern* new_SitePattern_from_json(json_node* node, Hashtable* hash);

#endif
