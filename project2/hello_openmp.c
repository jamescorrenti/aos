#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <sys/utsname.h>
#include "gtmp.h"
#include <time.h>

int main(int argc, char **argv)
{
  int num_threads;
  // Serial code
  printf("This is the serial section\n");
  if (argc < 2){
    fprintf(stderr, "Usage: ./hello_world [NUM_THREADS]\n");
    exit(1);
  }

  num_threads = strtol(argv[1], NULL, 10);
  omp_set_dynamic(0);
  if (omp_get_dynamic()) {printf("Warning: dynamic adjustment of threads has been set\n");}
  omp_set_num_threads(num_threads);
  gtmp_init(num_threads);
  clock_t begin = clock();

#pragma omp parallel
  {
    int thread_num = omp_get_thread_num();
    
    //printf("Thread %d reached the barrier.\n", thread_num); // num_threads, ugnm.nodename);
    
    gtmp_barrier(thread_num);

    //printf("Thread %d is past the barrier.\n", thread_num);    
  } 
  clock_t end = clock();
  // Resume serial code
  printf("Serial section resuming, execution time: %f\n", (double) (end - begin) / CLOCKS_PER_SEC);
  gtmp_finalize();
  return 0;


}

