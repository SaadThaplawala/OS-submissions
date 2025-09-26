#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// memdump: interpret memory according to format string
void memdump(char *fmt, char *data) {
  while(*fmt){
    switch(*fmt){
    case 'i': { // 4-byte int
      int val = *(int*)data;
      printf("%d\n", val);
      data += 4;
      break;
    }
    case 'p': { // 8-byte pointer (print as hex)
      uint64 val = *(uint64*)data;
      printf("%lx\n", val);
      data += 8;
      break;
    }
    case 'h': { // 2-byte short
      short val = *(short*)data;
      printf("%d\n", val);
      data += 2;
      break;
    }
    case 'c': { // 1-byte char
      char val = *data;
      printf("%c\n", val);
      data += 1;
      break;
    }
    case 's': { // 8-byte pointer to string
      char *str = *(char**)data;
      printf("%s\n", str);
      data += 8;
      break;
    }
    case 'S': { // inline null-terminated string
      printf("%s\n", data);
      data += strlen(data) + 1;
      break;
    }
    }
    fmt++;
  }
}

int
main(int argc, char *argv[])
{
  if(argc == 1){
    // Run the built-in examples
    printf("Example 1:\n");
    int a[2] = {61810, 2025};
    memdump("ii", (char*)a);

    printf("Example 2:\n");
    char *s = "a string";
    memdump("s", (char*)&s);

    printf("Example 3:\n");
    char buf3[] = "another";
    memdump("S", buf3);

    printf("Example 4:\n");
    struct {
      char c1;
      int x;
      short h;
      char c2;
      char *str;
    } ex4 = { 'B', 1819438967, 100, 'z', "xyzzy" };
    memdump("cihcS", (char*)&ex4);

    printf("Example 5:\n");
    char buf5[] = "hello";
    memdump("ccccc", buf5);
    exit(0);
  }

  // If called with arguments: argv[1] = fmt
  // read stdin into a buffer, then dump according to fmt
  char *fmt = argv[1];
  char buf[512];
  int n = read(0, buf, sizeof(buf)-1);
  if(n < 0){
    fprintf(2, "memdump: read error\n");
    exit(1);
  }
  buf[n] = '\0';
  memdump(fmt, buf);
  exit(0);
}
