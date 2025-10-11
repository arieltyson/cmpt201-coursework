#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct header {
  uint64_t size;
  struct header *next;
  int id;
};

void initialize_block(struct header *block, uint64_t size, struct header *next,
                      int id) {
  block->size = size;
  block->next = next;
  block->id = id;
}

/* First-fit: return id of the first block with size >= requested size. */
int find_first_fit(struct header *free_list_ptr, uint64_t size) {
  for (struct header *p = free_list_ptr; p != NULL; p = p->next) {
    if (p->size >= size)
      return p->id;
  }
  return -1; // not found
}

/* Best-fit: among blocks with size >= requested, pick the smallest size. */
int find_best_fit(struct header *free_list_ptr, uint64_t size) {
  int best_fit_id = -1;
  uint64_t best_size = UINT64_MAX;

  for (struct header *p = free_list_ptr; p != NULL; p = p->next) {
    if (p->size >= size && p->size < best_size) {
      best_size = p->size;
      best_fit_id = p->id;
    }
  }
  return best_fit_id;
}

/* Worst-fit: among blocks with size >= requested, pick the largest size. */
int find_worst_fit(struct header *free_list_ptr, uint64_t size) {
  int worst_fit_id = -1;
  uint64_t worst_size = 0;

  for (struct header *p = free_list_ptr; p != NULL; p = p->next) {
    if (p->size >= size && p->size > worst_size) {
      worst_size = p->size;
      worst_fit_id = p->id;
    }
  }
  return worst_fit_id;
}

int main(void) {

  struct header *free_block1 = (struct header *)malloc(sizeof(struct header));
  struct header *free_block2 = (struct header *)malloc(sizeof(struct header));
  struct header *free_block3 = (struct header *)malloc(sizeof(struct header));
  struct header *free_block4 = (struct header *)malloc(sizeof(struct header));
  struct header *free_block5 = (struct header *)malloc(sizeof(struct header));

  initialize_block(free_block1, 6, free_block2, 1);
  initialize_block(free_block2, 12, free_block3, 2);
  initialize_block(free_block3, 24, free_block4, 3);
  initialize_block(free_block4, 8, free_block5, 4);
  initialize_block(free_block5, 4, NULL, 5);

  struct header *free_list_ptr = free_block1;

  int first_fit_id = find_first_fit(free_list_ptr, 7);
  int best_fit_id = find_best_fit(free_list_ptr, 7);
  int worst_fit_id = find_worst_fit(free_list_ptr, 7);

  /* Print IDs (matches sample output format). */
  printf("The ID for First-Fit algorithm is: %d\n", first_fit_id);
  printf("The ID for Best-Fit algorithm is: %d\n", best_fit_id);
  printf("The ID for Worst-Fit algorithm is: %d\n", worst_fit_id);

  /* Clean up (not required for the lab, but good hygiene). */
  free(free_block1);
  free(free_block2);
  free(free_block3);
  free(free_block4);
  free(free_block5);

  return 0;
}

/*
------------------------------------------------------------------------------
Part 2 — PSEUDO-CODE: Coalescing Contiguous Free Blocks
Goal: After freeing a single newly freed block B, insert B into the free
      list (which is already coalesced and kept sorted by address), then
      merge B with an immediate predecessor and/or successor if adjacent.

Assumptions:
- struct header is at the start of each block.
- 'size' stores the payload (usable) bytes after the header.
- Two blocks X and Y are contiguous if:
    end(X) == start(Y)
  where:
    start(X) = (char*)X
    end(X)   = (char*)X + sizeof(struct header) + X->size
- free_list points to the head of a singly linked list sorted by address.

Helper:
  HEADER_SZ = sizeof(struct header)
  start(b) = (char*)b
  end(b)   = (char*)b + HEADER_SZ + b->size

Algorithm coalesce(free_list, B):
  if free_list == NULL:
      B->next = NULL
      return B

  // 1) Find insertion point by address
  prev = NULL
  curr = free_list
  while curr != NULL and curr < B:
      prev = curr
      curr = curr->next

  // 2) Insert B between prev and curr
  B->next = curr
  if prev != NULL:
      prev->next = B
  else:
      free_list = B   // B becomes new head

  // 3) Try merge with predecessor
  if prev != NULL and end(prev) == start(B):
      // absorb B into prev
      prev->size = prev->size + HEADER_SZ + B->size
      prev->next = B->next
      B = prev   // merged block is now 'B' for the next step

  // 4) Try merge with successor
  if curr != NULL and end(B) == start(curr):
      B->size = B->size + HEADER_SZ + curr->size
      B->next = curr->next

  return free_list

Notes:
- If both predecessor and successor are adjacent, both merges happen (forming
  a single larger block).
- Keeping the free list sorted by address makes coalescing O(n) with one pass.
- If 'size' in your implementation includes the header bytes already, then
  drop HEADER_SZ additions in the formulas above.
------------------------------------------------------------------------------
*/
