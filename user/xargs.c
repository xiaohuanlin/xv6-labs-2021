#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int
main(int argc, char *argv[])
{
  if(argc < 2){
    fprintf(2, "Usage: xargs cmd...\n");
    exit(1);
  }
  char buf[512];
  char* p_buf = buf;
  int n;

  while (1) {
    char arg[512];
    char* p_arg = arg;
    while ((n = read(0, p_buf, sizeof(char))) > 0 && *p_buf != '\n' && *p_buf != '\0') {
      *p_arg++ = *p_buf++;
    }
    *p_arg = '\0';
    if (n <= 0 || *p_buf == '\0') {
      break;
    }
    p_buf++;
    
    int pid;
    if ((pid = fork()) > 0) {
      // parent
      int status;
      wait(&status);
    } else {
      // child
      if (argc > MAXARG) {
        fprintf(2, "xargs: exceed max args length");
        exit(1);
      }
      char *args[argc];
      for (int i = 0; i < argc - 1; i++) {
        args[i] = argv[i + 1];
      }
      args[argc - 1] = arg;

      exec(argv[1], args);
      printf("exec failed!\n");
      exit(1);
    }
  }

  exit(0);
}
