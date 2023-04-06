// #include <sys/mman.h> //mmap
// #include <sys/stat.h> //fstat
// #include <fcntl.h> //open

#include <stdio.h> //printf, fopen/fclose, getdelim
#include <stdint.h> //int_t
#include <string.h> //strlen
#include <errno.h> //errno
#include <stdlib.h> //exit
#include <stdbool.h> //true/false
#include <ctype.h> //isspace, isdigit, 
#include <unistd.h> //ssize_t
#include "readfile.h"
#include "common.h" //mainArgs, readfileInfo, STOP,

#define MAXCOLS 1000 //TODO: pick a more appropriate number
extern int errno;

/* Private globals */
// To save passing to many functions
static const char *sol, *eol;

// Params
static int skiprows = 0; //n rows to skip before start processing txt
static int skipchars = 0; //in first usable row, n of chars to skip before start processing txt
static bool *col_is_selected; //array, len == ncols, indexes user selected col(s)
static int *selected_col_inds; //array, len == n selected cols, indexes data cols 
static char sep; //ie, the delimiter
static char whiteChar; //what is considered whitespace - ' ', '\t', or 0 for both
static char quote, dec; //quote style and decimal style
static bool stripWhite = true; //strip whitespacde
static bool header = true; //is the first row a row of column names
static char **colnames;
static int ncols = 0;
static int n_colselect = 0;
// To clean up on error or return
// static size_t fileSize;
// static int8_t *type = NULL, *tmpType = NULL, *size = NULL;
// static lenOff *colNames = NULL;


// For testing
struct mainArgs args = {
    .header = true,
    .rowLimit = INT64_MAX,
    .sep = 0,
    .dec = '.',
    .quote = '"'
    // .colselect = {"\"mpg\"", "\"drat\""} 
    //TODO: parse these from user input, make colselect[0]=NULL if not using, maybe rm quotes so user doesnt have to deal w it
}; 

void parse_selected_cols(void) {
    const char *sc[2] = {"\"drat\"", "\"mpg\""};
    n_colselect = (int)(sizeof(sc) / sizeof(*sc));
    args.colselect = malloc(sizeof(sc));
    for (int i = 0; i < n_colselect; ++i) {
        args.colselect[i] = malloc(sizeof(char*));
        strcpy(args.colselect[i], sc[i]);
        printf("Select col: %s\n", args.colselect[i]);
    }
}

struct FieldContext {
    int32_t off; //offset from sol
    int32_t len; //len from off to end of field
};
// another struct could have row number, field-offset, field-length


/* Utils */
void readfile_cleanup(void) {
    if (ncols > 0) {
        for (int i = 0; i < ncols; i++) {
            free(colnames[i]);
        }
        free(colnames); colnames = NULL;
    }
}



// // Recast a 'const char' as a 'char'
// static char* _const_cast(const char *ptr) {
//     union {const char *a; char *b;} tmp = { ptr };
//     return tmp.b;
// }


// arr = 2, 3, 5, 9  | val = 5
//     --0--1--^ ----> return 2
int array_pos(int *arr, int val, size_t arr_size) {
    if (arr) {
        for (size_t i = 0; i < arr_size; ++i) {
            if (arr[i] == val) {
                return i;
            }
        }
    }
    return -1;
}

// Move to the next non whitespace character
void skip_whitechar(const char **pch) {
    const char *ch = *pch;
    if (whiteChar == 0) {
        while (*ch == ' ' || *ch == '\t' || (*ch == '\0' && ch < eol)) ch++;
    } else {
        while (*ch==whiteChar || (*ch == '\0' && ch < eol)) ch++;
    }
    *pch = ch;
}

// Given a position, this tests if any common (and not so) line endings occur at 
// the position and moves to the end of the line if they do.
// (for a standard newline '\n', pch is not moved since it is already pointing
// at the end of the line. but if there are carriage returns, we want to move
// past those)
bool test_moveto_eol(const char **pch) {
    const char *ch = *pch;
    while (*ch == '\r') ch++; //skip leading carriage returns
    if (*ch == '\n') {
        while (ch[1] == '\r') ch++; //skip preceding carriage returns
        *pch = ch;
        return true;
    }
    return false; //if accounting for case of file with one single \r, see fread.c eol() 
}

