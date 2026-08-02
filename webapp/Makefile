EMCC ?= emcc
ROOT := ..
OUT_MJS := snlab.mjs
SINGLE_SHELL := shell-single.generated.html
SINGLE_OUT := snlab-single.html

LIB_SRCS := \
	$(ROOT)/src/sn_alloc.c \
	$(ROOT)/src/sn_ctx.c \
	$(ROOT)/src/sn_value.c \
	$(ROOT)/src/sn_int.c \
	$(ROOT)/src/sn_str.c \
	$(ROOT)/src/sn_float.c \
	$(ROOT)/src/sn_float_mp.c \
	$(ROOT)/src/sn_crypto.c \
	$(ROOT)/src/sn_math.c \
	$(ROOT)/src/sn_math_soft.c \
	$(ROOT)/src/sn_complex.c \
	$(ROOT)/src/sn_rng.c \
	$(ROOT)/src/sn_tensor.c \
	$(ROOT)/src/sn_api.c \
	$(ROOT)/web/sn_web_bridge.c

CFLAGS_EM := -std=c99 -O2 -I$(ROOT)/include -I$(ROOT)/src \
	-sALLOW_MEMORY_GROWTH=1 \
	-sINVOKE_RUN=0 \
	-sEXIT_RUNTIME=0 \
	-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToUTF8,lengthBytesUTF8,getValue,setValue \
	-sEXPORTED_FUNCTIONS=_snw_create,_snw_destroy,_snw_last_error,_snw_clear_error,_snw_set_format,_snw_new_value,_snw_free_value,_snw_set_f64,_snw_set_from_str,_snw_set_i64,_snw_set_int,_snw_set_float,_snw_set_i64_width,_snw_shift,_snw_crypto,_snw_copy,_snw_cast,_snw_seed_rng,_snw_random_u64,_snw_random_u64_mod,_snw_to_str,_snw_free_str,_snw_to_f64,_snw_kind,_snw_flags,_snw_clear_flags,_snw_unary,_snw_binary,_snw_ternary,_snw_cmp,_snw_version,_snw_new_cplx,_snw_free_cplx,_snw_set_cplx_d,_snw_cplx_set_reim,_snw_cplx_to_str,_snw_cplx_unary,_snw_cplx_binary,_snw_cplx_abs,_snw_cplx_arg,_snw_cplx_re,_snw_cplx_im,_snw_cplx_from_polar,_snw_new_tensor,_snw_free_tensor,_snw_tensor_from_str,_snw_tensor_to_str,_snw_tensor_dims,_snw_tensor_copy,_snw_tensor_unary,_snw_tensor_binary,_snw_tensor_scale,_snw_tensor_rms_norm,_snw_tensor_layer_norm,_snw_tensor_sin_pe,_snw_tensor_rope,_snw_tensor_reshape,_snw_tensor_attention_sdp,_snw_tensor_get_f64,_malloc,_free

.PHONY: all clean single-html smoke shell

all: $(OUT_MJS)

app.js: tools/assemble-app.js js/01-catalog.js js/02-runtime.js js/03-graph.js js/04-palette.js js/05-eval.js js/06-export.js js/07-ui.js
	node tools/assemble-app.js

$(OUT_MJS): $(LIB_SRCS) $(ROOT)/include/sn.h $(ROOT)/include/sn_flat.h $(ROOT)/web/sn_web_bridge.c
	$(EMCC) $(LIB_SRCS) $(CFLAGS_EM) \
		-sMODULARIZE=1 \
		-sEXPORT_NAME=createSnLab \
		-sENVIRONMENT=web,worker,node \
		-o $(OUT_MJS)

shell: $(SINGLE_SHELL)

$(SINGLE_SHELL): index.html styles.css app.js tools/build-single-shell.js tools/assemble-app.js js/01-catalog.js
	node tools/assemble-app.js
	node tools/build-single-shell.js

single-html: $(SINGLE_OUT)

$(SINGLE_OUT): $(LIB_SRCS) $(ROOT)/include/sn.h $(ROOT)/include/sn_flat.h $(SINGLE_SHELL)
	$(EMCC) $(LIB_SRCS) $(CFLAGS_EM) \
		-sWASM=0 \
		-sSINGLE_FILE=1 \
		-sENVIRONMENT=web \
		--shell-file $(SINGLE_SHELL) \
		-o $(SINGLE_OUT)

smoke: $(OUT_MJS)
	node _smoke.mjs

clean:
	rm -f snlab.mjs snlab.wasm $(SINGLE_SHELL) $(SINGLE_OUT)
