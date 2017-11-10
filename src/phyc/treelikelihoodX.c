/*
 *  treelikelihoodX.c
 *  PhyC
 *
 *  Created by Mathieu Fourment on 27/9/12.
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

#include "treelikelihoodX.h"


#include <stdio.h>
#include <math.h>

#include "treelikelihood.h"

#ifdef SSE3_ENABLED
#include <xmmintrin.h> // SSE
#include <pmmintrin.h> // SSE3
//#include <tmmintrin.h> // SSSE3
#endif

// I get 6% increase in speed for a codon dataset (2.09s vs. 2.21s)
// I get no difference for a flu nuc dataset ~11 s
//#define DESPERATE_OPTIMIZATION 1

#pragma mark -
#pragma mark Lower Likelihood

void update_partials_general( SingleTreeLikelihood *tlk, int partialsIndex, int partialsIndex1, int matrixIndex1, int partialsIndex2, int matrixIndex2 ) {
	
	if( tlk->partials[partialsIndex1] != NULL ){
		if(  tlk->partials[partialsIndex2] != NULL ){
			partials_undefined_and_undefined(tlk,
											   tlk->partials[partialsIndex1],
											   tlk->matrices[matrixIndex1],
											   tlk->partials[partialsIndex2],
											   tlk->matrices[matrixIndex2],
											   tlk->partials[partialsIndex]);
		}
		else {
			partials_states_and_undefined(tlk,
											tlk->mapping[matrixIndex2],
											tlk->matrices[matrixIndex2],
											tlk->partials[partialsIndex1],
											tlk->matrices[matrixIndex1],
											tlk->partials[partialsIndex]);
		}
		
	}
	else{
		if(  tlk->partials[partialsIndex2] != NULL ){
			partials_states_and_undefined(tlk,
											tlk->mapping[matrixIndex1],
											tlk->matrices[matrixIndex1],
											tlk->partials[partialsIndex2],
											tlk->matrices[matrixIndex2],
											tlk->partials[partialsIndex]);
			
		}
		else{
			partials_states_and_states(tlk,
										 tlk->mapping[matrixIndex1],
										 tlk->matrices[matrixIndex1],
										 tlk->mapping[matrixIndex2],
										 tlk->matrices[matrixIndex2],
										 tlk->partials[partialsIndex]);
		}
	}
	
	if ( tlk->scale ) {
		SingleTreeLikelihood_scalePartials( tlk, partialsIndex);
	}
}

void node_log_likelihoods_general( const SingleTreeLikelihood *tlk, const double *partials, const double *frequencies, double *outLogLikelihoods){
    int v = 0;
    int i = 0;
    
    const int nstate   = tlk->sm->nstate;
    const int patternCount = tlk->pattern_count;
    
    for ( int k = 0; k < patternCount; k++ ) {
        
        outLogLikelihoods[k] = 0;
        for ( i = 0; i < nstate; i++ ) {
            
            outLogLikelihoods[k] += frequencies[i] * partials[v];
            v++;
        }
        outLogLikelihoods[k] = log(outLogLikelihoods[k]);
        
        if ( tlk->scale ) {
            outLogLikelihoods[k] += getLogScalingFactor( tlk, k);
        }
    }
    
}

void node_likelihoods_general( const SingleTreeLikelihood *tlk, const double *partials, const double *frequencies, double *outLogLikelihoods){
    int v = 0;
    int i = 0;
    
    const int nstate   = tlk->sm->nstate;
    const int patternCount = tlk->pattern_count;
    
    for ( int k = 0; k < patternCount; k++ ) {
        
        outLogLikelihoods[k] = 0;
        for ( i = 0; i < nstate; i++ ) {
            
            outLogLikelihoods[k] += frequencies[i] * partials[v];
            v++;
        }
        
        if ( tlk->scale ) {
            outLogLikelihoods[k] += getLogScalingFactor( tlk, k);
        }
    }
    
}

void integrate_partials_general( const SingleTreeLikelihood *tlk, const double *inPartials, const double *proportions, double *outPartials ){	
	int i,k;
	double *pPartials = outPartials;
	const double *pInPartials = inPartials;
	
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count; 
	const int patternCount = tlk->pattern_count;

    if( catCount == 1 ){
        memcpy(outPartials, inPartials, sizeof(double)*patternCount*nstate);
        return;
    }
	
	for ( k = 0; k < patternCount; k++ ) {
		
		for ( i = 0; i < nstate; i++ ) {
			
			*pPartials++ = *pInPartials++ * proportions[0];
		}
	}
	
	
	for ( int l = 1; l < catCount; l++ ) {
		pPartials = outPartials;
		
		for ( k = 0; k < patternCount; k++ ) {
			
			for ( i = 0; i < nstate; i++ ) {
				
				*pPartials += *pInPartials++ * proportions[l];
				pPartials++;
			}
		}
	}

}

void partials_states_and_states( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *matrices2, double *partials ){
	int k,i,w;
	int u = 0;
	int state1, state2;
	double *pPartials = partials;
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	//uint8_t *patterns1 = tlk->sp->patterns2[idx1];
	//uint8_t *patterns2 = tlk->sp->patterns2[idx2];
    
#ifdef DESPERATE_OPTIMIZATION
	u = tlk->matrix_size * catCount - 1;
	pPartials = partials + (tlk->partials_size - 1);
	
	for ( int l = catCount+1; --l; ) {
		
		for ( k = patternCount+1; --k; ) {
			
			state1 = nstate - tlk->sp->patterns[k-1][idx1] - 1;
			state2 = nstate - tlk->sp->patterns[k-1][idx2] - 1;
			
			//state1 = nstate - patterns1[k-1] - 1;
			//state2 = nstate - patterns2[k-1] - 1;
            
			w = u;
			
			i = nstate+1;
			
			if (state1 >= 0 && state2 >= 0) {
				
				while ( --i ) {
					
					*pPartials = matrices1[w - state1] * matrices2[w - state2];
					pPartials--;
					w -= nstate;
				}
				
			}
			else if ( state1 >= 0 ) {
				// child 1 has a gap or unknown state so treat it as unknown
				
				while ( --i ) {
					
					*pPartials = matrices1[w - state1];
					pPartials--;
					w -= nstate;
				}
			}
			else if ( state2 >= 0 ) {
				// child 2 has a gap or unknown state so treat it as unknown
				
				while ( --i ) {
					
					*pPartials = matrices2[w - state2];
					pPartials--;
					w -= nstate;
				}
			}
			else {
				// both children have a gap or unknown state so set partials to 1
				
				while ( --i ) {
					*pPartials = 1.0;
					pPartials--;
				}
			}
		}
		u -= tlk->matrix_size;
	}
    
#else
	u = 0;
	pPartials = partials;
	for ( int l = 0; l < catCount; l++ ) {
		
		for ( k = 0; k < patternCount; k++ ) {
			
			state1 = tlk->sp->patterns[k][idx1];
			state2 = tlk->sp->patterns[k][idx2];
			
			//w = l * tlk->matrix_size;
			w = u;
			
			if (state1 < nstate && state2 < nstate) {
				
				for ( i = 0; i < nstate; i++ ) {
					
					*pPartials++ = matrices1[w + state1] * matrices2[w + state2];
					w += nstate;
				}
				
			}
			else if (state1 < nstate) {
				// child 1 has a gap or unknown state so treat it as unknown
				
				for ( i = 0; i < nstate ; i++ ) {
					
					*pPartials++ = matrices1[w + state1];
					
					w += nstate;
				}
			} else if (state2 < nstate ) {
				// child 2 has a gap or unknown state so treat it as unknown
				
				for ( i = 0; i < nstate ; i++ ) {
					
					*pPartials++ = matrices2[w + state2];
					
					w += nstate;
				}
			} else {
				// both children have a gap or unknown state so set partials to 1
				
				for ( i = 0; i < nstate; i++ ) {
					*pPartials++ = 1.0;
				}
			}
		}
		u += tlk->matrix_size;
	}
#endif
}


void partials_states_and_undefined( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, const double *partials2, const double *matrices2, double *partials3 ){
	double sum;
	int v = 0;
	int i,j,k;
	int w;
	int state1;
	const int nstate = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
#ifdef DESPERATE_OPTIMIZATION
	v = tlk->partials_size - 1;
	double *pPartials = partials3 + v;
	int u = tlk->matrix_size * tlk->cat_count-1;
	
	//uint8_t *patterns = tlk->sp->patterns2[idx1];
	
	for ( int l = catCount+1; --l; ) {
		
		for ( k = patternCount+1; --k; ) {
			
			state1 = nstate - tlk->sp->patterns[k-1][idx1] - 1;
			//state1 = nstate - patterns[k-1] - 1;
			
			w = u;
			
			i = nstate + 1;
			
			if ( state1 >= 0 ) {
				
				
				while ( --i ) {
					
					*pPartials = matrices1[w - state1];
					
					sum = 0.0;
					for ( j = nstate + 1; --j; ) {
						sum += matrices2[w] * partials2[v - nstate + j];
						w--;
					}
					
					*pPartials *= sum;
					pPartials--;
				}
				
			}
			else {
				// Child 1 has a gap or unknown state so don't use it
				
				while ( --i ) {
					
					*pPartials = 0.0;
					for ( j = nstate + 1; --j; ) {
						*pPartials += matrices2[w] * partials2[v - nstate + j];
						w--;
					}
					
					pPartials--;
				}
				
			}
			
			v -= nstate;
		}
		u -= tlk->matrix_size;
	}
	
#else
	double *pPartials = partials3;
	
	for ( int l = 0; l < catCount; l++ ) {
		
		for ( k = 0; k < patternCount; k++ ) {
			
			state1 = tlk->sp->patterns[k][idx1];
			
			w = l * tlk->matrix_size;
			
			if ( state1 < nstate) {
				
				
				for ( i = 0; i < nstate; i++) {
					
					*pPartials = matrices1[w + state1];
					
					sum = 0.0;
					for ( j = 0; j < nstate; j++) {
						sum += matrices2[w] * partials2[v + j];
						w++;
					}
					
					*pPartials *= sum;
					pPartials++;
				}
				
			}
			else {
				// Child 1 has a gap or unknown state so don't use it
				
				for ( i = 0; i < nstate; i++) {
					
					*pPartials = 0.0;
					for ( j = 0; j < nstate; j++) {
						*pPartials += matrices2[w] * partials2[v + j];
						w++;
					}
					
					pPartials++;
				}
				
			}
			
			v += nstate;
		}
	}
#endif
}



void partials_undefined_and_undefined( const SingleTreeLikelihood *tlk, const double *partials1, const double *matrices1, const double *partials2, const double *matrices2, double *partials3 ){
    double sum1, sum2;
    
    int v = 0;
    int i,j,k;
    
    const int nstate   = tlk->sm->nstate;
    const int catCount = tlk->cat_count;
    const int patternCount = tlk->pattern_count;
    
#ifdef DESPERATE_OPTIMIZATION
    v = tlk->partials_size - 1;
    double *pPartials = partials3 + v;
    int u = tlk->matrix_size * catCount-1;
    const double *m1 = NULL;
    const double *m2 = NULL;
    const double *p1 = NULL;
    const double *p2 = NULL;
    
    for ( int l = catCount+1; --l; ) {
        
        for ( k = patternCount+1; --k; ) {
            
            m1 = matrices1 + u;
            m2 = matrices2 + u;
            
            for ( i = nstate+1; --i ;  ) {
                
                sum1 = sum2 = 0.;
                p1 = partials1 + v;
                p2 = partials2 + v;
                
                for ( j = nstate+1; --j; ) {
                    sum1 += *m1 * *p1;
                    sum2 += *m2 * *p2;
                    m1--;
                    m2--;
                    p1--;
                    p2--;
                }
                
                *pPartials-- = sum1 * sum2;
            }
            v -= nstate;
        }
        u -= tlk->matrix_size;
    }
#else
    int w,z;
    double *pPartials = partials3;
    
    for ( int l = 0; l < catCount; l++ ) {
        
        for ( k = 0; k < patternCount; k++ ) {
            
            w = l * tlk->matrix_size;
            
            for ( i = 0; i < nstate; i++ ) {
                
                sum1 = sum2 = 0.;
                z = v;
                for ( j = 0; j < nstate; j++) {
                    sum1 += matrices1[w] * partials1[z];
                    sum2 += matrices2[w] * partials2[z];
                    z++;
                    w++;
                }
                
                *pPartials++ = sum1 * sum2;
            }
            v += nstate;
        }
    }
#endif
}

void update_partials_general_transpose( SingleTreeLikelihood *tlk, int nodeIndex1, int nodeIndex2, int nodeIndex3 ) {
    
    if( tlk->mapping[nodeIndex1] == -1 ){
        if(  tlk->mapping[nodeIndex2] == -1 ){
            partials_undefined_and_undefined(tlk,
                                             tlk->partials[nodeIndex1],
                                             tlk->matrices[nodeIndex1],
                                             tlk->partials[nodeIndex2],
                                             tlk->matrices[nodeIndex2],
                                             tlk->partials[nodeIndex3]);
        }
        else {
            partials_states_and_undefined_transpose(tlk,
                                          tlk->mapping[nodeIndex2],
                                          tlk->matrices[nodeIndex2],
                                          tlk->partials[nodeIndex1],
                                          tlk->matrices[nodeIndex1],
                                          tlk->partials[nodeIndex3]);
        }
        
    }
    else{
        if(  tlk->mapping[nodeIndex2] == -1 ){
            partials_states_and_undefined_transpose(tlk,
                                          tlk->mapping[nodeIndex1],
                                          tlk->matrices[nodeIndex1],
                                          tlk->partials[nodeIndex2],
                                          tlk->matrices[nodeIndex2],
                                          tlk->partials[nodeIndex3]);
            
        }
        else{
            partials_states_and_states_transpose(tlk,
                                       tlk->mapping[nodeIndex1],
                                       tlk->matrices[nodeIndex1],
                                       tlk->mapping[nodeIndex2],
                                       tlk->matrices[nodeIndex2],
                                       tlk->partials[nodeIndex3]);
        }
    }
    //    for (int i = 0; i < tlk->pattern_count*tlk->sp->nstate; i++) {
    //        printf(" %f",tlk->partials[nodeIndex3][i]);
    //    }
    //    printf("\n");
    if ( tlk->scale ) {
        SingleTreeLikelihood_scalePartials( tlk, nodeIndex3);
    }
}

// matrices1 and matrices2 are transposed. It can be used along other SSE functions
void partials_states_and_states_transpose( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *matrices2, double *partials ){
    
    int k;
    int state1, state2;
    int u = 0;
    int w;
    
    double *pPartials = partials;
    const int nstate = tlk->sm->nstate;
    
    for ( int l = 0; l < tlk->cat_count; l++ ) {
        
        for ( k = 0; k < tlk->pattern_count; k++ ) {
            
            state1 = tlk->sp->patterns[k][idx1];
            state2 = tlk->sp->patterns[k][idx2];
            
            w = u;
            
            if (state1 < nstate && state2 < nstate) {
                
                const double *m1 = &matrices1[w+nstate*state1];
                const double *m2 = &matrices2[w+nstate*state2];
                
                for ( int i = 0; i < nstate; i++) {
                    *pPartials++ = m1[i]*m2[i];
                }
            }
            else if (state1 < nstate) {
                // child 2 has a gap or unknown state so treat it as unknown
                memcpy(pPartials, &matrices1[w+nstate*state1], sizeof(double)*nstate);
                pPartials += nstate;
            }
            else if (state2 < nstate ) {
                // child 1 has a gap or unknown state so treat it as unknown
                memcpy(pPartials, &matrices2[w+nstate*state2], sizeof(double)*nstate);
                pPartials += nstate;
            }
            else {
                // both children have a gap or unknown state so set partials to 1
                for ( int i = 0; i < nstate; i++) {
                    *pPartials++ = 1.0;
                }
                
            }
        }
        u += tlk->matrix_size;
    }
    
}

// matrices1 are transposed. It can be used along SSE functions
void partials_states_and_undefined_transpose( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, const double *partials2, const double *matrices2, double *partials3 ){
    
    
    int v = 0;
    int k;
    int w = 0;
    int state1;
    
    double *pPartials = partials3;
    const double *m1 = matrices1;
    
    const double *m2,*p2;
    double sum = 0;
    const int nstate = tlk->sm->nstate;
    
    
    for ( int l = 0; l < tlk->cat_count; l++ ) {
        
        for ( k = 0; k < tlk->pattern_count; k++ ) {
            
            state1 = tlk->sp->patterns[k][idx1];
            
            m2 = &matrices2[w];
            p2 = &partials2[v];
            
            if ( state1 < nstate) {
                
                m1 = &matrices1[w + state1*nstate];
                
                for ( int i = 0; i < nstate; i++ ) {
                    sum = 0;
                    for ( int j = 0; j < nstate; j++ ) {
                        sum += *m2 * p2[j]; m2++;
                    }
                    *pPartials++ = *m1 * sum; m1++;
                }
            }
            else {
                // Child 1 has a gap or unknown state so don't use it
                
                for ( int i = 0; i < nstate; i++ ) {
                    *pPartials = 0;
                    for ( int j = 0; j < nstate; j++ ) {
                        *pPartials += *m2 * p2[j]; m2++;
                    }
                    pPartials++;
                }
            }
            v += nstate;
        }
        w += tlk->matrix_size;
    }
}

#pragma mark -
#pragma mark Lower Likelihood SSE

#ifdef SSE3_ENABLED

void node_log_likelihoods_general_even_SSE( const SingleTreeLikelihood *tlk, const double *partials, const double *frequencies, double *outLogLikelihoods){

    int i,k;
    
    const int nstate   = tlk->sm->nstate;
    const int patternCount = tlk->pattern_count;
    
    __m128d *f,*p;
    __m128d sum;
    double temp[2] __attribute__ ((aligned (16)));
    
    p = (__m128d*)partials;
    
    for ( k = 0; k < patternCount; k++ ) {
        
        f = (__m128d*)frequencies;
        
        sum = _mm_mul_pd(*f,*p);
        p++;
        f++;
        for ( i = 2; i < nstate; i+=2 ) {
            sum = _mm_add_pd(sum, _mm_mul_pd(*f,*p));
            p++;
            f++;
        }
        _mm_store_pd(temp, sum);
        outLogLikelihoods[k] = log(temp[0]+temp[1]);
        
        if ( tlk->scale ) {
            outLogLikelihoods[k] += getLogScalingFactor( tlk, k);
        }
    }
    
}
void node_likelihoods_general_even_SSE( const SingleTreeLikelihood *tlk, const double *partials, const double *frequencies, double *outLogLikelihoods){
    
    int i,k;
    
    const int nstate   = tlk->sm->nstate;
    const int patternCount = tlk->pattern_count;
    
    __m128d *f,*p;
    __m128d sum;
    double temp[2] __attribute__ ((aligned (16)));
    
    p = (__m128d*)partials;
    
    for ( k = 0; k < patternCount; k++ ) {
        
        f = (__m128d*)frequencies;
        
        sum = _mm_mul_pd(*f,*p);
        p++;
        f++;
        for ( i = 2; i < nstate; i+=2 ) {
            sum = _mm_add_pd(sum, _mm_mul_pd(*f,*p));
            p++;
            f++;
        }
        _mm_store_pd(temp, sum);
        outLogLikelihoods[k] = temp[0]+temp[1];
        
        if ( tlk->scale ) {
            outLogLikelihoods[k] += getLogScalingFactor( tlk, k);
        }
    }
    
}

void integrate_partials_general_even_SSE( const SingleTreeLikelihood *tlk, const double *inPartials, const double *proportions, double *outPartials ){
    int i,k,l;
    
    const int nstate   = tlk->sm->nstate;
    const int catCount = tlk->cat_count;
    const int patternCount = tlk->pattern_count;
    
    if( catCount == 1 ){
        memcpy(outPartials, inPartials, sizeof(double)*patternCount*nstate);
        return;
    }
    
    __m128d prop = _mm_set1_pd(proportions[0]);
    __m128d *pi = (__m128d*)inPartials;
    __m128d *p = (__m128d*)outPartials;
    
    for ( k = 0; k < patternCount; k++ ) {
        
        for ( i = 0; i < nstate; i+=2 ) {
            *p = _mm_mul_pd(*pi, prop);
            pi++;
            p++;
        }
    }
    
    
    for ( l = 1; l < catCount; l++ ) {
        prop = _mm_set1_pd(proportions[l]);
        p = (__m128d*)outPartials;
        
        for ( k = 0; k < patternCount; k++ ) {
            
            for ( i = 0; i < nstate; i+=2 ) {
                *p = _mm_add_pd(*p, _mm_mul_pd(*pi, prop));
                p++;
                pi++;
            }
        }
    }
    
}

// matrices1 and matrices2 are transposed.
void partials_states_and_states_even_SSE( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *matrices2, double *partials ){
    
    int i,k,l;
    int state1, state2;
    int w;
    
    double *pPartials = partials;
    const int nstate = tlk->sm->nstate;
    
    __m128d *m1,*m2;
    
    for ( l = 0; l < tlk->cat_count; l++ ) {
        
        for ( k = 0; k < tlk->pattern_count; k++ ) {
            
            state1 = tlk->sp->patterns[k][idx1];
            state2 = tlk->sp->patterns[k][idx2];
            
            w = l * tlk->matrix_size;
            
            if (state1 < nstate && state2 < nstate) {
                
                m1 = (__m128d*)&matrices1[w+nstate*state1];
                m2 = (__m128d*)&matrices2[w+nstate*state2];
                
                for ( i = 0; i < nstate; i+=2) {
                    _mm_store_pd(pPartials, _mm_mul_pd(*m1,*m2));
                    m1++;
                    m2++;
                    pPartials+=2;
                }
            }
            else if (state1 < nstate) {
                // child 2 has a gap or unknown state so treat it as unknown
                memcpy(pPartials, &matrices1[w+nstate*state1], sizeof(double)*nstate);
                pPartials += nstate;
            }
            else if (state2 < nstate ) {
                // child 1 has a gap or unknown state so treat it as unknown
                memcpy(pPartials, &matrices2[w+nstate*state2], sizeof(double)*nstate);
                pPartials += nstate;
            }
            else {
                // both children have a gap or unknown state so set partials to 1
                for ( int i = 0; i < nstate; i++) {
                    *pPartials++ = 1.0;
                }
                
            }
        }
    }
    
}

// matrices1 are transposed.
void partials_states_and_undefined_even_SSE( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, const double *partials2, const double *matrices2, double *partials3 ){
    
    
    int v = 0;
    int w = 0;
    int i,j,k,l;
    int state1;
    
    double *pPartials = partials3;
    const double *m1 = NULL;
    __m128d *m2,*p2;
    __m128d sum;
    double temp[2] __attribute__ ((aligned (16)));
    const int nstate = tlk->sm->nstate;
    
    
    for ( l = 0; l < tlk->cat_count; l++ ) {
        
        for ( k = 0; k < tlk->pattern_count; k++ ) {
            
            state1 = tlk->sp->patterns[k][idx1];
            
            w = l * tlk->matrix_size;
            
            m2 = (__m128d*)&matrices2[w];
            
            if ( state1 < nstate) {
                
                m1 = &matrices1[w + state1*nstate];
                
                for ( i = 0; i < nstate; i++ ) {
                    p2 = (__m128d*)&partials2[v];
                    sum = _mm_mul_pd(*m2, *p2);
                    m2++; p2++;
                    for ( j = 2; j < nstate; j+=2 ) {
                        sum = _mm_add_pd(sum,_mm_mul_pd(*m2, *p2));
                        m2++;
                        p2++;
                    }
                    _mm_store_pd(temp, sum);
                    *pPartials++ = *m1 * (temp[0]+temp[1]); m1++;
                }
            }
            else {
                // Child 1 has a gap or unknown state so don't use it
                
                for ( i = 0; i < nstate; i++ ) {
                    p2 = (__m128d*)&partials2[v];
                    sum = _mm_mul_pd(*m2, *p2);
                    m2++; p2++;
                    for ( j = 2; j < nstate; j+=2 ) {
                        sum = _mm_add_pd(sum,_mm_mul_pd(*m2, *p2));
                        m2++;
                        p2++;
                    }
                    _mm_store_pd(temp, sum);
                    *pPartials++ = temp[0]+temp[1];
                }
            }
            v += nstate;
        }
    }
}


void partials_undefined_and_undefined_even_SSE( const SingleTreeLikelihood *tlk, const double *partials1, const double *matrices1, const double *partials2, const double *matrices2, double *partials3 ){
    
    int v = 0;
    int w;
    int i,j,k,l;
    
    const int nstate   = tlk->sm->nstate;
    const int catCount = tlk->cat_count;
    const int patternCount = tlk->pattern_count;
    
    __m128d *p1,*p2,*m1,*m2,sum1,sum2;
    double temp[2] __attribute__ ((aligned (16)));
    
    double *pPartials = partials3;
    
    for ( l = 0; l < catCount; l++ ) {
        
        for ( k = 0; k < patternCount; k++ ) {
            
            w = l * tlk->matrix_size;
            
            m1 = (__m128d*)&matrices1[w];
            m2 = (__m128d*)&matrices2[w];
            
            for ( i = 0; i < nstate; i++ ) {
                
                p1 = (__m128d*)&partials1[v];
                p2 = (__m128d*)&partials2[v];
                
                sum1 = _mm_mul_pd(*m1, *p1);
                sum2 = _mm_mul_pd(*m2, *p2);
                
                m1++; m2++;
                p1++; p2++;
                
                for ( j = 2; j < nstate; j+=2) {
                    sum1 = _mm_add_pd(sum1,_mm_mul_pd(*m1, *p1));
                    sum2 = _mm_add_pd(sum2,_mm_mul_pd(*m2, *p2));
                    
                    m1++; m2++;
                    p1++; p2++;
                }
                _mm_store_pd(temp, sum1);
                *pPartials = temp[0]+temp[1];
                _mm_store_pd(temp, sum2);
                *pPartials++ *= temp[0]+temp[1];
            }
            v += nstate;
        }
    }
    
}

void update_partials_general_even_SSE( SingleTreeLikelihood *tlk, int partialsIndex, int partialsIndex1, int matrixIndex1, int partialsIndex2, int matrixIndex2 ) {
	
	if( tlk->partials[partialsIndex1] != NULL ){
		if(  tlk->partials[partialsIndex2] != NULL ){
			partials_undefined_and_undefined_even_SSE(tlk,
											 tlk->partials[partialsIndex1],
											 tlk->matrices[matrixIndex1],
											 tlk->partials[partialsIndex2],
											 tlk->matrices[matrixIndex2],
											 tlk->partials[partialsIndex]);
		}
		else {
			partials_states_and_undefined_even_SSE(tlk,
										  tlk->mapping[matrixIndex2],
										  tlk->matrices[matrixIndex2],
										  tlk->partials[partialsIndex1],
										  tlk->matrices[matrixIndex1],
										  tlk->partials[partialsIndex]);
		}
		
	}
	else{
		if(  tlk->partials[partialsIndex2] != NULL ){
			partials_states_and_undefined_even_SSE(tlk,
										  tlk->mapping[matrixIndex1],
										  tlk->matrices[matrixIndex1],
										  tlk->partials[partialsIndex2],
										  tlk->matrices[matrixIndex2],
										  tlk->partials[partialsIndex]);
			
		}
		else{
			partials_states_and_states_even_SSE(tlk,
									   tlk->mapping[matrixIndex1],
									   tlk->matrices[matrixIndex1],
									   tlk->mapping[matrixIndex2],
									   tlk->matrices[matrixIndex2],
									   tlk->partials[partialsIndex]);
		}
	}
	
	if ( tlk->scale ) {
		SingleTreeLikelihood_scalePartials( tlk, partialsIndex);
	}
}
#endif

#pragma mark -
#pragma mark Lower Likelihood REL

static void _partials_states_and_states_rel( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *matrices2, double *partials ){
	int k,i,u;
	int state1, state2;
	double *pPartials = partials;
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
    
	pPartials = partials;
	
    double sum1 = 0;
    double sum2 = 0;
    
    for ( k = 0; k < patternCount; k++ ) {
        
        state1 = tlk->sp->patterns[k][idx1];
        state2 = tlk->sp->patterns[k][idx2];
        
        
        if (state1 < nstate && state2 < nstate) {
            
            for ( i = 0; i < nstate; i++ ) {
                sum1 = sum2 = 0;
                u = 0;
                for ( int l = 0; l < catCount; l++ ) {
                    int j = u + nstate * i;
                    double  p = tlk->sm->get_proportion(tlk->sm, l);
                    sum1 += matrices1[j + state1]*p;
                    sum2 += matrices2[j + state2]*p;
                    u += tlk->matrix_size;
                }
                *pPartials++ = sum1 * sum2;
            }
            
        }
        else if (state1 < nstate) {
            // child 1 has a gap or unknown state so treat it as unknown
            
            for ( i = 0; i < nstate; i++ ) {
                sum1 = 0;
                u = 0;
                for ( int l = 0; l < catCount; l++ ) {
                    int j = u + nstate * i;
                    double  p = tlk->sm->get_proportion(tlk->sm, l);
                    sum1 += matrices1[j + state1]*p;
                    u += tlk->matrix_size;
                }
                *pPartials++ = sum1;
            }
            
        }
        else if (state2 < nstate ) {
            // child 2 has a gap or unknown state so treat it as unknown
            
            for ( i = 0; i < nstate; i++ ) {
                sum1 = 0;
                u = 0;
                for ( int l = 0; l < catCount; l++ ) {
                    int j = u + nstate * i;
                    double  p = tlk->sm->get_proportion(tlk->sm, l);
                    sum1 += matrices2[j + state2]*p;
                    u += tlk->matrix_size;
                }
                *pPartials++ = sum1;
            }
        }
        else {
            // both children have a gap or unknown state so set partials to 1
            
            for ( i = 0; i < nstate; i++ ) {
                *pPartials++ = 1.0;
            }
        }
    }
    
}


static void _partials_states_and_undefined_rel( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, const double *partials2, const double *matrices2, double *partials3 ){
	double sum;
	int v = 0;
	int h,i,j,k,l,u;
	int state1;
	const int nstate = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;

	double *pPartials = partials3;
	
		
    for ( k = 0; k < patternCount; k++ ) {
        
        state1 = tlk->sp->patterns[k][idx1];
        
        if ( state1 < nstate) {
            
            
            for ( i = 0; i < nstate; i++) {
                
                *pPartials = 0;
                u = 0;
                for ( l = 0; l < catCount; l++ ) {
                    int h = u + nstate * i;
                    double  p = tlk->sm->get_proportion(tlk->sm, l);
                    *pPartials += matrices1[h + state1]*p;
                    u += tlk->matrix_size;
                }
                
                sum = 0.0;
                for ( j = 0; j < nstate; j++) {
                    u = 0;
                    double m = 0;
                    for ( l = 0; l < catCount; l++ ) {
                        h = u + nstate * i;
                        double  p = tlk->sm->get_proportion(tlk->sm, l);
                        m += matrices2[h + j]*p;
                        u += tlk->matrix_size;
                    }
                    
                    sum += m * partials2[v + j];
                }
                
                *pPartials *= sum;
                pPartials++;
            }
            
        }
        else {
            // Child 1 has a gap or unknown state so don't use it
            
            for ( i = 0; i < nstate; i++) {
                
                *pPartials = 0.0;
                for ( j = 0; j < nstate; j++) {
                    u = 0;
                    double m = 0;
                    for ( l = 0; l < catCount; l++ ) {
                        int h = u + nstate * i;
                        double  p = tlk->sm->get_proportion(tlk->sm, l);
                        m += matrices2[h + j]*p;
                        u += tlk->matrix_size;
                    }
                    
                    *pPartials += m * partials2[v + j];
                    
                }
                
                pPartials++;
            }
            
        }
        
        v += nstate;
    }
	
}



static void _partials_undefined_and_undefined_rel( const SingleTreeLikelihood *tlk, const double *partials1, const double *matrices1, const double *partials2, const double *matrices2, double *partials3 ){
	double sum1, sum2;
	
	int v = 0;
	int i,j,k;
	
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	double *pPartials = partials3;
	
	
    for ( k = 0; k < patternCount; k++ ) {
        
        for ( i = 0; i < nstate; i++ ) {
            
            sum1 = sum2 = 0.;
            
            for ( j = 0; j < nstate; j++) {
                double m1 = 0;
                double m2 = 0;
                int u = 0;
                
                for ( int l = 0; l < catCount; l++ ) {
                    int h = u + nstate * i;
                    double  p = tlk->sm->get_proportion(tlk->sm, l);
                    m1 += matrices1[h+j]*p;
                    m2 += matrices2[h+j]*p;
                    u += tlk->matrix_size;
                }
                
                sum1 += m1 * partials1[v + j];
                sum2 += m2 * partials2[v + j];
            }
            
            *pPartials++ = sum1 * sum2;
        }
        v += nstate;
    }
	

}

void update_partials_general_rel( SingleTreeLikelihood *tlk, int nodeIndex1, int nodeIndex2, int nodeIndex3 ) {
	
    if( tlk->mapping[nodeIndex1] == -1 ){
        if(  tlk->mapping[nodeIndex2] == -1 ){
            _partials_undefined_and_undefined_rel(tlk,
                                                  tlk->partials[nodeIndex1],
                                                  tlk->matrices[nodeIndex1],
                                                  tlk->partials[nodeIndex2],
                                                  tlk->matrices[nodeIndex2],
                                                  tlk->partials[nodeIndex3]);
        }
        else {
            _partials_states_and_undefined_rel(tlk,
                                               tlk->mapping[nodeIndex2],
                                               tlk->matrices[nodeIndex2],
                                               tlk->partials[nodeIndex1],
                                               tlk->matrices[nodeIndex1],
                                               tlk->partials[nodeIndex3]);
        }
        
    }
    else{
        if(  tlk->mapping[nodeIndex2] == -1 ){
            _partials_states_and_undefined_rel(tlk,
                                               tlk->mapping[nodeIndex1],
                                               tlk->matrices[nodeIndex1],
                                               tlk->partials[nodeIndex2],
                                               tlk->matrices[nodeIndex2],
                                               tlk->partials[nodeIndex3]);
            
        }
        else{
            _partials_states_and_states_rel(tlk,
                                            tlk->mapping[nodeIndex1],
                                            tlk->matrices[nodeIndex1],
                                            tlk->mapping[nodeIndex2],
                                            tlk->matrices[nodeIndex2],
                                            tlk->partials[nodeIndex3]);
        }
    }
    
	if ( tlk->scale ) {
		SingleTreeLikelihood_scalePartials( tlk, nodeIndex3);
	}
}


#pragma mark -
#pragma mark Lower Likelihood OPENMP

void partials_undefined_and_undefined_omp( const SingleTreeLikelihood *tlk, const double *partials1, const double *matrices1, const double *partials2, const double *matrices2, double *partials3 ){
	
	const int nstate   = tlk->sm->nstate;
	
    //int nThreads = tlk->nthreads;
    
    #pragma omp parallel for schedule(dynamic,1) num_threads(2)
    for ( int lk = 0; lk < tlk->pattern_count*tlk->cat_count; lk++ ) {
        int l = lk / tlk->pattern_count;
        int k = lk % tlk->pattern_count;
        
        int w = l * tlk->matrix_size;
        int v = l*tlk->pattern_count + k * nstate;
        
        double *pPartials = partials3 + l*tlk->pattern_count + k;
        
        for ( int i = 0; i < nstate; i++ ) {
            double sum1, sum2;
            sum1 = sum2 = 0.;
            
            for ( int j = 0; j < nstate; j++) {
                sum1 += matrices1[w] * partials1[v + j];
                sum2 += matrices2[w] * partials2[v + j];
                
                w++;
            }
            
            *pPartials++ = sum1 * sum2;
        }
        
	}
}

#pragma mark -
#pragma mark Upper Likelihood

static void _calculate_branch_likelihood_undefined(SingleTreeLikelihood *tlk, double* rootPartials, const double* upperPartials, const double* partials, const double* matrices){
	memset(rootPartials, 0, sizeof(double)*tlk->sm->nstate*tlk->pattern_count);
	int v = 0;
	for(int l = 0; l < tlk->cat_count; l++) {
		int u = 0;
		const double weight = tlk->sm->get_proportion(tlk->sm, l);
		for(int k = 0; k < tlk->pattern_count; k++) {
			int w = l * tlk->matrix_size;
			const double* partialsChildPtr = partials+v;
			for(int i = 0; i < tlk->sm->nstate; i++) {
				const double* transMatrixPtr = &matrices[w];
				double sum = 0.0;
				for (int j = 0; j < tlk->sm->nstate; j++) {
					sum += transMatrixPtr[j] * partialsChildPtr[j];
				}
				rootPartials[u] += sum * upperPartials[v] * weight;
				u++;
				v++;
				w += tlk->sm->nstate;
			}
		}
	}
	
	
}

//TODO: not right, compare to function with 4 states
static void _calculate_branch_likelihood_state(SingleTreeLikelihood *tlk, double* rootPartials, const double* upperPartials, int partialsIndex, const double* matrices){
	memset(rootPartials, 0, sizeof(double)*tlk->sm->nstate*tlk->pattern_count);
	int v = 0;
	for(int l = 0; l < tlk->cat_count; l++) {
		int u = 0; // Index in resulting product-partials (summed over categories)
		const double weight = tlk->sm->get_proportion(tlk->sm, l);
		for(int k = 0; k < tlk->pattern_count; k++) {
			int w =  l * tlk->matrix_size + tlk->sp->patterns[k][partialsIndex];
			for(int i = 0; i < tlk->sm->nstate; i++) {
				rootPartials[u] += matrices[w] * upperPartials[v] * weight;
				u++;
				v++;
				w += tlk->sm->nstate;
			}
		}
	}
	
}

void calculate_branch_likelihood(SingleTreeLikelihood *tlk, double* rootPartials, int upperPartialsIndex, int partialsIndex, int matrixIndex){
	if( tlk->partials[partialsIndex] == NULL ){
		_calculate_branch_likelihood_state(tlk, rootPartials, tlk->partials[upperPartialsIndex], tlk->mapping[partialsIndex], tlk->matrices[matrixIndex]);
	}
	else{
		_calculate_branch_likelihood_undefined(tlk, rootPartials, tlk->partials[upperPartialsIndex], tlk->partials[partialsIndex], tlk->matrices[matrixIndex]);
	}
}

// Called by a node whose parent is NOT the root and the node's sibling is a leaf
static void _update_upper_partials_state( SingleTreeLikelihood *tlk, const double *matrix_upper, const double *partials_upper, const double *matrix_lower, int sibling_index, double *partials ){
    int w,w2,i,j,k;
    int v = 0;
    double sum1,sum2;
    int nstate = tlk->sm->nstate;
    int state;
    double *pPartials = partials;
    
    for ( int l = 0; l < tlk->cat_count; l++ ) {
		
		for ( k = 0; k < tlk->pattern_count; k++ ) {
            
            state = tlk->sp->patterns[k][ sibling_index ];
            
			w  = l * tlk->matrix_size;
			
			for ( i = 0; i < nstate; i++ ) {
				w2 = l * tlk->matrix_size + i;
				sum1 = sum2 = 0.;
                
                for ( j = 0; j < nstate; j++) {
                    sum1 += matrix_upper[w2] * partials_upper[v + j];
                    w2 += nstate;
                }
                
                if( state < nstate){
                    sum2 = matrix_lower[w+state];
                    w += nstate;
                }
                else {
                    sum2 = 1.0;
                    w += nstate;
                }
                
				*pPartials++ = sum1 * sum2 ;
			}
			v += nstate;
		}
    }
}

// Called by a node whose parent is NOT the root and the node's sibling is NOT a leaf
static void _update_upper_partials_undefined( SingleTreeLikelihood *tlk, const double *matrix_upper, const double *partials_upper, const double *matrix_lower, const double *partials_lower, double *partials ){
    int w,w2,i,j,k;
    int v = 0;
    double sum1,sum2;
    int nstate = tlk->sm->nstate;
    double *pPartials = partials;
    
    for ( int l = 0; l < tlk->cat_count; l++ ) {
		
		for ( k = 0; k < tlk->pattern_count; k++ ) {
			
			w  = l * tlk->matrix_size;
			
			for ( i = 0; i < nstate; i++ ) {
				w2 = l * tlk->matrix_size + i;
				sum1 = sum2 = 0.;
                
                for ( j = 0; j < nstate; j++) {
                    sum1 += matrix_upper[w2] * partials_upper[v + j];
                    sum2 += matrix_lower[w]  * partials_lower[v + j];
                    w++;
                    w2 += nstate;
                }
                
				*pPartials++ = sum1 * sum2 ;
			}
			v += nstate;
		}
    }
}

// Called by a child of the root but not if the child's sibling is NOT leaf
static void _update_upper_partials_root_and_undefined( const SingleTreeLikelihood *tlk, const double *partials1, const double *matrices1, const double *frequencies, double *partials_upper ){
    int w,i,j,k;
    int v = 0;
	double *pPartials = partials_upper;
    int nstate = tlk->sm->nstate;
    
	for ( int l = 0; l < tlk->cat_count; l++ ) {
		
		for ( k = 0; k < tlk->pattern_count; k++ ) {
			
			w = l * tlk->matrix_size;
			
			for ( i = 0; i < nstate; i++ ) {
				
				*pPartials = 0.;
				
				for ( j = 0; j < nstate; j++ ) {
					*pPartials += matrices1[w] * partials1[v + j];
					w++;
				}
				*pPartials *= frequencies[i];
				pPartials++;
			}
			v += nstate;
		}
    }
}

// Called by a child of the root and the child's sibling is a leaf
static void _update_upper_partials_root_and_state( const SingleTreeLikelihood *tlk, const double *matrices1, int idx1, const double *frequencies, double *partials_upper ){
    int w;
	double *pPartials = partials_upper;
    int state1;
    int nstate = tlk->sm->nstate;
    
	for ( int l = 0; l < tlk->cat_count; l++ ) {
		
		for ( int k = 0; k < tlk->pattern_count; k++ ) {
			
            state1 = tlk->sp->patterns[k][idx1];
            
			w = l * tlk->matrix_size;
            
            if( state1 < nstate ){
                for ( int i = 0; i < nstate; i++ ) {
                    *pPartials++ = matrices1[w+state1] * frequencies[i];
                    w+=nstate;
                }
            }
            else {
                memcpy(pPartials, frequencies, sizeof(double)*nstate);
                pPartials += nstate;
            }
            
		}
    }
}



void update_partials_upper_general( SingleTreeLikelihood *tlk, Node *node ){
    Node *parent = Node_parent(node);
    Node *sibling = Node_sibling(node);
	
	const double* freqs = tlk->get_root_frequencies(tlk);
    if( Node_isroot(parent) ){
        if( Node_isleaf(sibling) ){
            _update_upper_partials_root_and_state(tlk, tlk->matrices[Node_id(sibling)], tlk->mapping[Node_id(sibling)], freqs, tlk->partials_upper[Node_id(node)] );
        }
        else {
            _update_upper_partials_root_and_undefined(tlk, tlk->partials[Node_id(sibling)],  tlk->matrices[Node_id(sibling)],  freqs, tlk->partials_upper[Node_id(node)] );
        }
    }
    else if( Node_isleaf(sibling) ){
        _update_upper_partials_state(tlk, tlk->matrices[Node_id(parent)], tlk->partials_upper[Node_id(parent)], tlk->matrices[Node_id(sibling)], tlk->mapping[Node_id(sibling)], tlk->partials_upper[Node_id(node)]);
    }
    else {
        _update_upper_partials_undefined(tlk, tlk->matrices[Node_id(parent)], tlk->partials_upper[Node_id(parent)], tlk->matrices[Node_id(sibling)], tlk->partials[Node_id(sibling)], tlk->partials_upper[Node_id(node)]);
    }
}

static void _partial_lower_upper( const SingleTreeLikelihood *tlk, const double *partials_upper, const double *partials_lower, const double *matrix_lower, const double *proportions, double *pattern_lk ){
    int w,i,j,k;
    int v = 0;
    double p,sum;
    int nstate = tlk->sm->nstate;
    
    memset(pattern_lk, 0, tlk->pattern_count*sizeof(double));
    
	for ( int l = 0; l < tlk->cat_count; l++ ) {
		
		for ( k = 0; k < tlk->pattern_count; k++ ) {
			
			w = l * tlk->matrix_size;
            p = 0;
            
			for ( i = 0; i < nstate; i++ ) {
                sum = 0;
				
				for ( j = 0; j < nstate; j++) {
					sum += matrix_lower[w] * partials_lower[v + j];
					w++;
				}
				
				p += sum * partials_upper[v + i];
			}
            pattern_lk[k] += p * proportions[l];
			v += nstate;
		}
    }
}

static void _partial_lower_upper_leaf( const SingleTreeLikelihood *tlk, const double *partials_upper, int idx, const double *matrix_lower, const double *proportions, double *pattern_lk ){
    int w,j,k;
    int v = 0;
    double p = 0;
    int state;
    int nstate = tlk->sm->nstate;
    
    memset(pattern_lk, 0, tlk->pattern_count*sizeof(double));
    
	for ( int l = 0; l < tlk->cat_count; l++ ) {
		
		for ( k = 0; k < tlk->pattern_count; k++ ) {
			state = tlk->sp->patterns[k][idx];
            
			w = l * tlk->matrix_size;
            p = 0;
            if( state < nstate ){
                
                for ( j = 0; j < nstate; j++) {
                    p += matrix_lower[w+state] * partials_upper[v + j];
                    w += nstate;
                }
            }
            else {
                for ( j = 0; j < nstate; j++) {
                    p += partials_upper[v + j];
                }
            }
            pattern_lk[k] += p * proportions[l];
			v += nstate;
		}
    }
}

void node_log_likelihoods_upper_general( const SingleTreeLikelihood *tlk, Node *node ){
	int node_index = Node_id(node);
    
    if ( !Node_isleaf(node) ) {
        _partial_lower_upper(tlk, tlk->partials_upper[node_index], tlk->partials[node_index], tlk->matrices[node_index], tlk->sm->get_proportions(tlk->sm), tlk->node_pattern_lk );
    }
    else {
        _partial_lower_upper_leaf(tlk, tlk->partials_upper[node_index], tlk->mapping[node_index], tlk->matrices[node_index], tlk->sm->get_proportions(tlk->sm), tlk->node_pattern_lk );
    }
}




#pragma mark -
#pragma mark Lower likelihood with ancestral sequences known

/*
 * Calculate likelihood with unknown and known internal nodes
 */

