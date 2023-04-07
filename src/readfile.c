
#define _GNU_SOURCE //for getdelim
#include <stdio.h> //printf, fopen/fclose, getdelim
#include <stdlib.h> //
#include <stdint.h> //int_32t, int64_t, etc.
#include <string.h> //sterror, memset
#include <errno.h> //errno
#include <stdbool.h> //true/false
#include <ctype.h> //isspace, isdigit, 
#include <unistd.h> //ssize_t

#include "readfile.h"
#include "common.h" //mainArgs, readfileInfo, STOP,

// GLOBALS
extern int errno;
extern struct Args args;
extern const char *sol;
extern const char *eol;
extern char **colnames;

static int MAXCOLS = 1000; //TODO: pick a more appropriate number
// To clean up on error or return
// static char **colnames;
// Params
// static int skiprows = 0; //n rows to skip before start processing txt
// static int skipchars = 0; //in first usable row, n of chars to skip before start processing txt
// static bool *col_is_selected; //array, len == ncols, indexes user selected col(s)
// static int *selected_col_inds; //array, len == n selected cols, indexes data cols 
// static char sep; //ie, the delimiter
// static char whiteChar; //what is considered whitespace - ' ', '\t', or 0 for both
// static char quote, dec; //quote style and decimal style
// static bool stripWhite = true; //strip whitespacde
// static bool header = true; //is the first row a row of column names
// static int ncols = 0;
// static int n_colselect = 0;
// static size_t fileSize;
// static int8_t *type = NULL, *tmpType = NULL, *size = NULL;
// static lenOff *colNames = NULL;

///////////////////////////////// EXAMPLE //////////////////////////////////////
void pretty_print_table(FILE *fp) {    
    int32_t row = 0;
    int delim = '\n'; //TODO: implement the other common delims, like \r\n ?
    char* line = NULL; size_t lsize = 0; ssize_t llen = 0;
    while ((llen = getdelim(&line, &lsize, delim, fp)) > -1) { 
        if (row < args.skiprows) {
            row++; continue;
        }
        sol = line + args.skipchars;
        eol = line + llen;
        struct FieldContext fields[args.ncols];
        if (iterfields(sol, args.ncols, fields) < 1) STOP("readfile.c: pretty_print_table: Fields could not be parsed.\n"); //iterfields stores discovered field info in FieldContext   
        
        for (int i = 0; i < args.ncols; ++i) {
            if (!args.col_is_selected[i]) 
                continue;
            int print_idx = array_pos(args.selected_col_inds, i, args.n_colselect) + 1; 
            for (int32_t j = 0; j < fields[i].len; ++j)
                printf("%c", *(sol + fields[i].off + j));
            putchar('\t');
            if (print_idx == args.n_colselect) 
                putchar('\n');
        }
    
    } // line loop
}


////////////////////////////////// READFILE ////////////////////////////////////

