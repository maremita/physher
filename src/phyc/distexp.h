//
//  distexp.h
//  physher
//
//  Created by Mathieu Fourment on 15/03/2019.
//  Copyright © 2019 Mathieu Fourment. All rights reserved.
//

#ifndef distexp_h
#define distexp_h

#include <stdio.h>

#include "parameters.h"
#include "hashtable.h"
#include "mjson.h"
#include "distmodel.h"

Model* new_ExponentialDistributionModel_from_json(json_node* node, Hashtable* hash);

DistributionModel* new_ExponentialDistributionModel(const double lambda, const Parameters* x);

#endif /* distexp_h */
