/*$6
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#include "rumble.h"
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
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

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    unsigned int    x;
    unsigned char   *md = (unsigned char *) malloc(33);
    char            *ret = (char *) calloc(1, 65);
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

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

/*
 =======================================================================================================================
 =======================================================================================================================
 */
char *rumble_decode_base64(const char *src) {

    /*~~~~~~~~~~~~*/
    BIO     *b64,
            *bmem;
    ssize_t len;
    char    *buffer,
            *copy;
    /*~~~~~~~~~~~~*/

    len = strlen(src);
    buffer = (char *) calloc(1, len + 1);
    copy = (char *) calloc(1, len + 3);
    sprintf(copy, "%s\r\n", src);
    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new_mem_buf((void *) copy, len + 2);
    bmem = BIO_push(b64, bmem);
    BIO_read(bmem, buffer, len);
    BIO_free_all(bmem);
    free(copy);
    return (buffer);
}
