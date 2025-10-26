// Task 1 (example_1.c)

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Assertion failed: %s\n", #expr);                        \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define TEST(expr)                                                             \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Test failed: %s\n", #expr);                             \
      exit(1);                                                                 \
    } else {                                                                   \
      printf("Test passed: %s\n", #expr);                                      \
    }                                                                          \
  } while (0)

typedef struct node1 {
  uint64_t data;
  struct node1 *next;
} node1_t;

node1_t *head1 = NULL;

void insert_sorted1(uint64_t data) {
  node1_t *new_node = malloc(sizeof(node1_t));
  new_node->data = data;
  new_node->next = NULL;

  if (head1 == NULL) {
    head1 = new_node;
    return;
  }

  if (data < head1->data) {
    new_node->next = head1;
    head1 = new_node;
    return;
  }

  node1_t *curr = head1;
  node1_t *prev = NULL;

  while (curr != NULL && data >= curr->data) {
    prev = curr;
    curr = curr->next;
  }

  prev->next = new_node;
  new_node->next = curr;
}

int index_of1(uint64_t data) {
  node1_t *curr = head1;
  int index = 0;

  while (curr != NULL) {
    if (curr->data == data)
      return index;
    curr = curr->next;
    index++;
  }
  return -1;
}

int main_task1_demo(void) {
  insert_sorted1(1);
  insert_sorted1(2);
  insert_sorted1(5);
  insert_sorted1(3);

  TEST(index_of1(3) == 2);

  insert_sorted1(0);
  insert_sorted1(4);

  TEST(index_of1(4) == 4);
  return 0;
}

// Task 2 (example_2.c)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ASSERT2(expr)                                                          \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Assertion failed: %s\n", #expr);                        \
      fprintf(stderr, "  at %s:%d\n", __FILE__, __LINE__);                     \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

#define TEST2(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Test failed: %s\n", #expr);                             \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)

typedef struct node2 {
  uint64_t data;
  struct node2 *next;
} node2_t;

typedef struct info2 {
  uint64_t sum;
} info2_t;

node2_t *head2 = NULL;
info2_t info2 = {0};

static uint64_t sum_list2(void) {
  uint64_t s = 0;
  for (node2_t *p = head2; p != NULL; p = p->next)
    s += p->data;
  return s;
}

void insert_sorted2(uint64_t data) {
  node2_t *new_node = malloc(sizeof(node2_t));
  new_node->data = data;
  new_node->next = NULL;

  if (head2 == NULL) {
    head2 = new_node;
  } else if (data < head2->data) {
    new_node->next = head2;
    head2 = new_node;
  } else {
    node2_t *curr = head2;
    node2_t *prev = NULL;

    while (curr != NULL && data >= curr->data) {
      prev = curr;
      curr = curr->next;
    }

    prev->next = new_node;
    new_node->next = curr;
  }

  info2.sum += data;
}

int index_of2(uint64_t data) {
  node2_t *curr = head2;
  int index = 0;

  while (curr != NULL) {
    if (curr->data == data)
      return index;
    curr = curr->next;
    index++;
  }
  return -1;
}

int main_task2_demo(void) {
  insert_sorted2(1);
  ASSERT2(info2.sum == sum_list2());

  insert_sorted2(3);
  ASSERT2(info2.sum == sum_list2());

  insert_sorted2(5);
  ASSERT2(info2.sum == sum_list2());

  insert_sorted2(2);
  ASSERT2(info2.sum == sum_list2());

  TEST2(info2.sum == 1 + 3 + 5 + 2);
  TEST2(index_of2(2) == 1);
  return 0;
}