// the delims we auto-check for
// note: wont check for space, since all filetypes have lots of spaces
int isdelim(const char c) {
    if (c==',' || c=='\t' || c=='|' || c==';' || c==':') {
        return 1;
    }
    return 0;
}

// TODO? do we need to consider quote rules here?
char detect_fieldsep(const char *ch, int32_t llen) {
    char sep = 0;
    const char *eol = ch + llen;
    // skip any leading whitespace
    while (isspace(*ch) && ch < eol) ch++;
    enum seps{comma = 0, tab, pipe, semi, colon};
    int nseps = 6;
    int scores[nseps]; //initialize w zeros
    memset(scores, 0, sizeof(scores));
    // simple: count n of ocurrences of each sep in the given line
    while (ch < eol) {
        if (!isdelim(*ch)) {
            // printf(" %c ", *ch);
            ch++;
        } else {
            // printf(" !%c!", *ch);
            switch(*ch++) {
                case ',' : scores[comma]++; break;
                case '\t': scores[tab]++; break;
                case '|' : scores[pipe]++; break;
                case ';' : scores[semi]++; break;
                case ':' : scores[colon]++; break;
            }
        }
    }
    int best_score = scores[comma];
    int best_sep = comma;
    for (int i = 1; i < nseps; ++i) {
        if (scores[i] > best_score) {
            best_sep = i;
            best_score = scores[i];
        }
    }
    // printf("best sep: %d, score: %d\n", best_sep, best_score);
    if (best_score > 0) {
        switch(best_sep) {
            case comma : sep = ','; break;
            case tab   : sep = '\t'; break;
            case pipe  : sep = '|'; break;
            case semi  : sep = ';'; break;
            case colon : sep = ':'; break;
        }
    } else {
        sep = '\0';
    }
    return sep;
}

// Test if the current character is at the end of a field - either based on 'sep'
// or based on a line-ending sequence
bool test_end_of_field(const char *ch) {
    return ((*ch==sep) || ((*ch=='\n' || *ch=='\r' || *ch=='\0') 
                                        && (ch==eol || test_moveto_eol(&ch))));
}

// Advance the char pointer to the end of the current field, advancing the
// offset from the start of the line and calculating the field len (n chars)
void parse_field(const char **pch, int32_t *pFieldOff, int32_t *pFieldLen) {
    // The idea - advance fieldOff to be an int from start-line to start-field,
    //        then record fieldLen from start-field to end-field
    const char *ch = *pch;
    int32_t fieldOff = *pFieldOff; // may be greater than 0
    int32_t fieldLen = 0;
    // skip leading spaces
    if ((*ch == ' ' && stripWhite) || (*ch == '\0' && ch < eol)) {
        while(*(ch) == ' ' || (*ch == '\0' && ch < eol)) {
            ch++; fieldOff++; 
        }
    } 
    const char *fieldStart = ch;
    // TODO: implement alternative quote rules. For now, just keep all quotes.
    // if (*ch != quote ) or it does but we want to keep them:  
    {
        while (!test_end_of_field(ch)) ch++; //will end on sep, \n, \r, or eol
        fieldLen = (int32_t)(ch - fieldStart);
        // remove any lagging spaces 
        while (fieldLen > 0 && ((ch[-1]==' ' && stripWhite) || ch[-1]=='\0')) {
            fieldLen--; ch--;
        }
        if (fieldLen == 0) fieldLen = INT32_MIN; //blanks are NAs
    }
    *pch = ch;
    *pFieldOff = fieldOff;
    *pFieldLen = fieldLen;
}


// Counts the number of fields in a line, given pointer to start of line
int countfields(const char *ch) {
    // skip starting spaces even if sep==space, since they don't break up fields,
    // (skip_whitechar would ignore them since it doesn't skip seps)
    int ncol = 1;
    int32_t fieldOff = 0;
    int32_t fieldLen = 0;
    if (sep == ' ') while (*ch == ' ') ch++;
    skip_whitechar(&ch);
    if (test_moveto_eol(&ch)) {
        return 0; //empty line, 0 fields
    }
    while (ch < eol) {
        parse_field(&ch, &fieldOff, &fieldLen); 
        // parse_field advances ch to either sep or eol (\n, \r)
        if (sep==' ' && *ch==sep) {
            while (ch[1]==' ') ch++; //skip over multiple spaces
            // if next character is an end-of-line char, advance to it
            if (ch[1] == '\r' || ch[1] == '\n' || (ch[1] == '\0' && ch+1 == eol)) {
                ch++;
            }
        }
       // if sep, we have found a new field so count it and move to next
        if (*ch == sep) {
            ncol++; ch++;
            continue;
        }
        // if ch is an eol, we conclude
        // (this is what we want to happen for well behaved files)
        if (test_moveto_eol(&ch)) { 
            return ncol; 
        }
        if (ch!=eol) return -1; //only reachable if sep & quote rule are invalid for this line
    }
    return ncol;
}

