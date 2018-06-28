/**
 *  @file   heatdis.h
 *  @author Leonardo A. Bautista Gomez
 *  @date   May, 2014
 *  @brief  Heat distribution code to test VeloC.
 */
 
#ifndef _HEATDIS_H
#define _HEATDIS_H
 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>


#include "include/veloc.h"
#include "include/veloc_types.h"

#define PRECISION   0.00001
#define ITER_TIMES  600
#define ITER_OUT    50
#define WORKTAG     5
#define REDUCE      1

#ifdef __cplusplus
extern "C" {
#endif

void initData(int nbLines, int M, int rank, double *h);
double doWork(int numprocs, int rank, int M, int nbLines, double *g, double *h);

#ifdef __cplusplus
}
#endif

#endif /* ----- #ifndef _HEATDIS_H  ----- */
