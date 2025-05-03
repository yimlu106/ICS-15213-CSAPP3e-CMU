#ifndef __DOUBLYLINKEDLIST_H__
#define __DOUBLYLINKEDLIST_H__

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Doubly Linked List */

typedef struct DLLNode {
    void *data;                // Pointer to the data
    struct DLLNode *next;      // Pointer to the next node
    struct DLLNode *prev;      // Pointer to the previous node
} dll_node_t;

typedef struct DLL {
    dll_node_t *head;          // Pointer to the head of the list
    dll_node_t *tail;          // Pointer to the tail of the list
    int size;                  // Number of elements in the list
} dll_t;

dll_t* dll_init();
bool dll_insert_head(dll_t *dll, void *data, size_t data_size);
bool dll_insert_tail(dll_t *dll, void *data, size_t data_size);
bool dll_remove_node(dll_t *dll, dll_node_t *node);
bool dll_remove_head(dll_t *dll);
bool dll_remove_tail(dll_t *dll);
void dll_free(dll_t *dll);

#endif /* __DOUBLYLINKEDLIST_H__ */