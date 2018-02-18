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

unsigned long time_convert(struct timespec time)
{
	return time.tv_sec * NANOSECOND + time.tv_nsec;	
}

double cpu_percent_calc(unsigned long t2, unsigned long t1, unsigned long long t_diff){
	double value = t2 - t1;
	return value / t_diff * 100;
}

float current_run_time(struct timespec start_time)
{
	struct timespec current_time;
	clock_gettime(CLOCK_REALTIME, &current_time);
	printf("Time ran: %f\n", (float) (time_convert(current_time) - time_convert(start_time)));
	return (float) (time_convert(current_time) - time_convert(start_time)) / NANOSECOND;
}


int lowest_utilized_cpu(virDomainPtr dom, 
						int ncpus, int nparams, 
						virTypedParameterPtr first_params, 
						struct timespec time1, 
						virTypedParameterPtr sec_params, 
						struct timespec time2)
{
	int position;
	unsigned long long time_diff = time_convert(time2) - time_convert(time1);
	double cpus[ncpus], vcpus[ncpus];
	int lowest_cpu = 0;

	for (int i = 0; i < ncpus; i++){
		
		for (int j =0; j < nparams; j++){
			position = i * nparams + j;
			if (strcmp(first_params[position].field, "cpu_time") == 0){	
				cpus[i] = cpu_percent_calc(sec_params[position].value.ul, first_params[position].value.ul, time_diff);
			}
			else{
				vcpus[i] = cpu_percent_calc(sec_params[position].value.ul, first_params[position].value.ul, time_diff);
			}
		}
	
		printf("CPU %d: %f, VCPU %f\n", i, cpus[i], vcpus[i]);
		
		if (cpus[i] == vcpus[i] && cpus[i] > 0) {
			printf("We are leaving early on %d\n", i);
			return i;
		}
		
		if (cpus[lowest_cpu] > cpus[i])
			lowest_cpu = i;		
	}

	return lowest_cpu;
}

int main(int argc, char *argv[])
{
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
	int i;
	int numDomains;
	
	virTypedParameterPtr first_params, sec_params;
	unsigned int nparams, ncpus;
	int return_value = 1;
	int not_balanced = 1;
	struct timespec waittime;
	waittime.tv_sec = 0;
	waittime.tv_nsec = 0.05 * NANOSECOND;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		ERROR("Failed to open connection to qemu:///system\n");
		goto clean;
	}

	struct timespec first_time, second_time, process_start;
	virDomainPtr *doms = NULL; 

	while(not_balanced){
		numDomains = virConnectNumOfDomains(conn);
		numDomains = virConnectListAllDomains(conn, &doms, VIR_CONNECT_LIST_DOMAINS_ACTIVE);

		printf("Active Domains: %d\n", numDomains);
		clock_gettime(CLOCK_REALTIME, &process_start);

		for (i = 0; i < numDomains; i++){

			virDomainPtr dom = doms[i];
			
			if ( (ncpus = virDomainGetCPUStats(dom, NULL, 0, 0, 0, 0)) < 0){
				ERROR("Failed to get Number of CPUS");
				goto clean;
			}

			if ((nparams = virDomainGetCPUStats(dom, NULL, 0, 0, 1, 0)) < 0) {
				ERROR("Failed to get Number of Params");
				goto clean;
			}
			
			if  (!(first_params = calloc(ncpus * nparams, sizeof(virTypedParameter))) || 
				!(sec_params = calloc(ncpus * nparams, sizeof(virTypedParameter)))) {
				ERROR("Allocating memory for params failed");
				goto clean;
			}

			clock_gettime(CLOCK_REALTIME, &first_time);

			if (( nparams = virDomainGetCPUStats(dom, first_params, nparams, 0, ncpus, 0)) < 0 ) {
				ERROR("Failed to get Params for the CPU stats");
				goto clean;
			}

			nanosleep(&waittime, NULL);
			
			clock_gettime(CLOCK_REALTIME, &second_time);

			if (( nparams = virDomainGetCPUStats(dom, sec_params, nparams, 0, ncpus, 0)) < 0 ) {
				ERROR("Failed to get Params for the CPU stats");
				goto clean;
			}
			size_t cpumaplen = VIR_CPU_MAPLEN(ncpus);
			int cpu = lowest_utilized_cpu(dom, ncpus, nparams, first_params, first_time, sec_params, second_time);
			printf("Pin to CPU %d, maplen %lu, cpus %d\n", cpu, cpumaplen, ncpus);

			unsigned char map = 0x1 << cpu;
			if (virDomainPinVcpu(dom, 0, &map,cpumaplen) >0){
				ERROR("Failed to Pin");
				goto clean;
			}

			free(dom);
			virTypedParamsClear(first_params, nparams * ncpus);
			virTypedParamsClear(sec_params, nparams * ncpus);
		}

		
		float wait = current_run_time(process_start);

		while( (float)run_secs > wait){
			printf("Sleeping for %f\n", (run_secs - wait));
			usleep((run_secs - wait) * NANOSECOND / 1000);
			wait = current_run_time(process_start);
		}
	}

	return_value = 0;

	clean:
		
		if (doms) {
			free(doms);
		}
		if (first_params){
			virTypedParamsFree(first_params, nparams * ncpus);
		}
		if (sec_params){
			virTypedParamsFree(sec_params, nparams * ncpus);
		}
		if (conn) {
			virConnectClose(conn);
		}
		return return_value;
}
