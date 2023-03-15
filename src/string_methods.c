#include "string_methods.h"

#define _DEFAULT_SOURCE //for strdup, TODO: implement strdup
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

/* STRING METHODS
* TODO:
* Implement strings to int, float, etc... atoi/atol/atof not necessarily best:
* https://stackoverflow.com/questions/7021725/how-to-convert-a-string-to-integer-in-c
*/

/*TODO: Handle multiple delims (10,,,9,8) in a row
strtok explanation: https://stackoverflow.com/questions/3889992/how-does-strtok-split-the-string-into-tokens-in-c
*/
char** str_split(char* the_str, const char the_delim) {
	char **result_arr = 0;
	size_t count      = 0;
	char* tmp         = the_str;
	char* last_comma  = 0;
	char delim[2]     = {the_delim, 0};

	while (*tmp) {
		if (the_delim == *tmp) {
			count++;
			last_comma = tmp;
		}
		tmp++;
	}
	// Add space for a trailing token and the terminating null string.
	// (If the last comma is not at the end of the string, increment count)
	count += last_comma < (the_str + strlen(the_str) - 1);	
	count++;

	result_arr = malloc(sizeof(char*) * count);
	if (result_arr) {
		size_t idx = 0;
		char *token = strtok(the_str, delim);
		while (token) {
			// Extract a token and copy it into the result memory
			assert(idx < count);
			*(result_arr + idx++) = strdup(token);
			// To continue to the next token
			token = strtok(0, delim);
		}

		assert(idx == count - 1);
		*(result_arr + idx) = 0;
	}
	return result_arr;
}

char* strip_witespace(char *s) {
	//https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
	char *original = s;
	size_t len = 0;

	while (isspace((unsigned char) *s)) {
		s++;
	}
	if (*s) {
		char *p = s;
		while (*p) p++;
		while (isspace((unsigned char) *(--p)));
		p[1] = '\0';
		len = (size_t)(p - s + 1);
	}
	return (s == original) ? s : memmove(original, s, len+1);
}

int digits_only(const char *s) {
	while (*s) {
		if (isdigit(*s++) == 0) return 0;
	}
	return 1;
}

int digits_and_decimal(const char *s) {
	while (*s) {
		if ((!isdigit(*s)) & (*s != '.')) {
			return 0;
		}
		s++;
	}
	return 1;
}