// Takes char pointing to start of line, takes the desired ncols, and the 
// FieldContext array, which it will populate.
// Returns: number of found fields, 0 if none, -1 if error
int iterfields(const char *ch, const int ncol, struct FieldContext *fields) {
    //TODO: implement checking if n of parsed fields == ncol, + error handling
    const char *start = ch;
    int32_t fieldOff = 0;
    int32_t fieldLen = 0;
    int n_fields = 1;
    // skip starting spaces even if sep==space, since they don't break up fields,
    // (skip_whitechar ignores them since it doesn't skip seps)
    if (sep == ' ') while (*ch == ' ') {ch++;}
    skip_whitechar(&ch);
    if (test_moveto_eol(&ch)) {
        return 0; //empty line, 0 fields
    }
    while (ch < eol) {
        fieldOff = ch - start;
        parse_field(&ch, &fieldOff, &fieldLen);
        // parse_field advances ch to either sep or eol (\n, \r)
        fields[n_fields-1].off = fieldOff;
        fields[n_fields-1].len = fieldLen;
        if (sep == ' ' && *ch==sep) {
            while (ch[1]==' ') ch++; //skip over multiple spaces
            // if next character is an end-of-line char, advance to it
            if (ch[1] == '\r' || ch[1] == '\n' || (ch[1] == '\0' && ch+1 == eol)) {
                ch++;
            }
        }
       // if sep, we have found a new field so count it and move to next
        if (*ch == sep) {
            n_fields++; ch++;
            continue;
        }
        // if ch is at eol, conclude
        // (this is what we want to happen for well behaved files)
        if (test_moveto_eol(&ch)) {
            //TODO: handle cases of inconsistent row length
            if (n_fields < ncol) printf("Internal error: iterfields: Number of fields (%d) did not reach ncol (%d)\n", n_fields, ncol);
            if (n_fields > ncol) printf("Internal error: iterfields: Number of fields (%d) exceeds ncol (%d)\n", n_fields, ncol);
            return n_fields; 
        }
        if (ch!=eol) return -1; //should be only reachable if sep & quote rule are invalid for this line
    }
    return n_fields;
}


void pretty_print_table(FILE *fp) {    
    int32_t row = 0;
    int delim = '\n'; //TODO: implement the other common delims, like \r\n ?
    char* line = NULL; size_t lsize = 0; ssize_t llen = 0;
    while ((llen = getdelim(&line, &lsize, delim, fp)) > -1) { 
        if (row < skiprows) {
            row++; continue;
        }
        sol = line + skipchars;
        eol = line + llen;
        struct FieldContext fields[ncols];
        if (iterfields(sol, ncols, fields) < 1) STOP("Column names could not be parsed.\n"); //iterfields stores discovered field info in FieldContext   
        
        for (int i = 0; i < ncols; ++i) {
            if (!col_is_selected[i]) 
                continue;
            
            int idx = array_pos(selected_col_inds, i, n_colselect) + 1; 
            for (int32_t j = 0; j < fields[i].len; ++j)
                printf("%c", *(sol + fields[i].off + j));
            putchar('\t');
            if (idx == n_colselect) 
                putchar('\n');
        }
    
    } // line loop
}


