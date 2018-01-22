//
//  mcmc.c
//  physher
//
//  Created by Mathieu Fourment on 2/12/2017.
//  Copyright © 2017 Mathieu Fourment. All rights reserved.
//

#include "mcmc.h"

#include <strings.h>
#include <stdio.h>

#include "random.h"
#include "matrix.h"
#include "compoundmodel.h"
#include "treelikelihood.h"


void log_log(Log* logger, size_t iter){
	fprintf(logger->file, "%zu", iter);
	for (int i = 0; i < logger->model_count; i++) {
		fprintf(logger->file, "\t%e", logger->models[i]->logP(logger->models[i]));
	}
	for (int i = 0; i < Parameters_count(logger->x); i++) {
		fprintf(logger->file, "\t%e", Parameters_value(logger->x, i));
	}
	for (int i = 0; i < logger->simplex_count; i++) {
		Simplex* simplex = logger->simplexes[i]->obj;
		for (int j = 0; j < simplex->K; j++) {
			fprintf(logger->file, "\t%e", simplex->get_value(simplex, j));
		}
	}
	
	fprintf(logger->file, "\n");
}

void log_log_cpo(Log* logger, size_t iter){
	fprintf(logger->file, "%zu", iter);
	Model* treelikelihood = logger->models[0];
	SingleTreeLikelihood* tlk = treelikelihood->obj;
	
	for (int i = 0; i < tlk->sp->count; i++) {
		fprintf(logger->file, "\t%e", tlk->pattern_lk[i]);
	}
	fprintf(logger->file, "\n");
}

void log_log_with(Log* logger, size_t iter, const char* more){
	fprintf(logger->file, "%zu", iter);
	for (int i = 0; i < logger->model_count; i++) {
		fprintf(logger->file, "\t%e", logger->models[i]->logP(logger->models[i]));
	}
	for (int i = 0; i < Parameters_count(logger->x); i++) {
		fprintf(logger->file, "\t%e", Parameters_value(logger->x, i));
	}
	for (int i = 0; i < logger->simplex_count; i++) {
		Simplex* simplex = logger->simplexes[i]->obj;
		for (int j = 0; j < simplex->K; j++) {
			fprintf(logger->file, "\t%e", simplex->get_value(simplex, j));
		}
	}
	
	fprintf(logger->file, "\t%s\n", more);
}

void log_initialize(Log* logger){
	if (logger->filename != NULL) {
		char a[2] = "w";
		if(logger->append) a[0] = 'a';
		logger->file = fopen(logger->filename, a);
	}
}

void log_finalize(Log* logger){
	if (logger->filename != NULL) {
		fclose(logger->file);
	}
}

