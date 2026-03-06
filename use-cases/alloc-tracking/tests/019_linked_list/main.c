#include <stdlib.h>


typedef struct list_node {
    int data;
    struct list_node* next;
} list_node_t;

list_node_t* create_list(int length) {
    if (length <= 0) return NULL;
    
    list_node_t* head = malloc(sizeof(list_node_t));
    head->data = 0;
    
    list_node_t* current = head;
    for (int i = 1; i < length; i++) {
        current->next = malloc(sizeof(list_node_t));
        current = current->next;
        current->data = i;
    }
    current->next = NULL;
    
    return head;
}

int main() {
    list_node_t* head = create_list(5);

    return 0;
}
