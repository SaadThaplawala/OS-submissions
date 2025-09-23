#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  for(int i = 1; i <= 100; i++){
    if(i % 6 == 0 && i % 5 == 0)
      printf("sixfive\n");
    else if(i % 6 == 0)
      printf("six\n");
    else if(i % 5 == 0)
      printf("five\n");
    else
      printf("%d\n", i);
  }
  exit(0);
}
