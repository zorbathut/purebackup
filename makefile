
SOURCES = main parse debug tree item
CPPFLAGS = -DVECTOR_PARANOIA -Wall -Wno-sign-compare -Wno-uninitialized -O2 #-g -pg
LINKFLAGS = -lcrypto -O2 #-g -pg

CPP = g++

all: purebackup.exe

include $(SOURCES:=.d)

purebackup.exe: $(SOURCES:=.o)
	$(CPP) -o $@ $(SOURCES:=.o) $(LINKFLAGS) 

asm: $(SOURCES:=.S)

clean:
	rm -rf *.o *.exe *.d *.S

%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

%.S: %.cpp
	$(CPP) $(CPPFLAGS) -c -g -Wa,-a,-ad $< > $@

%.d: %.cpp
	bash -ec '$(CPP) $(CPPFLAGS) -MM $< | sed "s/$*.o/$*.o $@/g" > $@'
