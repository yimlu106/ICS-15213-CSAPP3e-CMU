#include <assert.h>
#include "../../doublylinkedlist.h"

dll_t* test_dll_init() {
    dll_t *mydll = dll_init();
    assert(mydll->head == NULL && mydll->tail == NULL && mydll->size == 0);
    return mydll;
}

void test_dll_free(dll_t *dll) {
    dll_free(dll);
}

void test_dll_insert_remove(dll_t *dll) {
    
    assert(dll->size == 0);

    int data1[4] = {1, 2, 3, 4};
    assert(dll_insert_head(dll, 2, data1, 4 * sizeof(int)));
    assert(dll->head->data && dll->size == 1);
    for (int i = 0; i < 4; i++) {
        assert(((int *) dll->head->data)[i] == data1[i]);
    }

    char data2[6] = "hello";
    assert(dll_insert_tail(dll, 3, data2, 6 * sizeof(char)));
    assert(dll->tail->data && dll->size == 2);
    for (int i = 0; i < 6; i++) {
        assert(((char *) dll->tail->data)[i] == data2[i]);
    }

    int data3 = 23;
    assert(dll_insert_head(dll, 1, &data3, sizeof(int)));
    assert(dll->head->data && dll->size == 3);
    assert((*((int *)dll->head->data)) == data3);

    assert(dll_remove_node(dll, dll->head->next) == 2);
    assert(dll->size == 2);
    assert((*((int *)dll->head->data)) == data3);
    for (int i = 0; i < 6; i++) {
        assert(((char *) dll->head->next->data)[i] == data2[i]);
    }

    assert(dll_remove_tail(dll) == 3);
    assert(dll->size == 1);
}

int main() {

    dll_t *mydll = test_dll_init();
    test_dll_insert_remove(mydll);
    test_dll_free(mydll);
    printf("tests on doubly linked list all passed!\n");

    return 0;
}