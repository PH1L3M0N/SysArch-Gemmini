# chipyard/generators/gemmini/software/gemmini-rocc-tests/ucla_parallel_sim/Makefile

include ../Makefrag

# Add your C file name here (without the .c extension)
tests = parallel_grid

tests_baremetal = $(tests:=-baremetal)
runs_baremetal = $(addsuffix .run,$(tests_baremetal))

BENCH_COMMON = $(abs_top_srcdir)/riscv-tests/benchmarks/common
GEMMINI_HEADERS = $(abs_top_srcdir)/include/gemmini.h $(abs_top_srcdir)/include/gemmini_params.h $(abs_top_srcdir)/include/gemmini_testutils.h

CFLAGS := $(CFLAGS) \
    -DPREALLOCATE=1 \
    -DMULTITHREAD=1 \
    -mcmodel=medany \
    -std=gnu99 \
    -O2 \
    -ffast-math \
    -fno-common \
    -fno-builtin-printf \
    -march=rv64gc -Wa,-march=rv64gc \
    -lm \
    -lgcc \
    -I$(abs_top_srcdir)/riscv-tests \
    -I$(abs_top_srcdir)/riscv-tests/env \
    -I$(abs_top_srcdir) \
    -I$(BENCH_COMMON)

CFLAGS_BAREMETAL := \
    $(CFLAGS) \
    -nostdlib \
    -nostartfiles \
    -static \
    -T $(BENCH_COMMON)/test.ld \
    -DBAREMETAL=1

all: $(tests_baremetal)

vpath %.c $(src_dir)

%-baremetal: %.c $(GEMMINI_HEADERS)
	$(CC_BAREMETAL) $(CFLAGS_BAREMETAL) $< $(LFLAGS) -o $@ \
		$(wildcard $(BENCH_COMMON)/*.c) $(wildcard $(BENCH_COMMON)/*.S) $(LIBS)

run-baremetal: $(runs_baremetal)

%-baremetal.run: %-baremetal
	$(RUNNER)$(abs_top_srcdir)/ucla_parallel_sim/$^

clean:
	rm -f $(tests_baremetal)