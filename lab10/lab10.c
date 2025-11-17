/* =========================
   ======== client.c =======
   ========================= */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8001
#define BUF_SIZE 1024
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define NUM_MSG 5

static const char *messages[NUM_MSG] = {"Hello", "Apple", "Car", "Green",
                                        "Dog"};

int main() {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    handle_error("connect");
  }

  char buf[BUF_SIZE];
  for (int i = 0; i < NUM_MSG; i++) {
    sleep(1);
    // prepare message (pad destination with NULs)
    strncpy(buf, messages[i], BUF_SIZE);

    if (write(sfd, buf, BUF_SIZE) == -1) {
      handle_error("write");
    } else {
      printf("Sent: %s\n", messages[i]);
    }
  }

  exit(EXIT_SUCCESS);
}

/* =========================
   ======== server.c =======
   ========================= */

/*
Questions to answer at top of server.c:
(You should not need to change client.c)

Understanding the Client:
1) How is the client sending data to the server? What protocol?
   - Via TCP (stream sockets): socket(AF_INET, SOCK_STREAM, 0) + connect() and
write().

2) What data is the client sending to the server?
   - Five fixed-size 1024-byte messages containing the strings:
     "Hello", "Apple", "Car", "Green", "Dog" (each padded in the buffer).

Understanding the Server:
1) Explain the argument that the `run_acceptor` thread is passed as an argument.
   - A pointer to `struct acceptor_args` that holds:
     (a) a `run` flag to control the accept loop,
     (b) a pointer to the shared message list handle,
     (c) a pointer to the mutex protecting that list.

2) How are received messages stored?
   - Each message becomes a dynamically allocated `list_node` whose `data`
     is a malloc’d 1024-byte buffer copy. Nodes are appended to a singly
     linked list via a tail pointer inside `list_handle`. Appends are
     protected by a mutex.

3) What does `main()` do with the received messages?
   - It waits until `MAX_CLIENTS * NUM_MSG_PER_CLIENT` messages arrive
     (checking count under a mutex), then stops the acceptor, joins it,
     walks the list to print “Collected: …”, frees all nodes, and verifies
totals.

4) How are threads used in this sample code?
   - One acceptor thread listens and spawns a per-client worker thread for
     each accepted connection. Each client thread reads messages (non-blocking),
     appends them to the shared list (mutex-protected), and exits when told to
stop.

Explain the use of non-blocking sockets in this lab.
- Why: To make the acceptor loop and per-client reader loops responsive to
  shutdown without getting stuck on blocking accept()/read() calls. We can
  poll and check run flags.
- How: Using fcntl(F_SETFL, flags | O_NONBLOCK) on the listening socket and
  each accepted client socket.
- Which sockets: The listening server socket (sfd) and each per-client socket
(cfd).
*/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define PORT 8001
#define LISTEN_BACKLOG 32
#define MAX_CLIENTS 4
#define NUM_MSG_PER_CLIENT 5

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

struct list_node {
  struct list_node *next;
  void *data;
};

struct list_handle {
  struct list_node *last; // tail pointer
  volatile uint32_t count;
};

struct client_args {
  atomic_bool run;

  int cfd;
  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

struct acceptor_args {
  atomic_bool run;

  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

static int init_server_socket(void) {
  struct sockaddr_in addr;

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    handle_error("bind");
  }

  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    handle_error("listen");
  }

  return sfd;
}

// Set a file descriptor to non-blocking mode
static void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    exit(EXIT_FAILURE);
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl FSETFL");
    exit(EXIT_FAILURE);
  }
}

static void add_to_list(struct list_handle *list_handle,
                        struct list_node *new_node) {
  struct list_node *last_node = list_handle->last;
  last_node->next = new_node;
  list_handle->last = last_node->next;
  list_handle->count++;
}

static int collect_all(struct list_node head) {
  struct list_node *node = head.next; // first node after head
  uint32_t total = 0;

  while (node != NULL) {
    printf("Collected: %s\n", (char *)node->data);
    total++;

    // Free node and advance
    struct list_node *next = node->next;
    free(node->data);
    free(node);
    node = next;
  }

  return (int)total;
}

