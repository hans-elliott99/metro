#ifndef STRING_METHODS_H_
#define STRING_METHODS_H_


char** str_split(char* the_str, const char the_delim);
char* trim_witespace_inplace(char *s);
int digits_only(const char *s);
int digits_and_decimal(const char *s);


#endif