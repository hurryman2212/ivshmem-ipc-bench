#include "common/common.h"

#include <stdio.h>
#include <stdlib.h>

void debug_validate(void *buffer, size_t size, uint8_t expect) {
  for (int i = 0; i < size; ++i)
    if (((uint8_t *)buffer)[i] != expect) {
      fprintf(stderr, "Validation failed after recv()!\n");
      exit(EXIT_FAILURE);
    }
}
