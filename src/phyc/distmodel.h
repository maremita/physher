//
//  distmodel.h
//  physher
//
//  Created by Mathieu Fourment on 3/06/2017.
//  Copyright © 2017 Mathieu Fourment. All rights reserved.
//

#ifndef distmodel_h
#define distmodel_h

#include <stdio.h>

#include "model.h"
#include "parameters.h"
#include "simplex.h"
#include "tree.h"
#include "mjson.h"

#include <gsl/gsl_rng.h>

struct _DistributionModel;
typedef struct _DistributionModel DistributionModel;

typedef enum distribution_parameterization{
	DISTRIBUTION_EXPONENTIAL_MEAN,
	DISTRIBUTION_EXPONENTIAL_RATE,
	DISTRIBUTION_GAMMA_SHAPE_RATE,
	DISTRIBUTION_GAMMA_SHAPE_SCALE,
	DISTRIBUTION_NORMAL_MEAN_SIGMA,
	DISTRIBUTION_NORMAL_MEAN_TAU
}distribution_parameterization;

typedef enum distribution_t{
	DISTRIBUTION_BETA,
	DISTRIBUTION_BETA_PRIME,
	DISTRIBUTION_CAUCHY,
	DISTRIBUTION_DIRICHLET,
	DISTRIBUTION_DISCRETE,
	DISTRIBUTION_EXPONENTIAL,
	DISTRIBUTION_GAMMA,
	DISTRIBUTION_GMRF,
	DISTRIBUTION_KUMARASWAMY,
	DISTRIBUTION_LOGNORMAL,
	DISTRIBUTION_NORMAL,
	DISTRIBUTION_NORMAL_MULTIVARIATE,
	DISTRIBUTION_ONE_ON_X,
	DISTRIBUTION_UNIFORM,
	DISTRIBUTION_WEIBULL
}distribution_t;

struct _DistributionModel{
	distribution_t type;
	Parameters* parameters;
	Parameters* x;
	Simplex* simplex;
	Tree* tree;
	double* tempx; // array to pass to multivariate distributions
	double* tempp;
	double (*logP)(DistributionModel*);
	double (*logP_with_values)(DistributionModel*, const double*);
	double (*dlogP)(DistributionModel*, const Parameter*);
	double (*d2logP)(DistributionModel*, const Parameter*);
	double (*ddlogP)(DistributionModel*, const Parameter*, const Parameter*);
	void (*sample)(DistributionModel*, double*);
	double (*sample_evaluate)(DistributionModel*);
	void (*free)(DistributionModel*);
	DistributionModel* (*clone)(DistributionModel*);
	void* data;
	double lp;
	double stored_lp;
	bool need_update;
	distribution_parameterization parameterization;
	gsl_rng* rng;
	double shift;
};


DistributionModel* new_UniformTreeDistribution(Tree* tree);

DistributionModel* new_DistributionModel(const Parameters* p, const Parameters* x);

DistributionModel* new_DistributionModelSimplex(Parameters* p, Simplex* simplex);

Model* new_DistributionModel2(const char* name, DistributionModel* dm);

Model* new_DistributionModel3(const char* name, DistributionModel* dm, Model* simplex);

Model* new_DistributionModel_from_json(json_node* node, Hashtable* hash);

double DistributionModel_dlog_0(DistributionModel* dm, const Parameter* p);

double DistributionModel_d2log_0(DistributionModel* dm, const Parameter* p);

double DistributionModel_ddlog_0(DistributionModel* dm, const Parameter* p1, const Parameter* p2);

#endif /* distmodel_h */
