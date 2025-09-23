#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int t = uptime();   // get ticks
  printf("Uptime: %d ticks\n", t);
  exit(0);
}
