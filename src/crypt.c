/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#include <string.h>
#ifndef PRIx32
#   define PRIx32  "x"
#endif
typedef struct
{
    uint32_t    total[2];
    uint32_t    state[8];
    uint8_t     buffer[64];
} sha256_context;
#define GET_UINT32(n, b, i) { \
        (n) = ((uint32_t) (b)[(i)] << 24) | ((uint32_t) (b)[(i) + 1] << 16) | ((uint32_t) (b)[(i) + 2] << 8) | ((uint32_t) (b)[(i) + 3]); \
    }
#define PUT_UINT32(n, b, i) { \
        (b)[(i)] = (uint8_t) ((n) >> 24); \
        (b)[(i) + 1] = (uint8_t) ((n) >> 16); \
        (b)[(i) + 2] = (uint8_t) ((n) >> 8); \
        (b)[(i) + 3] = (uint8_t) ((n)); \
    }

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void sha256_starts(sha256_context *ctx) {
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0x6A09E667;
    ctx->state[1] = 0xBB67AE85;
    ctx->state[2] = 0x3C6EF372;
    ctx->state[3] = 0xA54FF53A;
    ctx->state[4] = 0x510E527F;
    ctx->state[5] = 0x9B05688C;
    ctx->state[6] = 0x1F83D9AB;
    ctx->state[7] = 0x5BE0CD19;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void sha256_process(sha256_context *ctx, uint8_t data[64]) {

    /*~~~~~~~~~~~~~~*/
    uint32_t    temp1,
                temp2,
                W[64];
    uint32_t    A,
                B,
                C,
                D,
                E,
                F,
                G,
                H;
    /*~~~~~~~~~~~~~~*/

    GET_UINT32(W[0], data, 0);
    GET_UINT32(W[1], data, 4);
    GET_UINT32(W[2], data, 8);
    GET_UINT32(W[3], data, 12);
    GET_UINT32(W[4], data, 16);
    GET_UINT32(W[5], data, 20);
    GET_UINT32(W[6], data, 24);
    GET_UINT32(W[7], data, 28);
    GET_UINT32(W[8], data, 32);
    GET_UINT32(W[9], data, 36);
    GET_UINT32(W[10], data, 40);
    GET_UINT32(W[11], data, 44);
    GET_UINT32(W[12], data, 48);
    GET_UINT32(W[13], data, 52);
    GET_UINT32(W[14], data, 56);
    GET_UINT32(W[15], data, 60);
#define SHR(x, n)   ((x & 0xFFFFFFFF) >> n)
#define ROTR(x, n)  (SHR(x, n) | (x << (32 - n)))
#define S0(x)       (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define S1(x)       (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))
#define S2(x)       (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define S3(x)       (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define F0(x, y, z) ((x & y) | (z & (x | y)))
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define R(t)        (W[t] = S1(W[t - 2]) + W[t - 7] + S0(W[t - 15]) + W[t - 16])
#define P(a, b, c, d, e, f, g, h, x, K) { \
        temp1 = h + S3(e) + F1(e, f, g) + K + x; \
        temp2 = S2(a) + F0(a, b, c); \
        d += temp1; \
        h = temp1 + temp2; \
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];
    F = ctx->state[5];
    G = ctx->state[6];
    H = ctx->state[7];
    P(A, B, C, D, E, F, G, H, W[0], 0x428A2F98);
    P(H, A, B, C, D, E, F, G, W[1], 0x71374491);
    P(G, H, A, B, C, D, E, F, W[2], 0xB5C0FBCF);
    P(F, G, H, A, B, C, D, E, W[3], 0xE9B5DBA5);
    P(E, F, G, H, A, B, C, D, W[4], 0x3956C25B);
    P(D, E, F, G, H, A, B, C, W[5], 0x59F111F1);
    P(C, D, E, F, G, H, A, B, W[6], 0x923F82A4);
    P(B, C, D, E, F, G, H, A, W[7], 0xAB1C5ED5);
    P(A, B, C, D, E, F, G, H, W[8], 0xD807AA98);
    P(H, A, B, C, D, E, F, G, W[9], 0x12835B01);
    P(G, H, A, B, C, D, E, F, W[10], 0x243185BE);
    P(F, G, H, A, B, C, D, E, W[11], 0x550C7DC3);
    P(E, F, G, H, A, B, C, D, W[12], 0x72BE5D74);
    P(D, E, F, G, H, A, B, C, W[13], 0x80DEB1FE);
    P(C, D, E, F, G, H, A, B, W[14], 0x9BDC06A7);
    P(B, C, D, E, F, G, H, A, W[15], 0xC19BF174);
    P(A, B, C, D, E, F, G, H, R(16), 0xE49B69C1);
    P(H, A, B, C, D, E, F, G, R(17), 0xEFBE4786);
    P(G, H, A, B, C, D, E, F, R(18), 0x0FC19DC6);
    P(F, G, H, A, B, C, D, E, R(19), 0x240CA1CC);
    P(E, F, G, H, A, B, C, D, R(20), 0x2DE92C6F);
    P(D, E, F, G, H, A, B, C, R(21), 0x4A7484AA);
    P(C, D, E, F, G, H, A, B, R(22), 0x5CB0A9DC);
    P(B, C, D, E, F, G, H, A, R(23), 0x76F988DA);
    P(A, B, C, D, E, F, G, H, R(24), 0x983E5152);
    P(H, A, B, C, D, E, F, G, R(25), 0xA831C66D);
    P(G, H, A, B, C, D, E, F, R(26), 0xB00327C8);
    P(F, G, H, A, B, C, D, E, R(27), 0xBF597FC7);
    P(E, F, G, H, A, B, C, D, R(28), 0xC6E00BF3);
    P(D, E, F, G, H, A, B, C, R(29), 0xD5A79147);
    P(C, D, E, F, G, H, A, B, R(30), 0x06CA6351);
    P(B, C, D, E, F, G, H, A, R(31), 0x14292967);
    P(A, B, C, D, E, F, G, H, R(32), 0x27B70A85);
    P(H, A, B, C, D, E, F, G, R(33), 0x2E1B2138);
    P(G, H, A, B, C, D, E, F, R(34), 0x4D2C6DFC);
    P(F, G, H, A, B, C, D, E, R(35), 0x53380D13);
    P(E, F, G, H, A, B, C, D, R(36), 0x650A7354);
    P(D, E, F, G, H, A, B, C, R(37), 0x766A0ABB);
    P(C, D, E, F, G, H, A, B, R(38), 0x81C2C92E);
    P(B, C, D, E, F, G, H, A, R(39), 0x92722C85);
    P(A, B, C, D, E, F, G, H, R(40), 0xA2BFE8A1);
    P(H, A, B, C, D, E, F, G, R(41), 0xA81A664B);
    P(G, H, A, B, C, D, E, F, R(42), 0xC24B8B70);
    P(F, G, H, A, B, C, D, E, R(43), 0xC76C51A3);
    P(E, F, G, H, A, B, C, D, R(44), 0xD192E819);
    P(D, E, F, G, H, A, B, C, R(45), 0xD6990624);
    P(C, D, E, F, G, H, A, B, R(46), 0xF40E3585);
    P(B, C, D, E, F, G, H, A, R(47), 0x106AA070);
    P(A, B, C, D, E, F, G, H, R(48), 0x19A4C116);
    P(H, A, B, C, D, E, F, G, R(49), 0x1E376C08);
    P(G, H, A, B, C, D, E, F, R(50), 0x2748774C);
    P(F, G, H, A, B, C, D, E, R(51), 0x34B0BCB5);
    P(E, F, G, H, A, B, C, D, R(52), 0x391C0CB3);
    P(D, E, F, G, H, A, B, C, R(53), 0x4ED8AA4A);
    P(C, D, E, F, G, H, A, B, R(54), 0x5B9CCA4F);
    P(B, C, D, E, F, G, H, A, R(55), 0x682E6FF3);
    P(A, B, C, D, E, F, G, H, R(56), 0x748F82EE);
    P(H, A, B, C, D, E, F, G, R(57), 0x78A5636F);
    P(G, H, A, B, C, D, E, F, R(58), 0x84C87814);
    P(F, G, H, A, B, C, D, E, R(59), 0x8CC70208);
    P(E, F, G, H, A, B, C, D, R(60), 0x90BEFFFA);
    P(D, E, F, G, H, A, B, C, R(61), 0xA4506CEB);
    P(C, D, E, F, G, H, A, B, R(62), 0xBEF9A3F7);
    P(B, C, D, E, F, G, H, A, R(63), 0xC67178F2);
    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
    ctx->state[5] += F;
    ctx->state[6] += G;
    ctx->state[7] += H;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void sha256_update(sha256_context *ctx, uint8_t *input, uint32_t length) {

    /*~~~~~~~~~~~~~*/
    uint32_t    left,
                fill;
    /*~~~~~~~~~~~~~*/

    if (!length) return;
    left = ctx->total[0] & 0x3F;
    fill = 64 - left;
    ctx->total[0] += length;
    ctx->total[0] &= 0xFFFFFFFF;
    if (ctx->total[0] < length) ctx->total[1]++;
    if (left && length >= fill) {
        memcpy((void *) (ctx->buffer + left), (void *) input, fill);
        sha256_process(ctx, ctx->buffer);
        length -= fill;
        input += fill;
        left = 0;
    }

    while (length >= 64) {
        sha256_process(ctx, input);
        length -= 64;
        input += 64;
    }

    if (length) {
        memcpy((void *) (ctx->buffer + left), (void *) input, length);
    }
}

static uint8_t  sha256_padding[64] =
{
    0x80,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void sha256_finish(sha256_context *ctx, uint8_t digest[32]) {

    /*~~~~~~~~~~~~~~~~~~*/
    uint32_t    last,
                padn;
    uint32_t    high,
                low;
    uint8_t     msglen[8];
    /*~~~~~~~~~~~~~~~~~~*/

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);
    PUT_UINT32(high, msglen, 0);
    PUT_UINT32(low, msglen, 4);
    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);
    sha256_update(ctx, sha256_padding, padn);
    sha256_update(ctx, msglen, 8);
    PUT_UINT32(ctx->state[0], digest, 0);
    PUT_UINT32(ctx->state[1], digest, 4);
    PUT_UINT32(ctx->state[2], digest, 8);
    PUT_UINT32(ctx->state[3], digest, 12);
    PUT_UINT32(ctx->state[4], digest, 16);
    PUT_UINT32(ctx->state[5], digest, 20);
    PUT_UINT32(ctx->state[6], digest, 24);
    PUT_UINT32(ctx->state[7], digest, 28);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *rumble_sha256(const char *digest) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    sha256_context  ctx;
    char            *ret = (char *) calloc(1, 72);
    unsigned char   sha256sum[33];
    unsigned int    x;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    sha256_starts(&ctx);
    sha256_update(&ctx, (uint8_t *) digest, strlen(digest));
    sha256_finish(&ctx, sha256sum);
    for (x = 0; x < 32; x++) sprintf((char *) (ret + (x * 2)), "%02x", sha256sum[x]);
    return (ret);
}

