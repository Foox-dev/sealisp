#pragma once

#include <stdint.h>

typedef enum {
	NODE_BRANCH,
	NODE_LEAF,
	NODE_COLLISION,
} node_kind_t;

typedef struct trie_node trie_node_t;

struct trie_node {
	uint8_t kind;
	uint32_t bitmap;
	uint32_t refcount;
	void *slots[];
};

#define TRIE_BITS   5
#define TRIE_WIDTH  (1u << TRIE_BITS)
#define TRIE_MASK   (TRIE_WIDTH - 1u)
/* Sentinel refcount: node is permanently live, never freed (for canonical singletons). */
#define TRIE_STICKY UINT32_MAX

/* Number of populated slots in a node. */
static inline int trie_popcount(uint32_t bitmap) {
	return __builtin_popcount(bitmap);
}

/* Dense slot index for a given bit position in a bitmap. */
static inline int trie_slot_index(uint32_t bitmap, int bit) {
	return __builtin_popcount(bitmap & ((1u << bit) - 1));
}

trie_node_t *trie_node_alloc(node_kind_t kind, uint32_t bitmap);
void trie_node_retain(trie_node_t *node);
void trie_node_release(trie_node_t *node, void (*release_val)(void *));

/* Path-copy: return new node with slot at `bit` set to `val`.
   Retains shared branch/collision children; caller owns the new node. */
trie_node_t *trie_node_with_slot(trie_node_t *node, int bit, void *val);

/* Path-copy: return new node with slot at `bit` removed.
   Retains shared branch/collision children; caller owns the new node. */
trie_node_t *trie_node_without_slot(trie_node_t *node, int bit);
