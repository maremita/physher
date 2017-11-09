//
//  simplex.h
//  physher
//
//  Created by Mathieu Fourment on 7/11/2017.
//  Copyright © 2017 Mathieu Fourment. All rights reserved.
//

#ifndef simplex_h
#define simplex_h

#include <stdio.h>
#include "parameters.h"



struct _Simplex;
typedef struct _Simplex Simplex;


struct _Simplex{
	int K;
	//Parameters* cparameters; // K parameters
	Parameters* parameters; // K-1 parameters
	double* values; // K double
	const double* (*get_values)(Simplex*);
	double (*get_value)(Simplex*, int);
	void (*set_values)(Simplex*, const double*);
	
	void (*set_parameter_value)(Simplex*, int, double);
	bool need_update;
};

Simplex* new_Simplex_with_values(const double *x, int K);

Simplex* new_Simplex(int K);

void free_Simplex(Simplex* simplex);

Simplex* clone_Simplex(const Simplex* simplex);

Model * new_SimplexModel( const char* name, Simplex *simplex );

#endif /* simplex_h */
