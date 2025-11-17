#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RED "\e[9;31m"
#define GRN "\e[0;32m"
#define CRESET "\e[0m"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

static size_t read_all_bytes(const char *filename, void *buffer,
                             size_t buffer_size) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    handle_error("fopen");
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    handle_error("fseek");
  }
  long file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    handle_error("ftell");
  }
  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    handle_error("fseek");
  }

  if ((size_t)file_size > buffer_size) {
    fclose(file);
    handle_error("file too large for buffer");
  }

  size_t nread = fread(buffer, 1, (size_t)file_size, file);
  if (nread != (size_t)file_size) {
    fclose(file);
    handle_error("fread");
  }

  fclose(file);
  return nread;
}

static void print_file(const char *filename, const char *color) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    handle_error("fopen");
  }

  printf("%s", color);
  char line[256];
  while (fgets(line, sizeof(line), file)) {
    fputs(line, stdout);
  }
  fclose(file);
  printf(CRESET);
}

/*
  Verify that the file `message_path` matches the signature `sign_path`
  using `pubkey`.

  Returns:
     1 : Message matches signature
     0 : Signature did not verify (verification failed)
    -1 : Error occurred (unknown authenticity)
*/
static int verify(const char *message_path, const char *sign_path,
                  EVP_PKEY *pubkey) {
#define MAX_FILE_SIZE 512
  unsigned char message[MAX_FILE_SIZE];
  unsigned char signature[MAX_FILE_SIZE];

  // Load message and signature bytes
  size_t msg_len = read_all_bytes(message_path, message, sizeof(message));
  size_t sig_len = read_all_bytes(sign_path, signature, sizeof(signature));

  // Create and initialize a digest verification context
  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
  if (!mdctx) {
    // Allocation failure
    return -1;
  }

  int ok = -1; // default to "error / unknown"
  // Use SHA-256 as the digest; public key is already loaded
  if (EVP_DigestVerifyInit(mdctx, /*pctx*/ NULL, EVP_sha256(), /*engine*/ NULL,
                           pubkey) != 1) {
    ERR_print_errors_fp(stderr);
    goto cleanup;
  }

  if (EVP_DigestVerifyUpdate(mdctx, message, msg_len) != 1) {
    ERR_print_errors_fp(stderr);
    goto cleanup;
  }

  // Finalize: returns 1 (match), 0 (verify fail), or negative (error)
  {
    int r = EVP_DigestVerifyFinal(mdctx, signature, sig_len);
    if (r == 1) {
      ok = 1; // verified
    } else if (r == 0) {
      ok = 0; // signature does not verify
    } else {
      ERR_print_errors_fp(stderr);
      ok = -1; // error path
    }
  }

cleanup:
  EVP_MD_CTX_free(mdctx);
  return ok;
}

int main(void) {
  // File paths
  const char *message_files[] = {"message1.txt", "message2.txt",
                                 "message3.txt"};
  const char *signature_files[] = {"signature1.sig", "signature2.sig",
                                   "signature3.sig"};

  // Load the public key (PEM_read_PUBKEY expects a FILE* to the PEM)
  EVP_PKEY *pubkey = NULL;
  {
    FILE *pf = fopen("public_key.pem", "r");
    if (!pf) {
      handle_error("fopen public_key.pem");
    }
    pubkey = PEM_read_PUBKEY(pf, /*x*/ NULL, /*cb*/ NULL, /*u*/ NULL);
    fclose(pf);
    if (!pubkey) {
      // Print OpenSSL error stack to help debugging
      ERR_print_errors_fp(stderr);
      fprintf(stderr, "Failed to read public key.\n");
      exit(EXIT_FAILURE);
    }
  }

  // Verify each message/signature pair
  for (int i = 0; i < 3; i++) {
    printf("... Verifying message %d ...\n", i + 1);
    int result = verify(message_files[i], signature_files[i], pubkey);

    if (result < 0) {
      printf("Unknown authenticity of message %d\n", i + 1);
      print_file(message_files[i], CRESET);
    } else if (result == 0) {
      printf("Do not trust message %d!\n", i + 1);
      print_file(message_files[i], RED);
    } else {
      printf("Message %d is authentic!\n", i + 1);
      print_file(message_files[i], GRN);
    }
  }

  EVP_PKEY_free(pubkey);
  return 0;
}
