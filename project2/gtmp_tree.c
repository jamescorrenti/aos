#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include "gtmp.h"
#include <unistd.h>

/*

    From the MCS Paper: A software combining tree barrier with optimized wakeup

    type node = record
        k : integer //fan in of this node
      	count : integer // initialized to k
      	locksense : Boolean // initially false
      	parent : ^node // pointer to parent node; nil if root

	shared nodes : array [0..P-1] of node
	    //each element of nodes allocated in a different memory module or cache line

	processor private sense : Boolean := true
	processor private mynode : ^node // my group's leaf in the combining tree 

	procedure combining_barrier
	    combining_barrier_aux (mynode) // join the barrier
	    sense := not sense             // for next barrier
	    
	procedure combining_barrier_aux (nodepointer : ^node)
	    with nodepointer^ do
	        if fetch_and_decrement (&count) = 1 // last one to reach this node
		    if parent != nil
		        combining_barrier_aux (parent)
		    count := k // prepare for next barrier
		    locksense := not locksense // release waiting processors
		repeat until locksense = sense
*/


typedef struct _node_t{
  int k;
  int count;
  int locksense;
  struct _node_t* parent;
} node_t;

static int num_leaves;

static node_t* nodes;

void gtmp_barrier_aux(node_t* node, int sense);

node_t* _gtmp_get_node(int i){
  return &nodes[i];
}

void gtmp_init(int num_threads){
  int i, v, num_nodes;
  node_t* curnode;
  
  /*Setting constants */
  v = 1;
  while( v < num_threads)
    v *= 2;
  
  num_nodes = v - 1;
  num_leaves = v/2;
  
  /* Using posix_memalign() instead of mallocallows us to have avoid having threads spinning 
  and updating  the same segmentation of the cache, causing issues where one thread will 
  invlidate another Thread's cache without actually updating memory that the second thread 
  cares about*/
  // nodes = (node_t*) malloc(num_nodes * sizeof(node_t));
  posix_memalign((void **) &nodes, LEVEL1_DCACHE_LINESIZE, num_nodes * sizeof(node_t));

  for(i = 0; i < num_nodes; i++){
    curnode = _gtmp_get_node(i);
    curnode->k = i < num_threads - 1 ? 2 : 1;
    curnode->count = curnode->k;
    curnode->locksense = 0;
    curnode->parent = _gtmp_get_node((i-1)/2);
  }
  
  curnode = _gtmp_get_node(0);
  curnode->parent = NULL;
}

void gtmp_barrier(){
  node_t* mynode;
  int sense;

  mynode = _gtmp_get_node(num_leaves - 1 + (omp_get_thread_num() % num_leaves));
  
  /* 
     Rather than correct the sense variable after the call to 
     the auxilliary method, we set it correctly before.
   */
  sense = !mynode->locksense;
  
  gtmp_barrier_aux(mynode, sense);
}

void gtmp_barrier_aux(node_t* node, int sense){

/*
This section does not allow this code to run in parrallel and is blocking, 
__sync_fetch_and_sub() uses atomic instructions and does not require this section
of code to be run serially.

#pragma omp critical
{
  test = node->count;
  node->count--;
}
*/

  if (__sync_fetch_and_sub(&node->count, 1) == 1){
    if(node->parent != NULL)
      gtmp_barrier_aux(node->parent, sense);
    node->count = node->k;
    node->locksense = !node->locksense;
  }

  // There is high CPU contention, threads that are waiting will dominate the CPU
  // never letting control go back to threads that need to work in order to allow 
  // other threads to complete. The taskyield pragma helps alleviate that contention
  # pragma omp task
    while (node->locksense != sense){
      usleep(10);
      # pragma omp taskyield
    };
}

void gtmp_finalize(){
  free(nodes);
}

