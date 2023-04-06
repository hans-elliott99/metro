#ifndef COMMON_H_
#define COMON_H_

#include <stdlib.h>
#include <stdio.h>

#define STOP(...) do {fprintf(stderr, __VA_ARGS__); exit(0); } while(0)

struct mainArgs {
    const char *filename;
    // const char *input; //TODO: figure out piping on cli
    bool header;
    int64_t rowLimit;
    char sep;
    char dec;
    char quote;
    bool stripWhite;
    char **colselect;
};

struct readfileInfo {
    // provided by user (although may be modifed by readfile)
    const char *filename;
    bool header;
    int64_t rowLimit;
    char sep;
    char dec;
    char quote;
    // from readfile
    int skiprows; //n rows to skip before start processing txt
    int skipchars; //in first usable row, n of chars to skip before start processing txt
    int ncols; //n of detected cols
    int n_colselect; //n of selected cols
    bool *col_is_selected;  //array, len==ncols, true at indices of user selected cols
    int *selected_col_inds; //array, len==n_colselect, contains indices of the selected cols 
    char whiteChar; //what is considered whitespace - ' ', '\t', or  for both
    char **colnames; //array of column names
};


#endif //COMMON_H_