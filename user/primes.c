#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void get_primes(int parent_fds[2]) {
  char buf[1];
  int n;
  if ((n = read(parent_fds[0], buf, sizeof(buf))) <= 0) {
    exit(0);
  };
  int num = buf[0];
  printf("prime %d\n", num);

  int child_fds[2];
  int pid;
  pipe(child_fds);
  if ((pid = fork()) > 0) {
    close(child_fds[0]);
    while (read(parent_fds[0], buf, sizeof(buf)) > 0) {
      int n = buf[0];
      if (n % num != 0) {
        write(child_fds[1], buf, sizeof(buf));
      }
    }
    close(parent_fds[0]);
    close(child_fds[1]);
    int status;
    wait(&status);
  } else {
    close(child_fds[1]);
    get_primes(child_fds);
  }
  exit(0);
}

int
main(int argc, char *argv[])
{
  int fds[2];
  int pid;
  char buf[1];

  pipe(fds);
  if ((pid = fork()) > 0) {
    // parent
    close(fds[0]);
    for (int i = 2; i <= 35; i++) {
      buf[0] = i;
      write(fds[1], buf, sizeof(buf));
    }
    close(fds[1]);
    int status;
    wait(&status);
  } else {
    // child
    close(fds[1]);
    get_primes(fds);
  }
  exit(0);
}
