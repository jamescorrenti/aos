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
static const float SWITCH = 500.0;

struct CPUStats {
	double cpu_usage;
	int cpu;
};

void initialize_CPUstats(struct CPUStats *x,  int n)
{
	for(int i = 0; i < n; i++){
		x[i].cpu = 0;
		x[i].cpu_usage = 0.0;
	}
}

struct VCPUstats {
	virDomainPtr domain;
	struct CPUStats cpu_info;
};

void initialize_VCPUstats(struct VCPUstats *x,  int n, virDomainPtr *domain)
{
	for(int i = 0; i < n; i++){
		x[i].domain = domain[i];
		x[i].cpu_info.cpu_usage = 0.0;
		x[i].cpu_info.cpu = 0;
	}
}

void reset_VCPUstats(struct VCPUstats *x,  int n)
{
	for(int i = 0; i < n; i++){
		x[i].cpu_info.cpu_usage = 0.0;
		x[i].cpu_info.cpu = 0;
	}
}

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
	//printf("Time ran: %f\n", (float) (time_convert(current_time) - time_convert(start_time)));
	return (float) (time_convert(current_time) - time_convert(start_time)) / NANOSECOND;
}

struct CPUStats cpu_util(virDomainPtr dom, 
			int ncpus, int nparams, 
			virTypedParameterPtr first_params, 
			struct timespec time1, 
			virTypedParameterPtr sec_params, 
			struct timespec time2,
			struct CPUStats vcpu,
			struct CPUStats *cpus)
{
	//struct PUstats ret_vcpu = vcpu;
	virVcpuInfoPtr cpuinfo;
	unsigned char *cpumaps;
	size_t cpumaplen;

	printf("Domain: %s: ",virDomainGetName(dom));
	
	cpuinfo = calloc(4, sizeof(virVcpuInfo));
	cpumaplen = VIR_CPU_MAPLEN(ncpus);
	cpumaps = calloc(1, cpumaplen);
	virDomainGetVcpus(dom, cpuinfo, 1,cpumaps, cpumaplen);

	int position;
	unsigned long long time_diff = time_convert(time2) - time_convert(time1);

	for (int i = 0; i < ncpus; i++){
		
		for (int j =0; j < nparams; j++){
			position = i * nparams + j;
			if (strcmp(first_params[position].field, "cpu_time") == 0){	
				double calc =cpu_percent_calc(sec_params[position].value.ul, first_params[position].value.ul, time_diff);
				cpus[i].cpu_usage += calc;
				if (calc > vcpu.cpu_usage){
					vcpu.cpu_usage = calc;
					vcpu.cpu = i;
				}
			}
		}
				
	}
	for (int j = 0; j < 1; j++) {
		cpus[cpuinfo[j].cpu].cpu += 1;
		printf("CPU: %d ", cpuinfo[j].cpu);
		printf("vCPU %d affinity: ", vcpu.cpu);
		for (int m = 0; m < ncpus ; m++) {
			printf("%c",
			       VIR_CPU_USABLE(cpumaps, cpumaplen, j, m) ?
			       'y' : '-');
		}
		printf("\n");
	}
	free(cpuinfo);
	free(cpumaps);
	return vcpu;
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
	waittime.tv_nsec = 0.5 * NANOSECOND;

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		ERROR("Failed to open connection to qemu:///system\n");
		return 1;
	}
	int maxcpus = virNodeGetCPUMap(conn, NULL, NULL, 0);
	
	struct timespec first_time, second_time, process_start;
	virDomainPtr *doms; 

	while(not_balanced){
		numDomains = virConnectNumOfDomains(conn);
		numDomains = virConnectListAllDomains(conn, &doms, VIR_CONNECT_LIST_DOMAINS_ACTIVE);

		struct VCPUstats vcpus[numDomains];
		struct CPUStats cpus[maxcpus];
		initialize_CPUstats(cpus, maxcpus);

		initialize_VCPUstats(vcpus, numDomains, doms);
		printf("Active Domains: %d\n", numDomains);
		clock_gettime(CLOCK_REALTIME, &process_start);

		for (i = 0; i < numDomains; i++){

			virDomainPtr dom = vcpus[i].domain;
			
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
			
			vcpus[i].cpu_info = cpu_util(dom, ncpus, nparams, first_params, 
				first_time, sec_params, second_time, 
				vcpus[i].cpu_info, cpus);
			virTypedParamsClear(first_params, nparams * ncpus);
			virTypedParamsClear(sec_params, nparams * ncpus);
		}

		printf("\n********************************************\n");
		
		for (int i = 0; i < ncpus; i++) {
			printf("CPU %d, vCPUs assigned %d, usage %.2f%%\n", i,cpus[i].cpu, cpus[i].cpu_usage);
		}

		/*OK, so we have CPU utilization and data on which CPUS have more VCPUS, 
		what now? Determine the most congested CPUS and move to least congested CPUS */
		
		int busy_cpu = 0;
		int free_cpu = 0;
		
		for (int i =0; i < maxcpus; i++){
			if (cpus[i].cpu > cpus[busy_cpu].cpu)
				busy_cpu = i;

			if (cpus[i].cpu < cpus[free_cpu].cpu)
				free_cpu = i;
		}
		printf("Busiest CPU: %d Friest CPU: %d\n", busy_cpu, free_cpu);
		
		if (busy_cpu == free_cpu){
			not_balanced = 0;
			break;
		}
		else {
			int delta = (cpus[busy_cpu].cpu - cpus[free_cpu].cpu) / 2;
			printf("Lets balance %d and %d by %d\n", busy_cpu, free_cpu, delta);
			size_t cpumaplen = VIR_CPU_MAPLEN(ncpus);		
			unsigned char map = 0x1 << free_cpu;

			int filled = 0;
			
			while(delta - filled > 0){
				for(int j=0; j < numDomains; j++){
					if(vcpus[j].cpu_info.cpu == busy_cpu){
						printf("Pin to CPU %d, from %d\n", free_cpu, vcpus[j].cpu_info.cpu);
						if (virDomainPinVcpu(vcpus[j].domain, 0, &map,cpumaplen) > 0){
							ERROR("Failed to Pin");
							goto clean;
						}
						filled += 1;
						if (filled == delta) break;
					}
				}
			}
			reset_VCPUstats(vcpus, numDomains);

		}

		float wait = current_run_time(process_start);

		while( (float)run_secs > wait){
			printf("Sleeping for %f\n", (run_secs - wait));
			usleep((run_secs - wait) * NANOSECOND / 1000);
			wait = current_run_time(process_start);
		}
	}
	printf("Balanced and Exiting\n");
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
