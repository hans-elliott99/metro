#ifndef COMMON_H_
#define COMON_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>

#define STOP(...) do {fprintf(stderr, __VA_ARGS__); exit(0); } while(0)
#define NA_INT32 INT32_MIN
#define NA_FLOAT FLT_MIN

// GLOBALS
FILE *fp;
const char *sol, *eol;
char **colnames;

struct mainArgs {
    // arguments supplied by user (optionally)
    char *filename;
    // const char *input; //TODO: figure out piping on cli
    bool header; //whether there is a row of col names or not
    bool header_keep_quotes; //whether to keep quotes when parsing header, def false
    int64_t rowLimit; //how many rows to read in
    char sep; //the field seperator/delimiter, can be guessed
    char dec; //the decimal - default '.', could be ','
    char quote; //quote rule - not yet supported
    char **colselect; //the selected cols

    // from readfile
    int skiprows; //n rows to skip before start processing txt
    int skipchars; //in first usable row, n of chars to skip before start processing txt
    int ncols; //n of detected cols
    int n_colselect; //n of selected cols
    bool *col_is_selected;  //array, len==ncols, true at indices of user selected cols
    int *selected_col_inds; //array, len==n_colselect, contains indices of the selected cols, SORTED left to right 
    char whiteChar; //what is considered whitespace - ' ', '\t', or  for both
    // char **colnames; //array of column names
    bool stripWhite; //whether to strip whitespace, will be dependent on operation
} args;

// HELPERS
void clean_globals(void);
int array_pos(int *arr, int val, size_t arr_size);


// FIELD PARSING
struct FieldContext {
    int32_t off; //offset from sol
    int32_t len; //len from off to end of field
};
// another struct could have row number, field-offset, field-length



void skip_whitechar(const char **pch);
bool check_moveto_eol(const char **pch);
int isdelim(const char c);
char detect_fieldsep(const char *ch, int32_t llen);
bool check_end_of_field(const char *ch);
void parse_field(const char **pch, int32_t *pFieldOff, int32_t *pFieldLen, int keepQuotes);
int countfields(const char *ch);
int iterfields(const char *ch, const int ncol, struct FieldContext *fields, int keepQuotes);
float field_to_float(const char *sol, struct FieldContext field);


#endif //COMMON_H_