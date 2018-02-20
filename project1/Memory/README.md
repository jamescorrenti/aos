# memory_coordinator README

## Compile and Run:
From the Memory directory:
```sh
$ make
$ ./memory_coordinator <number of seconds>
```

Note:
`./memory_coordinator` will error out if you do not pass it a valid number.

## Code Description

### Main 

Main starts off by initializing important variables for the program, including making a connection with QEMU, getting all of the active domains. Main collects information on the Host machines memory usage, checks to see if the HyperVisor (HV) needs to reclaim memory from the Virtual Machines (VM) and sets a variable if the Host Machine's Free Memory is less than 256 MB.

It then loops through the execution loop, which gets all the domain information, calls the `memory_coodination` function and when `memory_coordination` is complete does some garbage collection and handles sleeping until the timer the memory coordinator needs to run again.

### memory_coordination

The `memory_coordination` function looks at every VMs memory statistics and determines if that particular VM has either the most or least unused memory. After determining the VMs which has the most/least unused memory the `memory_coordination` function will determine how much memory to free up from the most unused memory VM. 

If the HV needs memory, then the most unused memory VM will free up the amount needed by the HV. Else, the amount of memory to free is calculated by taking the difference between the most unused and least unused VMs unused Memory and divides it in half. This is checked to be less than a preset minimum value to ensure the freeing VM does not free too much memory and become unresponsive. Additionally, it is checked to be less than 256 KB, if so the function assumes that the Guest VMs memory is balanced. Following all these calculations, the memory is ballooned out of the most unused memory and the memory is ballooned into of the least unused memory.