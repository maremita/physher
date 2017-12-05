//
//  operator.h
//  physher
//
//  Created by Mathieu Fourment on 4/12/2017.
//  Copyright © 2017 Mathieu Fourment. All rights reserved.
//

#ifndef operator_h
#define operator_h

#include <stdio.h>

#include "parameters.h"

typedef struct Operator{
	char* name;
	Parameters* x;
	Model* model;
	int index;
	double* parameters;
	int* indexes;
	double weight;
	size_t rejected_count;
	size_t accepted_count;
	bool (*propose)(struct Operator*, double*);
	void (*store)(struct Operator*);
	void (*restore)(struct Operator*);
	void (*optimize)(struct Operator*, double);
}Operator;

Operator* new_Operator_from_json(json_node* node, Hashtable* hash);

void free_Operator(Operator* op);

#endif /* operator_h */
