/*
 * com0exe.c: DOS .com program to .exe converter
 * by pts@fazekas.hu at Sun May  8 22:29:49 CEST 2022
 *
 * Compile as:
 *
 *   $ gcc -ansi -pedantic -s -O2 -W -Wall -Wextra -Werror=implicit-function-declaration -Werror=int-conversion  -o com0exe com0exe.c
 */

#if defined(__TINYC__) && defined(__linux__)
  #define O_RDONLY 0
  #define O_WRONLY 1
  #define O_RDWR 2
  #define O_CREAT 0100
  #define O_TRUNC 01000
  #define SEEK_SET 0
  #define SEEK_CUR 1
  #define SEEK_END 2
  typedef unsigned long size_t;
  typedef long ssize_t;
  typedef long off_t;
  typedef unsigned short uint16_t;
  extern size_t strlen(const char *__s)  __attribute__((__nothrow__ , __leaf__)) __attribute__((__pure__)) __attribute__((__nonnull__(1)));
  extern void _exit(int __status) __attribute__((__noreturn__));
  extern int strcmp(const char *__s1, const char *__s2) __attribute__((__nothrow__ , __leaf__)) __attribute__((__pure__)) __attribute__((__nonnull__(1, 2)));
  extern int memcmp(const void *__s1, const void *__s2, size_t __n) __attribute__((__nothrow__ , __leaf__)) __attribute__((__pure__)) __attribute__((__nonnull__(1, 2)));
  extern void *memcpy(void *__restrict __dest, const void *__restrict __src, size_t __n) __attribute__((__nothrow__ , __leaf__)) __attribute__((__nonnull__(1, 2)));
  extern int open(const char *__file, int __oflag, ...) __attribute__((__nonnull__(1)));
  extern ssize_t read(int __fd, void *__buf, size_t __nbytes) ;
  extern ssize_t write(int __fd, const void *__buf, size_t __n);
  extern int close(int __fd);
  extern __off_t lseek(int __fd, __off_t __offset, int __whence) __attribute__((__nothrow__ , __leaf__));
  #define strerror(errno) "I/O error"
  #define exit(x) _exit(x)
  #define NULL ((void*)0)
#else
  #include <errno.h>
  #include <fcntl.h>
  #include <stdint.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
#endif

#ifdef __XSTATIC__
  #define exit(x) _exit(x)
#endif

#undef IS_LE  /* If defined to be nonzero, then the architecture is little-endian. */
#ifdef MSDOS
#define IS_LE 1
#define SEP2 '\\'
#define strerror(errno) "I/O error"
#else
#ifdef WIN32
#define IS_LE 1
#define SEP2 '\\'
#define strerror(errno) "I/O error"
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

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

/* Writes unsigned decimal to filehandle. */
static int write_u10(int fd, unsigned u) {
  char tmp[sizeof(u) * 3], *p = tmp, *q = tmp, *r, c;
  do {
    *p++ = '0' + u % 10;
  } while ((u /= 10) != 0);
  r = p--;
  while (q < p) {
    c = *q;
    *q++ = *p;
    *p-- = c;
  }
  return write(fd, tmp, r - tmp);
}

static ssize_t write_str3(int fd, const char *str1, const char *str2, const char *str3) {
  ssize_t got = write(fd, str1, strlen(str1));
  if (got < 0) return -1;
  got = write(fd, str2, strlen(str2));
  if (got < 0) return -1;
  got = write(fd, str3, strlen(str3));
  if (got < 0) return -1;
  return 1;
}

static void change_ext(char *fn, const char *ext) {
  char *p0 = fn + strlen(fn), *p = p0;
  for (; p != fn && p[-1] != '.' && p[-1] != '/' && p[-1] != SEP2; --p) {}
  if (p == fn || p[-1] != '.') {
    write_str3(STDERR_FILENO, "fatal: extension not found in filename: ", fn, "\n");
    exit(2);
  }
  if ((size_t)(p0 - p) > strlen(ext)) {
    write_str3(STDERR_FILENO, "fatal: extension too short in filename: ", fn, "\n");
    exit(2);
  }
  memcpy(p, ext, strlen(ext) + 1);
}

/* Same as in kvikdos. */
#define MAX_DOS_COM_SIZE 0xfef0

