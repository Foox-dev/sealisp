#include <stdlib.h>

#include "trie.h"

trie_node_t *trie_node_alloc(node_kind_t kind, uint32_t bitmap) {
	int count;
	trie_node_t *node;

	count = trie_popcount(bitmap);
	node = malloc(sizeof(trie_node_t) + count * sizeof(void *));
	node->kind = kind;
	node->bitmap = bitmap;
	node->refcount = 1;
	return node;
}

void trie_node_retain(trie_node_t *node) {
	if (node->refcount != TRIE_STICKY) {
		node->refcount++;
	}
}

void trie_node_release(trie_node_t *node, void (*release_val)(void *)) {
	int count, i;

	if (node->refcount == TRIE_STICKY) {
		return;
	}
	node->refcount--;
	if (node->refcount > 0) {
		return;
	}

	count = trie_popcount(node->bitmap);
	if (node->kind == NODE_LEAF) {
		if (release_val) {
			for (i = 0; i < count; i++) {
				release_val(node->slots[i]);
			}
		}
	} else {
		for (i = 0; i < count; i++) {
			trie_node_release((trie_node_t *)node->slots[i], release_val);
		}
	}
	free(node);
}

trie_node_t *trie_node_with_slot(trie_node_t *node, int bit, void *val) {
	uint32_t mask, new_bitmap;
	int new_idx, old_count, new_count, i;
	int replacing;
	trie_node_t *new_node;

	mask = 1u << bit;
	replacing = (node->bitmap & mask) != 0;
	new_bitmap = node->bitmap | mask;
	old_count = trie_popcount(node->bitmap);
	new_count = trie_popcount(new_bitmap);
	new_node = trie_node_alloc(node->kind, new_bitmap);
	new_idx = trie_slot_index(new_bitmap, bit);

	for (i = 0; i < new_idx; i++) {
		new_node->slots[i] = node->slots[i];
		if (node->kind != NODE_LEAF) {
			trie_node_retain((trie_node_t *)node->slots[i]);
		}
	}

	new_node->slots[new_idx] = val;

	if (replacing) {
		for (i = new_idx + 1; i < new_count; i++) {
			new_node->slots[i] = node->slots[i];
			if (node->kind != NODE_LEAF) {
				trie_node_retain((trie_node_t *)node->slots[i]);
			}
		}
	} else {
		for (i = new_idx; i < old_count; i++) {
			new_node->slots[i + 1] = node->slots[i];
			if (node->kind != NODE_LEAF) {
				trie_node_retain((trie_node_t *)node->slots[i]);
			}
		}
	}

	return new_node;
}

trie_node_t *trie_node_without_slot(trie_node_t *node, int bit) {
	uint32_t mask, new_bitmap;
	int old_idx, old_count, i;
	trie_node_t *new_node;

	mask = 1u << bit;
	new_bitmap = node->bitmap & ~mask;
	old_count = trie_popcount(node->bitmap);
	new_node = trie_node_alloc(node->kind, new_bitmap);
	old_idx = trie_slot_index(node->bitmap, bit);

	for (i = 0; i < old_idx; i++) {
		new_node->slots[i] = node->slots[i];
		if (node->kind != NODE_LEAF) {
			trie_node_retain((trie_node_t *)node->slots[i]);
		}
	}

	for (i = old_idx + 1; i < old_count; i++) {
		new_node->slots[i - 1] = node->slots[i];
		if (node->kind != NODE_LEAF) {
			trie_node_retain((trie_node_t *)node->slots[i]);
		}
	}

	return new_node;
}
