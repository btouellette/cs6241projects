# CS6241 Project Phase 1 - Alternate Makefile                   Chad D. Kersey
# With this makefile, there's no need to place the code inside of the LLVM
# directory structure.

passeslib = phase1.so
sources = InsertChecks.cpp OptimizeChecks.cpp

LD = llvm-ld
CXX = llvm-g++
LDFLAGS = -shared
CXXFLAGS = -fPIC -g

all: $(passeslib) libCheck.bc testcases.bc

$(passeslib): $(sources)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(sources)

testcases.bc : libCheck-debug.bc testcases.c
	llvm-gcc -o tmp0.bc --emit-llvm -c testcases.c
	opt -f -load ./phase1.so -stats -OptimizeChecks -InsertChecks -o tmp1.bc \
		tmp0.bc
	llvm-ld -o testcases tmp1.bc libCheck-debug.bc
	rm tmp1.bc

libCheck.bc : libCheck.c
	llvm-gcc --emit-llvm -c libCheck.c -o libCheck.bc

libCheck-debug.bc : libCheck.c
	llvm-gcc --emit-llvm -DDEBUG -c libCheck.c -o libCheck-debug.bc

clean:
	rm -f $(passeslib) testcases *.bc *.s *~ \#*
