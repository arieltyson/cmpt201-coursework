// lab4.c

// Enable glibc defaults so sbrk() is declared even without POSIX.
// (Alternatively: compile with -D_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE

#include <assert.h> // <-- add this
#include <errno.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>     // snprintf
#include <string.h>    // strerror, memset
#include <sys/types.h> // ssize_t
#include <unistd.h>
-- -- -- --
#ifndef BUF_SIZE
#define BUF_SIZE 256
#endif

         static void
         handle_error(const char *msg) {
  // Minimal, heap-free error path.
  // Print "<msg>: <strerror>\n" to STDERR and exit(1).
  char buf[BUF_SIZE];
  int n = snprintf(buf, sizeof(buf), "%s: %s\n", msg, strerror(errno));
  if (n > 0) {
    (void)write(STDERR_FILENO, buf, (size_t)n);
  }
  _exit(1);
}

// Provided in the lab (kept as-is)
void print_out(char *format, void *data, size_t data_size) {
  char buf[BUF_SIZE];
  ssize_t len = snprintf(buf, BUF_SIZE, format,
                         data_size == sizeof(uint64_t) ? *(uint64_t *)data
                                                       : *(void **)data);
  if (len < 0) {
    handle_error("snprintf");
  }
  write(STDOUT_FILENO, buf, (size_t)len);
}

// Small helpers to avoid confusion in the ternary above.
// For pointers, pass data_size = 0 (forces pointer branch).
// For u64, pass sizeof(uint64_t) (forces integer branch).
static inline void print_ptr(const char *fmt, const void *p) {
  void *q = (void *)p;
  print_out((char *)fmt, &q, 0);
}
static inline void print_u64(const char *fmt, uint64_t v) {
  print_out((char *)fmt, &v, sizeof(uint64_t));
}

// -------- Block header & layout --------
// NOTE: the lab text has a typo "unit64_t" — should be "uint64_t".
struct header {
  uint64_t size;
  struct header *next;
};
typedef struct header header_t;

enum { REGION_SIZE = 256, BLOCKS = 2, BLOCK_SIZE = REGION_SIZE / BLOCKS };

static_assert(BLOCK_SIZE >= (int)sizeof(header_t),
              "Block too small for header");
static_assert((BLOCK_SIZE % alignof(max_align_t)) == 0 ||
                  sizeof(void *) <= (size_t)BLOCK_SIZE,
              "Block size should be reasonable for alignment");

// Return the data payload byte-count inside a block (block size minus header).
static inline size_t payload_size(void) {
  return (size_t)BLOCK_SIZE - sizeof(header_t);
}

int main(void) {
  // 1) Grow the program break by 256 bytes.
  errno = 0;
  void *region = sbrk(REGION_SIZE);
  if (region == (void *)-1) {
    handle_error("sbrk(256)");
  }

  // Treat the 256B as two equal blocks of 128B each.
  unsigned char *base = (unsigned char *)region;
  header_t *first = (header_t *)base;
  header_t *second = (header_t *)(base + BLOCK_SIZE);

  // 2) Initialize headers (total block size stored in .size).
  first->size = BLOCK_SIZE;
  first->next = NULL;
  second->size = BLOCK_SIZE;
  second->next = first;

  // 3) Initialize payloads: first -> zeros, second -> ones.
  uint8_t *first_data = (uint8_t *)(first + 1);   // immediately after header
  uint8_t *second_data = (uint8_t *)(second + 1); // immediately after header
  const size_t nbytes = payload_size();

  memset(first_data, 0x00, nbytes);
  memset(second_data, 0x01, nbytes);

  // 4) Print addresses, header fields, and payload contents.
  // Addresses of each block (header addresses)
  print_ptr("first block:       %p\n", (void *)first);
  print_ptr("second block:      %p\n", (void *)second);

  // Header values
  print_u64("first block size:  %" PRIu64 "\n", first->size);
  print_ptr("first block next:  %p\n", (void *)first->next);
  print_u64("second block size: %" PRIu64 "\n", second->size);
  print_ptr("second block next: %p\n", (void *)second->next);

  // Payload bytes: many 0s then many 1s (one per line, like the example)
  for (size_t i = 0; i < nbytes; ++i) {
    print_u64("%" PRIu64 "\n", (uint64_t)first_data[i]); // 0
  }
  for (size_t i = 0; i < nbytes; ++i) {
    print_u64("%" PRIu64 "\n", (uint64_t)second_data[i]); // 1
  }

  // (Optional) Restore the program break so the process ends cleanly
  // without leaving the extra 256 bytes "in use".
  // Errors here are non-fatal for the lab, but we can check anyway.
  errno = 0;
  if (sbrk(-REGION_SIZE) == (void *)-1) {
    handle_error("sbrk(-256)");
  }

  return 0;
}
