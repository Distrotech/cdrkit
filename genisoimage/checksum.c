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
#include "sha256.h"
#include "sha512.h"
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

static void sha256_init(void *context)
{
    sha256_init_ctx(context);
}
static void sha256_update(void *context, unsigned char const *buf, unsigned int len)
{
    sha256_process_bytes(buf, len, context);
}
static void sha256_final(unsigned char *digest, void *context)
{
    sha256_finish_ctx(context, digest);
}

static void sha512_init(void *context)
{
    sha512_init_ctx(context);
}
static void sha512_update(void *context, unsigned char const *buf, unsigned int len)
{
    sha512_process_bytes(buf, len, context);
}
static void sha512_final(unsigned char *digest, void *context)
{
    sha512_finish_ctx(context, digest);
}

struct checksum_details
{
    char          *name;
    char          *prog;
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
        "md5sum",
        16,
        sizeof(struct mk_MD5Context),
        md5_init,
        md5_update,
        md5_final
    },
    {
        "SHA1",
        "sha1sum",
        20,
        sizeof(struct sha1_ctx),
        sha1_init,
        sha1_update,
        sha1_final
    },
    {
        "SHA256",
        "sha256sum",
        32,
        sizeof(struct sha256_ctx),
        sha256_init,
        sha256_update,
        sha256_final
    },
    {
        "SHA512",
        "sha512sum",
        64,
        sizeof(struct sha512_ctx),
        sha512_init,
        sha512_update,
        sha512_final
    }
};

struct algo_context
{
    void          *context;
    unsigned char *digest;
    int            enabled;
};

struct _checksum_context
{
    char          *owner;
    struct algo_context algo[NUM_CHECKSUMS];
};

struct checksum_info *checksum_information(enum checksum_types which)
{
    return (struct checksum_info *)&algorithms[which];
}

checksum_context_t *checksum_init_context(int checksums, const char *owner)
{
    int i = 0;
    struct _checksum_context *context = malloc(sizeof(struct _checksum_context));
    if (!context)
        return NULL;

    context->owner = strdup(owner);
    if (!context->owner)
    {
        free(context);
        return NULL;
    }   

    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        if ( (1 << i) & checksums)
        {
            context->algo[i].context = malloc(algorithms[i].context_size);
            if (!context->algo[i].context)
                return NULL;
            context->algo[i].digest = malloc(algorithms[i].digest_size);
            if (!context->algo[i].digest)
                return NULL;        
            algorithms[i].init(context->algo[i].context);
            context->algo[i].enabled = 1;
        }
        else
            context->algo[i].enabled = 0;
    }
    
    return context;
}

void checksum_free_context(checksum_context_t *context)
{
    int i = 0;
    struct _checksum_context *c = context;

    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        free(c->algo[i].context);
        free(c->algo[i].digest);
    }
    free(c->owner);
    free(c);
}

void checksum_update(checksum_context_t *context,
                     unsigned char const *buf, unsigned int len)
{
    int i = 0;
    struct _checksum_context *c = context;
    
    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        if (c->algo[i].enabled)
            algorithms[i].update(c->algo[i].context, buf, len);
    }
}

void checksum_final(checksum_context_t *context)
{
    int i = 0;
    struct _checksum_context *c = context;
    
    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        if (c->algo[i].enabled)
            algorithms[i].final(c->algo[i].digest, c->algo[i].context);
    }
}

void checksum_copy(checksum_context_t *context,
                   enum checksum_types which,
                   unsigned char *digest)
{
    struct _checksum_context *c = context;

    if (c->algo[which].enabled)
        memcpy(digest, c->algo[which].digest, algorithms[which].digest_size);
    else
        fprintf(stderr, "Asked for %s checksum, not enabled!\n",
                algorithms[which].name);
}

#ifdef CHECKSUM_SELF_TEST
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char buf[1024];
    int fd = -1;
    char *filename;
    int err = 0;
    static checksum_context_t *test_context = NULL;
    int i = 0;

    if (argc != 2)
    {
        fprintf(stderr, "Need a filename to act on!\n");
        return 1;
    }

    filename = argv[1];
    fd = open(filename, O_RDONLY);
    if (fd < 0)
    {
        fprintf(stderr, "Unable to open file %s, errno %d\n", filename, errno);
        return 1;
    }

    test_context = checksum_init_context(CHECK_ALL_USED, "test");
    if (!test_context)
    {
        fprintf(stderr, "Unable to initialise checksum context\n");
        return 1;
    }

    while(1)
    {
        err = read(fd, buf, sizeof(buf));
        if (err < 0)
        {
            fprintf(stderr, "Failed to read from file, errno %d\n", errno);
            return 1;
        }

        if (err == 0)
            break; // EOF

        /* else */
        checksum_update(test_context, buf, err);
    }
    close(fd);
    checksum_final(test_context);

    for (i = 0; i < NUM_CHECKSUMS; i++)
    {
        struct checksum_info *info;
        unsigned char r[64];
        int j = 0;

        info = checksum_information(i);
        memset(r, 0, sizeof(r));

        checksum_copy(test_context, i, r);

        printf("OUR %s:\n", info->name);
        for (j = 0; j < info->digest_size; j++)
            printf("%2.2x", r[j]);
        printf("  %s\n", filename);
        printf("system checksum program (%s):\n", info->prog);
        sprintf(buf, "%s %s", info->prog, filename);
        system(buf);
        printf("\n");
    }
    return 0;
}
#endif /* CHECKSUM_SELF_TEST */

