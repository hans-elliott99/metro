#include <stdio.h>
#include "common.h"

// TODO: add tests

int test_array_pos(void) {
	int success = 0;
	int a[5] = {0, 1, 2, 3, 4};
	for (int i = 0; i < 5; i++) {
		int idx = array_pos(a, i, sizeof(a));
		if (idx != i) {
			success++;
		}
	}
	return success;
} 

int test_skip_whitechar(void) {
	return 0;
}

int test_check_moveto_eol(void) {
	return 0;
}

int test_isdelim(void) {
	return 0;
}



////////////////////////
void test_test(void) {
	printf("- test_array_pos: ");
	printf(test_array_pos() ? "failed\n" : "passed\n");
}

int main() {
	printf("Running tests...\n");
	test_test();
	return 0;
}