Log* new_Log_from_json(json_node* node, Hashtable* hash){
	Log* logger = malloc(sizeof(Log));
	logger->x = new_Parameters(1);
	json_node* header_node = get_json_node(node, "header");
	json_node* x_node = get_json_node(node, "x");
	if (x_node != NULL) {
		get_parameters_references2(node, hash, logger->x, "x");
	}

	logger->every = get_json_node_value_size_t(node, "every", 1000);
	json_node* filename_node = get_json_node(node, "file");
	logger->write = log_log;
	logger->write_with = log_log_with;
	logger->file = stdout;
	logger->filename = NULL;
	if (filename_node != NULL) {
		char* filename = (char*)filename_node->value;
		if (strcasecmp(filename, "stderr") == 0) {
			logger->file = stderr;
		}
		else if (strcasecmp(filename, "stdout") != 0) {
			logger->filename = String_clone(filename);
			logger->append = get_json_node_value_bool(node, "append", false);
			logger->file = NULL;
		}
	}
	
	json_node* models_node = get_json_node(node, "models");
	logger->cpo = get_json_node_value_bool(node, "cpo", false);
	
	if (logger->cpo) {
		logger->write = log_log_cpo;
	}
	
	logger->model_count = 0;
	logger->models = NULL;
	if (models_node != NULL) {
		if (models_node->node_type == MJSON_ARRAY) {
			for (int i = 0; i < models_node->child_count; i++) {
				json_node* child = models_node->children[i];
				char* child_string = child->value;
				Model* m = Hashtable_get(hash, child_string+1);
				if(logger->model_count == 0){
					logger->models = malloc(sizeof(Model*));
				}
				else{
					logger->models = realloc(logger->models, sizeof(Model*)*(logger->model_count+1));
				}
				logger->models[i] = m;
				logger->model_count++;
			}
		}
		else if (models_node->node_type == MJSON_STRING) {
			char* ref = models_node->value;
			logger->models = malloc(sizeof(Model*));
			logger->models[0] = Hashtable_get(hash, ref+1);;
			logger->model_count++;
		}
	}
	
	json_node* simplexes_node = get_json_node(node, "simplexes");
	logger->simplex_count = 0;
	logger->simplexes = NULL;
	if (simplexes_node != NULL) {
		if (simplexes_node->node_type == MJSON_ARRAY) {
			for (int i = 0; i < simplexes_node->child_count; i++) {
				json_node* child = simplexes_node->children[i];
				char* child_string = child->value;
				Model* m = Hashtable_get(hash, child_string+1);
				if(logger->simplex_count == 0){
					logger->simplexes = malloc(sizeof(Model*));
				}
				else{
					logger->simplexes = realloc(logger->simplexes, sizeof(Model*)*(logger->simplex_count+1));
				}
				logger->simplexes[i] = m;
				logger->simplex_count++;
			}
		}
		else if (simplexes_node->node_type == MJSON_STRING) {
			char* ref = simplexes_node->value;
			logger->simplexes = malloc(sizeof(Model*));
			logger->simplexes[0] = Hashtable_get(hash, ref+1);
			logger->simplex_count++;
		}
	}
	
	logger->initialize = log_initialize;
	logger->finalize = log_finalize;
	
	return logger;
}

void free_Log(Log* logger){
	//printf("free logger");
	free_Parameters(logger->x);
	if (logger->filename != NULL) {
//		printf("close logger");
		free(logger->filename);
	}
	if(logger->file != NULL){
		fclose(logger->file);
	}
	if(logger->model_count > 0){
		free(logger->models);
	}
	free(logger);
}

void _mcmc_write_header(Log* logger, bool save_cold_ll){
	StringBuffer* buffer = new_StringBuffer(10);
	if(logger->cpo){
		Model* treelikelihood = logger->models[0];
		SingleTreeLikelihood* tlk = treelikelihood->obj;

		for (int i = 0; i < tlk->sp->count; i++) {
			StringBuffer_empty(buffer);
			StringBuffer_append_format(buffer, "%f", tlk->sp->weights[i]);
			fprintf(logger->file, "%c%s", (i == 0 ? '#': '\t'), buffer->c);
		}
		fprintf(logger->file, "\niter");
		for (int i = 0; i < tlk->sp->count; i++) {
			StringBuffer_set_string(buffer, "p");
			StringBuffer_append_format(buffer, "%d", i);
			fprintf(logger->file, "\t%s", buffer->c);
		}
	}
	else{
		fprintf(logger->file, "iter");
		for (int i = 0; i < logger->model_count; i++) {
			fprintf(logger->file, "\t%s", logger->models[i]->name);
		}
		for (int i = 0; i < Parameters_count(logger->x); i++) {
			fprintf(logger->file, "\t%s", Parameters_name(logger->x, i));
		}
		for (int i = 0; i < logger->simplex_count; i++) {
			Simplex* simplex = logger->simplexes[i]->obj;
			for (int j = 0; j < simplex->K; j++) {
				StringBuffer_set_string(buffer, logger->simplexes[i]->name);
				StringBuffer_append_format(buffer, "%d", (j+1));
				fprintf(logger->file, "\t%s", buffer->c);
			}
		}
		if(save_cold_ll){
			fprintf(logger->file, "\t%s", "coldll");
		}
	}
	free_StringBuffer(buffer);
	fprintf(logger->file, "\n");
}

