#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY_SIZE 5

// Function to add a line to history
void add_to_history(char **history, int *current_index, char *line) {
  // Free existing memory at this position if it exists
  if (history[*current_index] != NULL) {
    free(history[*current_index]);
  }

  // Allocate memory and copy the line
  history[*current_index] = malloc(strlen(line) + 1);
  if (history[*current_index] != NULL) {
    strcpy(history[*current_index], line);
  }

  // Move to next position (circular)
  *current_index = (*current_index + 1) % HISTORY_SIZE;
}

// Function to print history
void print_history(char **history, int current_index, int total_commands) {
  int commands_to_print =
      (total_commands < HISTORY_SIZE) ? total_commands : HISTORY_SIZE;

  // Calculate starting position for printing
  int start_index;
  if (total_commands < HISTORY_SIZE) {
    start_index = 0;
  } else {
    start_index = current_index;
  }

  // Print the history in chronological order
  for (int i = 0; i < commands_to_print; i++) {
    int index = (start_index + i) % HISTORY_SIZE;
    if (history[index] != NULL) {
      printf("%s", history[index]); // getline includes \n, so no need to add
    }
  }
}

int main(void) {
  char *history[HISTORY_SIZE] = {
      NULL};              // Array of string pointers, initialized to NULL
  char *line = NULL;      // For getline
  size_t len = 0;         // For getline
  ssize_t nread;          // For getline return value
  int current_index = 0;  // Current position in circular buffer
  int total_commands = 0; // Total commands entered

  while (1) {
    printf("Enter input: ");

    // Read a line using getline
    nread = getline(&line, &len, stdin);

    // Check for EOF (Ctrl+C will send EOF)
    if (nread == -1) {
      break;
    }

    // Remove trailing newline for comparison (but keep it in storage)
    char *line_for_comparison = malloc(strlen(line) + 1);
    strcpy(line_for_comparison, line);

    // Remove newline for comparison
    char *newline_pos = strchr(line_for_comparison, '\n');
    if (newline_pos != NULL) {
      *newline_pos = '\0';
    }

    // Check if the command is "print"
    if (strcmp(line_for_comparison, "print") == 0) {
      // Add "print" to history first
      add_to_history(history, &current_index, line);
      total_commands++;

      // Then print the history
      print_history(history, current_index, total_commands);
    } else {
      // Add the command to history
      add_to_history(history, &current_index, line);
      total_commands++;
    }

    free(line_for_comparison);
  }

  // Clean up memory before exiting
  for (int i = 0; i < HISTORY_SIZE; i++) {
    if (history[i] != NULL) {
      free(history[i]);
    }
  }

  if (line != NULL) {
    free(line);
  }

  return 0;
}
