#include <cassert>
#include <stdio.h>
#include <string.h>

int main() {
  char src[20] = "Hello, World!";
  char dst[20];

  memset(dst, 'a', sizeof(dst));

  // +1 to include '\0'
  assert(strlen(src) + 1 <= strlen(dst));
  memcpy(dst, src, strlen(src) + 1);

  printf("%s", dst);
  return 0;
}
