#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Allowed separators
char *seps = " -\r\t\n./,";

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: sixfive file...\n");
    exit(1);
  }

  for(int i = 1; i < argc; i++){
    int fd = open(argv[i], O_RDONLY);
    if(fd < 0){
      fprintf(2, "sixfive: cannot open %s\n", argv[i]);
      exit(1);
    }

    char buf[32];   // buffer to store digits of current number
    int bi = 0;     // buffer index
    char c;
    while(read(fd, &c, 1) == 1){
      if(strchr(seps, c)){
        // separator hit → check number
        if(bi > 0){
          buf[bi] = '\0';   // null terminate
          int n = atoi(buf);
          if(n % 5 == 0 || n % 6 == 0){
            printf("%d\n", n);
          }
          bi = 0; // reset buffer
        }
      } else if(c >= '0' && c <= '9'){
        if(bi < sizeof(buf)-1){
          buf[bi++] = c;
        }
      }
    }

    // End of file: flush last number if exists
    if(bi > 0){
      buf[bi] = '\0';
      int n = atoi(buf);
      if(n % 5 == 0 || n % 6 == 0){
        printf("%d\n", n);
      }
    }

    close(fd);
  }

  exit(0);
}
