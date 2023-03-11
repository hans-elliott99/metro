#ifndef VECTOR_H_
#define VECTOR_H_

// #include <stdio.h>
// #include <stdlib.h>
#define VEC_SUCCESS 0
#define VEC_FAIL -1
#define VECTOR_INIT_CAPACITY 6
#define VECTOR_INIT(v) Vec v;\
	vector_init(&v)

struct VectorData
{
	void **items;  //array of void pointers
	unsigned int count; //number of elements
	unsigned int capacity;  //total capacity of the vector 	unsigned int count;     //number of elements in the vector
};
// (total memory usage would be capacity*sizeof(void*))
/*
	Should use a union because on some platflorms sizeof void* is not >= int:
	https://stackoverflow.com/questions/7828393/c-programming-casting-a-void-pointer-to-an-int#:~:text=That%20will%20work%20on%20all%20platforms/environments%20where%20sizeof(void*)%20%3E%3D%20sizeof(int)%2C%20which%20is%20probably%20most%20of%20them%2C%20but%20I%20think%20not%20all%20of%20them.%20You%27re%20not%20supposed%20to%20rely%20on%20it.
*/


typedef struct Vector Vec;
struct Vector
{
	struct VectorData Data;
};

int vector_count(Vec *v);
int vector_pushback(Vec *v, void *item);
int vector_pushi(Vec *v, long int integer);

int vector_set(Vec *v, int idx, void *item);
void* vector_get(Vec *v, int idx);
int vector_delete(Vec *v, int idx);
void* vector_pop(Vec *v);
int vector_free(Vec *v);
void vector_init(Vec *v);
int vector_resize(Vec *v, unsigned int capacity);

#endif //VECTOR_H_