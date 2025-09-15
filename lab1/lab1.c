#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  char *input_buffer = NULL;
  size_t buffer_capacity = 0;
  ssize_t characters_read;

  printf("Please enter some text: ");

  characters_read = getline(&input_buffer, &buffer_capacity, stdin);

  if (characters_read == -1) {
    perror("getline failed");
    free(input_buffer);
    return EXIT_FAILURE;
  }

  char *current_token;
  char *strtok_context;

  printf("Tokens:\n");

  current_token = strtok_r(input_buffer, " ", &strtok_context);

  while (current_token != NULL) {
    printf("%s\n", current_token);

    current_token = strtok_r(NULL, " ", &strtok_context);
  }

  free(input_buffer);

  return EXIT_SUCCESS;
}
