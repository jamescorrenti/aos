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
bool local_sense = false;
static int total_threads;
int count;


int fetch_and_decrement(int *count){
    int new_count;
    #pragma omp atomic read 
        new_count = *count;
    
    #pragma omp atomic write
        *count = new_count - 1;
    return new_count;
}

void gtmp_init(int num_threads){
    total_threads = num_threads;
    count = num_threads;
}

void gtmp_barrier(){
    
    if (fetch_and_decrement(&count) == 1)
    {
        printf("Unlocking\n");
        count = total_threads;
        sense = local_sense;
    }
    else
    {
        while(sense != local_sense)
        {
            printf("Thread spinning\n");
            usleep(1);
        }
    }
    printf("Thread Exiting\n");
}

void gtmp_finalize(){
}
