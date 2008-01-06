/*
 * checksum.c
 *
 * Copyright (c) 2008- Steve McIntyre <steve@einval.com>
 *
 * Implementation of a generic checksum interface, used in JTE.
 *
 * GNU GPL v2
 */

#include <mconfig.h>
#include "genisoimage.h"
#include <timedefs.h>
#include <fctldefs.h>
#include <regex.h>
#include <stdlib.h>
#include "md5.h"
#include "sha1.h"
#include "checksum.h"

static void md5_init(void *context)
{
    mk_MD5Init(context);
}
static void md5_update(void *context, unsigned char const *buf, unsigned int len)
{
    mk_MD5Update(context, buf, len);
}
static void md5_final(unsigned char *digest, void *context)
{
    mk_MD5Final(digest, context);
}

static void sha1_init(void *context)
{
    sha1_init_ctx(context);
}
static void sha1_update(void *context, unsigned char const *buf, unsigned int len)
{
    sha1_process_bytes(buf, len, context);
}
static void sha1_final(unsigned char *digest, void *context)
{
    sha1_finish_ctx(context, digest);
}

struct checksum_details
{
    char          *name;
    int            digest_size;
    int            context_size;
    void          (*init)(void *context);
    void          (*update)(void *context, unsigned char const *buf, unsigned int len);
    void          (*final)(unsigned char *digest, void *context);
};

static const struct checksum_details algorithms[] = 
{
    {
        "MD5",
        16,
        sizeof(struct mk_MD5Context),
        md5_init,
        md5_update,
        md5_final
    },
    {
        "SHA1",
        20,
        sizeof(struct sha1_ctx),
        sha1_init,
        sha1_update,
        sha1_final
    }
};

struct _checksum_context
{
    void          *context;
    unsigned char *digest;
    int            enabled;
};

struct checksum_info *checksum_information(enum checksum_types which)
{
    return (struct checksum_info *)&algorithms[which];
}

checksum_context_t *checksum_init_context(int checksums)
{
    int i = 0;
    struct _checksum_context *context = malloc(NUM_CHECKSUMS * sizeof(struct _checksum_context));
    if (!context)
        return NULL;

    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        if ( (1 << i) & checksums)
        {
            context[i].context = malloc(algorithms[i].context_size);
            if (!context[i].context)
                return NULL;
            context[i].digest = malloc(algorithms[i].digest_size);
            if (!context[i].digest)
                return NULL;        
            algorithms[i].init(context[i].context);
            context[i].enabled = 1;
        }
        else
            context[i].enabled = 0;
    }
    
    return context;
}

void checksum_free_context(checksum_context_t *context)
{
    int i = 0;
    struct _checksum_context *c = context;

    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        free(c[i].context);
        free(c[i].digest);
    }
    free(c);
}

void checksum_update(checksum_context_t *context,
                     unsigned char const *buf, unsigned int len)
{
    int i = 0;
    struct _checksum_context *c = context;
    
    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        if (c[i].enabled)
            algorithms[i].update(c[i].context, buf, len);
    }
}

void checksum_final(checksum_context_t *context)
{
    int i = 0;
    struct _checksum_context *c = context;
    
    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        if (c[i].enabled)
            algorithms[i].final(c[i].digest, c[i].context);
    }
}

void checksum_copy(checksum_context_t *context,
                   enum checksum_types which,
                   unsigned char *digest)
{
    struct _checksum_context *c = context;

    if (c[which].enabled)
        memcpy(digest, c[which].digest, algorithms[which].digest_size);
}


