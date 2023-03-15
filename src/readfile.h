#ifndef READFILE_H_
#define READFILE_H_

#include <stdlib.h>
#include <stdio.h>

#define STOP(...) do {fprintf(stderr, __VA_ARGS__); exit(0); } while(0)

int read_file(char *filename);


#endif //READFILE_H_