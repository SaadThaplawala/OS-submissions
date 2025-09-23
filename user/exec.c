#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    printf("usage: exec program [args]\n");
    exit(1);
  }

  exec(argv[1], &argv[1]);
  printf("exec: failed to exec %s\n", argv[1]);
  exit(0);
}
