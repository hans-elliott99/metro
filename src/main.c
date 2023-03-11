#define CVECTOR_LOGARITHMIC_GROWTH

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include <assert.h>
#include <ctype.h>

#include "cvector.h"
#include "cvector_utils.h"
#include "string_methods.h"

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
	return elements;
}

int get_types(char **tokens) {
	printf("Getting types...\n");
	for (size_t i = 0; *(tokens + i); i++) {
		printf("%s %d\n",
			 *(tokens + i),
			 digits_and_decimal(*(tokens + i)));
	}
	return 0;
}


// TODO:
// Start implementing data.frame
// Implement cvector_extras as needed (sorting, unique, etc.)
//		Look at R's vector and see what methods are established.
//		Also, technically R's data.frames are named lists of arrays, where the
//		arrays (/vectors) are required to be the same length. 



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


int main(int argc, char **argv) {
	load_file(argc, argv);
}

/*
TODO:
- string splitting and cleaning is done, need to work on reading lines into a 
	dataframe format, storing column info, table info, etc.

NOTES:
- Reading in text files - is the only way to iterate over rows?
- If all columns are the same data-type, then the dataframe can be treated as a
  matrix and could in theory use a contiguous array for all data 
- How to define NA values (NULL ? then control flow for null)



columns cut cl tool: https://github.com/ColumPaget/ColumsCut
*/