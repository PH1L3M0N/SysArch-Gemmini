#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "include/gemmini_testutils.h"

#define M_DIM 128
#define N_DIM 128
#define K_DIM 128
#define REPS  25

#define TILE_I 1
#define TILE_J 1
#define TILE_K 1

#define HW_TILEBLOCK_ROWS 8
#define HW_TILEBLOCK_COLS 8
#define HW_TOTAL_TILES    (HW_TILEBLOCK_ROWS * HW_TILEBLOCK_COLS) 
#define HW_PE_ROWS        64
#define HW_PE_COLS        64
#define HW_TOTAL_PES      (HW_PE_ROWS * HW_PE_COLS)            

static elem_t A[M_DIM][K_DIM] row_align(1);
static elem_t B[K_DIM][N_DIM] row_align(1);

static acc_t C[M_DIM][N_DIM] row_align_acc(1);
static acc_t D[M_DIM][N_DIM] row_align_acc(1);

static inline uint64_t rdcycle64(void) {
  uint64_t c;
  asm volatile ("rdcycle %0" : "=r"(c));
  return c;
}

static inline elem_t init_val(int i, int j) {
  int v = (i * 131 + j * 17 + 7) % 5;
  return (elem_t)(v - 2);
}

static void print_ratio_fixed6(const char* label, uint64_t numer, uint64_t denom) {
  if (denom == 0) { printf("%s: NaN (denom=0)\n", label); return; }
  uint64_t integer = numer / denom;
  uint64_t frac = ((numer % denom) * 1000000ULL) / denom;
  printf("%s: %llu.%06llu\n",
         label,
         (unsigned long long)integer,
         (unsigned long long)frac);
}

static void print_bytes_human(const char* label, uint64_t bytes) {
  uint64_t kib = bytes / 1024ULL;
  uint64_t mib = bytes / (1024ULL * 1024ULL);
  printf("%s: %llu B (%llu KiB, %llu MiB)\n",
         label,
         (unsigned long long)bytes,
         (unsigned long long)kib,
         (unsigned long long)mib);
}

static void print_spad_acc_sizes(void) {
#ifdef DIM
  uint64_t bytes_per_spad_row = (uint64_t)DIM * (uint64_t)sizeof(elem_t);
  uint64_t bytes_per_acc_row  = (uint64_t)DIM * (uint64_t)sizeof(acc_t);

  printf("Bytes/SPAD row = %llu (DIM=%d * sizeof(elem_t)=%llu)\n",
         (unsigned long long)bytes_per_spad_row, DIM,
         (unsigned long long)sizeof(elem_t));

  printf("Bytes/ACC  row = %llu (DIM=%d * sizeof(acc_t)=%llu)\n",
         (unsigned long long)bytes_per_acc_row, DIM,
         (unsigned long long)sizeof(acc_t));

#if defined(SPAD_ROWS)
  uint64_t spad_rows = (uint64_t)SPAD_ROWS;
  printf("SPAD_ROWS=%llu (from SPAD_ROWS macro)\n", (unsigned long long)spad_rows);
#elif defined(BANK_NUM) && defined(BANK_ROWS)
  uint64_t spad_rows = (uint64_t)BANK_NUM * (uint64_t)BANK_ROWS;
  printf("SPAD_ROWS=%llu (derived: BANK_NUM=%d * BANK_ROWS=%d)\n",
         (unsigned long long)spad_rows, BANK_NUM, BANK_ROWS);
#else
  printf("SPAD capacity: cannot compute (need SPAD_ROWS or BANK_NUM & BANK_ROWS)\n");
  uint64_t spad_rows = 0;
#endif

#if defined(SPAD_ROWS) || (defined(BANK_NUM) && defined(BANK_ROWS))
  uint64_t spad_bytes = spad_rows * bytes_per_spad_row;
  print_bytes_human("SPAD capacity", spad_bytes);
#endif

#ifdef ACC_ROWS
  uint64_t acc_rows = (uint64_t)ACC_ROWS;
  uint64_t acc_bytes = acc_rows * bytes_per_acc_row;
  printf("ACC_ROWS=%llu\n", (unsigned long long)acc_rows);
  print_bytes_human("ACC capacity", acc_bytes);
#else
  printf("ACC capacity: cannot compute (ACC_ROWS not defined)\n");
#endif

#else
  printf("Cannot compute sizes: DIM not defined\n");
#endif
}

// Optional: sanity-check whether fixed tiles fit SPAD/ACC for this config
static void print_tile_fit(void) {
#if defined(DIM) && defined(BANK_NUM) && defined(BANK_ROWS) && defined(ACC_ROWS)
  const bool double_buffered = true; // because we use WS below
  const size_t max_spad_rows = double_buffered ? (size_t)(BANK_NUM * BANK_ROWS / 2)
                                               : (size_t)(BANK_NUM * BANK_ROWS);
  const size_t max_acc_rows  = double_buffered ? (size_t)(ACC_ROWS / 2)
                                               : (size_t)ACC_ROWS;

  const size_t spad_rows = (size_t)tiled_matmul_total_spad_rows(TILE_I, TILE_J, TILE_K);
  const size_t acc_rows  = (size_t)tiled_matmul_total_acc_rows(TILE_I, TILE_J);

  printf("SW tiles (mat units): tile_I=%d tile_J=%d tile_K=%d (DIM=%d => elems %d x %d x %d)\n",
         TILE_I, TILE_J, TILE_K, DIM,
         TILE_I * DIM, TILE_J * DIM, TILE_K * DIM);
  printf("Tile footprint: spad_rows=%llu/%llu  acc_rows=%llu/%llu\n",
       (unsigned long long)spad_rows,
       (unsigned long long)max_spad_rows,
       (unsigned long long)acc_rows,
       (unsigned long long)max_acc_rows);

  if (spad_rows > max_spad_rows) {
    printf("WARNING: tile footprint exceeds SPAD rows (reduce TILE_*).\n");
  }
  if (acc_rows > max_acc_rows) {
    printf("WARNING: tile footprint exceeds ACC rows (reduce TILE_I/TILE_J).\n");
  }
#else
  printf("Tile fit check skipped (need DIM, BANK_NUM, BANK_ROWS, ACC_ROWS defined)\n");
#endif
}