// children and ancestor are known
static void _partials_states_and_states_ancestral( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *matrices2, int idx3, double *partials ){
	
	int w,k,u = 0;
	int state1, state2, state3;
	double *pPartials = partials;
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	for ( int l = 0; l < catCount; l++ ) {
		
		for ( k = 0; k < patternCount; k++ ) {
			
			state1 = tlk->sp->patterns[k][idx1];
			state2 = tlk->sp->patterns[k][idx2];
			state3 = tlk->sp->patterns[k][idx3];
			
			w = u;
			
            int p = w + nstate * state3;
            
			if (state1 < nstate && state2 < nstate) {
                *pPartials++ = matrices1[p + state1] * matrices2[p + state2];
			}
			else if (state1 < nstate) {
				// child 1 has a gap or unknown state so treat it as unknown
				
                *pPartials++ = matrices1[p + state1];
			}
            else if (state2 < nstate ) {
				// child 2 has a gap or unknown state so treat it as unknown
				
                *pPartials++ = matrices2[p + state2];
			}
            else {
				// both children have a gap or unknown state so set partials to 1
				
				*pPartials++ = 1.0;
				
			}
		}
		u += tlk->matrix_size;
	}
    
}



static void _partials_states_and_undefined_ancestral( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *partials2, const double *matrices2, int idx3, double *partials3 ){
    
	int v = 0;
	int k;
	int w;
	int state1,state2,state3;
	const int nstate = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
    
	double *pPartials = partials3;
	
	for ( int l = 0; l < catCount; l++ ) {
		
		for ( k = 0; k < patternCount; k++ ) {
			
			state1 = tlk->sp->patterns[k][idx1];
			state2 = tlk->sp->patterns[k][idx2];
			state3 = tlk->sp->patterns[k][idx3];
			
			w = l * tlk->matrix_size;
            
            int p = w + nstate * state3;
			
			if ( state1 < nstate) {
                *pPartials++ = matrices1[p + state1] * matrices2[p + state2] * partials2[v];
			}
			else {
				// Child 1 has a gap or unknown state so don't use it
                
				*pPartials++ = matrices2[p + state2] * partials2[v];
			}
			
			v++;
		}
    }
}