int read_file(char *filename) {
    // ================================
    // [1] Process Arguments
    // ================================
    printf("[1] Processing arguments.\n");
    args.filename = filename; //Temporary, since eventually want to pass args struct into read_file

    // overwrite some private globals when user-specified
    sep = args.sep;
    dec = args.dec;
    quote = args.quote;
    stripWhite = args.stripWhite;
    header = args.header;
    whiteChar = (sep == ' ' ? '\t' : (sep == '\t' ? ' ' : 0)); //0: both
    parse_selected_cols();

    // Set some local args
    // int64_t rowLimit = args.rowLimit;
    ////////////////////////////////////////////////////////////////////////////
    //=========================================================================
    // [2] Open file and determine where first row starts
    //=========================================================================
    printf("[2] Opening file %s...\n", args.filename);
    FILE *fp = fopen(args.filename, "rb");
    if (fp == NULL) {
        STOP("ERROR %d: %s. (File could not be read).\n", errno, strerror(errno));
    }
    ////////////////////////////////////////////////////////////////////////////
    //=========================================================================
    // [3] Find start of data
    // Need to determine if we need to skip any lines or characters
    //========================================================================= 
    printf("[3] Finding start of data.\n");
    int delim = '\n'; //TODO: implement the other common delims, like \r\n ?
    char* line = NULL;
    size_t lsize = 0;
    ssize_t llen = 0;
    // getdelim returns n of chars read, or -1 if fail
    while ((llen = getdelim(&line, &lsize, delim, fp)) > -1) { 
        sol = line;
        eol = line + llen;
        // skip empty lines
        if (llen == 0) {
            printf("skipping empty line\n");
            skiprows++;
            continue;
        }
        // skip over UTF-8 BOM.
        // TODO: handle other encodings - https://en.wikipedia.org/wiki/Byte_order_mark
        if (llen > 3 && memcmp(line, "\xEF\xBB\xBF", 3) == 0) {
            sol += 3;
            skipchars += 3;
            printf("Found & skipped UTF-8 BOM\n");
        }
        // skip whitespace (is isspace going to skip carriage returns??)
        while (sol < eol && (isspace(*sol) || *sol=='\0')) { //skip embedded null bytes
            sol++;
            skipchars++;
            printf("skipping space\n");
        }
        // if the line was just whitespace, sol is now pointing at eol
        if (sol == eol) {
            printf("skipping empty line w/ just whitespace\n");
            skiprows++;
            skipchars = 0;
            continue;
        }
        ///////////////////////////////////////////////////////////////////////
        //=========================================================================
        // [4] Extract field info
        // Detect field separator. TODO: detect end of line ?
        //========================================================================= 
        printf("[4] Extracting field info.\n");
        if (!args.sep) {
            sep = detect_fieldsep(sol, llen);
            printf("Detected field seperator: '%c' (%d)\n", sep, sep);
        }
        ///////////////////////////////////////////////////////////////////////
        //=========================================================================
        // [5] Extract column info
        // Detect number of columns & identify which columns to skip
        //========================================================================= 
        printf("[5] Extracting column info.\n");
        // Get number of columns (count n fields in header row)
        ncols = countfields(sol);
        if (ncols < 0) STOP("ERROR: In 'countfields', could not parse number of cols.\n");
        if (ncols == 0) STOP("ERROR: Zero columns found.\n"); //these cases should be skipped above already
        if (ncols > MAXCOLS) STOP("ERROR: Number of cols exceeds maximum.\n");
        printf("NUMBER OF COLS: %d\n", ncols);

        // Determine number of 'active' columns - ie, have been selected for use.
        // If this is not specified by the user, then we assume to use all cols.
        // 'colselect_inds' are the indices in the array of colnames which 
        // correspond to active columns.
        // TODO: we want the user to be able to specify cols by number, too.
        if (!n_colselect || n_colselect==0) n_colselect = ncols;
        // if (args.colselect[0]) {
            // n_colselect = (int)(sizeof(args.colselect) / sizeof(char*));
        printf("NUMBER OF SELECTED COLS: %d\n", n_colselect);
        
        // int colselect_inds[n_colselect]; memset(colselect_inds, 0, sizeof(colselect_inds)); // don't know if this or the below method is going to be more efficient for indexing select cols
        // bool is_selected[ncols]; memset(is_selected, 0, sizeof(is_selected)); // this might be better since we can use the same for loop to index columns and is_selected
        col_is_selected = calloc(ncols, sizeof(bool));
        selected_col_inds = calloc(n_colselect, sizeof(int));
        // Iterate over header to extract field pointers, then strip colnames.
        // While iterating, determine the relative indices of selected columns.
        // TODO: if args.header is false, make colnames like V1, V2, etc. for sake of printing
        struct FieldContext fields[ncols];
        if (iterfields(sol, ncols, fields) < 1) STOP("Column names could not be parsed.\n"); //iterfields stores discovered field info in FieldContext   
        colnames = malloc(ncols * sizeof(char*));
        int n_colsfound = 0;
        for (int i = 0; i < ncols; i++) {
            // Write colname from the file line into the colnames array of str 
            colnames[i] = (char*)malloc(sizeof(char) * (fields[i].len+1));
            memcpy(colnames[i], sol+fields[i].off, fields[i].len); //dest, src, n
            colnames[i][fields[i].len] = '\0';
            // Check if the colname matches one of the selected cols
            if (args.colselect[0] && n_colsfound < n_colselect) {
                for (int s = 0; s < n_colselect; s++) {
                    // TODO: eliminate redoing a strcmp for indices which held a selected col 
                    if (strcmp(colnames[i], args.colselect[s]) == 0) {
                        selected_col_inds[n_colsfound] = i;
                        col_is_selected[i] = true;
                        n_colsfound++;
                        printf("  %d/%d selected cols found.\n", n_colsfound, n_colselect);
                    }
                } // iter over selected cols
            }
        } // iter over all cols 

        //////////////////////////
        // Temp Info Dump 
        /////////////////////////
        printf("Column Names:\n");
        for (int i = 0; i < ncols; i++) {
            printf("Field: ");
            for (int j = 0; j < fields[i].len; ++j) printf("%c", *(colnames[i] + j)); //sol + fields[i].off
            printf("\t(off = %d | len = %d)", fields[i].off, fields[i].len);
            printf(col_is_selected[i] ? " [x]\n" : "\n");
        }

        // for (int i = 0; *(sol + i); i++) printf("%c ", *(sol + i));
        printf("\ninput line: %s", line);
        printf("nchars: %ld | bufsize : %ld\n", llen, lsize);
        printf("processed line: %s", sol);
        printf("skiprows: %d | skipchar %d\n", skiprows, skipchars);
        printf("---------------------------\n");
        // If we've successfully parsed the header, break out of loop
        break;
    }
    if (llen == -1 && errno==0) {
        printf("WARNING: Reached end of file and found only whitespace.\n");
    } else if (llen == -1) {
        printf("ERROR %d: %s (Internal error reading file).\n", errno, strerror(errno));
    }
    free(line);


    // Next: figure out how we're going to iterate through all the data and pluck out the cols we're interested in.
    rewind(fp);
    
    // Dispatch fp to operations
    pretty_print_table(fp);
    
    // while ((llen = getdelim(&line, &lsize, delim, fp)) > -1) { 
    //     printf("%s\n", line);
    //     break;
    // }


    if (fclose(fp) == EOF) STOP("ERROR %d: %s. (Failed in closing file).", 
                                 errno, strerror(errno));
    printf("[*] cleanup\n");
    readfile_cleanup();
    return 1;

}

