#include <iostream>
#include "lfsr.h"

void lfsr_calculate(uint16_t *reg) {
    uint16_t tmp = *reg;
   	(*reg) >>= 1;
   	uint16_t v = !!(tmp & 1) ^ !!(tmp & 4) ^ !!(tmp & 8) ^ !!(tmp & 32);
   	(*reg) = ((*reg) & ~(1 << 15)) | (v << 15);
}

