//
//  gradascent.h
//  physher
//
//  Created by Mathieu Fourment on 30/11/2017.
//  Copyright © 2017 Mathieu Fourment. All rights reserved.
//

#ifndef gradascent_h
#define gradascent_h

#include <stdio.h>
#include "optimizer.h"

opt_result optimize_stochastic_gradient(Parameters* parameters, opt_func f, opt_grad_func grad_f, double eta, void *data, OptStopCriterion *stop, double *fmin);

#endif /* gradascent_h */
