CC = gcc


all: vcpu_scheduler

vcpu_scheduler: vcpu_scheduler.c
	$(CC) -o $@ $< -lvirt

clean:
	rm -f *.o vcpu_scheduler