static inline void tmma_fixed(size_t I, size_t J, size_t K,
                              const elem_t* A_ptr, const elem_t* B_ptr,
                              const acc_t*  D_ptr, acc_t* C_ptr,
                              size_t strideA_elems, size_t strideB_elems,
                              size_t strideD_elems, size_t strideC_elems) {

  scale_t A_sf = (scale_t)1;
  scale_t B_sf = (scale_t)1;
  scale_acc_t D_sf = (scale_acc_t)1;

  int act = NO_ACTIVATION;
  acc_scale_t scale = (acc_scale_t)1;
  acc_scale_t bert_scale = (acc_scale_t)1;

  bool repeating_bias = false;
  bool transpose_A = false;
  bool transpose_B = false;

  bool full_C = true;  
  bool low_D  = false;
  uint8_t weightA = 1;

  enum tiled_matmul_type_t ttype = WS;

  tiled_matmul(I, J, K,
      A_ptr, B_ptr, (const void*)D_ptr, (void*)C_ptr,
      strideA_elems, strideB_elems, strideD_elems, strideC_elems,
      A_sf, B_sf, D_sf,
      act, scale, bert_scale,
      repeating_bias,
      (size_t)TILE_I, (size_t)TILE_J, (size_t)TILE_K,
      transpose_A, transpose_B,
      full_C, low_D,
      weightA,
      ttype);
}

int main() {
  printf("=== GEMMINI FC BENCH (fixed SW tiling, measured throughput) ===\n");
  printf("Problem: M=%d N=%d K=%d  REPS=%d\n", M_DIM, N_DIM, K_DIM, REPS);

  printf("Hardware SA: %dx%d tile-block mesh (%d tiles total), %dx%d PEs (%d total)\n",
         HW_TILEBLOCK_ROWS, HW_TILEBLOCK_COLS, HW_TOTAL_TILES,
         HW_PE_ROWS, HW_PE_COLS, HW_TOTAL_PES);

#ifdef DIM
  printf("DIM=%d\n", DIM);
#endif
#ifdef BANK_NUM
  printf("BANK_NUM=%d\n", BANK_NUM);
#endif
#ifdef BANK_ROWS
  printf("BANK_ROWS=%d\n", BANK_ROWS);
#endif
#ifdef ACC_ROWS
  printf("ACC_ROWS=%d\n", ACC_ROWS);
#endif

  print_spad_acc_sizes();
  print_tile_fit();

  printf("[phase] init matrices\n");
  for (int i = 0; i < M_DIM; i++)
    for (int k = 0; k < K_DIM; k++)
      A[i][k] = init_val(i, k);

  for (int k = 0; k < K_DIM; k++)
    for (int j = 0; j < N_DIM; j++)
      B[k][j] = init_val(k, j);

  memset(C, 0, sizeof(C));
  memset(D, 0, sizeof(D));

  printf("[phase] flush\n");
  gemmini_flush(0);

  printf("[phase] warmup start\n");
  tmma_fixed((size_t)M_DIM, (size_t)N_DIM, (size_t)K_DIM,
             &A[0][0], &B[0][0],
             &D[0][0], &C[0][0],
             (size_t)K_DIM, (size_t)N_DIM,
             (size_t)N_DIM, (size_t)N_DIM);
  gemmini_fence();
  printf("[phase] warmup done\n");

  uint64_t start = rdcycle64();

  printf("[phase] timed loop start\n");
  for (int r = 0; r < REPS; r++) {
    tmma_fixed((size_t)M_DIM, (size_t)N_DIM, (size_t)K_DIM,
               &A[0][0], &B[0][0],
               &D[0][0], &C[0][0],
               (size_t)K_DIM, (size_t)N_DIM,
               (size_t)N_DIM, (size_t)N_DIM);
  }
  gemmini_fence();
  printf("[phase] timed loop done\n");

  // checksum to prevent dead-code elimination + sanity
  volatile int64_t checksum = 0;
  for (int i = 0; i < M_DIM; i++)
    for (int j = 0; j < N_DIM; j++)
      checksum += (int64_t)C[i][j];
  printf("checksum=%lld\n", (long long)checksum);

  uint64_t end = rdcycle64();

  uint64_t cycles = end - start;
  uint64_t total_macs =
      (uint64_t)M_DIM * (uint64_t)N_DIM * (uint64_t)K_DIM * (uint64_t)REPS;

  printf("Measured cycles: %llu\n", (unsigned long long)cycles);
  printf("Total MACs: %llu\n", (unsigned long long)total_macs);
  print_ratio_fixed6("Measured MACs/cycle", total_macs, cycles);

  print_ratio_fixed6("Measured MACs/cycle/PE (rough)", total_macs, cycles * (uint64_t)HW_TOTAL_PES);

  return 0;
}
