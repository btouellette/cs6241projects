OBJS = brhist.bc common.bc dct64_i386.bc decode_i386.bc fft.bc formatBitstream.bc get_audio.bc gpkplotting.bc gtkanal.bc id3tag.bc ieeefloat.bc interface.bc l3bitstream.bc lame.bc layer3.bc main.bc mpglib_main.bc newmdct.bc parse.bc portableio.bc psymodel.bc quantize.bc quantize-pvt.bc reservoir.bc rtp.bc tabinit.bc tables.bc takehiro.bc timestatus.bc util.bc vbrquantize.bc VbrTag.bc version.bc

CPPFLAGS = -DHAVEMPGLIB -DLAMEPARSE -DNDEBUG -D__NO_MATH_INLINES -DLAMESNDFILE
LDFLAGS = -lm

all : lame-checked lame-unchecked 

% : %.bc
	llc -f -o tmp.s $<
	gcc -lm -o $@ tmp.s
	rm tmp.s

lame-checked.bc : $(OBJS) ../../libCheck.bc ../../phase1.so
	./add_checks_to_objs.sh $(OBJS)
	llvm-ld -lm -o tmp *.checked ../../libCheck.bc
	opt -f -o lame-checked.bc -std-compile-opts tmp.bc
	rm tmp tmp.bc

lame-unchecked.bc : $(OBJS)
	llvm-ld -lm -o tmp $(OBJS)
	opt -f -o lame-unchecked.bc -std-compile-opts tmp.bc
	rm tmp tmp.bc

%.bc : %.c
	llvm-gcc -O0 --emit-llvm $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) *.s *.bc *.checked lame-unchecked lame-checked
