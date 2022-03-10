/*-
 * Copyright (C) 2011, 2017 Gabor Kovesdan <gabor@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

/*
 * Returns a 32-bit hash of the given buffer with the given length.
 * The init value should be 0, or the previous hash value to extend
 * the previous hash.
 */
static uint32_t
hash32_buf(const void *buf, size_t len, uint32_t hash)
{
	const unsigned char *p = buf;

	while (len > 0) {
		hash = HASHSTEP(hash, *p++);
		len--;
	}

	return (hash);
}

/*
 * Initializes a hash table that can hold table_size number of entries,
 * each of which has a key of key_size bytes and a value of value_size
 * bytes. On successful allocation returns a pointer to the hash table.
 * Otherwise, returns NULL and sets errno to indicate the error.
 */
hashtable *
hashtable_init(size_t table_size, size_t key_size, size_t value_size)
{
	hashtable *tbl;

	tbl = malloc(sizeof(hashtable));
	if (tbl == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	tbl->entries = calloc(sizeof(hashtable_entry *), table_size);
	if (tbl->entries == NULL) {
		free(tbl);
		errno = ENOMEM;
		return (NULL);
	}

	tbl->tbl_size = table_size;
	tbl->key_size = key_size;
	tbl->val_size = value_size;

	return (tbl);
}

/*
 * Places the given key-value pair to the hashtable tbl. Returns:
 *     HASH_OK:      if key was newly added to the table with value.
 *     HASH_UPDATED: if key was present, and has been updated with value.
 *     HASH_FULL:	 if the table is full and the entry couldn't be added.
 *     HASH_FAIL:	 if a memory error occurred.
 */
int
hashtable_put(hashtable *tbl, const void *key, const void *value)
{
	uint32_t hash = hash32_buf(key, tbl->key_size, hash) % tbl->tbl_size;
	uint32_t initial_hash = hash;

	/* On hash collision entries are inserted at the next free space. */
	while (tbl->entries[hash] != NULL) {
		if (memcmp(tbl->entries[hash]->key, key, tbl->key_size) == 0)
		{
			memcpy(tbl->entries[hash]->value, value, tbl->val_size);
			return (HASH_UPDATED);
		}

		hash = (hash + 1) % tbl->tbl_size;

		/* This means the table is full, and the key isn't present. */
		if (hash == initial_hash) {
			return (HASH_FULL);
		}
	}

	tbl->entries[hash] = malloc(sizeof(hashtable_entry));
	if (tbl->entries[hash] == NULL) {
		errno = ENOMEM;
		return (HASH_FAIL);
	}

	tbl->entries[hash]->key = malloc(tbl->key_size);
	if (tbl->entries[hash]->key == NULL) {
		errno = ENOMEM;
		free(tbl->entries[hash]);
		return (HASH_FAIL);
	}

	tbl->entries[hash]->value = malloc(tbl->val_size);
	if (tbl->entries[hash]->value == NULL) {
		errno = ENOMEM;
		free(tbl->entries[hash]->key);
		free(tbl->entries[hash]);
		return (HASH_FAIL);
	}

	memcpy(tbl->entries[hash]->key, key, tbl->key_size);
	memcpy(tbl->entries[hash]->value, value, tbl->val_size);

	return (HASH_OK);
}

/*
 * Returns a pointer to the hashtable_entry pointer with the given key,
 * or NULL, if no such entry is found.
 */
static hashtable_entry **
hashtable_lookup(const hashtable *tbl, const void *key)
{
	uint32_t hash = hash32_buf(key, tbl->key_size, hash) % tbl->tbl_size;
	uint32_t initial_hash = hash;

	while (tbl->entries[hash] != NULL) {
		if (memcmp(tbl->entries[hash]->key, key, tbl->key_size) == 0) {
			return (&(tbl->entries[hash]));
		}

		hash = (hash + 1) % tbl->tbl_size;

		/* This means the table is full, and the key isn't present. */
		if (hash == initial_hash) {
			return (NULL);
		}
	}

	return (NULL);
}

/*
 * Retrieves the value for key from the hash table tbl and places
 * it to the space indicated by the value argument.
 * Returns HASH_OK if the value has been found and retrieved or
 * HASH_NOTFOUND otherwise.
 */
int
hashtable_get(hashtable *tbl, const void *key, void *value)
{
	hashtable_entry **entry_ptr = hashtable_lookup(tbl, key);
	hashtable_entry *entry = *entry_ptr;

	if (entry == NULL) {
		return (HASH_NOTFOUND);
	}

	memcpy(value, entry->value, tbl->val_size);
	return (HASH_OK);
}

/*
 * Removes the entry with the specifified key from the hash table
 * tbl. Returns HASH_OK if the entry has been found and removed
 * or HASH_NOTFOUND otherwise.
 */
int
hashtable_remove(hashtable *tbl, const void *key)
{
	hashtable_entry **entry_ptr = hashtable_lookup(tbl, key);
	hashtable_entry *entry = *entry_ptr;

	if (entry == NULL) {
		return (HASH_NOTFOUND);
	}

	free(entry->key);
	free(entry->value);
	free(entry);

	*entry_ptr = NULL;
	return (HASH_OK);
}

/*
 * Frees the resources associated with the hash table tbl.
 * This includes the table itself, as the hashtable_init function
 * allocates heap memory for that too.
 */
void
hashtable_free(hashtable *tbl)
{
	if (tbl == NULL) {
		return;
	}

	if (tbl->entries != NULL) {
		for (size_t i = 0; i < tbl->tbl_size; i++) {
			hashtable_entry *entry = tbl->entries[i];
			if (entry != NULL)
			{
				free(entry->key);
				free(entry->value);
				free(entry);
			}
		}

		free(tbl->entries);
	}
	free(tbl);
}
