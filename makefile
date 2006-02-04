
SOURCES = main parse debug tree item state util minizip/zip minizip/unzip minizip/ioapi
CPPFLAGS = -DVECTOR_PARANOIA -Wall -Wno-sign-compare -Wno-uninitialized -O2 -DWIN32API #-g -pg
CFLAGS = -O2 #-g -pg
LINKFLAGS = -lcrypto -lz -O2 #-g -pg

C = gcc
CPP = g++

all: purebackup.exe

include $(SOURCES:=.d)

purebackup.exe: $(SOURCES:=.o) makefile
	$(CPP) -o $@ $(SOURCES:=.o) $(LINKFLAGS) 

run: purebackup.exe makefile
	purebackup.exe backup

asm: $(SOURCES:=.S) makefile

clean:
	rm -rf *.o *.exe *.d *.S minizip/*.o minizip/*.d minizip/*.S

%.o: %.cpp makefile
	$(CPP) $(CPPFLAGS) -c -o $@ $<

%.o: %.c makefile
	$(C) $(CFLAGS) -c -o $@ $<

%.S: %.cpp makefile
	$(CPP) $(CPPFLAGS) -c -g -Wa,-a,-ad $< > $@

%.d: %.cpp makefile
	bash -ec '$(CPP) $(CPPFLAGS) -MM $< | sed "s*$*.o*$*.o $@*g" > $@'

%.d: %.c makefile
	bash -ec '$(C) $(CFLAGS) -MM $< | sed "s*$*.o*$*.o $@*g" > $@'