static void _partials_undefined_and_undefined_ancestral( const SingleTreeLikelihood *tlk, int idx1, const double *partials1, const double *matrices1, int idx2, const double *partials2, const double *matrices2, int idx3, double *partials3 ){
	
	int v = 0;
	int k;
	int state1,state2,state3;
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	int w;
	double *pPartials = partials3;
	
	for ( int l = 0; l < catCount; l++ ) {
		
        w = l * tlk->matrix_size;
        
		for ( k = 0; k < patternCount; k++ ) {
            
            state1 = tlk->sp->patterns[k][idx1];
			state2 = tlk->sp->patterns[k][idx2];
			state3 = tlk->sp->patterns[k][idx3];
            
            int p = w + nstate * state3;
            
            *pPartials++ = matrices1[p + state1] * partials1[v] * matrices2[p + state2] * partials2[v];
            
			v++;
		}
	}
}

// children are internal and known
// ancestor is not known
static void _partials_undefined_and_undefined_ancestral2( const SingleTreeLikelihood *tlk, int idx1, const double *partials1, const double *matrices1, int idx2, const double *partials2, const double *matrices2, double *partials3 ){
	int state1,state2;
	
	int v = 0;
	int i,k;
	
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	int w;
	double *pPartials = partials3;
	
	for ( int l = 0; l < catCount; l++ ) {
		
		for ( k = 0; k < patternCount; k++ ) {
			
            state1 = tlk->sp->patterns[k][idx1];
			state2 = tlk->sp->patterns[k][idx2];
            
			w = l * tlk->matrix_size;
			
			for ( i = 0; i < nstate; i++ ) {
				
				*pPartials++ = matrices1[w + state1] * partials1[v] * matrices2[w + state2] * partials2[v];
                w += nstate;
				
			}
			v++;
		}
	}
}

