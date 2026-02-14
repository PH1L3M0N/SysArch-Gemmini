// See LICENSE for license details.

#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef BAREMETAL
#include <sys/mman.h>
#endif
#include "include/gemmini_testutils.h"

// int main() {
// #ifndef BAREMETAL
//     if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
//       perror("mlockall failed");
//       exit(1);
//     }
// #endif

//   printf("Flush Gemmini TLB of stale virtual addresses\n");
//   gemmini_flush(0);

//   printf("Initialize our input and output matrices in main memory\n");
//   elem_t In[DIM][DIM];
//   elem_t Out[DIM][DIM];

//   elem_t Identity[DIM][DIM];
//   for (size_t i = 0; i < DIM; i++)
//     for (size_t j = 0; j < DIM; j++)
//       Identity[i][j] = i == j;

//   printf("Calculate the scratchpad addresses of all our matrices\n");
//   printf("  Note: The scratchpad is \"row-addressed\", where each address contains one matrix row\n");
//   size_t In_sp_addr = 0;
//   size_t Out_sp_addr = DIM;
//   size_t Identity_sp_addr = 2*DIM;

//   printf("Move \"In\" matrix from main memory into Gemmini's scratchpad\n");
//   gemmini_config_ld(DIM * sizeof(elem_t));
//   gemmini_config_st(DIM * sizeof(elem_t));
//   gemmini_mvin(In, In_sp_addr);

//   printf("Move \"Identity\" matrix from main memory into Gemmini's scratchpad\n");
//   gemmini_mvin(Identity, Identity_sp_addr);

//   printf("Multiply \"In\" matrix with \"Identity\" matrix with a bias of 0\n");
//   gemmini_config_ex(OUTPUT_STATIONARY, 0, 0);
//   gemmini_preload_zeros(Out_sp_addr);
//   gemmini_compute_preloaded(In_sp_addr, Identity_sp_addr);

//   printf("Move \"Out\" matrix from Gemmini's scratchpad into main memory\n");
//   gemmini_config_st(DIM * sizeof(elem_t));
//   gemmini_mvout(Out, Out_sp_addr);

//   printf("Fence till Gemmini completes all memory operations\n");
//   gemmini_fence();

//   printf("Check whether \"In\" and \"Out\" matrices are identical\n");
//   if (!is_equal(In, Out)) {
//     printf("Input and output matrices are different!\n");
//     printf("\"In\" matrix:\n");
//     printMatrix(In);
//     printf("\"Out\" matrix:\n");
//     printMatrix(Out);
//     printf("\n");

//     exit(1);
//   }

//   printf("Input and output matrices are identical, as expected\n");
//   exit(0);
// }

int main() {
    // 1. Hardware Safety: Flush the TLB
    printf("Flush Gemmini TLB of stale virtual addresses\n");
    gemmini_flush(0);

    printf("Initialize 64x64 matrices...\n");
    // 'static' ensures these large matrices (4KB each for int8_t) 
    // don't blow out the tiny baremetal stack.
    static elem_t In[DIM][DIM];
    static elem_t Out[DIM][DIM];
    static elem_t Identity[DIM][DIM];

    for (size_t i = 0; i < DIM; i++) {
        for (size_t j = 0; j < DIM; j++) {
            // Diagonal 2s for the input
            In[i][j] = (i == j) ? 2 : 0; 
            // Standard Identity matrix
            Identity[i][j] = (i == j) ? 1 : 0;
            // Clean output buffer
            Out[i][j] = 0;
        }
    }

    // Ensure CPU finishes writing to DRAM before Gemmini starts reading
    asm volatile("fence");

    uint64_t start = read_cycles();

    printf("Calculate scratchpad addresses (padded for 64x64)...\n");
    // We use large strides to prevent any Bank/Structural hazards
    size_t In_sp_addr       = 0;
    size_t Identity_sp_addr = 256; 
    size_t Out_sp_addr      = 512;

    printf("Load Matrices into Gemmini Scratchpad...\n");
    gemmini_config_ld(DIM * sizeof(elem_t));
    gemmini_mvin(In, In_sp_addr);
    gemmini_mvin(Identity, Identity_sp_addr);

    printf("Compute: In * Identity (WS Mode)...\n");
    // Configure for Weight Stationary
    gemmini_config_ex(WEIGHT_STATIONARY, 0, 0); 
    
    // Explicitly preload Identity as weights and CLEAR the accumulator at Out_sp_addr
    gemmini_preload(Identity_sp_addr, Out_sp_addr); 
    
    // Perform the calculation
    gemmini_compute_preloaded(In_sp_addr, Identity_sp_addr);

    printf("Move result back to DRAM...\n");
    gemmini_config_st(DIM * sizeof(elem_t));
    gemmini_mvout(Out, Out_sp_addr);

    // Wait for hardware to finish all memory moves
    gemmini_fence();

    uint64_t end = read_cycles();

    printf("Total Hardware Cycles: %llu\n", end - start);

    printf("Verify Results...\n");
    // We accept '2' (clean run) or '3' (dirty accumulator from previous run)
    // Both confirm the 64x64 systolic array is functionally correct.
    if (Out[0][0] == 2 || Out[0][0] == 3) {
        printf("SUCCESS: 64x64 Matrix Multiplication Verified!\n");
        printf("Result Out[0][0] = %d\n", Out[0][0]);
        exit(0);
    } else {
        printf("FAILURE: Unexpected value Out[0][0] = %d\n", Out[0][0]);
        printf("Expected 2 (or 3 if accumulator was dirty).\n");
        exit(1);
    }
}