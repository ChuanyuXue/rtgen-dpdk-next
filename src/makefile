# Define the compiler
CC = gcc
DPDK_LIBS = $(shell pkg-config --libs libdpdk)
DPDK_CFLAGS = $(shell pkg-config --cflags libdpdk)
DEBUG = -g
BUILDDIR = build

# Create the build directory if it doesn't exist
$(shell mkdir -p $(BUILDDIR))

# Define the executables
talker: $(BUILDDIR)/talker.o $(BUILDDIR)/flow.o  $(BUILDDIR)/engine.o $(BUILDDIR)/sche.o $(BUILDDIR)/utility.o $(BUILDDIR)/header.o $(BUILDDIR)/sync.o
	$(CC) -o $(BUILDDIR)/$@ $^ $(DPDK_LIBS) $(DEBUG)

listener: $(BUILDDIR)/listener.o $(BUILDDIR)/flow.o  $(BUILDDIR)/engine.o $(BUILDDIR)/utility.o $(BUILDDIR)/header.o $(BUILDDIR)/sync.o
	$(CC)  -o $(BUILDDIR)/$@ $^ $(DPDK_LIBS) $(DEBUG)

# Define the object files
$(BUILDDIR)/sche.o: sche.c sche.h flow.h
	$(CC) -c -o $@ $<

$(BUILDDIR)/flow.o: flow.c flow.h 
	$(CC) -c -o $@ $<

$(BUILDDIR)/talker.o: talker.c talker.h  flow.h
	$(CC) -c -o $@ $<

$(BUILDDIR)/engine.o: engine_dpdk.c engine_dpdk.h  flow.h
	$(CC) -c -o $@ $< $(DPDK_CFLAGS)

$(BUILDDIR)/header.o: header_dpdk.c header_dpdk.h
	$(CC) -c -o $@ $< $(DPDK_CFLAGS) 

$(BUILDDIR)/sync.o: ptpclient_dpdk.c ptpclient_dpdk.h
	$(CC) -c -o $@ $< $(DPDK_CFLAGS)

$(BUILDDIR)/listener.o: listener.c  flow.h
	$(CC) -c -o $@ $<

$(BUILDDIR)/utility.o: utility.c utility.h
	$(CC) -c -o $@ $<

# Define the clean target
clean:
	rm -rf $(BUILDDIR)/*.o $(BUILDDIR)/talker $(BUILDDIR)/listener
