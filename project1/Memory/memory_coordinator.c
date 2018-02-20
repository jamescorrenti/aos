#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libvirt/libvirt.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#define ERROR(...) do { \
	fprintf(stderr, "error %s:%d : ", __FUNCTION__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	fprintf(stderr, "\n"); \
} while(0)

static const long NANOSECOND = 1000000000.0;
static const long MB = 1024;
static const long FREE_MINIMUM = 262144; //256 * 1024;

struct MemStats{
	virDomainPtr domain;
	long actual;
	long swap_in;
	long swap_out;
	long major_fault;
	long minor_fault;
	long unused;
	long used;
	long aval;
	long rss;

};

void printMemStats(struct MemStats s){
	printf("Domain %s mem stats: \n", virDomainGetName(s.domain));
	printf("Actual: %lu MB \n", s.actual / MB);
	printf("Swap in: %lu MB \n", s.swap_in / MB);
	printf("Swap out: %lu MB \n", s.swap_out / MB);
	printf("Major fault: %lu MB \n", s.major_fault / MB);
	printf("minor fault: %lu MB \n", s.minor_fault /MB);
	printf("unused: %lu MB \n", s.unused / MB);
	printf("used: %lu MB \n", s.used / MB);
	printf("Available: %lu MB \n", s.aval / MB);
	printf("RSS: %lu MB \n\n", s.rss / MB);
}

void setMemStats(struct MemStats *s, virDomainMemoryStatStruct mem[8], virDomainPtr dom){
	s->domain = dom;
	s->actual = mem[0].val;
	s->swap_in = mem[1].val;
	s->swap_out = mem[2].val;
	s->major_fault = mem[3].val;
	s->minor_fault = mem[4].val;
	s->unused = mem[5].val;
	s->aval = mem[6].val;
	s->rss = mem[7].val;	
	s->used = s->aval - s->unused;
}


unsigned long time_convert(struct timespec time)
{
	/* Takes a timespec time and converts it into nanoseconds*/ 
	return time.tv_sec * NANOSECOND + time.tv_nsec;	
}

float current_run_time(struct timespec start_time)
{
	/* Takes a timespec time and returns how long ago that was in nanoseconds */ 
	struct timespec current_time;
	clock_gettime(CLOCK_REALTIME, &current_time);
	return (float) (time_convert(current_time) - time_convert(start_time)) / NANOSECOND;
}


void memory_coordination(virDomainPtr *doms, int nDoms, long free_mem){
	
	/* It seems that the documentation for virDomainMemoryStatTags does not 
	 * match up with reality....this is what I found corresponds to values from
	 * pythons memoryinfo dictionary
	 * 0: actual
	 * 1: swap out/swap in
	 * 2: swap out/swap in
	 * 3: major fault
	 * 4: minor fault
	 * 5: unused
	 * 6: aval
	 * 7: rss
	 */

	struct MemStats most_unused;
	struct MemStats least_unused;
	most_unused.unused = 0;
	least_unused.unused = 0;

	for (int i =0; i < nDoms; i++){
		unsigned int nr_stat = 8;
		unsigned int unused = 5;
		unsigned int actual = 0;
		unsigned int aval = 6;
		virDomainMemoryStatStruct memory[nr_stat];
		int stats;
		virDomainPtr dom = doms[i];
		
		printf("Domain: %s ", virDomainGetName(dom));

		if ((stats = virDomainMemoryStats(dom, memory, nr_stat, 0)) < 0){
			ERROR("Failed to get Memory Stats");
			break;
		}

		if ( memory[unused].val > most_unused.unused) {
			setMemStats(&most_unused, memory, dom);
		} 

		if ( memory[unused].val < least_unused.unused || least_unused.unused == 0) {
			setMemStats(&least_unused, memory, dom);
		}

		printf("Actual: %llu MB, Aval: %llu MB, unused: %llu MB \n", 
			memory[actual].val / MB,
			memory[aval].val / MB, 
			memory[unused].val / MB);	
	}

	printf("***********************************************\n");

	const char * least_unused_domain = virDomainGetName(least_unused.domain);
	const char * most_unused_domain = virDomainGetName(most_unused.domain);

	printf("\nLeast unused: %s, %lu MB \n", least_unused_domain, least_unused.actual /MB);
	printf("Most Unused: %s, %lu MB \n", most_unused_domain, most_unused.actual /MB);
	unsigned long mem_swap = (most_unused.unused - least_unused.unused) / 2;
	
	if (free_mem == 0){
		if (mem_swap < 256) 
			mem_swap = 0;

		printf("Increasing %s by %lu KB\n", least_unused_domain, mem_swap);

		if (virDomainSetMemory(least_unused.domain, (least_unused.actual + mem_swap))  < 0){
			ERROR("Did not add data to least unused domain");	
		}
		
		if((most_unused.actual - mem_swap) < (256 * MB) )
			mem_swap = 256 * MB;
		else 
			mem_swap = 	most_unused.actual - mem_swap;	
	}

	else {
		printf("%s giving up %lu to HV\n", most_unused_domain, free_mem);
		mem_swap = most_unused.actual - free_mem;

	}

	printf("Decreasing %s to %lu MB\n\n", most_unused_domain, mem_swap / MB);
	if (virDomainSetMemory(most_unused.domain, mem_swap) < 0){
		ERROR("Did not remove data from wasteful domain");
	} 
	
}

