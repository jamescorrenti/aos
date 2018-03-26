#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include "gtmpi.h"
#include <math.h>
#include <stdbool.h>

// I capped the max number of rounds to 100 due to hardware limitation of my system
#define MAX_PROCS 100

/*
    From the MCS Paper: The scalable, distributed dissemination barrier with only local spinning.

    type flags = record
        myflags : array [0..1] of array [0..LogP - 1] of Boolean
	    partnerflags : array [0..1] of array [0..LogP - 1] of ^Boolean
	
    processor private parity : integer := 0
    processor private sense : Boolean := true
    processor private localflags : ^flags

    shared allnodes : array [0..P-1] of flags
        //allnodes[i] is allocated in shared memory
	//locally accessible to processor i

    //on processor i, localflags points to allnodes[i]
    //initially allnodes[i].myflags[r][k] is false for all i, r, k
    //if j = (i+2^k) mod P, then for r = 0 , 1:
    //    allnodes[i].partnerflags[r][k] points to allnodes[j].myflags[r][k]

    procedure dissemination_barrier
        for instance : integer: 0 to LogP-1
	    localflags^.partnerflags[parity][instance]^ := sense
	    repeat until localflags^.myflags[parity][instance] = sense
	if parity = 1
	    sense := not sense
	parity := 1 - parity
*/

typedef struct _flags
{
    int myflags[2][MAX_PROCS];
    int partnerflags[2][MAX_PROCS];
} flags;

static int P;
static flags *allnodes;
static int num_flags;

void gtmpi_init(int num_threads){
    P = num_threads;
    int i, k, j;
   
    num_flags = (int) ceil(log2(P));

    allnodes = malloc(num_flags * sizeof(flags));
    MPI_Comm_rank(MPI_COMM_WORLD, &i);
    int t;

    for (k = 0; k < num_flags; k++){
        for (j = 0; j < P; j++){

            t = ceil(exp2(k));
            
            // Instead of setting my flags to spin on a boolean, myflag is set to the
            // process that will message this one instead and we can spin on MPI_recv
            // for this node 

            if (i == (int) (j+ t) % P){
                allnodes->myflags[0][k] = j;
                allnodes->myflags[1][k] = j;
            }

            if(j == (int) (i+ t) % P){
                allnodes->partnerflags[0][k] = j;
                allnodes->partnerflags[1][k] = j;
            }
        }
    }    
}

void gtmpi_barrier(){
    int parity = 0;
    bool sense = true;
    int instance, vpid;
    int msg = 1, t = 1;
    
    MPI_Comm_rank(MPI_COMM_WORLD, &vpid);

    for (instance = 0; instance < num_flags; instance++){
        MPI_Send(&msg, 1, MPI_INT, allnodes->partnerflags[parity][instance], t, MPI_COMM_WORLD);   
        MPI_Recv(&msg, 1, MPI_INT, allnodes->myflags[parity][instance], t, MPI_COMM_WORLD, NULL);
    }

    if (parity == 1){
        sense = !sense;
    }

    parity = 1 - parity;
}

void gtmpi_finalize(){
    free(allnodes);
}
