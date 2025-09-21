#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  char *line = NULL;
  size_t len = 0;
  ssize_t nread;
  pid_t pid;
  int status;

  puts("Enter programs to run.");

  while (1) {
    fputs("> ", stdout);
    fflush(stdout);

    nread = getline(&line, &len, stdin);
    if (nread < 0) {
      break;
    }

    if (line[nread - 1] == '\n') {
      line[nread - 1] = '\0';
    }

    pid = fork();
    if (pid < 0) {
      perror("fork");
      free(line);
      return EXIT_FAILURE;
    }

    if (pid == 0) {
      execl(line, line, (char *)NULL);
      perror("Exec failure");
      _exit(EXIT_FAILURE);
    } else {
      if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        free(line);
        return EXIT_FAILURE;
      }
      puts("Enter programs to run.");
    }
  }
  free(line);
  return EXIT_SUCCESS;
}
