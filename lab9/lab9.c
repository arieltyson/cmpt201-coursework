/*

1. What is the address of the server it is trying to connect to (IP address and
port number).
   -> 127.0.0.1:8000 (from ADDR "127.0.0.1" and PORT 8000; connect() uses
these).

2. Is it UDP or TCP? How do you know?
   -> TCP. socket(AF_INET, SOCK_STREAM, 0) creates a TCP stream socket, and the
code uses connect().

3. The client is going to send some data to the server. Where does it get this
data from? How can you tell in the code?
   -> From standard input (the terminal). It calls read(STDIN_FILENO, buf,
BUF_SIZE) inside a loop, then writes that data to the socket with write(sfd,
buf, num_read).

4. How does the client program end? How can you tell that in the code?
   -> The read loop stops when read() returns <= 1 (EOF or a single byte like
just '\n' by this lab’s rule). After the loop, if num_read == -1 it errors;
otherwise it closes the socket and exits(EXIT_SUCCESS).
*/

// client.c

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8000
#define BUF_SIZE 64
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

int main(void) {
  struct sockaddr_in addr;
  int sfd;
  ssize_t num_read;
  char buf[BUF_SIZE];

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  int res = connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
  if (res == -1) {
    handle_error("connect");
  }

  while ((num_read = read(STDIN_FILENO, buf, BUF_SIZE)) > 1) {
    if (write(sfd, buf, num_read) != num_read) {
      handle_error("write");
    }
    printf("Just sent %zd bytes.\n", num_read);
  }

  if (num_read == -1) {
    handle_error("read");
  }

  close(sfd);
  exit(EXIT_SUCCESS);
}

// server.c

#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 64
#define PORT 8000
#define LISTEN_BACKLOG 32

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

/* --------- Shared state (protected by mutexes) --------- */
static int total_message_count = 0; /* incremented on every message */
static int client_id_counter = 1;   /* monotonically increasing IDs */

static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t client_id_mutex = PTHREAD_MUTEX_INITIALIZER;

/* --------- Per-client argument bundle --------- */
struct client_info {
  int cfd;       /* client socket fd returned by accept() */
  int client_id; /* assigned client id */
};

/* Trim a single trailing '\n' (nice for printing). */
static inline void chomp(char *s) {
  size_t n = strlen(s);
  if (n && s[n - 1] == '\n')
    s[n - 1] = '\0';
}

static void *handle_client(void *arg) {
  struct client_info *info = (struct client_info *)arg;
  const int cfd = info->cfd;
  const int cid = info->client_id;

  char buf[BUF_SIZE];
  ssize_t nread;

  /* Main receive loop: read() blocks until data or EOF. */
  for (;;) {
    nread = read(cfd, buf, sizeof(buf));
    if (nread > 0) {
      /* Null-terminate for safe printing; strip newline for nicer logs. */
      char msg[BUF_SIZE + 1];
      if ((size_t)nread > sizeof(msg) - 1)
        nread = (ssize_t)sizeof(msg) - 1;
      memcpy(msg, buf, (size_t)nread);
      msg[nread] = '\0';
      chomp(msg);

      /* Bump global message counter under mutex. */
      int current_total = 0;
      if (pthread_mutex_lock(&count_mutex) != 0) {
        /* If locking fails, we still try to proceed without crashing. */
      }
      total_message_count++;
      current_total = total_message_count;
      if (pthread_mutex_unlock(&count_mutex) != 0) {
      }

      /* Log in the required format. */
      printf("Msg #%4d; Client ID %d: %s\n", current_total, cid, msg);
      fflush(stdout);
      continue;
    }

    if (nread == 0) {
      /* Peer performed an orderly shutdown. */
      break;
    }

    /* nread < 0 ⇒ error */
    if (errno == EINTR) {
      /* Interrupted by a signal: retry. */
      continue;
    } else {
      perror("read");
      break;
    }
  }

  /* Cleanup this client. */
  if (close(cfd) == -1) {
    perror("close(client)");
  }
  printf("Ending thread for client %d\n", cid);
  fflush(stdout);

  /* Free the dynamically allocated arg bundle. */
  free(info);
  return NULL;
}

int main(void) {
  signal(SIGPIPE, SIG_IGN);

  /* --- Create, bind, listen --- */
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    handle_error("socket");

  /* Allow quick restart on the same port (helps during dev). */
  int opt = 1;
  if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
    handle_error("setsockopt(SO_REUSEADDR)");
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    handle_error("bind");
  }
  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    handle_error("listen");
  }

  /* --- Accept loop: one thread per client --- */
  for (;;) {
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    int cfd = accept(sfd, (struct sockaddr *)&peer, &peer_len);
    if (cfd == -1) {
      if (errno == EINTR)
        continue; /* interrupted, retry */
      handle_error("accept");
    }

    /* Assign a new client id under mutex. */
    int cid;
    if (pthread_mutex_lock(&client_id_mutex) == 0) {
      cid = client_id_counter++;
      pthread_mutex_unlock(&client_id_mutex);
    } else {
      /* If lock fails, fall back to a best-effort id. */
      cid = -1;
    }

    /* Allocate per-client argument bundle. */
    struct client_info *info = (struct client_info *)malloc(sizeof(*info));
    if (!info) {
      perror("malloc(client_info)");
      close(cfd);
      continue;
    }
    info->cfd = cfd;
    info->client_id = cid;

    /* Announce new client (matches sample output). */
    printf("New client created! ID %d on socket FD %d\n", cid, cfd);
    fflush(stdout);

    /* Spawn and detach the handler thread. */
    pthread_t tid;
    int rc = pthread_create(&tid, NULL, handle_client, info);
    if (rc != 0) {
      errno = rc;
      perror("pthread_create");
      close(cfd);
      free(info);
      continue;
    }
    pthread_detach(tid);
  }

  if (close(sfd) == -1)
    handle_error("close(server)");
  return 0;
}
