#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

uint64_t
hash(const char *s, size_t len)
{
	uint64_t h = 5381;

	for (size_t i = 0; i < len; i++)
		h = ((h << 5) + h) + *s++;

	return h;
}

uint64_t
hash_str(const char *s)
{
	return hash(s, strlen(s));
}

uint64_t
hash_int(int i)
{
	return hash((char *)&i, sizeof i);
}

struct value {
	enum {
		VAL_INT,
		VAL_STR
	} type;

	union {
		int integer;
		char *string;
	} d;
};

void
print_value(struct value *v)
{
	if (!v) {
		printf("(nil)\n");
		return;
	}

	switch (v->type) {
	case VAL_INT: printf("%d\n", v->d.integer); break;
	case VAL_STR: printf("%s\n", v->d.string); break;
	}
}

#define NUM_TABLE_BUCKETS 32

struct table {
	struct bucket {
		uint64_t *key;
		struct value *val;
		size_t num;
	} bucket[NUM_TABLE_BUCKETS];
};

struct value *table_lookup(struct table *t, uint64_t key);
struct value *table_add(struct table *t, uint64_t key, struct value *v);
struct table *new_table();

struct table *
new_table()
{
	struct table *t = malloc(sizeof *t);
	memset(t, 0, sizeof *t);
	return t;
}

struct value *
table_add(struct table *t, uint64_t key, struct value *v)
{
	struct value *o = table_lookup(t, key);
	if (o) return *o = *v, o;

	int idx = key % NUM_TABLE_BUCKETS;
	struct bucket *b = t->bucket + idx;
	b->key  = realloc(b->key,  sizeof b->key[0]  * (b->num + 1));
	b->val  = realloc(b->val,  sizeof b->val[0]  * (b->num + 1));

	b->key [b->num] = key;
	b->val [b->num] = *v;

	b->num++;
}

struct value *
table_lookup(struct table *t, uint64_t key)
{
	int idx = key % NUM_TABLE_BUCKETS;

	struct bucket *b = t->bucket + idx;

	for (int i = 0; i < b->num; i++) {
		if (b->key[i]) {
			return b->val + i;
		}
	}

	return NULL;
}

int
main(int argc, char **argv)
{
	struct table *t = new_table();

	struct value v  = (struct value){ VAL_STR, { .string  = "test" } };
	struct value v2 = (struct value){ VAL_INT, { .integer = 256    } };

	table_add(t, hash_str("foo"), &v);
	print_value(table_lookup(t, hash_str("foo")));

	table_add(t, hash_str("bar"), &v2);
	print_value(table_lookup(t, hash_str("bar")));

	print_value(table_lookup(t, hash_str("nonexistant")));

	return EXIT_SUCCESS;
}
