
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
#include "common.h" //mainArgs, readfileInfo, STOP, fp, sol, eol, colnames

// GLOBALS
extern int errno;

extern FILE *fp;
extern struct mainArgs args;
extern const char *sol;
extern const char *eol;
extern char **colnames;

static int MAXCOLS = 1000; //TODO: pick a more appropriate number


///////////////////////////////// EXAMPLE //////////////////////////////////////
void pretty_print_table(FILE *fp) {
    printf("--- -------%.*s----------------------- ---\n", (int)strlen(args.filename), "------------------------------------");
    printf("--- TABLE: %s. %d COLS. %d SELECTED. ---\n", args.filename, args.ncols, args.n_colselect);  
    int32_t row = 0;
    int delim = '\n'; //TODO: implement the other common delims, like \r\n ?
    char* line = NULL; size_t lsize = 0; ssize_t llen = 0;
    // ITER ROWS
    while ((llen = getdelim(&line, &lsize, delim, fp)) > -1) {
        if (row < args.skiprows) {
            row++; continue;
        }
        sol = line + args.skipchars;
        eol = line + llen;
        struct FieldContext fields[args.ncols];
        int it = iterfields(sol, args.ncols, fields, 1);
        if (it == -1) STOP("readfile.c: pretty_print_table: Fields could not be parsed.\n"); //iterfields stores discovered field info in FieldContext   
        if (it == 0) continue; //empty line
        // ITER COLS
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
    }
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
    // [2] Open file
    //=========================================================================
    printf("[2] Opening file %s...\n", args.filename);
    fp = fopen(args.filename, "rb");
    if (fp == NULL) {
        STOP("ERROR: %s [%d] (file %s could not be read).\n", strerror(errno), errno, args.filename);
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
        //======================================================================
        // [4] Extract field info
        // Detect field separator. (TODO: detect end of line ?)
        //====================================================================== 
        printf("[4] Extracting field info.\n");
        if (!args.sep) {
            args.sep = detect_fieldsep(sol, llen);
            printf("Detected field seperator: '%c' (%d)\n", args.sep, args.sep);
        } else {
            printf("Provided field seperator: '%c' (%d)\n", args.sep, args.sep);
        }
	    args.whiteChar = (args.sep == ' ' ? '\t' : (args.sep == '\t' ? ' ' : 0)); //0: both
        ///////////////////////////////////////////////////////////////////////
        //======================================================================
        // [5] Extract column info
        // Detect number of columns & identify which columns to skip
        //======================================================================
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
        if (!args.n_colselect) 
            args.n_colselect = args.ncols;
        printf("NUMBER OF SELECTED COLS: %d\n", args.n_colselect);

        // arrays for indexing the selected columns out of the total columns 
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
        if (iterfields(sol, args.ncols, fields, args.header_keep_quotes) < 1) 
            STOP("Column names could not be parsed.\n"); //iterfields stores discovered field info in FieldContext   
        colnames = malloc(args.ncols * sizeof(char*));
        int n_colsfound = 0;
        for (int i = 0; i < args.ncols; ++i) {
            // Write colname from the file line into the colnames array
            if (fields[i].len == NA_INT32) {
                // NOTE: this solution works as long as field.len is never used
                //  to iterate, since NA_INT32 = -2147483648.
                // TODO: if multiple NAs, add NA0, NA1, etc so user could select
                //  them as desired.
                colnames[i] = malloc(sizeof("NA0"));
                snprintf(colnames[i], sizeof("NA0"), "NA%d", i);
            } else {
                colnames[i] = malloc(fields[i].len + 1);
                strncpy(colnames[i], sol + fields[i].off, fields[i].len); //dest, src, n
                colnames[i][fields[i].len+1] = '\0';
            }
            // printf("colnames[%d]=%s\n", i, colnames[i]);
            // Check if the colname matches one of the selected cols
            if (args.colselect && n_colsfound < args.n_colselect) {
                for (int s = 0; s < args.n_colselect; ++s) {
                    // TODO: eliminate redoing a strcmp for indices which held a selected col 
                    if (strcmp(colnames[i], args.colselect[s]) == 0) {
                        args.selected_col_inds[n_colsfound] = i;
                        args.col_is_selected[i] = true;
                        n_colsfound++;
                        printf("  %d/%d selected cols (%s) found.\n",
                                 n_colsfound, args.n_colselect, colnames[i]);
                    }
                }
            } else if (!args.colselect) { //if no selected cols, all cols are active
                args.selected_col_inds[i] = i;
                args.col_is_selected[i] = true;
            }
        }
        if (args.colselect && n_colsfound == 0) {
            STOP("ERROR: Selected cols were not found.\n");
        }
        if (args.colselect && n_colsfound < args.n_colselect) {
            printf("Warning: Only %d out of %d selected cols were found. Continuing with found cols.\n",
                    n_colsfound, args.n_colselect);
            // so downstream operations don't have to worry about this, we act 
            // as if only the n_colsfound cols were selected by the user
            args.n_colselect = n_colsfound;
        }

        //////////////////////////
        // Temp Info Dump 
        /////////////////////////
        printf("Columns:\n");
        for (int i = 0; i < args.ncols; i++) {
            printf("Field: %s", colnames[i]);
            printf("\t(off = %d | len = %d)", fields[i].off, fields[i].len);
            printf(args.col_is_selected[i] ? " [x]\n" : "\n");
        }

        // for (int i = 0; *(sol + i); i++) printf("%c ", *(sol + i));
        printf("\ninput line: %s", line);
        printf("nchars: %ld | bufsize : %ld\n", llen, lsize);
        printf("processed line: %s", sol);
        printf("skiprows: %d | skipchar %d\n", args.skiprows, args.skipchars);
        // If we've successfully parsed the header, break out of loop
        break;
    }
    if (llen == -1 && errno==0) {
        printf("WARNING: Reached end of file and found only whitespace.\n");
    } else if (llen == -1) {
        STOP("ERROR: %s (Internal error (%d) reading file).\n", strerror(errno), errno);
    }
    free(line);
    // bring pointer back to start of file - only necessary if we're 
    rewind(fp);
    ///////////////////////////////////////////////////////////////////////
    //=========================================================================
    // [5] Close Up.
    // Detect number of columns & identify which columns to skip
    //========================================================================= 
    // Next: figure out how we're going to iterate through all the data and pluck out the cols we're interested in.
    
    // Dispatch fp to operations
    // pretty_print_table(fp);
    

    
    return 1;

}


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