static void *run_client(void *args) {
  struct client_args *cargs = (struct client_args *)args;
  const int cfd = cargs->cfd;
  set_non_blocking(cfd);

  char msg_buf[BUF_SIZE];

  while (atomic_load(&cargs->run)) {
    ssize_t bytes_read = read(cfd, &msg_buf, BUF_SIZE);
    if (bytes_read == -1) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        perror("Problem reading from socket");
        break;
      }
      // non-blocking: nothing to read right now
      // small sleep avoids hot spinning
      usleep(1000);
    } else if (bytes_read > 0) {
      // Create node with a copy of the data
      struct list_node *new_node = (struct list_node *)malloc(sizeof *new_node);
      if (!new_node) {
        perror("malloc list_node");
        break;
      }
      new_node->next = NULL;
      new_node->data = malloc(BUF_SIZE);
      if (!new_node->data) {
        free(new_node);
        perror("malloc node->data");
        break;
      }
      memcpy(new_node->data, msg_buf, BUF_SIZE);

      // Append safely
      pthread_mutex_lock(cargs->list_lock);
      add_to_list(cargs->list_handle, new_node);
      pthread_mutex_unlock(cargs->list_lock);
    } else {
      // bytes_read == 0: peer closed, exit
      break;
    }
  }

  if (close(cfd) == -1) {
    perror("client thread close");
  }
  return NULL;
}

static void *run_acceptor(void *args) {
  int sfd = init_server_socket();
  set_non_blocking(sfd);

  struct acceptor_args *aargs = (struct acceptor_args *)args;
  pthread_t threads[MAX_CLIENTS];
  struct client_args client_args[MAX_CLIENTS];

  for (int i = 0; i < MAX_CLIENTS; i++) {
    threads[i] = (pthread_t)0;
    memset(&client_args[i], 0, sizeof client_args[i]);
  }

  printf("Accepting clients...\n");

  uint16_t num_clients = 0;
  while (atomic_load(&aargs->run)) {
    if (num_clients < MAX_CLIENTS) {
      int cfd = accept(sfd, NULL, NULL);
      if (cfd == -1) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
          handle_error("accept");
        }
        // nothing to accept right now
        usleep(1000);
      } else {
        printf("Client connected!\n");

        client_args[num_clients].cfd = cfd;
        atomic_store(&client_args[num_clients].run, true);
        client_args[num_clients].list_handle = aargs->list_handle;
        client_args[num_clients].list_lock = aargs->list_lock;

        // Create a new thread to handle the client
        if (pthread_create(&threads[num_clients], NULL, run_client,
                           &client_args[num_clients]) != 0) {
          perror("pthread_create client");
          close(cfd);
        } else {
          num_clients++;
        }
      }
    } else {
      // Already at capacity; give chance to be stopped by main
      usleep(1000);
    }
  }

  printf("Not accepting any more clients!\n");

  // Shutdown and cleanup client threads
  for (int i = 0; i < num_clients; i++) {
    atomic_store(&client_args[i].run, false);
  }
  for (int i = 0; i < num_clients; i++) {
    if (threads[i]) {
      pthread_join(threads[i], NULL);
    }
    // run_client() already closes each cfd
  }

  if (close(sfd) == -1) {
    perror("closing server socket");
  }
  return NULL;
}

int main_server() {
  pthread_mutex_t list_mutex;
  pthread_mutex_init(&list_mutex, NULL);

  // Singly-linked list with a sentinel head node
  struct list_node head = {.next = NULL, .data = NULL};
  struct list_handle list_handle = {
      .last = &head, // tail initially points to head
      .count = 0,
  };

  pthread_t acceptor_thread;
  struct acceptor_args aargs = {
      .run = true,
      .list_handle = &list_handle,
      .list_lock = &list_mutex,
  };
  pthread_create(&acceptor_thread, NULL, run_acceptor, &aargs);

  // Wait until enough messages are received (busy-wait with small sleep)
  const uint32_t target = (uint32_t)(MAX_CLIENTS * NUM_MSG_PER_CLIENT);
  for (;;) {
    pthread_mutex_lock(&list_mutex);
    uint32_t current = list_handle.count;
    pthread_mutex_unlock(&list_mutex);
    if (current >= target)
      break;
    usleep(1000);
  }

  // Stop acceptor and wait for it to clean up its client threads
  atomic_store(&aargs.run, false);
  pthread_join(acceptor_thread, NULL);

  // Verify totals and collect/print/free messages
  if (list_handle.count != target) {
    printf("Not enough messages were received!\n");
    pthread_mutex_destroy(&list_mutex);
    return 1;
  }

  int collected = collect_all(head);
  printf("Collected: %d\n", collected);
  if ((uint32_t)collected != list_handle.count) {
    printf("Not all messages were collected!\n");
    pthread_mutex_destroy(&list_mutex);
    return 1;
  } else {
    printf("All messages were collected!\n");
  }

  pthread_mutex_destroy(&list_mutex);
  return 0;
}