/* Base 64 encode/decode */
static const char   b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const int    BASE64_INPUT_SIZE = 57;
static const int    BASE64_CHARS_PER_LINE = 72;
#define isbase64(c) (c && strchr(b64_table, c))
#define b64enc(v)   (v > 63) ? '=' : b64_table[(int) v]

/*
 =======================================================================================================================
 =======================================================================================================================
 */
inline char value(char c) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char  *p = strchr(b64_table, c);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (p) {
        return (p - b64_table);
    } else {
        return (0);
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int rumble_unbase64(unsigned char *dest, const unsigned char *src, size_t srclen) {

    /*~~~~~~~~~~~~~~~~~~~~~~*/
    unsigned char   *p = dest;
    /*~~~~~~~~~~~~~~~~~~~~~~*/

    *dest = 0;
    if (*src == 0) {
        return (0);
    }

    do
    {
        /*~~~~~~~~~~~~~~~~~~~~~~*/
        char    a = value(src[0]);
        char    b = value(src[1]);
        char    c = value(src[2]);
        char    d = value(src[3]);
        /*~~~~~~~~~~~~~~~~~~~~~~*/

        *p++ = (a << 2) | (b >> 4);
        *p++ = (b << 4) | (c >> 2);
        *p++ = (c << 6) | d;
        if (!isbase64(src[1])) {
            p -= 2;
            break;
        } else if (!isbase64(src[2])) {
            p -= 2;
            break;
        } else if (!isbase64(src[3])) {
            p--;
            break;
        }

        src += 4;
        while (*src && (*src == 13 || *src == 10)) src++;
    } while (srclen -= 4);
    *p = 0;
    return (p - dest);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *rumble_decode_base64(const char *src) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    size_t  ilen = strlen(src);
    char    *output = (char *) malloc(ilen);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    rumble_unbase64((unsigned char *) output, (const unsigned char *) src, ilen);
    return (output);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *rumble_encode_base64(const char *src, size_t len) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    base64_encodestate  state;
    char                *output;
    int                 n;
    size_t              olen = (len * (4 / 3)) + 1024;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    output = malloc(olen);
    state.step = 1;
    state.result = 0;
    state.stepcount = 0;
    n = base64_encode_block(src, len, output, &state);
    if (n) n = base64_encode_blockend((char *) output + n, &state);
    return (output);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char base64_encode_value(char value_in) {
    if (value_in > 63) return ('=');
    return (b64_table[(int) value_in]);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int base64_encode_block(const char *plaintext_in, size_t length_in, char *code_out, base64_encodestate *state_in) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    const char          *plainchar = plaintext_in;
    const char *const   plaintextend = plaintext_in + length_in;
    char                *codechar = code_out;
    char                result;
    char                fragment;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    result = state_in->result;
    switch (state_in->step) {
        while (1)
        {
        case 1:
            if (plainchar == plaintextend) {
                state_in->result = result;
                state_in->step = 1;
                return (codechar - code_out);
            }

            fragment = *plainchar++;
            result = (fragment & 0x0fc) >> 2;
            *codechar++ = b64enc(result);
            result = (fragment & 0x003) << 4;

        case 2:
            if (plainchar == plaintextend) {
                state_in->result = result;
                state_in->step = 2;
                return (codechar - code_out);
            }

            fragment = *plainchar++;
            result |= (fragment & 0x0f0) >> 4;
            *codechar++ = b64enc(result);
            result = (fragment & 0x00f) << 2;

        case 3:
            if (plainchar == plaintextend) {
                state_in->result = result;
                state_in->step = 3;
                return (codechar - code_out);
            }

            fragment = *plainchar++;
            result |= (fragment & 0x0c0) >> 6;
            *codechar++ = b64enc(result);
            result = (fragment & 0x03f) >> 0;
            *codechar++ = b64enc(result);
            ++(state_in->stepcount);
            if (state_in->stepcount == BASE64_CHARS_PER_LINE / 4) {
                *codechar++ = '\n';
                state_in->stepcount = 0;
            }
        }
    }

    /* control should not reach here */
    return (codechar - code_out);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int base64_encode_blockend(char *code_out, base64_encodestate *state_in) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/
    char    *codechar = code_out;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/

    switch (state_in->step)
    {
    case 2: *codechar++ = b64enc(state_in->result); *codechar++ = '='; *codechar++ = '='; break;
    case 3: *codechar++ = b64enc(state_in->result); *codechar++ = '='; break;
    case 1: break;
    }

    *codechar++ = '\n';
    return (codechar - code_out);
}
