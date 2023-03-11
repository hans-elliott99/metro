#include <stdio.h>
#include <stdlib.h>
#include "vector.h"


void vector_init(Vec *v) {
	// Initialize data
	v->Data.capacity = VECTOR_INIT_CAPACITY;
	v->Data.count = 0;
	v->Data.items = malloc(sizeof(void*) * VECTOR_INIT_CAPACITY);
}

int vector_count(Vec *v) {
	int total = VEC_FAIL;
	if (v) {
		total = v->Data.count;
	}
	return total;
}

int vector_resize(Vec *v, unsigned int capacity) {
	/*reallocate items in v to the new size, adding or removing elements as
		necessary.
	*/
	int status = VEC_FAIL;
	if (v) {
		void **items = realloc(v->Data.items, sizeof(void*) * capacity);
		if (items) {
			v->Data.items = items;
			v->Data.capacity = capacity;
			status = VEC_SUCCESS;
		}
	}
	return status;
}



int vector_set(Vec *v, int idx, void *item) {
	int status = VEC_FAIL;
	if (v) {
		if ((idx >= 0) && (idx < (int)v->Data.count)) {
			v->Data.items[idx] = item;
			status = VEC_SUCCESS;
		}
	}
	return status;
}

void* vector_get(Vec *v, int idx) {
	void *item = NULL;
	if (v) {
		if ((idx >= 0) && (idx < (int)v->Data.count)) {
			item = v->Data.items[idx];
		}
	}
	return item;
}

int vector_delete(Vec *v, int idx) {
	int status = VEC_FAIL;
	if (v) {
		if ((idx < 0) || (idx > (int)v->Data.count)) 
			return status;
		v->Data.items[idx] = NULL;
		//Shift all elememnts above the removed item to the left
		for (int i = idx; i < (int)v->Data.count - 1; i++) {
			v->Data.items[idx] = v->Data.items[idx + 1];
			v->Data.items[idx + 1] = NULL;
		}
		v->Data.count--;

		// Resize vector if it's been reduced significantly
		if ((v->Data.count > 0) && (v->Data.count == v->Data.capacity / 4)) {
			vector_resize(v, v->Data.capacity / 2);
		}
		status = VEC_SUCCESS;
	}
	return status;
}

int vector_free(Vec *v) {
	int status = VEC_FAIL;
	if (v) {
		free(v->Data.items);
		v->Data.items = NULL;
		v->Data.count = 0;
		status = VEC_SUCCESS;
	}
	return status;
}

void* vector_pop(Vec *v) {
	/*pop and return last element*/
	void *item = NULL;
	if (v) {
		if (v->Data.count < 1) {
			return item;
		} else {
			item = vector_get(v, v->Data.count-1);
			vector_delete(v, v->Data.count-1);
		}
	}
	return item;
}

int vector_pushback(Vec *v, void *item) {
	int status = VEC_FAIL;
	if (v) {
		if (v->Data.count == v->Data.capacity) {
			/*then we need to resize the vector*/
			status = vector_resize(v, v->Data.capacity * 2);
			if (status != VEC_FAIL) {
				v->Data.items[v->Data.count++] = item;
			}
		} else {
			/*don't need to resize*/
			v->Data.items[v->Data.count++] = item;
			status = VEC_SUCCESS;
		}
	}
	return status;
}

int vector_pushi(Vec *v, long int integer) {
	int status = vector_pushback(v, &integer);  
	return status;
}

// int vector_insert(Vec *v, int idx, void *item) {
// 	int status = VEC_FAIL;
// 	if (v) {
// 		if ((idx < 0) || (idx > (int)v->Data.count)) {

// 		} 
	
// 	}
// }

#include "vector.h"
void vector_test() {
	VECTOR_INIT(v);
	// APPENDING TO THE VECTOR AND GETTING ELEMENTS
	// char *s = "100.009"; vector_pushback(&v, s); OR:
	vector_pushback(&v, "some string");
	printf("some string: %s\n", (char*)vector_get(&v, 0));
	double x = 200.001;
	vector_pushback(&v, &x);
	printf("200.001, a double: %f\n", *(double*)vector_get(&v, 1));
	int y = 300;
	vector_pushback(&v, &y);
	printf("300, an int: %d\n", *(int*)vector_get(&v, 2));

	// DELETING ELEMENTS	
	printf("Num elements: %d\n", vector_count(&v));
	vector_delete(&v, 0);
	printf("Num elements after one delete: %d\n", vector_count(&v));

	// ADDING A BUNCH OF ELEMENTS
	printf("vector capacity: %d\n", v.Data.capacity);
	for (int i = 0; i < 200; i++) {
		vector_pushi(&v, i);
	}
	for (int i = 2; i < 200; i++)
		printf("%d ", *(int*)vector_get(&v, i));

	printf("Vector capacity after adding 200 items: %d\n", v.Data.capacity);
	printf("Pop last item (should be 200): %d\n", *(int*)vector_pop(&v));
	printf("Final vector count (should be 201): %d\n", vector_count(&v));
}

