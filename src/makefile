# Define the compiler 
CC = gcc

# Define the executables
talker: talker.o flow.o system.o udp.o sche.o
	$(CC) -o $@ $^

listener: listener.o flow.o system.o udp.o
	$(CC) -o $@ $^

# Define the object files
sche.o: sche.c sche.h system.h flow.h
	$(CC) -c -o $@ $<

flow.o: flow.c flow.h system.h
	$(CC) -c -o $@ $<

system.o: system.c system.h
	$(CC) -c -o $@ $<

talker.o: talker.c talker.h system.h flow.h
	$(CC) -c -o $@ $<

udp.o: udp.c udp.h system.h flow.h
	$(CC) -c -o $@ $<

# Define the clean target
clean:
	rm -f talker listener *.o