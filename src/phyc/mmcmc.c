//
//  mmcmc.c
//  physher
//
//  Created by Mathieu Fourment on 17/12/2017.
//  Copyright © 2017 Mathieu Fourment. All rights reserved.
//

#include "mmcmc.h"

#include "matrix.h"
#include "filereader.h"
#include "marginal.h"

Vector* read_log_last_column( const char *filename, size_t burnin ){
	int count = 0;
	char *ptr = NULL;
	double *temp = NULL;
	int l;
	Vector* vec = new_Vector(1000);
	
	FileReader *reader = new_FileReader(filename, 1000);
	reader->read_line(reader);// discard header
	
	while ( reader->read_line(reader) ) {
		StringBuffer_trim(reader->buffer);
		
		if ( reader->buffer->length == 0){
			continue;
		}
		if ( count >= burnin){
			ptr = reader->line;
			l = 0;
			temp = String_split_char_double( ptr, '\t', &l );
			Vector_push(vec, temp[l-1]);
			free(temp);
		}
		count++;
	}
	free_FileReader(reader);
	Vector_pack(vec);
	return  vec;
}

void mmcmc_run(MMCMC* mmcmc){
	StringBuffer* buffer = new_StringBuffer(10);
	MCMC* mcmc = mmcmc->mcmc;
	Vector** lls = malloc(mmcmc->temperature_count*sizeof(Vector*));
	double* lrssk = malloc(mmcmc->temperature_count*sizeof(double));
	double* temperatures = malloc(mmcmc->temperature_count*sizeof(double));
	char** filenames = malloc(sizeof(char*)*mcmc->log_count);
	
	for (int j = 0; j < mcmc->log_count; j++) {
		filenames[j] = NULL;
		if(mcmc->logs[j]->filename != NULL){
			filenames[j] = String_clone(mcmc->logs[j]->filename);
		}
	}
	
	// temperatures should be in decreasing order
	for (int i = 0; i < mmcmc->temperature_count; i++) {
		for (int j = 0; j < mcmc->log_count; j++) {
			if(filenames[j] != NULL){
				StringBuffer_empty(buffer);
				StringBuffer_append_format(buffer, "%d%s",i, filenames[j]);
				free(mcmc->logs[j]->filename);
				mcmc->logs[j]->filename = StringBuffer_tochar(buffer);
			}
		}
		printf("Temperature: %f - %s\n", mmcmc->tempratures[i],buffer->c);
		mcmc->chain_temperature = mmcmc->tempratures[i];
		mcmc->run(mcmc);
		
		// saved in increasing order
		lls[mmcmc->temperature_count-i-1] = read_log_last_column(buffer->c, mmcmc->burnin);
		temperatures[mmcmc->temperature_count-i-1] = mmcmc->tempratures[i];
			
	}

	printf("Harmonic mean: %f\n", log_harmonic_mean(lls[mmcmc->temperature_count-1]));
	
	double lrss = log_marginal_stepping_stone(lls, mmcmc->temperature_count, temperatures, lrssk);
	printf("Stepping stone marginal likelihood: %f\n", lrss);
	for (int i = 1; i < mmcmc->temperature_count; i++) {
		printf("%f: %f\n", temperatures[i-1], lrssk[i]);
	}
	
	double lrps = log_marginal_path_sampling(lls, mmcmc->temperature_count, temperatures, lrssk);
	printf("Path sampling marginal likelihood: %f\n", lrps);
	for (int i = 0; i < mmcmc->temperature_count; i++) {
		printf("%f: %f\n", temperatures[i], lrssk[i]);
	}
	lrps = log_marginal_path_sampling_modified(lls, mmcmc->temperature_count, temperatures, lrssk);
	printf("Modified Path sampling marginal likelihood: %f\n", lrps);
	for (int i = 0; i < mmcmc->temperature_count; i++) {
		printf("%f: %f\n", temperatures[i], lrssk[i]);
	}
	
	// Leave it as a standard mcmc with original loggers
	mcmc->chain_temperature = -1;
	for (int j = 0; j < mcmc->log_count; j++) {
		if(filenames != NULL){
			mcmc->logs[j]->filename = String_clone(filenames[j]);
			free(filenames[j]);
		}
	}
	
	free(filenames);
	free_StringBuffer(buffer);
	free(lrssk);
	free(temperatures);
	for (int i = 0; i < mmcmc->temperature_count; i++) {
		free_Vector(lls[i]);
	}
	free(lls);
}

MMCMC* new_MMCMC_from_json(json_node* node, Hashtable* hash){
	MMCMC* mmcmc = malloc(sizeof(MMCMC));
	json_node* mcmc_node = get_json_node(node, "mcmc");
	json_node* temp_node = get_json_node(node, "temperatures");
	
	mmcmc->burnin = get_json_node_value_double(node, "burnin", 0);
	
	if (temp_node->node_type == MJSON_ARRAY) {
		mmcmc->temperature_count = temp_node->child_count;
		mmcmc->tempratures = dvector(mmcmc->temperature_count);
		for (int i = 0; i < temp_node->child_count; i++) {
			mmcmc->tempratures[i] = atof((char*)temp_node->children[i]->value);
		}
	}
	mmcmc->mcmc = new_MCMC_from_json(mcmc_node, hash);
	mmcmc->run = mmcmc_run;
	return mmcmc;
}

void free_MMCMC(MMCMC* mmcmc){
	free_MCMC(mmcmc->mcmc);
	free(mmcmc->tempratures);
	free(mmcmc);
}
