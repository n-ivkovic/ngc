#include "llist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void* llist_push(struct llist* list, const void* val, const size_t val_size)
{
	struct llist_node* node = NULL;
	void* node_val = NULL;

	if (!list)
		goto error;

	// Alloc value of new list node
	node_val = calloc(1, val_size);
	if (!node_val)
		goto error;

	// Copy to value of new list node
	if (!memcpy(node_val, val, val_size))
		goto error;

	// Alloc new list node
	node = malloc(sizeof(struct llist_node));
	if (!node)
		goto error;

	// Init new list node
	node->next = NULL;
	node->val = node_val;
	node->val_size = val_size;

	// List is empty - set new list node as head
	if (!list->head) {
		list->head = node;
		list->len++;
		return node_val;
	}

	// Loop through list until last node is reached
	struct llist_node* tail = list->head;
	for (; tail->next; tail = tail->next);

	// Push new node to back of list
	tail->next = node;
	list->len++;

	return node_val;

	error:
	if (node) free(node);
	if (node_val) free(node_val);
	return NULL;
}

size_t llist_copy(struct llist* dst, const struct llist src)
{
	if (!dst)
		return 0;

	size_t pushed = 0;
	size_t dst_len_prev = dst->len;

	struct llist_node* node = src.head;
	for (size_t ind = 0; node && ind < src.len; node = node->next, ind++) {
		if (!llist_push(dst, node->val, node->val_size)) {
			llist_part_empty(dst, dst_len_prev);
			return 0;
		}

		pushed++;
	}

	return pushed;
}

void* llist_get(const struct llist list, const size_t ind)
{
	// Index out of bounds
	if (ind >= list.len)
		return NULL;

	struct llist_node* node = list.head;
	size_t node_ind = 0;

	// Loop through list until index found or a NULL node reached
	for (; node && node_ind < ind; node = node->next, node_ind++);

	if (!node)
		return NULL;

	return node->val;
}

/**
 * Free linked list node value.
 *
 * @param val Value to free.
 */
static void llist_val_free(void* val)
{
	if (val) free(val);
}

static void llist_node_free(struct llist_node* node, void (*f)(void*))
{
	if (!node)
		return;
		
	if (f)
		f(node->val);
	
	free(node);
}

void llist_delegate_empty(struct llist* list, void (*f)(void*))
{
	if (!list)
		return;

	list->len = 0;

	while (list->head) {
		// Pop first node off list
		struct llist_node* head_prev = list->head;
		list->head = list->head->next;

		// Free popped node
		llist_node_free(head_prev, f);
	}
}

inline void llist_empty(struct llist* list)
{
	llist_delegate_empty(list, llist_val_free);
}

void llist_part_delegate_empty(struct llist* list, const size_t ind, void (*f)(void*))
{
	// Empty whole list using better suited function
	if (ind == 0) {
		llist_delegate_empty(list, f);
		return;
	}

	if (!list)
		return;

	// Index out of bounds
	if (ind >= list->len)
		return;

	struct llist_node* node_curr = list->head;
	struct llist_node* node_prev = NULL;

	// Loop through list until node at index found or a NULL node reached
	for (size_t node_ind = 0; node_curr && node_ind < ind; node_ind++) {
		node_prev = node_curr;
		node_curr = node_curr->next;
	}

	// NULL node reached before index
	if (!node_curr)
		return;

	// Sever nodes from index onwards
	node_prev->next = NULL;
	list->len = ind;

	while (node_curr) {
		// Loop to next node
		node_prev = node_curr;
		node_curr = node_curr->next;

		// Free previous node
		llist_node_free(node_prev, f);
	}
}

inline void llist_part_empty(struct llist* list, const size_t ind)
{
	llist_part_delegate_empty(list, ind, llist_val_free);
}
