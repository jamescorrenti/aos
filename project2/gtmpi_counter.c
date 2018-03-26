#include <stdlib.h>
#include <mpi.h>
#include <stdio.h>
#include "gtmpi.h"
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


static MPI_Status* status_array;
static int P;

void gtmpi_init(int num_threads){
  P = num_threads;
}

void gtmpi_barrier(){
  int vpid, i;
  MPI_Status stat;
  MPI_Comm_rank(MPI_COMM_WORLD, &vpid);
  int last_proc = P - 1;


  /*
   * In order to improve perfromance, I looked to reduce the number of communications
   * going accross the network. Before, each processor was sending and receiving from
   * everynode in the network. Now with this implementation, only the last processor 
   * receives a message and sends a message to every processor. All the other processors
   * just send and receive messages from the last processor. 
   */
  if (last_proc == vpid){
    for(i = 0; i < last_proc; i++){
      MPI_Recv(NULL, 0, MPI_INT, i, 1, MPI_COMM_WORLD, NULL);
    }
    for(i = 0; i < last_proc; i++)
      MPI_Send(NULL, 0, MPI_INT, i, 1, MPI_COMM_WORLD);
  }
  else{
    MPI_Send(NULL, 0, MPI_INT, last_proc, 1, MPI_COMM_WORLD);
    MPI_Recv(NULL, 0, MPI_INT, last_proc, 1, MPI_COMM_WORLD, NULL);
  }

}

void gtmpi_finalize(){
}

