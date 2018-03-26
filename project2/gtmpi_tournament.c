#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <math.h>
#include <stdbool.h>
#include "gtmpi.h"
#include <mpi.h>

/*
    From the MCS Paper: A scalable, distributed tournament barrier with only local spinning

    type round_t = record
        role : (winner, loser, bye, champion, dropout)
		opponent : ^Boolean
		flag : Boolean
    
    shared rounds : array [0..P-1][0..LogP] of round_t
        // row vpid of rounds is allocated in shared memory
	// locally accessible to processor vpid

    processor private sense : Boolean := true
    processor private vpid : integer // a unique virtual processor index

    //initially
    //    rounds[i][k].flag = false for all i,k
    //rounds[i][k].role = 
    //    winner if k > 0, i mod 2^k = 0, i + 2^(k-1) < P , and 2^k < P
    //    bye if k > 0, i mode 2^k = 0, and i + 2^(k-1) >= P
    //    loser if k > 0 and i mode 2^k = 2^(k-1)
    //    champion if k > 0, i = 0, and 2^k >= P
    //    dropout if k = 0
    //    unused otherwise; value immaterial
    //rounds[i][k].opponent points to 
    //    round[i-2^(k-1)][k].flag if rounds[i][k].role = loser
    //    round[i+2^(k-1)][k].flag if rounds[i][k].role = winner or champion
    //    unused otherwise; value immaterial
    procedure tournament_barrier
        round : integer := 1
	loop   //arrival
	    case rounds[vpid][round].role of
	        loser:
	            rounds[vpid][round].opponent^ :=  sense
		    repeat until rounds[vpid][round].flag = sense
		    exit loop
   	        winner:
	            repeat until rounds[vpid][round].flag = sense
		bye:  //do nothing
		champion:
	            repeat until rounds[vpid][round].flag = sense
		    rounds[vpid][round].opponent^ := sense
		    exit loop
		dropout: // impossible
	    round := round + 1
	loop  // wakeup
	    round := round - 1
	    case rounds[vpid][round].role of
	        loser: // impossible
		winner:
		    rounds[vpid[round].opponent^ := sense
		bye: // do nothing
		champion: // impossible
		dropout:
		    exit loop
	sense := not sense
*/

enum result { winner, loser, bye, champion, dropout, unused };

typedef struct _round_t{
	enum result role;
	int opponent;
	bool flag;
} round_t;

static round_t *rounds;
static int P, num_rounds;

void gtmpi_init(int num_threads){
	P = num_threads;
  	int i, k;

  	num_rounds = (int) ceil(log2(P));
	MPI_Comm_rank(MPI_COMM_WORLD, &i);
	rounds = malloc((num_rounds + 1) * sizeof(round_t));
	for(k = 0; k <= num_rounds; k++){		
		rounds[k].flag = true;
		int exponent = exp2(k);
		int i_mod_exp = (i % exponent);
		int t = exp2(k-1);
		if (k > 0) {
			if (i == 0 && exponent >= P) {
				rounds[k].role = champion;
			}
			else if(i_mod_exp == 0) {
				if (i + t >= P) {
					rounds[k].role = bye;
				}
				else {
					if (exponent < P)
						rounds[k].role = winner;
					else {
						rounds[k].role = unused; 
					}
				}
			}
			else if (i_mod_exp == t) {
				rounds[k].role = loser;
			}
			else {
				rounds[k].role = unused; 
			}
		}	
		else if (k == 0) {
		    rounds[k].role = dropout;
		}
		else {
			rounds[k].role = unused; 
		}

		if (rounds[k].role == loser){
			rounds[k].opponent = i - t;
		}
		else if (rounds[k].role == winner || rounds[k].role == champion ){
			rounds[k].opponent = i + t;
		}
		else 
			rounds[k].opponent = -1;
	}
}

void gtmpi_barrier(){
	int r = 1;
	int msg = 1;
	int t = 1;
	int opp,i;
	enum result role;
	MPI_Comm_rank(MPI_COMM_WORLD, &i);

	while(true){		
		role = rounds[r].role;
		opp = rounds[r].opponent;
		if (role == winner){
			MPI_Recv(&msg, 1, MPI_INT, opp, t, MPI_COMM_WORLD, NULL);
		}

		if (role == champion){
			MPI_Recv(&msg, 1, MPI_INT, opp, t, MPI_COMM_WORLD, NULL);
			MPI_Send(&msg, 1, MPI_INT, opp, t, MPI_COMM_WORLD);	
			break;
		}
		if (role == loser){
			MPI_Send(&msg, 1, MPI_INT, opp, t, MPI_COMM_WORLD);	
			MPI_Recv(&msg, 1, MPI_INT, opp, t, MPI_COMM_WORLD, NULL);
			break;
		}
		if (r <= num_rounds)
			r += 1;
	}

	while(true){
		if (r > 0)
			r -= 1;
		
		role = rounds[r].role;
		opp = rounds[r].opponent;

		if (role == winner){
			MPI_Send(&msg, 1, MPI_INT, opp, t, MPI_COMM_WORLD);
		}

		if (role == dropout){
			break;
		}
	}
}

void gtmpi_finalize(){
	free(rounds);
}