int main(int argc, char * argv[]){

	if (argc != 2) {
		printf("Not the right number of args, exiting\n");
		return -1;
	}

	int run_secs = atoi(argv[1]);

	if (run_secs == 0){
		ERROR("Not a number");
		return 1;
	}

	printf("Run time: %d\n", run_secs);

	virConnectPtr conn;
	int return_val = 1;
	int running =5;
	int numDomains=0;
	conn = virConnectOpen("qemu:///system");
	struct timespec process_start;

	if (conn == NULL) {
		ERROR("Failed to open connection to qemu:///system\n");
		goto exit;
	}

	int cellNum = VIR_NODE_MEMORY_STATS_ALL_CELLS;
	int nparams;
	virNodeMemoryStats *params;


	if (virNodeGetMemoryStats(conn, cellNum, NULL, &nparams, 0) == 0 && nparams != 0) {
	    if ((params = malloc(sizeof(virNodeMemoryStats) * nparams)) == NULL)
		    memset(params, 0, sizeof(virNodeMemoryStats) * nparams);
	}

	while(running){
		long free_mem = 0;		
	    if (virNodeGetMemoryStats(conn, cellNum, params, &nparams, 0))
		        ERROR("Failed to get Node memory stats");

		for (int i = 0; i < nparams; i++) {
				printf("%s : %lld MB\n", params[i].field, params[i].value / MB);
				if (strcmp(params[i].field, "free") == 0 && params[i].value < FREE_MINIMUM )
					free_mem = FREE_MINIMUM - params[i].value; 		
		}

		virDomainPtr *doms;
		numDomains = virConnectNumOfDomains(conn);
		numDomains = virConnectListAllDomains(conn, &doms, VIR_CONNECT_LIST_DOMAINS_ACTIVE);

		for (int i =0; i < numDomains; i++){
			virDomainPtr dom = doms[i];
			if ((virDomainSetMemoryStatsPeriod(dom, 1, VIR_DOMAIN_AFFECT_CURRENT)) < 0){
				ERROR("FAIL TO SET PERIOD");
			}
		}

		printf("Active Domains: %d\n", numDomains);
		clock_gettime(CLOCK_REALTIME, &process_start);
		
		memory_coordination(doms, numDomains, free_mem);

		float wait = current_run_time(process_start);
		
		for (int i =0; i < numDomains; i++){
			virDomainFree(doms[i]);	
		}
		free(doms);

		while( (float)run_secs > wait){
			usleep((run_secs - wait) * NANOSECOND / 1000);
			wait = current_run_time(process_start);
		}
	}
	printf("Memory is good and we are exiting\n");
	return_val = 0;
	goto exit;

exit:
	if (conn)
		virConnectClose(conn);
	if (params)
		free(params);
	return return_val;
}