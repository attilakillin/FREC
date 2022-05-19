/* $FreeBSD: user/gabor/tre-integration/contrib/tre/lib/hashtable.h 225162 2011-08-25 01:47:26Z gabor $ */

#ifndef HASHTABLE_H
#define HASHTABLE_H 1

#include <sys/types.h>

#define HASH_OK 0
#define HASH_UPDATED 1
#define HASH_FAIL 2
#define HASH_FULL 3
#define HASH_NOTFOUND 4

#define HASHSTEP(x, c) (((x << 5) + x) + (c))

typedef struct hashtable_entry
{
	void *key;                  /* Pointer to an entry key. */
	void *value;                /* Pointer to an entry value. */
} hashtable_entry;

typedef struct hashtable
{
	size_t tbl_size;            /* Max entry count of the table. */
	size_t key_size;            /* Size of an entry key. */
	size_t val_size;            /* Size of an entry value. */
	hashtable_entry **entries;  /* Array of entry pointers. */
} hashtable;

hashtable *hashtable_init(size_t, size_t, size_t);
int hashtable_put(hashtable *, const void *, const void *);
int hashtable_get(hashtable *, const void *, void *);
int hashtable_remove(hashtable *, const void *);
void hashtable_free(hashtable *);

#endif /* HASHTABLE_H */
