.PHONY: all clean run
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
