#include <stdlib.h>

size_t size = sizeof(int);

void some_function() {
	size = sizeof(long); // side effects, modifies global state
}

int main() {
	size = sizeof(unsigned int);

	some_function(); // has side effect
	
	int* int_array = (int*) malloc(5 * size);
	free(int_array);

	return 0;
}
