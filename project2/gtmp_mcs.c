#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include "gtmp.h"
#include <stdbool.h>
#include <math.h>
#include <unistd.h>

/*
    From the MCS Paper: A scalable, distributed tree-based barrier with only local spinning.

    type treenode = record
    parentsense : Boolean
	parentpointer : ^Boolean
	childpointers : array [0..1] of ^Boolean
	havechild : array [0..3] of Boolean
	childnotready : array [0..3] of Boolean
	dummy : Boolean //pseudo-data

    shared nodes : array [0..P-1] of treenode
        // nodes[vpid] is allocated in shared memory
        // locally accessible to processor vpid
    processor private vpid : integer // a unique virtual processor index
    processor private sense : Boolean

    // on processor i, sense is initially true
    // in nodes[i]:
    //    havechild[j] = true if 4 * i + j + 1 < P; otherwise false
    //    parentpointer = &nodes[floor((i-1)/4].childnotready[(i-1) mod 4],
    //        or dummy if i = 0
    //    childpointers[0] = &nodes[2*i+1].parentsense, or &dummy if 2*i+1 >= P
    //    childpointers[1] = &nodes[2*i+2].parentsense, or &dummy if 2*i+2 >= P
    //    initially childnotready = havechild and parentsense = false
	
    procedure tree_barrier
        with nodes[vpid] do
	    repeat until childnotready = {false, false, false, false}
	    childnotready := havechild //prepare for next barrier
	    parentpointer^ := false //let parent know I'm ready
	    // if not root, wait until my parent signals wakeup
	    if vpid != 0
	        repeat until parentsense = sense
	    // signal children in wakeup tree
	    childpointers[0]^ := sense
	    childpointers[1]^ := sense
	    sense := not sense
*/

typedef struct _mcs_node
{
    bool parentsense;
    bool* parent;
    bool* child[2];
    bool havechild[4];
    bool child_not_ready[4];
    bool dummy;

} mcs_node;

static mcs_node* nodes;

static int P;

void gtmp_init(int num_threads){
    int i, j;
    P = num_threads;
    //nodes = (mcs_node*) malloc(sizeof(mcs_node) * P);

    posix_memalign((void **) &nodes, LEVEL1_DCACHE_LINESIZE, P * sizeof(mcs_node));
    mcs_node* cur_node;
    for(i = 0; i < P; i++){
        cur_node = &nodes[i];
        cur_node->parentsense = false;
        for(j = 0; j < 4; j++){
            cur_node->havechild[j] = 4 * i + j + 1< P ? true : false;
            cur_node->child_not_ready[j] = cur_node->havechild[j];
        }
        if (i == 0){
            cur_node->parent = &cur_node->dummy;
        }else
            cur_node->parent = &nodes[(int) floor((i - 1)/4)].child_not_ready[(i-1) % 4];
        cur_node->child[0] = 2*i+1 < P ? &nodes[2*i+1].parentsense: &cur_node->dummy;
        cur_node->child[1] = 2*i+2 < P ? &nodes[2*i+2].parentsense: &cur_node->dummy;
    }
}

bool get_child_not_ready(mcs_node *node){
    for(int j = 0; j < 4; j++){

        if (node->child_not_ready[j] == true){
            return true;
        }
    }
    return false;
}

void gtmp_barrier(){
    int j;
    int vpid = omp_get_thread_num();
    mcs_node *node = &nodes[vpid];
    bool sense = true;


    while(get_child_not_ready(node)){usleep(10000);};

    

    for(j = 0; j < 4; j++)
        node->child_not_ready[j] = node->havechild[j];
    
    *node->parent = false;    

    if (vpid != 0)
        # pragma omp task
            while(node->parentsense != sense){
              usleep(1);
              # pragma omp taskyield
            };        
    for(j = 0; j < 2; j++){
        *node->child[j] = sense;
    }
    sense = !sense;
}

void gtmp_finalize(){
    free(nodes);
}
