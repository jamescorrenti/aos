#include <stdlib.h>
#include <stdio.h>
#include <libvirt/libvirt.h>

int main(int argc, char *argv[])
{
	// Found this on https://libvirt.org/guide/html/Application_Development_Guide-Connections.html 
	// Feb 11
	virConnectPtr conn;
	int i;
	int numDomains;
	int *activeDomains; 

	conn = virConnectOpen("qemu:///system");
	if (conn == NULL) {
		fprintf(stderr, "Failed to open connection to qemu:///system\n");
		return 1;
	}

	numDomains = virConnectNumOfDomains(conn);
	activeDomains = malloc(sizeof(int) * numDomains);
	numDomains = virConnectListDomains(conn, activeDomains, numDomains);

	printf("Active Domains IDs:\n");

	for (i = 0; i < numDomains; i++){
		printf("%d\n", activeDomains[i]);
	}

	free(activeDomains);

	virConnectClose(conn);

	return 0;
}