int read_file(void) {
    printf("[ READFILE ]\n");
    // ================================
    // [1] Process Arguments & do setup
    // ================================
    printf("[1] Processing arguments.\n");
    // if anything needs to be done after main file handles command line args

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
            args.skiprows++;
            continue;
        }
        // skip over UTF-8 BOM.
        // TODO: handle other encodings - https://en.wikipedia.org/wiki/Byte_order_mark
        if (llen > 3 && memcmp(line, "\xEF\xBB\xBF", 3) == 0) {
            sol += 3;
            args.skipchars += 3;
            printf("Found & skipped UTF-8 BOM\n");
        }
        // skip whitespace (is isspace going to skip carriage returns??)
        while (sol < eol && (isspace(*sol) || *sol=='\0')) { //skip embedded null bytes
            sol++;
            args.skipchars++;
            printf("skipping space\n");
        }
        // if the line was just whitespace, sol is now pointing at eol
        if (sol == eol) {
            printf("skipping empty line w/ just whitespace\n");
            args.skiprows++;
            args.skipchars = 0;
            continue;
        }
        ///////////////////////////////////////////////////////////////////////
        //=========================================================================
        // [4] Extract field info
        // Detect field separator. TODO: detect end of line ?
        //========================================================================= 
        printf("[4] Extracting field info.\n");
        if (!args.sep) {
            args.sep = detect_fieldsep(sol, llen);
        }
        printf("Detected field seperator: '%c' (%d)\n", args.sep, args.sep);
        ///////////////////////////////////////////////////////////////////////
        //=========================================================================
        // [5] Extract column info
        // Detect number of columns & identify which columns to skip
        //========================================================================= 
        printf("[5] Extracting column info.\n");
        // Get number of columns (count n fields in header row)
        args.ncols = countfields(sol);
        if (args.ncols < 0) STOP("ERROR: In 'countfields', could not parse number of cols.\n");
        if (args.ncols == 0) STOP("ERROR: Zero columns found.\n"); //these cases should be skipped above already
        if (args.ncols > MAXCOLS) STOP("ERROR: Number of cols exceeds maximum.\n");
        printf("NUMBER OF COLS: %d\n", args.ncols);

        // Determine number of 'active' columns - ie, have been selected for use.
        // If this is not specified by the user, then we assume to use all cols.
        // 'colselect_inds' are the indices in the array of colnames which 
        // correspond to active columns.
        // TODO: we want the user to be able to specify cols by number, too.
        if (args.n_colselect > args.ncols) {
            STOP("ERROR: Number of columns selected exceeds number of found cols.\n");
        }
        if (!args.n_colselect || args.n_colselect==0) 
            args.n_colselect = args.ncols;
        // if (args.colselect[0]) {
            // n_colselect = (int)(sizeof(args.colselect) / sizeof(char*));
        printf("NUMBER OF SELECTED COLS: %d\n", args.n_colselect);
        
        // int colselect_inds[n_colselect]; memset(colselect_inds, 0, sizeof(colselect_inds)); // don't know if this or the below method is going to be more efficient for indexing select cols
        // bool is_selected[ncols]; memset(is_selected, 0, sizeof(is_selected)); // this might be better since we can use the same for loop to index columns and is_selected
        args.col_is_selected = calloc(args.ncols, sizeof(bool));
        args.selected_col_inds = calloc(args.n_colselect, sizeof(int));
        // Iterate over header to extract field pointers, then strip colnames.
        // While iterating, determine the relative indices of selected columns.
        // TODO: if args.header is false, make colnames like V1, V2, etc. for sake of printing
        // URGENT TODO: 
        // by default, let's skip over quotes when parsing col name
        // since Bash and other shells usually strip quotes when passing args from
        // cli to script 
        struct FieldContext fields[args.ncols];
        if (iterfields(sol, args.ncols, fields) < 1) STOP("Column names could not be parsed.\n"); //iterfields stores discovered field info in FieldContext   
        colnames = malloc(args.ncols * sizeof(char*));
        int n_colsfound = 0;
        for (int i = 0; i < args.ncols; ++i) {
            // Write colname from the file line into the colnames array 
            colnames[i] = malloc(fields[i].len + 1);
            strncpy(colnames[i], sol + fields[i].off, fields[i].len); //dest, src, n
            colnames[i][fields[i].len] = '\0';
            // Check if the colname matches one of the selected cols
            if (args.colselect && n_colsfound < args.n_colselect) {
                for (int s = 0; s < args.n_colselect; ++s) {
                    // TODO: eliminate redoing a strcmp for indices which held a selected col 
                    if (strcmp(colnames[i], args.colselect[s]) == 0) {
                        args.selected_col_inds[n_colsfound] = i;
                        args.col_is_selected[i] = true;
                        n_colsfound++;
                        printf("  %d/%d selected cols found.\n", n_colsfound, args.n_colselect);
                    }
                }
            } else { //if no selected cols, all cols are active
                args.selected_col_inds[i] = i;
                args.col_is_selected[i] = true;
            }
        }
        if (args.colselect && n_colsfound == 0) {
            STOP("ERROR: Selected cols were not found.\n");
        }

        //////////////////////////
        // Temp Info Dump 
        /////////////////////////
        printf("Column Names:\n");
        for (int i = 0; i < args.ncols; i++) {
            printf("Field: ");
            for (int j = 0; j < fields[i].len; ++j) printf("%c", *(colnames[i] + j)); //sol + fields[i].off
            printf("\t(off = %d | len = %d)", fields[i].off, fields[i].len);
            printf(args.col_is_selected[i] ? " [x]\n" : "\n");
        }

        // for (int i = 0; *(sol + i); i++) printf("%c ", *(sol + i));
        printf("\ninput line: %s", line);
        printf("nchars: %ld | bufsize : %ld\n", llen, lsize);
        printf("processed line: %s", sol);
        printf("skiprows: %d | skipchar %d\n", args.skiprows, args.skipchars);
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
