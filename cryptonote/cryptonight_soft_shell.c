// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
// Portions Copyright (c) 2018 The Monero developers
// Portions Copyright (c) 2018 The TurtleCoin Developers

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "crypto/oaes_lib.h"
#include "crypto/c_keccak.h"
#include "crypto/c_groestl.h"
#include "crypto/c_blake256.h"
#include "crypto/c_jh.h"
#include "crypto/c_skein.h"
#include "crypto/int-util.h"
#include "crypto/hash-ops.h"
#include "crypto/variant2_int_sqrt.h"

#if defined(_MSC_VER)
#include <malloc.h>
#endif

#define AES_BLOCK_SIZE  16
#define AES_KEY_SIZE    32
#define INIT_SIZE_BLK   8
#define INIT_SIZE_BYTE  (INIT_SIZE_BLK * AES_BLOCK_SIZE)

extern int aesb_single_round(const uint8_t *in, uint8_t*out, const uint8_t *expandedKey);
extern int aesb_pseudo_round(const uint8_t *in, uint8_t *out, const uint8_t *expandedKey);

static void do_soft_shell_blake_hash(const void* input, size_t len, char* output) {
    blake256_hash((uint8_t*)output, input, len);
}

void do_soft_shell_groestl_hash(const void* input, size_t len, char* output) {
    groestl(input, len * 8, (uint8_t*)output);
}

static void do_soft_shell_jh_hash(const void* input, size_t len, char* output) {
    int r = jh_hash(HASH_SIZE * 8, input, 8 * len, (uint8_t*)output);
    assert(SUCCESS == r);
}

static void do_soft_shell_skein_hash(const void* input, size_t len, char* output) {
    int r = c_skein_hash(8 * HASH_SIZE, input, 8 * len, (uint8_t*)output);
    assert(SKEIN_SUCCESS == r);
}

static void (* const extra_hashes[4])(const void *, size_t, char *) = {
    do_soft_shell_blake_hash, do_soft_shell_groestl_hash, do_soft_shell_jh_hash, do_soft_shell_skein_hash
};

static inline size_t e2i(const uint8_t* a, size_t count) {
    return (*((uint64_t*) a) / AES_BLOCK_SIZE) & (count - 1);
}

static void mul(const uint8_t* a, const uint8_t* b, uint8_t* res) {
    ((uint64_t*) res)[1] = mul128(((uint64_t*) a)[0], ((uint64_t*) b)[0], (uint64_t*) res);
}

static void sum_half_blocks(uint8_t* a, const uint8_t* b) {
    uint64_t a0, a1, b0, b1;
    a0 = SWAP64LE(((uint64_t*) a)[0]);
    a1 = SWAP64LE(((uint64_t*) a)[1]);
    b0 = SWAP64LE(((uint64_t*) b)[0]);
    b1 = SWAP64LE(((uint64_t*) b)[1]);
    a0 += b0;
    a1 += b1;
    ((uint64_t*) a)[0] = SWAP64LE(a0);
    ((uint64_t*) a)[1] = SWAP64LE(a1);
}

static inline void copy_block(uint8_t* dst, const uint8_t* src) {
    ((uint64_t*) dst)[0] = ((uint64_t*) src)[0];
    ((uint64_t*) dst)[1] = ((uint64_t*) src)[1];
}

