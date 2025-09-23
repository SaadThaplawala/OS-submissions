#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("usage: sleep ticks\n");
    exit(1);
  }

  int ticks = atoi(argv[1]);
  if (ticks <= 0) {
    exit(0);
  }

  pause(ticks);

  exit(0);
}
