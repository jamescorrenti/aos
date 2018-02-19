CC=gcc
CFLAGS=-Wall -I. -lvirt

make: vcpu_scheduler.c
	$(CC) vcpu_scheduler.c -o vcpu_scheduler $(CFLAGS)
clean:
	rm -f vcpu_scheduler
