#define CVECTOR_LOGARITHMIC_GROWTH

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "cvector.h"
#include "cvector_utils.h"
#include "string_methods.h"
#include "readfile.h"

#define BUFSIZE 8192 //eventually this will have to be much bigger or dynamic 
#define END_OF_LINE -1


enum DataTypes {
	STR = 0,
	INT = 1,
	FLOAT = 2,
	DOUBLE = 3
};


struct FileReader {
	char **names;
	char **keep_names;
	int *types;
	size_t start_row;
	size_t start_col;
	cvector_vector_type(char**) rows;
};

char** read_header(char *line) {
	// Take in first line, determine number of columns (and eventually data type)
	// and split them based on delimiter to get column names.
	// Eventually also filter out columns based on input
	char **names;
	const char delim = ',';
	names = str_split(line, delim);
	return names;
}

char** read_line(char *line) {
	char **elements;
	const char delim = ',';
	elements = str_split(line, delim);
	// trim whitepace: memcpy (https://stackoverflow.com/questions/26329140/replacing-strings-in-an-array-in-c)
	return elements;
}

int get_types(char **tokens) {
	printf("Getting types...\n");
	for (size_t i = 0; *(tokens + i); i++) {
		// Trime whitespace, then evaluate to see if it is a digit
		char *tokenp = *(tokens + i);
		strip_witespace(tokenp);
		if (digits_only(tokenp)) {
			printf("%s is an int\n", tokenp);
		} else if (digits_and_decimal(tokenp)) {
			printf("%s is a float\n", tokenp);
		} else {
			printf("%s is a string\n", tokenp);
		}
	}
	return 0;
}




int load_file(int argc, char **argv) {

	FILE *infile;

	if (argc == 1) {
		fprintf(stderr, "ERROR: No file provided.\n");
		return 1;
	}

	infile = fopen(argv[1], "r");
	/*
		Read a line from the file and split on delimeter
	*/
	char line[BUFSIZE];
	size_t row_i = 0;
	char **tokens;
	// int *types;
	while(fgets(line, BUFSIZE - 1, infile) != NULL) {
		/*Header (first row)*/
		if (row_i == 0) {
			tokens = read_header(line);
		} else if (row_i == 1) {
		/*Deduce Types*/
			tokens = read_line(line);	
			get_types(tokens); //types=
		} else {
		/*Other Lines*/
			tokens = read_line(line);
		}
		row_i++;
		// // For now, just print them
		// for (int i = 0; *(tokens + i); i++) {
		// 	printf("%s ", *(tokens + i));
		// 	free(*(tokens + i));
		// }
		free(tokens);	
	}

	fclose(infile);
	return 0;
}


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

/*
NOTES:
	columns cut cl tool: https://github.com/ColumPaget/ColumsCut
*/
