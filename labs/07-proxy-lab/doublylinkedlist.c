#include "doublylinkedlist.h"

dll_t* dll_init() {
    dll_t *dll = malloc(sizeof(dll_t));
    if (dll) {
        dll->head = NULL;
        dll->tail = NULL;
        dll->size = 0;
    }
    return dll;
}

bool dll_insert_head(dll_t *dll, void *data, size_t data_size) {
    if (!dll || !data) return false;

    dll_node_t *new_head = malloc(sizeof(dll_node_t)), *old_head = dll->head;
    if (!new_head) return false;

    void *new_data = malloc(data_size);
    if (!new_data) {
        free(new_head);
        return false;
    }

    memcpy(new_data, data, data_size);
    new_head->data = new_data;
    new_head->prev = NULL;
    new_head->next = old_head; 
    
    dll->head = new_head;
    if (!dll->tail) {
        assert(dll->size == 0);
        dll->tail = new_head;
    } else {
        old_head->prev = new_head;
    }
    dll->size++;
    
    return true;
}

bool dll_insert_tail(dll_t *dll, void *data, size_t data_size) {
    if (!dll || !data) return false;

    dll_node_t *new_tail = malloc(sizeof(dll_node_t)), *old_tail = dll->tail;
    if (!new_tail) return false;

    void *new_data = malloc(data_size);
    if (!new_data) {
        free(new_tail);
        return false;
    }

    memcpy(new_data, data, data_size);
    new_tail->data = new_data;
    new_tail->prev = old_tail; 
    new_tail->next = NULL;

    dll->tail = new_tail;
    if (!old_tail) {
        assert(dll->size == 0);
        dll->head = new_tail;
    } else {
        old_tail->next = new_tail;
    }
    dll->size++;
    
    return true;
}

bool dll_remove_node(dll_t *dll, dll_node_t *node) {
    
    if (!dll || !node) return false;

    dll_node_t *next_node = node->next;
    dll_node_t *prev_node = node->prev;

    if (next_node)
        next_node->prev = prev_node;
    if (prev_node)
        prev_node->next = next_node;
    
    if (node == dll->head) {
        dll->head = next_node;
    } else if (node == dll->tail) {
        dll->tail = prev_node;
    }

    node->next = NULL;
    node->prev = NULL;
    
    free(node->data);
    free(node);
    dll->size--;

    return true;
}

bool dll_remove_head(dll_t *dll) {
    return dll_remove_node(dll, dll->head);
}

bool dll_remove_tail(dll_t *dll) {
    return dll_remove_node(dll, dll->tail);
}

void dll_free(dll_t *dll) {
    if (dll) {
        dll_node_t *curr_node = dll->head;
        while (curr_node) {
            dll_node_t *next_node = curr_node->next;
            free(curr_node->data);
            free(curr_node);
            curr_node = next_node;
        }
        free(dll);
    }
}