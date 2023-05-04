#ifndef IPC_BENCH_COMMON_H
#define IPC_BENCH_COMMON_H

#include "common/arguments.h"
#include "common/benchmarks.h"
#include "common/utility.h"

#include <stddef.h>
#include <stdint.h>

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

void debug_validate(void *buffer, size_t size, uint8_t expect);

/* For client -> server */
#define CTS_BITS_01010101 0x55
/* For server -> client */
#define STC_BITS_10101010 0xAA

#endif /* IPC_BENCH_COMMON_H */
