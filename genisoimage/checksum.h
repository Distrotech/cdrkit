/*
 * checksum.h
 *
 * Copyright (c) 2008- Steve McIntyre <steve@einval.com>
 *
 * Definitions and prototypes for a generic checksum interface, used
 * in JTE. Inspired heavily by the interface to the MD5 code we're
 * using already.
 *
 * GNU GPL v2
 */

enum checksum_types
{
    CHECK_MD5 = 0,
    CHECK_SHA1,
    NUM_CHECKSUMS
};

#define CHECK_MD5_USED   (1 << CHECK_MD5)
#define CHECK_SHA1_USED  (1 << CHECK_SHA1)
#define CHECK_ALL_USED   0xFFFFFFFF

typedef void checksum_context_t;

struct checksum_info
{
    char          *name;
    char          *prog;
    int            digest_size;
};

/* Ask the library for information about a particular checksum
 * algorithm. Returns a pointer to internal memory - DO NOT
 * MODIFY/FREE! */
struct checksum_info *checksum_information(enum checksum_types which);

/* Allocate / initialise a context for the chosen checksums. OR
 * together the desired checksums as the parameter */
checksum_context_t   *checksum_init_context(int checksums, const char *owner);

/* Cleanup and free a context when it's finished with */
void                  checksum_free_context(checksum_context_t *context);

/* Pass a new buffer full of data through the checksum code */
void                  checksum_update(checksum_context_t *context,
                                      unsigned char const *buf,
                                      unsigned int len);

/* Finish the current set of checksums */
void                  checksum_final(checksum_context_t *context);

/* Extract a particular algorithm's checksum once checksum_final() has
 * been called. Use the details in checksum_information() above first
 * to see how big the digest will be. */
void                  checksum_copy(checksum_context_t *context,
                                    enum checksum_types which,
                                    unsigned char *digest);