static char buf[16384 + 11];

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
  uint16_t com_size, com_size_with_trailer, com_size_read, exehdr_size, entry;
  char *com_start;

  (void)argc;
  if (!argv[0] || !argv[1] || strcmp(argv[1], "--help") == 0) {
    write_str3(STDERR_FILENO, "com0exe: DOS .com program to .exe converter\n"
               "This is free software, GNU GPL >=2.0. There is NO WARRANTY. Use at your risk.\n"
               "Usage: ", argv[0] ? "com0exe" : argv[0],
               " [<flag> ...] <dos-com-file> [<output-exe-file>]\n"
               "Flags:\n"
               "--noret: Assumes that .com program doesn't use ret to exit.\n");
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
      write_str3(STDERR_FILENO, "fatal: unknown command-line flag: ", arg, "\n");
      exit(1);
    }
  }
  /* Now: argv contains remaining (non-flag) arguments. */
  if (!argv[0]) {
    write_str3(STDERR_FILENO, "fatal: missing <dos-com-file> input filename\n", "", "");
    exit(1);
  }
  infn = *argv++;
  outfn = argv[0] ? *argv++ : NULL;

  {
    if ((infd = open(infn, O_RDONLY | O_BINARY)) < 0) {
      write_str3(STDERR_FILENO, "fatal: error opening input file: ", infn, ": ");
     error_errno:
      write_str3(STDERR_FILENO, strerror(errno), "\n", "");
      exit(2);
    }
    if ((got = read(infd, buf, 16384)) < 0) { read_error:
      write_str3(STDERR_FILENO, "fatal: error reading input file: ", infn, ": ");
      goto error_errno;
    }
    if (got == 16384) {  /* !! test with smaller. */
      long file_size;
      if ((file_size = lseek(infd, 0, SEEK_END)) < 0) { seek_error:
        write_str3(STDERR_FILENO, "fatal: error seeking input file: ", infn, ": ");
        goto error_errno;
      }
      if (file_size > MAX_DOS_COM_SIZE) {
        write_str3(STDERR_FILENO, "fatal: .com program too long: ", infn, ": ");
        write_u10(STDERR_FILENO, (unsigned)file_size);
        (void)!write(STDERR_FILENO, "\n", 1);
        exit(2);
      }
      if (lseek(infd, 16384, SEEK_SET) != 16384) goto seek_error;
      com_size = (unsigned)file_size;
      com_size_read = 16384;
    } else {
      com_size = com_size_read = got;
    }
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
    com_size_read -= delta;
  }
  if (variant == 43) {
    entry = com_size;
    com_size_with_trailer = com_size + sizeof(variant_43_trailer);
  } else {
    entry = variant & 8;
    com_size_with_trailer = com_size;
    if (variant == 27) {
      memcpy((char*)exehdr + 24, variant_27_intro, sizeof(variant_27_intro));
    }
  }
  if (variant < 32) {
    exehdr_size = variant;
    for (; exehdr_size + com_size < 28; com_start[com_size++] = '\0', ++com_size_read, ++com_size_with_trailer) {}
  } else {
    exehdr_size = 32;
    memcpy((char*)exehdr + 24, output_signature, sizeof(output_signature));
  }
  if (entry >= 0xff00) {
    write_str3(STDERR_FILENO, "fatal: .exe program entry point would be to high: ", infn, "\n");
    exit(2);
  }
  if (exehdr_size + com_size_with_trailer > MAX_DOS_COM_SIZE + variant) {  /* This should never happen, we've checked everything above. */
    write_str3(STDERR_FILENO, "fatal: .exe program would be too long: ", infn, "\n");
    exit(2);
  }

  { uint16_t *eh = exehdr;
    const uint16_t exehdr_para = variant >> 4;  /* = (variant >= 32) ? 2 : 1; */
    const uint16_t imagex_size = exehdr_size + com_size_with_trailer;
    const uint16_t imagex_size_lastsize = imagex_size & 0x1ff;
    const uint16_t imagex_size_nblocks = (imagex_size >> 9) + (imagex_size_lastsize ? 1 : 0);  /* No uint16_t overflow. */
    const uint16_t imagea_para = (imagex_size_nblocks << 5) - exehdr_para;
    /* By doing a minimum operation here we make the memory requirements of
     * large (close to 64 KiB) .exe files up to 496 bytes larger than
     * necessary. Otherwise we wouldn't be able to load such a long code due
     * to rounding to 512-byte boundary.
     */
    const uint16_t imagea_minalloc = (imagea_para >= 0xff0) ? 0 : 0xff0 - imagea_para;
    *eh++ /* [EXE_SIGNATURE] */ = 'M' | 'Z' << 8;
    *eh++ /* [EXE_LASTSIZE] */ = imagex_size_lastsize;
    *eh++ /* [EXE_NBLOCKS] */ = imagex_size_nblocks;
    *eh++ /* [EXE_NRELOC] */ = 0;
    *eh++ /* [EXE_HDRSIZE] */ = exehdr_para;
    *eh++ /* [EXE_MINALLOC] */ = imagea_minalloc;
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
  if ((outfd = open(outfn, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0644)) < 0) {
    write_str3(STDERR_FILENO, "fatal: error opening output file: ", outfn, ": ");
    goto error_errno;
  }
  if ((int)write(outfd, exehdr, exehdr_size) != (int)exehdr_size) { write_error:
    write_str3(STDERR_FILENO, "fatal: error writing output file: ", outfn, ": ");
    goto error_errno;
  }
  if (com_size == com_size_read && variant == 43) {
    memcpy(com_start + com_size, variant_43_trailer, sizeof(variant_43_trailer));
    com_size += sizeof(variant_43_trailer);
    com_size_read += sizeof(variant_43_trailer);
    variant = 0;
  }
  if (write(outfd, com_start, com_size_read) != com_size_read) goto write_error;
  com_size -= com_size_read;
  while (com_size > 0) {
    int attempt = (com_size > 16384) ? 16384 : com_size;
    if (read(infd, buf, attempt) != attempt) goto read_error;
    com_size -= attempt;
    if (com_size == 0 && variant == 43) {
      memcpy(buf + (uint16_t)attempt, variant_43_trailer, sizeof(variant_43_trailer));
      attempt += sizeof(variant_43_trailer);
    }
    if (write(outfd, buf, attempt) != attempt) goto write_error;
  }
  close(outfd);
  close(infd);
  return 0;  /* EXIT_SUCCESS. */
}
