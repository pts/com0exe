com0exe: DOS .com program to .exe converter
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
com0exe is a command-line tool to convert DOS .com executable programs to
DOS .exe. It is compatible with any .com program (unlike other similar
tools), and when asked to do so, it can add a very small .exe header
(24 bytes).

Usage:

  com0exe [--noret] <input.com> [<output.exe>]

com0exe is written in architecture-independent ANSI C, it depends on a very
small part of the standard C library, and it works on many different host
systems (tested on Linux, macOS, Windows and DOS).

By default, com0exe adds 43 bytes of overhead (24 bytes of .exe header, 8
bytes of padding, and 11 bytes of stack and register setup code), and the
resulting .exe works indistinguishably from its .com counterpart: registers
and stack are set up exactly the same way.

com0exe can create 4 .exe variants of various compatibility features and
overhead. The variants are identified by the number of bytes they add; they
are: 24, 27, 32 and 43. A comparison table:

  variant                      24      27      32      43
  ----------------------------------------------------------
  overhead size (bytes)        24      27      32      43
  CS == DS == ES == SS == PSP  YES     YES     YES     YES
  `org 0x100'                  YES     YES     YES     YES
  `int 0x20' exits             YES     YES     YES     YES
  `ret' exits                  no      YES     no      YES
  DI == SP == 0xfffe           no      no      YES     YES
  DI == SP == 0                YES     YES     no      no
  SI                           0x108   0x108   0x100   0x100
  user code starts at CS:...   0x108   0x10b   0x100   0x100
  ZF (flag)                    old     0       old     old

By default, com0exe creates variant 43, which is compatible with any .com
program. To get variant 32, pass the --noret command-line flag to com0exe.
Please note that variant 32 doesn't support `ret' for exiting to DOS,
because it doesn't push 0 required for that to the stack. So with variant
32, .com programs must exit with `int 0x20', or `int 0x4c' with AH == 0x4c.

To get the even smaller variants 24 and 27, prepare your .com file by
starting it in a special way in your assembly source:

  ; Start your .com file like this for variant 27.
  db 0beh, 8, 1        ; mov si, 0x108;
  db 031h, 0ffh        ; xor di, di;
  db 0ebh, 4           ; jmp strict short 0x10b;
  db 0fah, 0fah, 0fah  ; cli; cli; cli;  Will be skipped over.
  db 0f4h              ; hlt;  Will be skipped over.

  ; Start your .com file like this for variant 24.
  db 05fh,             ; pop di;  Zero, pushed by DOS at .com load time.
  db 0beh, 8, 1        ; mov si, 0x108;
  db 0ebh, 2           ; jmp strict short 0x108;
  db 0fah              ; cli;  Will be skipped over.
  db 0f4h              ; hlt;  Will be skipped over.

com0exe recognizes the .com prefixes above, and it creates the corresponding
short variant automatically. Please note that your .com programs will still
work with these prefixes added, and they will work equivalently to the .exe
output of com0exe. The reason why prefixes are needed is alignment: the
small .exe header is only possible if the beginning of the code is aligned
to the right modulus of the 16-byte paragraph size.

Please note that com0exe is a very rarely needed tool, because the default
.com output of assemblers and compilers already works on DOS, and it has 0 bytes of
overhead. The output of com0exe is useful if an .exe program file is needed,
e.g. in a Win32 PE stub or when using an executable packer.

As a side effect, variant 24 proves that it's possible to write a DOS .exe
program with only 24 bytes of headers. Many linkers produce an 512-byte DOS
.exe header by default, which is completely unnecessary most of time.

__END__
