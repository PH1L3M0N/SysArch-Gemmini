// chipyad/gemmini/software/gemmini-rocc-tests/ucla_parallel_sim/parallel_grid.c

#include <stdio.h>
#include <stdint.h>
#include "include/gemmini.h"
#include "include/gemmini_testutils.h"

int main() {
    printf("UCLA 64x64: Starting SILENT 49-Tile Stress Test...\n");

    static elem_t input_tiles[49][64][64] row_align(1);
    static elem_t weight_tiles[49][64][64] row_align(1);
    
    gemmini_flush(0);
    gemmini_fence();

    uint64_t start = read_cycles();

    // Loop with NO printf inside
    for (int i = 0; i < 49; i++) {
        int b_idx = i % 10; 

        uint32_t s_in  = b_idx * 64;
        uint32_t s_w   = (b_idx + 10) * 64;
        uint32_t s_out = (b_idx + 20) * 64;

        gemmini_config_ld(64 * sizeof(elem_t));
        gemmini_config_ex(0, 0, 0); 
        
        gemmini_mvin(input_tiles[i], s_in);
        gemmini_mvin(weight_tiles[i], s_w);
        gemmini_preload_zeros(s_out);

        gemmini_extended_compute_preloaded(s_in, s_w, 64, 64, 64, 64);

        if (i % 10 == 9) gemmini_fence();
    }

    gemmini_fence(); 
    uint64_t end = read_cycles();

    // Final Performance Data
    uint64_t cycles = end - start;
    uint64_t total_ops = 49L * 2L * 64L * 64L * 64L; 
    float ops_per_cycle = (float)total_ops / cycles;

    printf("\n--- Results ---\n");
    printf("Total Hardware Cycles: %llu\n", cycles);
    printf("Throughput: %.2f Ops/Cycle\n", ops_per_cycle);
    printf("SUCCESS: 49 Tiles completed without UART timeout!\n");

    return 0;
}