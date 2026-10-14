#pragma once
#include <stdint.h>
// Persistent per-device secrets. Keep physical UART0 logs private.
void secrets_init(void);
const uint8_t *secrets_key(void); // 32 binary bytes, NEVER transmit to clients
const char *secrets_pop(void);   // 12 hex digits, printed over physical UART0
