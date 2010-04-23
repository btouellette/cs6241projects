KERNELS = loop1
PASSES = -BoundChecks -OptimizeChecks -InsertChecks

all: $(KERNELS)

%: %.c ../libCheck-debug.bc ../phase1.so
	llvm-gcc -o tmp0.bc --emit-llvm -c $<
	opt -f -load ../phase1.so -stats $(PASSES) -o tmp1.bc tmp0.bc
	llvm-ld -o $@ tmp1.bc ../libCheck-debug.bc
	rm tmp0.bc tmp1.bc

clean:
	rm -f *.bc $(KERNELS) *~ \#*
