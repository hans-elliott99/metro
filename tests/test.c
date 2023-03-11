#include <stdio.h>

#include "cvector.h"
#include "string_methods.h"

/*TODO: Automate checking of accuracy (return True if all tests pass, etc.)*/

int test_cvector() {
	printf("\nCVECTOR:\n");
	// Initialize Cvector and add a bunch of elements	
	cvector_vector_type(int) v = NULL;
	for (int i = 0; i < 257; i++) {
		cvector_push_back(v, i);
	}
	// Print the first 10
	for (int i = 0; i < 10; i++)
		printf("%d == %d, ", i, v[i]);

	printf("\nCapacity = 256*2 == %lu\n", cvector_capacity(v));
	cvector_free(v);

	// String Vector
	cvector_vector_type(char*) str = NULL;
	cvector_push_back(str, "hello");
	cvector_push_back(str, "goodbye");
	for (size_t i = 0; i < cvector_size(str); ++i) {
		printf("string[%lu] = %s\n", i, str[i]);
	}
	cvector_free(str);

	// Vector of Array of Strings
	cvector_vector_type(char**) c = NULL;
	char *arr[] = {"Testing", "an", "array", "of", "strings"};
	cvector_push_back(c, arr);

	for (int i = 0; i < 5; i++) {
		printf("%s ", c[0][i]);
	}
	printf("\n\n");
	return 0;
}

int test_string_methods() {
	printf("STRING METHODS:\n");
	
	/* String Split & Trim Whitespace */
	char **tokens;
	char *s0, *s1, *s2, *s3;
	char bad_string[50] = "Cheers  ,  To,The,  Governor!  	\n\n";
	
	tokens = str_split(bad_string, ',');
	s0 = trim_witespace_inplace(tokens[0]);
	s1 = trim_witespace_inplace(tokens[1]);
	s2 = trim_witespace_inplace(tokens[2]);
	s3 = trim_witespace_inplace(tokens[3]);
	printf("_%s_%s_%s_%s_\n", s0,s1,s2,s3);
	// Strings can still be "free"d since they exist at same place in memory
	free(s0); free(s1); free(s2); free(s3);

	/* Test Strings for Digits and Decimals */
	char *row[] = {"123", "999.99", "100.0h", "word", "w0rd", "000", 0};
	for (int i = 0; row[i]; i ++) {
		if (digits_only(row[i])) {
			printf("%s = Int: %ld\n", row[i], atol(row[i]));
		} else if (digits_and_decimal(row[i])) {
			printf("%s = Float: %f\n", row[i], atof(row[i]));
		} else {
			assert( !digits_only(row[i]) & !(digits_and_decimal(row[i])) );
			printf("%s = String\n", row[i]);
		}
	}	

	printf("\n\n");
	return 0;
}



int main() {
	printf("Testing...\n");
	test_cvector();
	test_string_methods();
	return 0;
}