static void swap_blocks(uint8_t* a, uint8_t* b) {
    size_t i;
    uint8_t t;
    for (i = 0; i < AES_BLOCK_SIZE; i++) {
        t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

static inline void xor_blocks(uint8_t* a, const uint8_t* b) {
    ((uint64_t*) a)[0] ^= ((uint64_t*) b)[0];
    ((uint64_t*) a)[1] ^= ((uint64_t*) b)[1];
}

static inline void xor_blocks_dst(const uint8_t* a, const uint8_t* b, uint8_t* dst) {
    ((uint64_t*) dst)[0] = ((uint64_t*) a)[0] ^ ((uint64_t*) b)[0];
    ((uint64_t*) dst)[1] = ((uint64_t*) a)[1] ^ ((uint64_t*) b)[1];
}

void cryptonight_soft_shell_hash(const char* input, char* output, uint32_t len, uint32_t scratchpad, uint32_t iterations) {
    uint32_t MEMORY = scratchpad;
    uint32_t ITER = iterations;
    uint32_t ITER_DIV = iterations / 2;
    uint32_t CN_INIT = MEMORY / INIT_SIZE_BYTE;
    uint32_t CN_AES_INIT = MEMORY / AES_BLOCK_SIZE;

    uint8_t *long_state = (uint8_t *)malloc(MEMORY);
    if (long_state == NULL) {
        fprintf(stderr, "Failed to allocate memory for soft shell\n");
        _exit(1);
    }

    union cn_slow_hash_state {
        union hash_state hs;
        struct {
            uint8_t k[64];
            uint8_t init[INIT_SIZE_BYTE];
        };
    } state;

    uint8_t text[INIT_SIZE_BYTE];
    uint8_t a[AES_BLOCK_SIZE];
    uint8_t b[AES_BLOCK_SIZE];
    uint8_t c[AES_BLOCK_SIZE];
    uint8_t aes_key[AES_KEY_SIZE];
    oaes_ctx* aes_ctx;

    hash_process(&state.hs, (const uint8_t*) input, len);
    memcpy(text, state.init, INIT_SIZE_BYTE);
    memcpy(aes_key, state.hs.b, AES_KEY_SIZE);
    aes_ctx = (oaes_ctx*) oaes_alloc();
    size_t i, j;

    oaes_key_import_data(aes_ctx, aes_key, AES_KEY_SIZE);
    for (i = 0; i < CN_INIT; i++) {
        for (j = 0; j < INIT_SIZE_BLK; j++) {
            aesb_pseudo_round(&text[AES_BLOCK_SIZE * j],
                    &text[AES_BLOCK_SIZE * j],
                    aes_ctx->key->exp_data);
        }
        memcpy(&long_state[i * INIT_SIZE_BYTE], text, INIT_SIZE_BYTE);
    }

    for (i = 0; i < 16; i++) {
        a[i] = state.k[i] ^ state.k[32 + i];
        b[i] = state.k[16 + i] ^ state.k[48 + i];
    }

    for (i = 0; i < ITER_DIV; i++) {
        j = e2i(a, CN_AES_INIT);
        aesb_single_round(&long_state[j * AES_BLOCK_SIZE], c, a);
        xor_blocks_dst(c, b, &long_state[j * AES_BLOCK_SIZE]);
        j = e2i(c, CN_AES_INIT);
        mul(c, &long_state[j * AES_BLOCK_SIZE], a);
        sum_half_blocks(a, &long_state[j * AES_BLOCK_SIZE]);
        swap_blocks(a, &long_state[j * AES_BLOCK_SIZE]);
        xor_blocks(a, c);
        copy_block(b, c);
    }

    memcpy(text, state.init, INIT_SIZE_BYTE);
    oaes_key_import_data(aes_ctx, &state.hs.b[32], AES_KEY_SIZE);
    for (i = 0; i < CN_INIT; i++) {
        for (j = 0; j < INIT_SIZE_BLK; j++) {
            xor_blocks(&text[j * AES_BLOCK_SIZE],
                    &long_state[i * INIT_SIZE_BYTE + j * AES_BLOCK_SIZE]);
            aesb_pseudo_round(&text[j * AES_BLOCK_SIZE],
                    &text[j * AES_BLOCK_SIZE],
                    aes_ctx->key->exp_data);
        }
    }
    memcpy(state.init, text, INIT_SIZE_BYTE);
    hash_permutation(&state.hs);
    extra_hashes[state.hs.b[0] & 3](&state, 200, output);
    oaes_free((OAES_CTX **) &aes_ctx);
    free(long_state);
}
