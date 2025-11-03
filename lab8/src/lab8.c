// Lab 8 - Sorting + parallel counting with uthash
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // strcmp
#include <uthash.h>

#define THREAD_COUNT 3
typedef const char *word_t;

typedef struct {
  word_t word;       /* key */
  size_t count;      /* value */
  UT_hash_handle hh; /* makes this struct hashable */
} word_count_entry_t;

typedef word_count_entry_t *count_map_t;

/* ---------- creation & printing helpers ---------- */

static word_count_entry_t *create_entry(word_t word, size_t count) {
  word_count_entry_t *ptr = (word_count_entry_t *)malloc(sizeof(*ptr));
  if (!ptr) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  ptr->word = word;
  ptr->count = count;
  return ptr;
}

static int sort_func(word_count_entry_t *a, word_count_entry_t *b) {
  return strcmp(a->word, b->word);
}

static void print_counts(count_map_t word_map) {
  printf("%-32s%-10s\n", "Word", "Count");
  word_count_entry_t *current, *tmp;
  HASH_ITER(hh, word_map, current, tmp) {
    printf("%-32s%-10zu\n", current->word, current->count);
  }
}

static void delete_table(count_map_t word_map) {
  word_count_entry_t *current, *tmp;
  HASH_ITER(hh, word_map, current, tmp) {
    HASH_DEL(word_map, current);
    free(current);
  }
}

/* ---------- threading argument bundle ---------- */

typedef struct {
  count_map_t *map;      /* shared hash map (pointer to head pointer) */
  word_t *words;         /* this thread's slice of input words */
  size_t num_words;      /* count in the slice */
  pthread_mutex_t *lock; /* shared mutex protecting the map (may be NULL) */
} count_thread_args_t;

static count_thread_args_t *pack_args(count_map_t *map, word_t *words,
                                      size_t num_words, pthread_mutex_t *lock) {
  count_thread_args_t *args = (count_thread_args_t *)malloc(sizeof(*args));
  if (!args) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  args->map = map;
  args->words = words;
  args->num_words = num_words;
  args->lock = lock;
  return args;
}

/* ---------- core counting over a chunk ---------- */
/* Task 4: make this thread-safe. We lock per-element (fine-grained) to
   keep critical sections short while avoiding races on find/add/update. */
static void add_word_counts_in_chunk(count_map_t *map, word_t *words,
                                     size_t num_words, pthread_mutex_t *lock) {
  for (size_t i = 0; i < num_words; i++) {
    if (lock)
      pthread_mutex_lock(lock);

    word_count_entry_t *w = NULL;
    HASH_FIND_STR(*map, words[i], w);
    if (w) {
      w->count++;
    } else {
      w = create_entry(words[i], 1);
      HASH_ADD_STR(*map, word, w);
    }

    if (lock)
      pthread_mutex_unlock(lock);
  }
}

/* ---------- thread entry ---------- */
static void *counter_thread_func(void *param) {
  count_thread_args_t *args = (count_thread_args_t *)param;
  add_word_counts_in_chunk(args->map, args->words, args->num_words, args->lock);
  return NULL;
}

/* ---------- sequential (reference) ---------- */
static count_map_t count_words_seq(word_t *words, size_t num_words) {
  count_map_t map = NULL;
  add_word_counts_in_chunk(&map, words, num_words, NULL);
  return map;
}

/* ---------- parallel implementation (Task 2) ---------- */
static count_map_t count_words_parallel(word_t *words, size_t num_words) {
  count_map_t map = NULL;

  pthread_mutex_t count_mutex;
  if (pthread_mutex_init(&count_mutex, NULL) != 0) {
    perror("pthread_mutex_init");
    exit(EXIT_FAILURE);
  }

  pthread_t threads[THREAD_COUNT];
  count_thread_args_t *threads_args[THREAD_COUNT];

  const size_t chunk_size = num_words / THREAD_COUNT;
  const size_t remainder = num_words % THREAD_COUNT;

  /* Launch threads */
  for (size_t i = 0; i < THREAD_COUNT; i++) {
    size_t start_index = i * chunk_size;
    size_t this_count = chunk_size + (i == THREAD_COUNT - 1 ? remainder : 0);

    threads_args[i] =
        pack_args(&map, words + start_index, this_count, &count_mutex);

    int rc =
        pthread_create(&threads[i], NULL, counter_thread_func, threads_args[i]);
    if (rc != 0) {
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }
  }

  /* Join threads & free argument bundles */
  for (size_t i = 0; i < THREAD_COUNT; i++) {
    int rc = pthread_join(threads[i], NULL);
    if (rc != 0) {
      perror("pthread_join");
      exit(EXIT_FAILURE);
    }
    free(threads_args[i]);
  }

  pthread_mutex_destroy(&count_mutex);
  return map;
}

/* ---------- main ---------- */
int main(void) {
  word_t words_in[13] = {"the",  "quick", "brown", "fox", "jumps",
                         "over", "the",   "lazy",  "dog", "the",
                         "the",  "fox",   "brown"};
  const size_t words_in_len = 13;

  count_map_t word_map = NULL;

  /* Use the parallel version for the finished lab. For comparison, the
     sequential version is one line below (leave it commented if you want
     parallel). */
  // word_map = count_words_seq(words_in, words_in_len);
  word_map = count_words_parallel(words_in, words_in_len);

  if (word_map) {
    /* Task 1: sort by word (ascending) before printing */
    HASH_SORT(word_map, sort_func);
    print_counts(word_map);
    delete_table(word_map);
  }

  return 0;
}
