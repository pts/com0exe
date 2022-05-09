#
# This Makefile is for Unix systems with GCC as C compiler. Tested on Linux.
#

.PHONY: all clean run test
.SUFFIXES:
MAKEFLAGS += -r

ALL = com0exe

CFLAGS = -ansi -pedantic -s -O2 -W -Wall -Wextra -Werror=implicit-function-declaration -Werror=int-conversion $(XCFLAGS)
XCFLAGS =  # To be overridden from the command-line.
TCC = pts-tcc

SRCDEPS = com0exe.c

all: $(ALL)

clean:
	rm -f $(ALL) com0exe32 com0exe64 com0exe.static

com0exe: $(SRCDEPS)
	gcc $(CFLAGS) -o $@ $<

com0exe32: $(SRCDEPS)
	gcc -m32 -fno-pic -march=i686 -mtune=generic $(CFLAGS) -o $@ $<

com0exe64: $(SRCDEPS)
	gcc -m64 -march=k8 -mtune=generic $(CFLAGS) -o $@ $<

com0exe.static: $(SRCDEPS)
	xstatic gcc -D__XSTATIC__ -m32 -fno-pic -march=i686 -mtune=generic $(CFLAGS) -o $@ $<

com0exe.tcc: $(SRCDEPS)
	$(TCC) -m32 -s -O2 -W -Wall -Wextra -o $@ $<

com0exe.win32.exe: $(SRCDEPS)
	i686-w64-mingw32-gcc -mconsole -fno-pic -march=i686 -mtune=generic $(CFLAGS) -o $@ $<

com0exe.com: $(SRCDEPS)  # For DOS.
	owcc -bcom -s -Os -W -Wall -fno-stack-check -march=i86 -o $@ $<

com0exe.dos.exe: $(SRCDEPS)  # For DOS. Just for testing. Use com0exe.com in production.
	owcc -bdos -s -Os -W -Wall -fno-stack-check -march=i86 -o $@ $<

test: com0exe
	./com0exe         test/dumpre24.com test/tmp24.exe  && cmp test/dumpre24.exe test/tmp24.exe
	./com0exe --      test/dumpre27.com test/tmp27.exe  && cmp test/dumpre27.exe test/tmp27.exe
	./com0exe --noret test/dumprr.com   test/tmp32.exe  && cmp test/dumpre32.exe test/tmp32.exe
	./com0exe         test/dumpregs.com test/tmp43.exe  && cmp test/dumpre43.exe test/tmp43.exe
	./com0exe         test/long.com     test/tmpl43.exe && cmp test/long.exe     test/tmpl43.exe
