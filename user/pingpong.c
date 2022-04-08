#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int pid = fork();
  int fds[2];
  int buf[1];
  int n, status;
  pipe(fds);

  if (pid != 0) {
    // parent
    write(fds[0], "a", 1);
    wait(&status);
    read(fds[1], buf, sizeof(buf));
    printf("%d: received pong\n", getpid());
  } else {
    // child
    n = read(fds[1], buf, sizeof(buf));
    printf("%d: received ping\n", getpid());
    write(fds[0], buf, n);
  }
  exit(0);
}
