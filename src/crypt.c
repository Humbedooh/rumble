/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include "private.h"
#include <openssl/sha.h>
#include <string.h>
#ifndef PRIx32
#   define PRIx32  "x"
#endif

/*
 =======================================================================================================================
    char* rumble_sha256_fast(const unsigned char* d) Converts the string (d) into a SHA-256 digest (64 byte hex
    string). Note: For extra speed, digests are printed out "backwards" as: DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA
    DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA This should have no effect on security and is roughly 3 times faster.
 =======================================================================================================================
 */
char *rumble_sha256_fast(const unsigned char *d) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    unsigned int    *x;
    unsigned char   *md = (unsigned char *) malloc(33);
    char            *ret = (char *) malloc(65);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!ret) merror();
    SHA256(d, strlen((const char *) d), md);
    x = (unsigned int *) md;
    sprintf((char *) ret, "%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32, x[0], x[1], x[2],
            x[3], x[4], x[5], x[6], x[7]);
    memset(md, 0, 33);  /* Erase md, just in case. */
    free(md);
    return (ret);
}

/*
 =======================================================================================================================
    char* rumble_sha256(const unsigned char* d) Converts the string (d) into a SHA-256 digest (64 byte hex string).
 =======================================================================================================================
 */
char *rumble_sha256(const unsigned char *d) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    unsigned int    x;
    unsigned char   *md = (unsigned char *) calloc(1, 33);
    char            *ret = (char *) calloc(1, 72);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!ret) merror();
    SHA256(d, strlen((const char *) d), md);
    for (x = 0; x < 32; x++) sprintf((char *) (ret + (x * 2)), "%02x", md[x]);
    memset(md, 0, 33);  /* Erase md, just in case. */
    free(md);
    return (ret);
}

/*
 =======================================================================================================================
    char* rumble_sha160(const unsigned char* d) Converts the string (d) into a hex SHA1 160 bit digest (40 byte hex
    string). This is used for simpler tasks, such as grey-listing, where collisions are of less importance. Note: For
    extra speed, digests are printed out "backwards" as: DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA DDCCBBAA This should have
    no effect on security and is roughly 3 times faster.
 =======================================================================================================================
 */
char *rumble_sha160(const unsigned char *d) {

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    unsigned int    *x;
    unsigned char   *md = (unsigned char *) malloc(21);
    char            *ret = (char *) malloc(41);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if (!ret) merror();
    SHA1(d, strlen((const char *) d), md);
    x = (unsigned int *) md;
    sprintf((char *) ret, "%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32, x[0], x[1], x[2], x[3], x[4]);
    memset(md, 0, 21);  /* Erase md, just in case. */
    free(md);
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
int rumble_unbase64(unsigned char *dest, const unsigned char *src, int srclen) {

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

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    size_t  ilen = strlen(src);
    char    *output = malloc(ilen);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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
int base64_encode_block(const char *plaintext_in, int length_in, char *code_out, base64_encodestate *state_in) {

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
