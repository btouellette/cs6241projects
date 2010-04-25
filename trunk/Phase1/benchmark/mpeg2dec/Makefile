OBJS = display.bc getbits.bc getblk.bc gethdr.bc getpic.bc getvlc.bc idct.bc idctref.bc motion.bc mpeg2dec.bc recon.bc spatscal.bc store.bc subspic.bc systems.bc verify.bc

CPPFLAGS = -DHAVEMPGLIB -DLAMEPARSE -DNDEBUG -D__NO_MATH_INLINES -DLAMESNDFILE
LDFLAGS = -lm

all : mpeg2dec-checked mpeg2dec-unchecked 

% : %.bc
	llc -f -o tmp.s $<
	g++ -lm -o $@ tmp.s
	rm tmp.s

mpeg2dec-checked.bc : $(OBJS) ../../libCheck.bc ../../phase1.so
	./add_checks_to_objs.sh $(OBJS)
	llvm-ld -lm -o tmp *.checked ../../libCheck-debug.bc
	opt -f -o mpeg2dec-checked.bc -std-compile-opts -std-link-opts tmp.bc
	rm tmp tmp.bc

mpeg2dec-unchecked.bc : $(OBJS)
	llvm-ld -lm -o tmp $(OBJS)
	opt -f -o mpeg2dec-unchecked.bc -std-compile-opts -std-link-opts tmp.bc
	rm tmp tmp.bc

%.bc : %.c
	llvm-gcc -g -O0 --emit-llvm $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) *.s *.bc *.checked mpeg2dec-unchecked mpeg2dec-checked
