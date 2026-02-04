#ifndef LLIST_H
#define LLIST_H

#include <stddef.h>

/**
 * Linked list node. Required to be freed.
 */
struct llist_node {
	struct llist_node* next;
	void* val;
	size_t val_size;
};

/**
 * Linked list.
 */
struct llist {
	struct llist_node* head;
	size_t len;
};

/**
 * Push copy of value to end of linked list.
 * Copy of value required to be freed from list using llist_empty.
 *
 * @param list Linked list to push value to.
 * @param val Pointer to value to copy.
 * @param val_size Size of value to copy.
 * @returns Pointer to copy of value added to linked list. NULL if error.
 */
void* llist_push(struct llist* list, const void* val, const size_t val_size);

/***
 * Push copies of values from one linked list to the end of another.
 * Copies of values required to be freed from list using llist_empty.
 *
 * @param dst Linked list to push values to.
 * @param src Linked list to push values from.
 * @returns Number of values copied.
 */
size_t llist_copy(struct llist* dst, const struct llist src);

/**
 * Get value at index of linked list.
 *
 * @param list Linked list to get value from.
 * @param ind Index of value to get.
 * @returns Pointer to value at index. NULL if not found.
 */
void* llist_get(const struct llist list, const size_t ind);

/**
 * Free values within linked list using delegate function.
 *
 * @param list Linked list to free values of.
 * @param f Delegate function used to free values.
 */
void llist_delegate_empty(struct llist* list, void (*f)(void*));

/**
 * Free values within linked list.
 *
 * @param list Linked list to free values of.
 */
void llist_empty(struct llist* list);

/**
 * Free values within linked list after given index using delegate function.
 *
 * @param list Linked list to free values of.
 * @param ind Index to free values from.
 * @param f Delegate function used to free values.
 */
void llist_part_delegate_empty(struct llist* list, const size_t ind, void (*f)(void*));

/**
 * Free values within linked list after given index.
 *
 * @param list Linked list to free values of.
 * @param ind Index to free values from.
 */
void llist_part_empty(struct llist* list, const size_t ind);

#endif
