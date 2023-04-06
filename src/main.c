#include <stdio.h>
#include <stdlib.h>

#include "readfile.h"

#define BUFSIZE 8192 //eventually this will have to be much bigger or dynamic 
#define END_OF_LINE -1


void test_readfile(int argc, char **argv) {
	// TODO: implement piping input from command line https://www.delftstack.com/howto/c/pipe-in-c/
	if (argc == 1) {
		fprintf(stderr, "ERROR: No file provided.\n");
		return;
	}
	read_file(argv[1]);
}


int main(int argc, char **argv) {
	// load_file(argc, argv);
	test_readfile(argc, argv);
}