// idx1 is a leaf and idx2 is internal and known
// ancestor is not known
static void _partials_states_and_undefined_ancestral2( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *partials2, const double *matrices2, double *partials3 ){
	
	int v = 0;
	int i,k;
	int w;
	int state1,state2;
	const int nstate = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	double *pPartials = partials3;
	
	for ( int l = 0; l < catCount; l++ ) {
		
		for ( k = 0; k < patternCount; k++ ) {
			
			state1 = tlk->sp->patterns[k][idx1];
			state2 = tlk->sp->patterns[k][idx2];
			
			w = l * tlk->matrix_size;
			
			if ( state1 < nstate) {
				
				for ( i = 0; i < nstate; i++ ) {
                    
                    *pPartials++ = matrices1[w + state1] * matrices2[w + state2] * partials2[v];
                    w += nstate;
                    
                }
				
			}
			else {
				// Child 1 has a gap or unknown state so don't use it
				
				for ( i = 0; i < nstate; i++ ) {
                    
                    *pPartials++ = matrices1[w + state1] * matrices2[w + state2] * partials2[v];
                    w += nstate;
                }
				
			}
			
			v++;
		}
	}
    
}

void update_partials_general_ancestral( SingleTreeLikelihood *tlk, int nodeIndex1, int nodeIndex2, int nodeIndex3 ) {
    
    // Internal node known
    if( tlk->mapping[nodeIndex3] != -1 ){
        // node1 and node2 are leaves
        if( tlk->partials[nodeIndex1] == NULL && tlk->partials[nodeIndex2] == NULL ){
            _partials_states_and_states_ancestral(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        // node1 is a leaf
        else if( tlk->partials[nodeIndex1] == NULL ){
            _partials_states_and_undefined_ancestral(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        // node2 is a leaf
        else if( tlk->partials[nodeIndex2] == NULL ){
            _partials_states_and_undefined_ancestral(tlk, tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        else {
            _partials_undefined_and_undefined_ancestral(tlk, tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        // 2 more cases where one of the internal nodes is unknown
        // 2 more cases when there is a leaf and unknown internal node
    }
    // Internal node unknown
    else {
        // node1: leaf node2: leaf
        if( tlk->partials[nodeIndex1] == NULL && tlk->partials[nodeIndex2] == NULL ){
            partials_states_and_states(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex3]);
        }
        // node1: leaf node2: internal
        else if( tlk->partials[nodeIndex1] == NULL ){
            // node 2 known
            if( tlk->mapping[nodeIndex2] != -1 ){
                _partials_states_and_undefined_ancestral2(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex3]);
            }
            else {
                partials_states_and_undefined(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex3]);
            }
        }
        // node2: leaf node1: internal
        else if( tlk->partials[nodeIndex2] == NULL ){
            // node 1 known
            if( tlk->mapping[nodeIndex1] != -1 ){
                _partials_states_and_undefined_ancestral2(tlk, tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->partials[nodeIndex3]);
            }
            else {
                partials_states_and_undefined(tlk, tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->partials[nodeIndex3]);
            }
        }
        // node1: internal known node2: internal known
        else {
            _partials_undefined_and_undefined_ancestral2(tlk, tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex3]);
        }
        // 2 more cases where one of the internal nodes is unknown
    }
    
	if ( tlk->scale ) {
		SingleTreeLikelihood_scalePartials( tlk, nodeIndex3);
	}
}


#pragma mark -
#pragma mark REL Lower likelihood ancestral sequences known


// children and ancestor are known
static void _partials_states_and_states_ancestral_rel( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *matrices2, int idx3, double *partials ){
	
	int k,u = 0;
	int state1, state2, state3;
	double *pPartials = partials;
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
    for ( k = 0; k < patternCount; k++ ) {
        
        state1 = tlk->sp->patterns[k][idx1];
        state2 = tlk->sp->patterns[k][idx2];
        state3 = tlk->sp->patterns[k][idx3];
        
        u = 0;
        
        double sum1 = 0;
        double sum2 = 0;
        
        if (state1 < nstate && state2 < nstate) {
            for ( int l = 0; l < catCount; l++ ) {
                int i = u + nstate * state3;
                double  p = tlk->sm->get_proportion(tlk->sm, l);
                sum1 += matrices1[i + state1]*p;
                sum2 += matrices2[i + state2]*p;
                u += tlk->matrix_size;
            }
            *pPartials++ = sum1*sum2;
        }
        else if (state1 < nstate) {
            // child 1 has a gap or unknown state so treat it as unknown
            for ( int l = 0; l < catCount; l++ ) {
                int i = u + nstate * state3;
                double  p = tlk->sm->get_proportion(tlk->sm, l);
                sum1 += matrices1[i + state1]*p;
                u += tlk->matrix_size;
            }
            *pPartials++ = sum1;
        }
        else if (state2 < nstate ) {
            // child 2 has a gap or unknown state so treat it as unknown
            
            for ( int l = 0; l < catCount; l++ ) {
                int i = u + nstate * state3;
                double  p = tlk->sm->get_proportion(tlk->sm, l);
                sum1 += matrices2[i + state2]*p;
                u += tlk->matrix_size;
            }
            *pPartials++ = sum1;
        }
        else {
            // both children have a gap or unknown state so set partials to 1
            
            *pPartials++ = 1.0;
            
        }
    }
	
}

static void _partials_states_and_states_ancestral_relb( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *matrices2, int idx3, double *partials ){
	
	int k;
    //int u = 0;
	int state1, state2, state3;
	//double *pPartials = partials;
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
    int max_r = 0;
    int max_l = 0;
    double max = -INFINITY;
    
    //printf("%d", idx3);
    
    for ( int l = 0; l < catCount; l++ ) {
        
        for ( int r = 0; r < catCount; r++ ) {
            
            double lnl = 0;
            
            for ( k = 0; k < patternCount; k++ ) {
                
                state1 = tlk->sp->patterns[k][idx1];
                state2 = tlk->sp->patterns[k][idx2];
                state3 = tlk->sp->patterns[k][idx3];
                
                int i = l*tlk->matrix_size + nstate * state3;
                int j = r*tlk->matrix_size + nstate * state3;
                
                if (state1 < nstate && state2 < nstate) {
                    lnl += log(matrices1[i + state1] * matrices2[j + state2]) * tlk->sp->weights[k];
                }
                else if (state1 < nstate) {
                    // child 1 has a gap or unknown state so treat it as unknown
                    lnl += log(matrices1[i + state1]) * tlk->sp->weights[k];
                }
                else if (state2 < nstate ) {
                    // child 2 has a gap or unknown state so treat it as unknown
                    
                    lnl += log(matrices2[j + state2]) * tlk->sp->weights[k];
                }
                
                
            }
            printf(" %f [%d %d]",lnl,l,r);
            if( lnl > max ){
                max_r = r;
                max_l = l;
                max = lnl;
            }
        }
    
    }
	printf(" -> %d %d\n",max_l,max_r);
}




static void _partials_states_and_undefined_ancestral_rel( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *partials2, const double *matrices2, int idx3, double *partials3 ){
    
	int v = 0;
	int k,u;
	int state1,state2,state3;
	const int nstate = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
    
	double *pPartials = partials3;
			
    for ( k = 0; k < patternCount; k++ ) {
        
        state1 = tlk->sp->patterns[k][idx1];
        state2 = tlk->sp->patterns[k][idx2];
        state3 = tlk->sp->patterns[k][idx3];
        
        u = 0;
        
        double sum1 = 0;
        double sum2 = 0;
        
        if ( state1 < nstate) {
            for ( int l = 0; l < catCount; l++ ) {
                int i = u + nstate * state3;
                double  p = tlk->sm->get_proportion(tlk->sm, l);
                sum1 += matrices1[i + state1]*p;
                sum2 += matrices2[i + state2]*p;
                u += tlk->matrix_size;
            }
            *pPartials++ = sum1 * sum2 * partials2[v];
        }
        else {
            // Child 1 has a gap or unknown state so don't use it
            
            for ( int l = 0; l < catCount; l++ ) {
                int i = u + nstate * state3;
                double  p = tlk->sm->get_proportion(tlk->sm, l);
                sum2 += matrices2[i + state2]*p;
                u += tlk->matrix_size;
            }
            *pPartials++ = sum2 * partials2[v];
        }
        v++;
    }
    
}



static void _partials_undefined_and_undefined_ancestral_rel( const SingleTreeLikelihood *tlk, int idx1, const double *partials1, const double *matrices1, int idx2, const double *partials2, const double *matrices2, int idx3, double *partials3 ){
	
	int v = 0;
	int k,u;
	int state1,state2,state3;
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	double *pPartials = partials3;
	
    for ( k = 0; k < patternCount; k++ ) {
        
        state1 = tlk->sp->patterns[k][idx1];
        state2 = tlk->sp->patterns[k][idx2];
        state3 = tlk->sp->patterns[k][idx3];
        
        u = 0;
        double sum1 = 0;
        double sum2 = 0;
        
        for ( int l = 0; l < catCount; l++ ) {
            int i = u + nstate * state3;
            double  p = tlk->sm->get_proportion(tlk->sm, l);
            sum1 += matrices1[i + state1]*p;
            sum2 += matrices2[i + state2]*p;
            u += tlk->matrix_size;
        }
        *pPartials++ = sum1 * sum2 * partials1[v] * partials2[v];
        
        v++;
    }

}

// children are internal and known
// ancestor is not known
static void _partials_undefined_and_undefined_ancestral2_rel( const SingleTreeLikelihood *tlk, int idx1, const double *partials1, const double *matrices1, int idx2, const double *partials2, const double *matrices2, double *partials3 ){
	int state1,state2;
	
	int v = 0;
	int i,k,u;
	
	const int nstate   = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	double *pPartials = partials3;
	
	
		
    for ( k = 0; k < patternCount; k++ ) {
        
        state1 = tlk->sp->patterns[k][idx1];
        state2 = tlk->sp->patterns[k][idx2];
        
        for ( i = 0; i < nstate; i++ ) {
            
            u = 0;
            double sum1 = 0;
            double sum2 = 0;
            
            for ( int l = 0; l < catCount; l++ ) {
                int j = u + nstate * i;
                double  p = tlk->sm->get_proportion(tlk->sm, l);
                sum1 += matrices1[j + state1]*p;
                sum2 += matrices2[j + state2]*p;
                u += tlk->matrix_size;
            }
            
            *pPartials++ = sum1 * partials1[v] * sum2 * partials2[v];
        }
        v++;
    }
	
}

// idx1 is a leaf and idx2 is internal and known
// ancestor is not known
static void _partials_states_and_undefined_ancestral2_rel( const SingleTreeLikelihood *tlk, int idx1, const double *matrices1, int idx2, const double *partials2, const double *matrices2, double *partials3 ){
	
	int v = 0;
	int i,k;
	int w;
	int state1,state2;
	const int nstate = tlk->sm->nstate;
	const int catCount = tlk->cat_count;
	const int patternCount = tlk->pattern_count;
	
	double *pPartials = partials3;
    
    error("_partials_states_and_undefined_ancestral2_rel not done yet\n");
	
	for ( int l = 0; l < catCount; l++ ) {
		
		for ( k = 0; k < patternCount; k++ ) {
			
			state1 = tlk->sp->patterns[k][idx1];
			state2 = tlk->sp->patterns[k][idx2];
			
			w = l * tlk->matrix_size;
			
			if ( state1 < nstate) {
				
				for ( i = 0; i < nstate; i++ ) {
                    
                    *pPartials++ = matrices1[w + state1] * matrices2[w + state2] * partials2[v];
                    w += nstate;
                    
                }
				
			}
			else {
				// Child 1 has a gap or unknown state so don't use it
				
				for ( i = 0; i < nstate; i++ ) {
                    
                    *pPartials++ = matrices1[w + state1] * matrices2[w + state2] * partials2[v];
                    w += nstate;
                }
				
			}
			
			v++;
		}
	}
    
}

void update_partials_general_ancestral_rel( SingleTreeLikelihood *tlk, int nodeIndex1, int nodeIndex2, int nodeIndex3 ) {
    
    // internal is known
    if( tlk->mapping[nodeIndex3] != -1 ){
        // leaves are always known
        if( tlk->partials[nodeIndex1] == NULL && tlk->partials[nodeIndex2] == NULL ){
            _partials_states_and_states_ancestral_rel(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        else if( tlk->partials[nodeIndex1] == NULL ){
            _partials_states_and_undefined_ancestral_rel(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        else if( tlk->partials[nodeIndex2] == NULL ){
            _partials_states_and_undefined_ancestral_rel(tlk, tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        else {
            _partials_undefined_and_undefined_ancestral_rel(tlk, tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
    }
    else {
        if( tlk->partials[nodeIndex1] == NULL ){
            _partials_states_and_undefined_ancestral2_rel(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex3]);
        }
        else if( tlk->partials[nodeIndex2] == NULL ){
            _partials_states_and_undefined_ancestral2_rel(tlk, tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->partials[nodeIndex3]);
        }
        else {
            _partials_undefined_and_undefined_ancestral2_rel(tlk, tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex3]);
        }
    }
    
	if ( tlk->scale ) {
		SingleTreeLikelihood_scalePartials( tlk, nodeIndex3);
	}
}

void update_partials_general_ancestral_relb( SingleTreeLikelihood *tlk, int nodeIndex1, int nodeIndex2, int nodeIndex3 ) {
    
    // internal is known
    if( tlk->mapping[nodeIndex3] != -1 ){
        // leaves are always known
        if( tlk->partials[nodeIndex1] == NULL && tlk->partials[nodeIndex2] == NULL ){
            printf( "%s", Tree_node(tlk->tree, nodeIndex3)->name);
            _partials_states_and_states_ancestral_relb(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        else if( tlk->partials[nodeIndex1] == NULL ){
            _partials_states_and_undefined_ancestral_rel(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        else if( tlk->partials[nodeIndex2] == NULL ){
            _partials_states_and_undefined_ancestral_rel(tlk, tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
        else {
            _partials_undefined_and_undefined_ancestral_rel(tlk, tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex3], tlk->partials[nodeIndex3]);
        }
    }
    else {
        if( tlk->partials[nodeIndex1] == NULL ){
            _partials_states_and_undefined_ancestral2_rel(tlk, tlk->mapping[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex3]);
        }
        else if( tlk->partials[nodeIndex2] == NULL ){
            _partials_states_and_undefined_ancestral2_rel(tlk, tlk->mapping[nodeIndex2], tlk->matrices[nodeIndex2], tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->partials[nodeIndex3]);
        }
        else {
            _partials_undefined_and_undefined_ancestral2_rel(tlk, tlk->mapping[nodeIndex1], tlk->partials[nodeIndex1], tlk->matrices[nodeIndex1], tlk->mapping[nodeIndex2], tlk->partials[nodeIndex2], tlk->matrices[nodeIndex2], tlk->partials[nodeIndex3]);
        }
    }
    
	if ( tlk->scale ) {
		SingleTreeLikelihood_scalePartials( tlk, nodeIndex3);
	}
}

// we do not integrate anything
void integrate_partials_general_rel( const SingleTreeLikelihood *tlk, const double *inPartials, const double *proportions, double *outPartials ){
	
    memcpy(outPartials, inPartials, sizeof(double)*tlk->pattern_count*tlk->sm->nstate);
	
    
}
