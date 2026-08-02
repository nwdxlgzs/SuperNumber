CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -Iinclude -Isrc
AR ?= ar
ARFLAGS ?= rcs
# Optional extra flags (e.g. EXTRA_CFLAGS=-DSN_DEBUG_ALLOC for test-debug)
EXTRA_CFLAGS ?=

SRC = \
	src/sn_alloc.c \
	src/sn_ctx.c \
	src/sn_value.c \
	src/sn_int.c \
	src/sn_str.c \
	src/sn_float.c \
	src/sn_float_mp.c \
	src/sn_crypto.c \
	src/sn_math.c \
	src/sn_math_soft.c \
	src/sn_complex.c \
	src/sn_rng.c \
	src/sn_tensor.c \
	src/sn_api.c

OBJ = $(SRC:.c=.o)

TEST_SRC = tests/test_runner.c tests/test_alloc.c tests/test_int.c tests/test_float.c \
	tests/test_crypto.c tests/test_math.c tests/test_rng.c tests/test_extra.c tests/test_complex.c tests/test_cross.c tests/test_tensor.c

.PHONY: all clean test test-debug lib example bench

all: lib

lib: libsn.a

libsn.a: $(OBJ)
	$(AR) $(ARFLAGS) $@ $(OBJ)

src/%.o: src/%.c include/sn.h src/internal/sn_impl.h
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@

tests/test_runner.exe tests/test_runner: $(TEST_SRC) libsn.a
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(TEST_SRC) -L. -lsn -lm -o $@

test: lib
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(TEST_SRC) -L. -lsn -lm -o tests/test_runner
	./tests/test_runner

# Rebuild with SN_DEBUG_ALLOC (Windows-safe: avoid nested CFLAGS parsing)
test-debug:
	$(MAKE) clean
	$(MAKE) test EXTRA_CFLAGS=-DSN_DEBUG_ALLOC

example: lib examples/basic.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) examples/basic.c -L. -lsn -lm -o examples/basic

bench: lib bench/bench_sn.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) bench/bench_sn.c -L. -lsn -lm -o bench/bench_sn
	./bench/bench_sn

clean:
	rm -f $(OBJ) libsn.a tests/test_runner tests/test_runner.exe examples/basic examples/basic.exe bench/bench_sn bench/bench_sn.exe


# Optional: hide flat symbols on ELF shared builds (GCC/Clang).
# On Windows/MinGW visibility attrs are no-ops; prefer not exporting flat from a DLL.
.PHONY: hide-flat shared
hide-flat:
	$(MAKE) clean
	$(MAKE) lib EXTRA_CFLAGS="$(EXTRA_CFLAGS) -fvisibility=hidden -DSN_HIDE_FLAT"
	$(MAKE) test CC=$(CC) EXTRA_CFLAGS="$(EXTRA_CFLAGS) -fvisibility=hidden -DSN_HIDE_FLAT"

# Example shared library (Unix-like). Flat remain hidden when SN_HIDE_FLAT+ELF.
shared: $(SRC) include/sn.h
	$(CC) -shared -fPIC -fvisibility=hidden -DSN_HIDE_FLAT $(CFLAGS) $(EXTRA_CFLAGS) -o libsn.so $(SRC) -lm

# Optional IEEE softfp64 oracle (Bellard softfp in playground/libbf). No __int128 required.
.PHONY: softfp-probe softfp-lib
softfp-lib:
	$(CC) -O2 -Iplayground/libbf -c playground/libbf/softfp.c -o playground/libbf/softfp.o
	$(CC) -O2 -Iplayground/libbf -c playground/libbf/cutils.c -o playground/libbf/cutils.o

softfp-probe: lib softfp-lib
	$(CC) -O2 -Iinclude -Isrc -Iplayground/libbf tests/_softfp_probe.c playground/libbf/softfp.o playground/libbf/cutils.o -L. -lsn -lm -o tests/_softfp_probe
	./tests/_softfp_probe

