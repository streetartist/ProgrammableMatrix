#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Board: ESP32-S3-WROOM-1-N16R8. GPIO35-37 reserved by OPI PSRAM.
// IDs 0..19: H2-1..H2-20; 20..39: H6-1..H6-20
// IDs 40..48: matrix EXT1..EXT9 (=GPIO1..GPIO9)
// IDs 49..51: 5V, 3.3V, GND (LOW CURRENT MATRIX ONLY).
#define BOARD_NODES       52u
#define BOARD_NETS         8u
#define BOARD_CHIPS        4u
#define BOARD_IO          18u
#define BOARD_EXT         9u
#define BOARD_POWER5      49u
#define BOARD_POWER33     50u
#define BOARD_GND         51u

// IO index 0..8 is connected to the crossbar (nodes 40..48).
// IO index 9..17 is direct expansion and NOT a crossbar node.
// GPIO10 is permanently reserved for the active-HIGH physical pairing button.
static const int BOARD_IO_GPIOS[BOARD_IO] = {
    1,2,3,4,5,6,7,8,9,
    18,21,38,39,40,41,42,47,48
};

// Active-high pairing button: GPIO10 -> switch -> 3.3V, 10k pull-down -> GND.
#define BOARD_PAIR_BUTTON 10

// Four CH446Q; DAT, CLOCK and RESET common; STB independent.
#define MATRIX_DAT   11
#define MATRIX_CLK   12
#define MATRIX_RST   13
static const int MATRIX_STB[BOARD_CHIPS] = {14,15,16,17};

// Assign each node to chip and x. See README and board mapping in API.
static inline unsigned node_chip(unsigned n) { return n / 16u; }
static inline unsigned node_x(unsigned n) { return n % 16u; }

// Raw mapping in this firmware (ordered by X pad on each chip), matching schematic:
// U2: H2 1..16
// U3: H2 17..20, H6 1..12
// U4: H6 13..20, EXT1..EXT8
// U5: EXT9, 5V, 3.3V, GND, spare
// Logical node -> physical (chip, x); NOT simply n/16 for node>=20.
static inline unsigned mapped_chip(unsigned node) {
    if (node < 16) return 0;
    if (node < 32) return 1;
    if (node < 40) return 2;
    if (node < 48) return 2;
    return 3;
}
static inline unsigned mapped_x(unsigned node) {
    if (node < 16) return node;
    if (node < 20) return node - 16;
    if (node < 32) return node - 16; // H6-1..12 => U3 X4..15
    if (node < 40) return node - 32; // H6-13..20 => U4 X0..7
    if (node < 48) return node - 32; // EXT1..8 => U4 X8..15
    return node - 48;                // EXT9 => U5 X0, power => X1..3
}
static inline int node_is_external_io(unsigned node) {return node >= 40 && node <= 48;}
static inline int io_valid(unsigned id) {return id < BOARD_IO;}

// Routes fixed to at most one Y for each node, individual Y independent.
// No 5V/3.3V/GND mixed on one net. By default power cannot be wired
// directly to any MCU GPIO (even GND: output may otherwise short).
