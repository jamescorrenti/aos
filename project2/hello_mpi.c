#include <stdio.h>
#include <sys/utsname.h>
#include <mpi.h>
#include "gtmpi.h"
#include <time.h>
#include <omp.h>

/*
  if (argc < 2){
    fprintf(stderr, "Usage: ./hello_world [NUM_THREADS]\n");
    exit(1);
  }

  num_threads = strtol(argv[1], NULL, 10);
  omp_set_dynamic(0);
  if (omp_get_dynamic()) {printf("Warning: dynamic adjustment of threads has been set\n");}
  omp_set_num_threads(num_threads);
  
  clock_t begin = clock();

#pragma omp parallel
  {
    
    
    //printf("Thread %d reached the barrier.\n", thread_num); // num_threads, ugnm.nodename);
    
    gtmp_barrier(thread_num);

    //printf("Thread %d is past the barrier.\n", thread_num);    
  } 
  clock_t end = clock();
  // Resume serial code
  printf("Serial section resuming, execution time: %f\n", (double) (end - begin) / CLOCKS_PER_SEC);
  
  return 0;

*/



int main(int argc, char **argv)
{
  double inittime;
  //clock_t begin = clock();
  int numprocs, myrank;
  //printf("This is the serial section\n");
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  inittime = MPI_Wtime();
  gtmpi_init(numprocs);
  //printf("Proc %d reached the barrier.\n", myrank); // num_threads, ugnm.nodename);

  gtmpi_barrier();
  //clock_t end = clock();
  // Resume serial code
  //printf("Serial section resuming, execution time: %f\n", (double) (end - begin) / CLOCKS_PER_SEC);
  //printf("Proc %d is past the barrier.\n", myrank);    
  printf("This is the time %f\n", MPI_Wtime() - inittime );
  
  MPI_Finalize();
  gtmpi_finalize();

  return 0;
}




