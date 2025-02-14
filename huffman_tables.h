#ifndef HUFFMAN_TABLES_H
#define HUFFMAN_TABLES_H

#include <stdint.h>

// Standard Huffman tables for baseline JPEG (Luminance DC)
extern const uint8_t standard_huffman_dc_bits[17];
extern const uint8_t standard_huffman_dc_values[12];

// Standard Huffman tables for baseline JPEG (Luminance AC)
extern const uint8_t standard_huffman_ac_bits[17];
extern const uint16_t standard_huffman_ac_values[162]; // AC tables are larger
extern const uint8_t standard_huffman_ac_values_lengths[162];

#endif