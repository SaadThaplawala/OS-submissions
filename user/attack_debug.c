#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/riscv.h"
#include "user/user.h"

#define PAGES_TO_ALLOC   128
#define PAGE             4096
#define MIN_SECRET_LEN   8    // lab secret length minimum
#define MAX_TRIES        6
#define MAX_SECRET_LEN   64

static int is_alnum(char c) {
  if ('0' <= c && c <= '9') return 1;
  if ('A' <= c && c <= 'Z') return 1;
  if ('a' <= c && c <= 'z') return 1;
  return 0;
}

int
main(void)
{
  // set exact_search to 1 if you want to search directly for "xyzzy"
  int exact_search = 1;  // set to 1 while testing secret xyzzy
  const char *needle = "xyzzy";
  int needle_len = 5;

  for (int run = 0; run < MAX_TRIES; run++) {
    void *mem = sbrk(PAGE * PAGES_TO_ALLOC);
    if (mem == (void*)-1) exit(1);
    unsigned char *buf = (unsigned char*)mem;
    int total = PAGE * PAGES_TO_ALLOC;

    printf("sbrk returned base 0x%lx\n", (unsigned long)buf);

    int idx = 0;
    while (idx < total) {
      unsigned char cur = buf[idx];
      unsigned char prev = (idx == 0) ? 0 : buf[idx - 1];
      if (!is_alnum((char)cur) || is_alnum((char)prev)) { idx++; continue; }

      int k = idx;
      while (k < total && (k - idx) < MAX_SECRET_LEN && is_alnum((char)buf[k])) k++;
      int length = k - idx;
      if (length < MIN_SECRET_LEN) { idx = k; continue; }

      unsigned char next = (k < total) ? buf[k] : 0;
      if (next != '\0' && is_alnum((char)next)) { idx = k; continue; }

      // copy candidate safely
      char out[MAX_SECRET_LEN + 1];
      int to_copy = length;
      if (to_copy > MAX_SECRET_LEN) to_copy = MAX_SECRET_LEN;
      for (int m = 0; m < to_copy; m++) out[m] = (char)buf[idx + m];
      out[to_copy] = '\0';

      // exact search helper (useful while testing)
      if (exact_search && to_copy >= needle_len) {
        for (int p = 0; p + needle_len <= to_copy; p++) {
          int match = 1;
          for (int q = 0; q < needle_len; q++) {
            if (out[p+q] != needle[q]) { match = 0; break; }
          }
          if (match) {
            printf("EXACT FOUND at addr 0x%lx idx=%d len=%d: %s\n",
                   (unsigned long)buf + idx + p, idx + p, needle_len, needle);
            exit(0);
          }
        }
      }

      // pretty print candidate summary
      printf("FOUND at addr 0x%lx idx=%d len=%d: %s\n",
             (unsigned long)buf + idx, idx, length, out);

      // print hex + ASCII context (~32 bytes centered on the string)
      int start = idx >= 16 ? idx - 16 : 0;
      int finish = (idx + length + 16 < total) ? idx + length + 16 : total;
      printf("HEX:");
      for (int b = start; b < finish; b++) {
        // xv6 printf supports %x, not width specifiers like %02x
        printf(" %x", (unsigned)buf[b] & 0xff);
      }
      printf("\nASCII:");
      for (int b = start; b < finish; b++) {
        unsigned char ch = buf[b];
        if (ch >= 32 && ch < 127) printf("%c", ch); else printf(".");
      }
      printf("\n");

      printf("page_no=%d (offset in page=%d)\n", idx / PAGE, idx % PAGE);

      idx = k;
    }
  }

  printf("done: no more candidates\n");
  exit(0);
}