void run(MCMC* mcmc){
	Model* model = mcmc->model;
	
	size_t iter = 0;
	
	int tuneFrequency = 10;
	
	double* weights = dvector(mcmc->operator_count);
	double sum = 0;
	for (int i = 0; i < mcmc->operator_count; i++) {
		sum += mcmc->operators[i]->weight;
	}
	for (int i = 0; i < mcmc->operator_count; i++) {
		weights[i] = mcmc->operators[i]->weight/sum;
	}
	
	StringBuffer *buffer = new_StringBuffer(20);
	double logP;
	double logP2; // not heated
	
	// Initialize loggers
	for (int i = 0; i < mcmc->log_count; i++) {
		mcmc->logs[i]->initialize(mcmc->logs[i]);
	}
	
	// loggers log headers
	if(mcmc->chain_temperature < 0){
		logP = model->logP(model);
		for (int i = 0; i < mcmc->log_count; i++) {
			_mcmc_write_header(mcmc->logs[i], false);
			mcmc->logs[i]->write(mcmc->logs[i], iter);
		}
	}
	else{
		CompoundModel* cm = (CompoundModel*)model->obj;
		logP2 = cm->models[0]->logP(cm->models[0]);
		logP = logP2 * mcmc->chain_temperature;
		for (int i = 1; i < cm->count; i++) {
			logP += cm->models[i]->logP(cm->models[i]);
		}

		StringBuffer_empty(buffer);
		StringBuffer_append_format(buffer, "%e", logP2);
		for (int i = 0; i < mcmc->log_count; i++) {
			_mcmc_write_header(mcmc->logs[i], true);
			mcmc->logs[i]->write_with(mcmc->logs[i], iter, buffer->c);
		}
	}
	
	while ( iter < mcmc->chain_length) {
		
		// Choose operator
		int op_index = roulette_wheel(weights, mcmc->operator_count);
		Operator* op = mcmc->operators[op_index];
		
		double logHR = 0;
		
		op->store(op);
		bool success = op->propose(op, &logHR);
		
		// operator did not propose a valid value
		if (success == false) {
			iter++;
			continue;
		}
		double proposed_logP;
		double proposed_logP2;
		if(mcmc->chain_temperature < 0){
			proposed_logP = model->logP(model);
		}
		else{
			CompoundModel* cm = (CompoundModel*)model->obj;
			proposed_logP2 = cm->models[0]->logP(cm->models[0]);
			proposed_logP = proposed_logP2 * mcmc->chain_temperature;
			for (int i = 1; i < cm->count; i++) {
				proposed_logP += cm->models[i]->logP(cm->models[i]);
			}
		}
		
		double alpha = proposed_logP - logP + logHR;
		
		// accept
		if ( alpha >=  0 || alpha > log(random_double()) ) {
//			printf("%zu %f %f\n", iter, logP, proposed_logP);
			logP = proposed_logP;
			logP2 = proposed_logP2;
			op->accepted_count++;
		}
		// reject
		else {
//			printf("%zu %f %f *\n", iter, logP, proposed_logP);
			op->restore(op);
			op->rejected_count++;
		}
		
		iter++;
		
		if(op->optimize != NULL){// && iter % tuneFrequency == 0){
			op->optimize(op, alpha);
		}
		
		if(mcmc->chain_temperature < 0){
			for (int i = 0; i < mcmc->log_count; i++) {
				if(iter % mcmc->logs[i]->every == 0){
					mcmc->logs[i]->write(mcmc->logs[i], iter);
				}
			}
		}
		else{
			StringBuffer_empty(buffer);
			StringBuffer_append_format(buffer, "%e", logP2);
			for (int i = 0; i < mcmc->log_count; i++) {
				if(iter % mcmc->logs[i]->every == 0){
					mcmc->logs[i]->write_with(mcmc->logs[i], iter, buffer->c);
				}
			}
		}
	}
	
	for (int i = 0; i < mcmc->log_count; i++) {
		mcmc->logs[i]->finalize(mcmc->logs[i]);
	}
	
	if( mcmc->verbose > 0){
		for (int i = 0; i < mcmc->operator_count; i++) {
			Operator* op = mcmc->operators[i];
			printf("Acceptance ratio %s: %f %f (failures: %zu)\n", op->name, ((double)op->accepted_count/(op->accepted_count+op->rejected_count)), op->parameters[0], op->failure_count);
		}
	}
	free(weights);
	free_StringBuffer(buffer);
}

