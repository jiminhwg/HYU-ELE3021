#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main (int argc, char *argv[]) {
  fprintf(1, "My student ID is 2023049998\n");
  fprintf(1, "My pid is %d\n", getpid());
  fprintf(1, "My ppid is %d\n", getppid());

  exit(0);
}