# Optional multiprec transcendental vs Bellard libbf (playground may use __int128).
.PHONY: libbf-math-probe libbf-lib
libbf-lib:
	$(CC) -O2 -Iplayground/libbf -c playground/libbf/libbf.c -o playground/libbf/libbf.o
	$(CC) -O2 -Iplayground/libbf -c playground/libbf/cutils.c -o playground/libbf/cutils.o


.PHONY: libbf-arith-probe
libbf-arith-probe: lib libbf-lib
	$(CC) -O2 -Iinclude -Isrc -Iplayground/libbf tests/_libbf_arith_probe.c playground/libbf/libbf.o playground/libbf/cutils.o -L. -lsn -lm -o tests/_libbf_arith_probe
	./tests/_libbf_arith_probe

libbf-math-probe: lib libbf-lib
	$(CC) -O2 -Iinclude -Isrc -Iplayground/libbf tests/_libbf_math_probe.c playground/libbf/libbf.o playground/libbf/cutils.o -L. -lsn -lm -o tests/_libbf_math_probe
	./tests/_libbf_math_probe

.PHONY: libbf-str-probe
libbf-str-probe: lib libbf-lib
	$(CC) -O2 -Iinclude -Isrc -Iplayground/libbf tests/_libbf_str_probe.c playground/libbf/libbf.o playground/libbf/cutils.o -L. -lsn -lm -o tests/_libbf_str_probe
	./tests/_libbf_str_probe


.PHONY: erf-mp-res-probe
erf-mp-res-probe: lib
	$(CC) -O2 -Iinclude -Isrc tests/_erf_mp_res_probe.c -L. -lsn -lm -o tests/_erf_mp_res_probe
	./tests/_erf_mp_res_probe

.PHONY: agm-thr-probe
agm-thr-probe: lib libbf-lib
	$(CC) -O2 -Iinclude -Isrc -Iplayground/libbf -DSN_SOFT_LOG_AGM_MIN_M=$(or $(AGM_THR),200) tests/_agm_thr_probe.c playground/libbf/libbf.o playground/libbf/cutils.o -L. -lsn -lm -o tests/_agm_thr_probe
	./tests/_agm_thr_probe

.PHONY: libbf-mp-res-probe
libbf-mp-res-probe: lib libbf-lib
	$(CC) -O2 -Iinclude -Isrc -Iplayground/libbf tests/_libbf_mp_res_probe.c playground/libbf/libbf.o playground/libbf/cutils.o -L. -lsn -lm -o tests/_libbf_mp_res_probe
	./tests/_libbf_mp_res_probe


# Optional integer vs mini-gmp (vendored GMP source: playground/gmp-6.3.0).
.PHONY: gmp-int-probe mini-gmp-lib
mini-gmp-lib:
	$(CC) -O2 -Iplayground/gmp-6.3.0/mini-gmp -c playground/gmp-6.3.0/mini-gmp/mini-gmp.c -o playground/gmp-6.3.0/mini-gmp/mini-gmp.o

gmp-int-probe: lib mini-gmp-lib
	$(CC) -O2 -Iinclude -Isrc -Iplayground/gmp-6.3.0/mini-gmp tests/_gmp_int_probe.c playground/gmp-6.3.0/mini-gmp/mini-gmp.o -L. -lsn -lm -o tests/_gmp_int_probe
	./tests/_gmp_int_probe


.PHONY: specials-mp-res-probe
specials-mp-res-probe: lib
	$(CC) -O2 -Iinclude -Isrc tests/_specials_mp_res_probe.c -L. -lsn -lm -o tests/_specials_mp_res_probe
	./tests/_specials_mp_res_probe

.PHONY: highorder-mp-res-probe
highorder-mp-res-probe: lib
	$(CC) -O2 -Iinclude -Isrc tests/_highorder_mp_res_probe.c -L. -lsn -lm -o tests/_highorder_mp_res_probe
	./tests/_highorder_mp_res_probe

.PHONY: polygamma-mp-res-probe
polygamma-mp-res-probe: lib
	$(CC) -O2 -Iinclude -Isrc tests/_polygamma_mp_res_probe.c -L. -lsn -lm -o tests/_polygamma_mp_res_probe
	./tests/_polygamma_mp_res_probe