MCMC* new_MCMC_from_json(json_node* node, Hashtable* hash){
	MCMC* mcmc = malloc(sizeof(MCMC));
	json_node* model_node = get_json_node(node, "model");
	mcmc->operator_count = 0;
	json_node* ops = get_json_node(node, "operators");
	json_node* logs = get_json_node(node, "log");
	mcmc->chain_length = get_json_node_value_size_t(node, "length", 100000);
	mcmc->verbose = get_json_node_value_int(node, "verbose", 1);
	mcmc->run = run;
	
	if (ops->child_count == 0) {
		fprintf(stderr, "Please specify at least one operator\n");
		exit(1);
	}
	
	if(model_node->node_type == MJSON_OBJECT){
		char* type = get_json_node_value_string(model_node, "type");
		char* id = get_json_node_value_string(model_node, "id");
		
		if (strcasecmp(type, "compound") == 0) {
			mcmc->model = new_CompoundModel_from_json(model_node, hash);
		}
		else if (strcasecmp(type, "treelikelihood") == 0) {
			mcmc->model = new_TreeLikelihoodModel_from_json(model_node, hash);
		}
		Hashtable_add(hash, id, mcmc->model);
		// could be a parametric distribution
	}
	// ref
	else if(model_node->node_type == MJSON_STRING){
		char* model_string = model_node->value;
		mcmc->model = Hashtable_get(hash, model_string+1);
		mcmc->model->ref_count++;
	}
	
	for (int i = 0; i < ops->child_count; i++) {
		json_node* child = ops->children[i];
		Operator* op = new_Operator_from_json(child, hash);
		char* id_string = get_json_node_value_string(child, "id");
		Hashtable_add(hash, id_string, op);
		if(mcmc->operator_count == 0){
			mcmc->operators = malloc(sizeof(Operator*));
		}
		else{
			mcmc->operators = realloc(mcmc->operators, sizeof(Operator*)*(mcmc->operator_count+1));
		}
		mcmc->operators[mcmc->operator_count] = op;
		mcmc->operator_count++;
	}
	
	mcmc->log_count = 0;
    if(logs != NULL){
        for (int i = 0; i < logs->child_count; i++) {
            json_node* child = logs->children[i];
            if(mcmc->log_count == 0){
                mcmc->logs = malloc(sizeof(Log*));
            }
            else{
                mcmc->logs = realloc(mcmc->logs, sizeof(Log*)*(mcmc->log_count+1));
            }
            mcmc->logs[i] = new_Log_from_json(child, hash);
            mcmc->log_count++;
        }
    }
	mcmc->chain_temperature = -1;
	return mcmc;
}

void free_MCMC(MCMC* mcmc){
	free_Model(mcmc->model);
	for (int i = 0; i < mcmc->operator_count; i++) {
		free_Operator(mcmc->operators[i]);
	}
	for (int i = 0; i < mcmc->log_count; i++) {
		free_Log(mcmc->logs[i]);
	}
	free(mcmc->logs);
	free(mcmc->operators);
	free(mcmc);
}
