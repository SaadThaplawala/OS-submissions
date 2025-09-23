#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/* parse decimal or 0x hex */
unsigned int
parseaddr(char *s)
{
  unsigned int val = 0;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
    while (*s) {
      char c = *s++;
      int d;
      if (c >= '0' && c <= '9') d = c - '0';
      else if (c >= 'a' && c <= 'f') d = 10 + c - 'a';
      else if (c >= 'A' && c <= 'F') d = 10 + c - 'A';
      else break;
      val = (val << 4) + d;
    }
  } else {
    val = atoi(s);
  }
  return val;
}

int
main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("usage: memdump base length\n");
    exit(1);
  }

  uint64 base = parseaddr(argv[1]);   // use xv6’s uint64 type
  int length = atoi(argv[2]);

  if (length <= 0) {
    printf("length must be > 0\n");
    exit(1);
  }

  unsigned char *p = (unsigned char *) (uint64) base;

  for (int i = 0; i < length; i++) {
    printf("%02x ", p[i]);
    if ((i + 1) % 16 == 0)
      printf("\n");
  }
  printf("\n");

  exit(0);
}
