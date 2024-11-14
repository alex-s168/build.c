#ifndef _SLOWDB_H
#define _SLOWDB_H

typedef struct slowdb slowdb;

// Add a new record, returns slowdb_error on fail
static void slowdb_put(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen);

// Get a HEAP ALLOCATED pointer to an existing record. *vallen contains the size of the returned value 
// in bytes and can be NULL if you are not interested in it.
static unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen);

static void slowdb_remove(slowdb *instance, const unsigned char *key, int keylen);

// replace data with new data of SAME VAL LEN 
static void slowdb_replaceOrPut(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen);

// Open existing, or create new
static slowdb *slowdb_open(const char *filename);

// Close. Must be called to ensure all changes are written to disk
static void slowdb_close(slowdb *instance);

typedef struct {
    slowdb *_db;
    size_t _ent_id;
} slowdb_iter;

static slowdb_iter slowdb_iter_new(slowdb* slowdb);
// if this returns NULL, it means that the current item is not valid and you need to go to the next item
static unsigned char * slowdb_iter_get_key(slowdb_iter* iter, int* lenout);
// if this returns NULL, it means that the current item is not valid and you need to go to the next item
static unsigned char * slowdb_iter_get_val(slowdb_iter* iter, int* lenout);
// returns 1 if there was a next; ALSO HAS TO BE CALLED BEFORE THE FIRST ELEM
static int slowdb_iter_next(slowdb_iter* iter);

/* ========================================================================== */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

struct slowdb {
    FILE* fp;

    size_t* ents;
    size_t  ents_len;
};

struct slowdb_ent_header {
    char valid;
    uint16_t key_len;
    uint16_t data_len;
} __attribute__((packed));

static char slowdb_header_magic[8] = {':', 's', 'l', 'o', 'w', 'd', 'b', '0'};

struct slowdb_header {
    char magic[8];
} __attribute__((packed));

static slowdb *slowdb_open(const char *filename)
{
    slowdb* db = (slowdb*) malloc(sizeof(slowdb));
    if (db == NULL) return NULL;

    db->fp = fopen(filename, "a+");
    if (db->fp == NULL) {
        free(db);
        return NULL;
    }

    struct slowdb_header header;
    if ( fread(&header, sizeof(header), 1, db->fp) != 1 ) {
        rewind(db->fp);
        memcpy(header.magic, slowdb_header_magic, 8);
        fwrite(&header, sizeof(header), 1, db->fp);
    }
    else {
        if ( memcmp(header.magic, slowdb_header_magic, 8) ){
            fclose(db->fp);
            free(db);
            return NULL;
        }

        db->ents_len = 0;
        db->ents = NULL;
        while (true)
        {
            size_t where = ftell(db->fp);
            struct slowdb_ent_header eh;
            if ( fread(&eh, sizeof(eh), 1, db->fp) != 1 )
                break;
            if (eh.valid) {
                db->ents = (size_t*)
                    realloc(db->ents, sizeof(size_t) * (db->ents_len + 1));
                db->ents[db->ents_len ++] = where;
            }
            fseek(db->fp, eh.key_len + eh.data_len, SEEK_CUR);
        }
    }

    return db;
}

static unsigned char *slowdb_get(slowdb *instance, const unsigned char *key, int keylen, int *vallen)
{
    int found = 0;
    for (size_t i = 0; i < instance->ents_len; i ++)
    {
        fseek(instance->fp, instance->ents[i], SEEK_SET);
        struct slowdb_ent_header header;
        fread(&header, sizeof(header), 1, instance->fp);
        if (!header.valid || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            found = header.data_len;
            break;
        }
    }

    if (vallen)
        *vallen = found;

    if (!found) return NULL;

    unsigned char * res = (unsigned char*) malloc(found);
    if (res == NULL) return NULL;

    fread(res, 1, found, instance->fp);

    return res;
}

static void slowdb_replaceOrPut(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen)
{
    for (size_t i = 0; i < instance->ents_len; i ++)
    {
        fseek(instance->fp, instance->ents[i], SEEK_SET);
        struct slowdb_ent_header header;
        fread(&header, sizeof(header), 1, instance->fp);
        if (!header.valid || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            assert(header.data_len == vallen);
            fwrite(val, 1, vallen, instance->fp);
            return;
        }
    }

    (void) slowdb_put(instance, key, keylen, val, vallen);
}

static void slowdb_remove(slowdb *instance, const unsigned char *key, int keylen)
{
    for (size_t i = 0; i < instance->ents_len; i ++)
    {
        fseek(instance->fp, instance->ents[i], SEEK_SET);
        struct slowdb_ent_header header;
        fread(&header, sizeof(header), 1, instance->fp);
        if (!header.valid || header.key_len != keylen)
            continue;
        unsigned char * k = (unsigned char *) malloc(header.key_len);
        if (k == NULL) continue;
        fread(k, 1, header.key_len, instance->fp);
        if (!memcmp(key, k, header.key_len)) {
            fseek(instance->fp, instance->ents[i], SEEK_SET);
            header.valid = 0;
            fwrite(&header, sizeof(header), 1, instance->fp);
            break;
        }
    }
}

static void slowdb_put(slowdb *instance, const unsigned char *key, int keylen, const unsigned char *val, int vallen)
{
    fseek(instance->fp, 0, SEEK_END);
    size_t where = ftell(instance->fp);

    struct slowdb_ent_header header;
    header.key_len = keylen;
    header.data_len = vallen;
    header.valid = 1;

    fwrite(&header, sizeof(header), 1, instance->fp);
    fwrite(key, 1, keylen, instance->fp);
    fwrite(val, 1, vallen, instance->fp);

    instance->ents = (size_t*)
        realloc(instance->ents, sizeof(size_t) * (instance->ents_len + 1));
    instance->ents[instance->ents_len++] = where;
}

static void slowdb_close(slowdb *instance)
{
    fclose(instance->fp);
    free(instance->ents);
    free(instance);
}

static slowdb_iter slowdb_iter_new(slowdb* slowdb)
{
    return (slowdb_iter) { ._db = slowdb, ._ent_id = SIZE_MAX };
}

static unsigned char * slowdb_iter_get_key(slowdb_iter* iter, int* lenout)
{
    fseek(iter->_db->fp, iter->_db->ents[iter->_ent_id], SEEK_SET);
    struct slowdb_ent_header header;
    fread(&header, sizeof(header), 1, iter->_db->fp);
    if (!header.valid) return NULL;

    unsigned char * k = (unsigned char *) malloc(header.key_len);
    if (k == NULL) return NULL;
    fread(k, 1, header.key_len, iter->_db->fp);
    if (lenout) *lenout = header.key_len;
    return k;
}

static unsigned char * slowdb_iter_get_val(slowdb_iter* iter, int* lenout)
{
    fseek(iter->_db->fp, iter->_db->ents[iter->_ent_id], SEEK_SET);
    struct slowdb_ent_header header;
    fread(&header, sizeof(header), 1, iter->_db->fp);
    if (!header.valid) return NULL;

    fseek(iter->_db->fp, header.key_len, SEEK_CUR);

    unsigned char * v = (unsigned char *) malloc(header.data_len);
    if (v == NULL) return NULL;
    fread(v, 1, header.data_len, iter->_db->fp);
    if (lenout) *lenout = header.data_len;
    return v;
}

// returns 1 if there was a next
static int slowdb_iter_next(slowdb_iter* iter)
{
    iter->_ent_id ++;
    return iter->_ent_id < iter->_db->ents_len;
}

#endif
