/*
 * com0exe.c: DOS .com program to .exe converter
 * by pts@fazekas.hu at Sun May  8 22:29:49 CEST 2022
 *
 * Compile as:
 *
 *   $ gcc -ansi -pedantic -s -O2 -W -Wall -Wextra -Werror=implicit-function-declaration -Werror=int-conversion  -o com0exe com0exe.c
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#undef IS_LE  /* If defined to be nonzero, then the architecture is little-endian. */
#ifdef MSDOS
#define IS_LE 1
#define SEP2 '\\'
#else
#ifdef WIN32
#define IS_LE 1
#define SEP2 '\\'
#else
#define SEP2 '/'
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define IS_LE 1
#else
/* __amd64__ and __i386__ is GCC and Clang, _M_IX86 is OpenWatcom, Borland C++, Turbo C++. */
#if defined(__amd64__) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(__AMD64__) || defined(__ia64__) || defined(__arm__) || defined(__arm64__)
#define IS_LE 1
#endif
#endif
#endif
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

static void change_ext(char *fn, const char *ext) {
  char *p0 = fn + strlen(fn), *p = p0;
  for (; p != fn && p[-1] != '.' && p[-1] != '/' && p[-1] != SEP2; --p) {}
  if (p == fn || p[-1] != '.') {
    fprintf(stderr, "fatal: extension not found in filename: %s\n", fn);
    exit(2);
  }
  if ((size_t)(p0 - p) > strlen(ext)) {
    fprintf(stderr, "fatal: extension too short in filename: %s\n", fn);
    exit(2);
  }
  strcpy(p, ext);
}

/* Same as in kvikdos. */
#define MAX_DOS_COM_SIZE 0xfef0

static char buf[MAX_DOS_COM_SIZE + 1 + 11];

static uint16_t exehdr[16];  /* 32 bytes, including the last 8 trailing bytes for output_signature or variant_27_intro. */

static const unsigned char variant_43_trailer[11] = {
    0xbe, 0, 0,  /* mov si, 0; */
    0x56,        /* push si; */
    0xbe, 0, 1,  /* mov si, 0x100; */
    0x89, 0xe7,  /* mov di, sp; */
    0xff, 0xe6,  /* jmp si; */
};
static const unsigned char variant_27_intro[3] = {
    0x31, 0xff,  /* xor di, di; */
    0x57,        /* push di; */
};
static const unsigned char output_signature[8] = { 'C', 'O', 'M', '0', 'E', 'X', 'E', ':' };  /* "COM0EXE:". */
static const unsigned char variant_24_com_prefix[8] = {
    0x5f,        /* pop di; */
    0xbe, 8, 1,  /* mov si, 0x108; */
    0xeb, 2,     /* jmp strict short 0x108; */
    0xfa,        /* cli; */
    0xf4,        /* hlt; */
};
static const unsigned char variant_27_com_prefix[11] = {
    0xbe, 8, 1,  /* mov si, 0x108; */
    0x31, 0xff,  /* xor di, di; */
    0xeb, 4,     /* jmp strict short 0x10b; */
    0xfa,        /* cli; */
    0xfa,        /* cli; */
    0xfa,        /* cli; */
    0xf4,        /* hlt; */
};

/* All offsets are in 2-byte words. */
#define EXE_SIGNATURE 0
#define EXE_LASTSIZE 1
#define EXE_NBLOCKS 2
#define EXE_NRELOC 3
#define EXE_HDRSIZE 4
#define EXE_MINALLOC 5
#define EXE_MAXALLOC 6
#define EXE_SS 7
#define EXE_SP 8
#define EXE_CHECKSUM 9  /* Ignored by kvikdos. */
#define EXE_IP 10
#define EXE_CS 11
#define EXE_RELOCPOS 12
#define EXE_NOVERLAY 13  /* Ignored and not even loaded by kvikdos. */