// 4/5/2023, For next time:
//    are we just passing file metadata (start, sep, etc.) between readfile and
//    main and then main is passing this to the appropriate function?
//    if so, iterfields & its dependents should be common functions accessible
//     by all operations, which will require some refactoring (might have to pass
//    'eol' around a bunch, and maybe make these accept one of the arg structs)

/*
    Program Structure:

    Overall:
        1. Main receives input file & sends to readfile to prep for parsing
            - remove unwanted start & end bytes
            - determine how to parse fields - sep, quote, dec, whitespace
            - determine n of cols & col names
        2. Main determines what operation to perform on the data & which columns to use
            - the operation could require 1 column, in which case that col must be user specified
                - send to function for dispatching 1 column operations
            - the operation could require >1 column, in which case the cols must be user specified or all cols will be used
                - send to function for dispatching multi-column operations
        3. Main dispatches an operation function and provides it with the metadata collected by readfile.
            - Operations open file, use readfileInfo to jump straight into parsing.
            - Operation functions could perform type conversion based on the type they require
            - Operation funcions could call shared type-conversion functions, for ex, "charToInt" on each field.
            - Type conversions will try their best on the given input, but will rely on users to specify appropriate columns,
              and will convert non-parsable data to 'NA' - this info will be delivered to the user.

    readfile:
        1. extract & check arguments
        2. open and start reading file
        3. handle byte-order mark / remove encoding bytes at beginnged & end (TODO: add more checks, like fread)
        5. skip rows as needed - if starts w/ blank, or if some nrows to skip is specified
        6. Auto-detect number of columns (could try to infer sep, dec, and quote too).
        7. parse column names
        9. send relevant information about columns to skip, rows to skip, etc. back to main
        - Idea is to provide info on where to start reading the file (and where to stop),
          and how to parse fields.
*/
