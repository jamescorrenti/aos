#include <omp.h>
#include "gtmp.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

/*
    From the MCS Paper: A sense-reversing centralized barrier

    shared count : integer := P
    shared sense : Boolean := true
    processor private local_sense : Boolean := true

    procedure central_barrier
        local_sense := not local_sense // each processor toggles its own sense
	if fetch_and_decrement (&count) = 1
	    count := P
	    sense := local_sense // last processor toggles global sense
        else
           repeat until sense = local_sense
*/

bool sense = true;
static int total_threads;
int count;

void gtmp_init(int num_threads){
    total_threads = num_threads;
    count = num_threads;
}

void gtmp_barrier(){    
    bool local_sense = false;
    if (__sync_fetch_and_sub(&count, 1)  == 1)
    { 
        count = total_threads;
        sense = local_sense;
    }
    
    # pragma omp task
        while(sense != local_sense){
            # pragma omp taskyield
            usleep(10000);
        };        
    

}

void gtmp_finalize(){
}