int main(int argc, char **argv) {
  char noret = 0, variant;
  char *infn, *outfn;
  int infd, outfd, got;
  uint16_t com_size, exehdr_size, entry;
  char *com_start;

  (void)argc;
  if (!argv[0] || !argv[1] || strcmp(argv[1], "--help") == 0) {
    fprintf(stderr, "com0exe: DOS .com program to .exe converter\n"
                    "This is free software, GNU GPL >=2.0. There is NO WARRANTY. Use at your risk.\n"
                    "Usage: %s [<flag> ...] <dos-com-file> [<output-exe-file>]\n"
                    "Flags:\n"
                    "--noret: Assumes that .com program doesn't use ret to exit.\n",
                    argv[0]);
    exit(argv[0] && argv[1] ? 0 : 1);
  }
  ++argv;
  while (argv[0]) {
    char *arg = *argv++;
    if (arg[0] != '-' || arg[1] == '\0') {
      --argv; break;
    } else if (arg[1] == '-' && arg[2] == '\0') {
      break;
    } else if (0 == strcmp(arg, "--noret")) {
      noret = 1;
    } else {
      fprintf(stderr, "fatal: unknown command-line flag: %s\n", arg);
      exit(1);
    }
  }
  /* Now: argv contains remaining (non-flag) arguments. */
  if (!argv[0]) {
    fprintf(stderr, "fatal: missing <dos-com-file> input filename\n");
    exit(1);
  }
  infn = *argv++;
  outfn = argv[0] ? *argv++ : NULL;

  { char *p = buf;
    if ((infd = open(infn, O_RDONLY)) < 0) {
      fprintf(stderr, "fatal: error opening input file: %s: %s\n", infn, strerror(errno));
      exit(2);
    }
    p = buf;
    if ((got = read(infd, buf, 16384)) < 0) { read_error:
      fprintf(stderr, "fatal: error reading input file: %s: %s\n", infn, strerror(errno));
      exit(2);
    }
    p += got;
    if (got == 16384) {
      if ((got = read(infd, p, 16384)) < 0) goto read_error;
      p += got;
      if (got == 16384) {
        if ((got = read(infd, p, 16384)) < 0) goto read_error;
        p += got;
        if (got == 16384) {
          const int need = MAX_DOS_COM_SIZE + 1 - (p - buf);
          { struct SA { int StaticAssert_MaxSize : MAX_DOS_COM_SIZE > 3 * 16384U; }; }
          if ((got = read(infd, p, need)) < 0) goto read_error;
          p += got;
          if (got == need) {
            fprintf(stderr, "fatal: .com program too long: %s: %u\n", infn, (unsigned)(p - buf));
            exit(2);
          }
        }
      }
    }
    close(infd);
    com_size = p - buf;
  }
  if (com_size >= 10 && memcmp(buf, variant_24_com_prefix, sizeof(variant_24_com_prefix)) == 0) {
    variant = 24;
  } else if (com_size >= 12 && memcmp(buf, variant_27_com_prefix, sizeof(variant_27_com_prefix)) == 0) {
    variant = 27;
  } else if (noret) {
    variant = 32;
  } else {
    variant = 43;
  }
  { const uint16_t delta = (variant >= 32) ? 0 : variant - 16;
    com_start = buf + delta;
    com_size -= delta;
  }
  if (variant == 43) {
    entry = com_size;
    memcpy(com_start + com_size, variant_43_trailer, sizeof(variant_43_trailer));
    com_size += sizeof(variant_43_trailer);
  } else {
    entry = variant & 8;
    if (variant == 27) {
      memcpy((char*)exehdr + 24, variant_27_intro, sizeof(variant_27_intro));
    }
  }
  if (variant < 32) {
    exehdr_size = variant;
    for (; exehdr_size + com_size < 28; com_start[com_size++] = '\0') {}
  } else {
    exehdr_size = 32;
    memcpy((char*)exehdr + 24, output_signature, sizeof(output_signature));
  }
  if (entry >= 0xff00) {
    fprintf(stderr, "fatal: .exe program entry point would be to high: %s\n", infn);
    exit(2);
  }

  { uint16_t *eh = exehdr;
    const uint16_t exehdr_para = variant >> 4;  /* = (variant >= 32) ? 2 : 1; */
    const uint16_t imagex_size = exehdr_size + com_size;
    const uint16_t imagex_size_lastsize = imagex_size & 0x1ff;
    const uint16_t imagex_size_nblocks = (imagex_size >> 9) + (imagex_size_lastsize ? 1 : 0);  /* No uint16_t overflow. */
    const uint16_t imagea_para = (imagex_size_nblocks << 5) - exehdr_para;
    if ((exehdr_size & ~15) + com_size > MAX_DOS_COM_SIZE) {
      fprintf(stderr, "fatal: .exe program would be too long: %s\n", infn);
      exit(2);
    }
    if (imagea_para > 0xff0) {
      fprintf(stderr, "fatal: .exe program would be too long (para alloc count): %s\n", infn);
      exit(2);
    }
    *eh++ /* [EXE_SIGNATURE] */ = 'M' | 'Z' << 8;
    *eh++ /* [EXE_LASTSIZE] */ = imagex_size_lastsize;
    *eh++ /* [EXE_NBLOCKS] */ = imagex_size_nblocks;
    *eh++ /* [EXE_NRELOC] */ = 0;
    *eh++ /* [EXE_HDRSIZE] */ = exehdr_para;
    *eh++ /* [EXE_MINALLOC] */ = 0xff0 - imagea_para;
    *eh++ /* [EXE_MAXALLOC] */ = 0xffff;
    *eh++ /* [EXE_SS] */ = 0xfff0;
    *eh++ /* [EXE_SP] */ = (variant == 32) ? 0xfffe : 0;
    *eh++ /* [EXE_CHECKSUM] */ = 0;
    *eh++ /* [EXE_IP] */ = 0x100 + entry;
    *eh++ /* [EXE_CS] */ = 0xfff0;
  }
#if IS_LE
#else  /* Make sure that the exehdr fields are little endian. */
  { uint16_t *p;
    for (p = exehdr; p != exehdr + 12; ++p) {
      const uint16_t v = *p;
      ((char*)p)[0] = v;
      ((char*)p)[1] = v >> 8;
    }
  }
#endif

  if (!outfn) {
    outfn = infn;  /* Owned by argv. */
    change_ext(outfn, "exe");
  }
  { const char *p;
    uint16_t remaining;
    if ((outfd = open(outfn, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
      fprintf(stderr, "fatal: error opening output file: %s: %s\n", outfn, strerror(errno));
      exit(2);
    }
    if ((int)write(outfd, exehdr, exehdr_size) != (int)exehdr_size) { write_error:
      fprintf(stderr, "fatal: error writing output file: %s: %s\n", outfn, strerror(errno));
      exit(2);
    }
    p = com_start; remaining = com_size;
    while (remaining > 0) {
      const int attempt = (remaining > 16384) ? 16384 : remaining;
      if (write(outfd, p, attempt) != attempt) goto write_error;
      p += attempt; remaining -= attempt;
    }
    close(outfd);
  }
  return 0;  /* EXIT_SUCCESS. */
}
