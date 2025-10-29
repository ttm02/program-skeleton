#include <stdlib.h>
#include <stdio.h>

typedef struct node {
    int value;
    struct node* left;
    struct node* right;
} node_t;

node_t* create_tree(int depth) {
    if (depth <= 0) return NULL;
    
    node_t* root = malloc(sizeof(node_t));
    root->value = depth;
    root->left = create_tree(depth - 1);
    root->right = create_tree(depth - 1);
    
    return root;
}

void process_tree(node_t* root) {
    if (root->left != NULL) {
        int value = root->left->value;
        printf("val: %d\n", value);
    }
}


int main() {
    node_t* root = create_tree(5);

    process_tree(root);

    return 0;
}
