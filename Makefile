CXXFLAGS=-g -O -W -Wall -MMD -I. -I/usr/include/libdwarf

EXES=dump_debug_info

TARGETS=$(EXES) macros.html sizeof.html

all: $(TARGETS)

check: all
	./runtests.sh

dump_debug_info: binary.o scanner.o dump_debug_info.o
	$(CXX) $(CXXFLAGS) -o $@ $^

macros.html: macros.tsv
	./tsv2html.rb $< > $@

macros.tsv: libc-2.17-i686.txt ./gen_macros.rb
	./gen_macros.rb > macros.tsv

libc-2.17-i686.txt:
	./gen_macros.sh || rm $@

sizeof.html: sizeof.tsv
	./tsv2html.rb $< > $@

sizeof.tsv: libc-2.17-i686.json ./gen_sizeof.rb
	./gen_sizeof.rb > sizeof.tsv

libc-2.17-i686.json: dump_debug_info
	./gen_sizeof.sh || rm $@

clean:
	rm -f *.o $(TARGETS)

-include *.d
