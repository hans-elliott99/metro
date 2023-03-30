#include <sys/mman.h> //mmap
#include <sys/stat.h> //fstat
#include <fcntl.h> //open
#include <unistd.h> //close

#include <stdio.h> //printf, fopen/fclose, getdelim
#include <stdint.h> //int_t
#include <string.h> //strlen
#include <errno.h> //errno
#include <stdlib.h> //exit
#include <stdbool.h> //true/false
#include <ctype.h> //isspace, isdigit, 

#include "readfile.h"

extern int errno;

/* Private globals */
// To save passing to many functions
static const char *sol, *eol;
static char sep; //ie, the delimiter
static char whiteChar; //what is considered whitespace - ' ', '\t', or 0 for both
static char quote, dec; //quote style and decimal style
static bool stripWhite = true; //strip whitespacde
static bool fill = true; //fill ragged rows
static bool header = true; //is the first row a row of column names

// To clean up on error or return
// static void *mmp = NULL;
// static void *mmp_copy = NULL;
static size_t fileSize;
static int8_t *type = NULL, *tmpType = NULL, *size = NULL;
// static lenOff *colNames = NULL;
// static freadMainArgs args = {0}; 

struct mainArgs {
    const char *filename;
    const char *input;
    int64_t rowLimit;
    char sep;
    char dec;
    char quote;
    bool stripWhite;
    bool header;
    bool keepLeadingZeros; 
};

struct mainArgs args = {
    .rowLimit = INT64_MAX,
    .sep = ',',
    .dec = '.',
    .quote = '"',
    .stripWhite = true,
    .header = true,
    .keepLeadingZeros = true,
}; 

struct Field {
    int32_t off; //offset from sol
    int32_t len; //len from off to end of field
};

struct FieldParserContext {
    const char **ch;
    // const char **start;
    int32_t off; //offset from char
    int32_t len; //signed, negative is NA, 0 is empty ("")
};

/* Utils */

bool readfile_cleanup(void) {
    free(type); type = NULL;
    free(tmpType); tmpType = NULL;
    free(size); size = NULL;
    bool requires_clean = false;
    // bool requires_clean = (mmp);
    // if (mmp != NULL) {
    //     if (munmap(mmp, fileSize)) {
    //         printf("System error %d when unmapping file: %s\n", 
    //                 errno, strerror(errno));
    //     mmp = NULL;
    //     }
    // }
    fileSize = 0;
    sol = eol = NULL;
    sep = whiteChar = quote = dec = '\0';
    stripWhite = fill = true;

    // printf("\nFile successfully unmapped.\n");
    return requires_clean;
}

// Recast a 'const char' as a 'char/
// static char* _const_cast(const char *ptr) {
//     union {const char *a; char *b;} tmp = { ptr };
//     return tmp.b;
// }

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

// Test if the current character is at the end of a field - either based on 'sep'
// or based on a line-ending sequence
bool test_end_of_field(const char *ch) {
    return ((*ch==sep) || ((*ch=='\n' || *ch=='\r' || *ch=='\0') 
                                        && (ch==eol || test_moveto_eol(&ch))));
}

// Given a FieldParserContext pointing to the start of a potential field, 
// advance its 'ch' ptr to the actual start of the field and calculate the 
// length to the proper end of the field.
// Field can be obtained immeditely after by ch - len
// void parse_field(const char **pch, const char **pFieldStart, int32_t *pFieldLen) {
//     const char *ch = *pch;
//     const char *fieldStart;
//     int32_t fieldLen;
//     // const char *fieldStart;
//     // int32_t fieldLen = 0;
//     // skip leading spaces
//     if ((*ch == ' ' && stripWhite) || (*ch == '\0' && ch < eol)) {
//         while(*(ch) == ' ' || (*ch == '\0' && ch < eol)) {ch++; }
//     } 
//     fieldStart = ch;
//     // TODO: implement alternative quote rules. For now, just keep all quotes.
//     // if (*ch != quote ) or it does but we want to keep them:  
//     {
//         while (!test_end_of_field(ch)) ch++; //will end on sep, \n, \r, or eol
//         fieldLen = (int32_t)(ch - fieldStart);
//         // remove any lagging spaces 
//         while (fieldLen > 0 && ((ch[-1]==' ' && stripWhite) || ch[-1]=='\0')) {
//             fieldLen--; ch--;
//         }
//         if (fieldLen == 0) fieldLen = INT32_MIN; //blanks are NAs
//     }
//     *pch = ch;
//     *pFieldStart = fieldStart;
//     *pFieldLen = fieldLen;
// }


void parse_field(const char **pch, int32_t *pFieldOff, int32_t *pFieldLen) {
    // Idea - advance fieldOff to be an int from start of line to start of field
    //        then record fieldLen from start-of-field to end-of-field
    const char *ch = *pch;
    int32_t fieldOff = *pFieldOff;
    int32_t fieldLen = *pFieldLen;
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
        //parse_field advances ch to either sep, \n, \r, or eol
        if (sep == ' ' && *ch==sep) {
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

int iterfields(const char *ch, int n_fields, struct Field *fields) {
    // skip starting spaces even if sep==space, since they don't break up fields,
    // (skip_whitechar would ignore them since it doesn't skip seps)
    const char *start = ch;
    int32_t fieldOff = 0;
    int32_t fieldLen = 0;
    int ncol = 1;
    if (sep == ' ') while (*ch == ' ') {ch++;}
    skip_whitechar(&ch);
    if (test_moveto_eol(&ch)) {
        return 0; //empty line, 0 fields
    }
    while (ch < eol) {
        fieldOff = ch - start;
        parse_field(&ch, &fieldOff, &fieldLen);
        // parse_field advances ch to either sep, \n, \r, or eol
        fields[ncol-1].off = fieldOff;
        fields[ncol-1].len = fieldLen;
        if (sep == ' ' && *ch==sep) {
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
        // if ch is at eol, conclude
        // (this is what we want to happen for well behaved files)
        if (test_moveto_eol(&ch)) {
            //TODO: handle cases of inconsistent row length
            if (n_fields < ncol) printf("Number of fields did not reach ncol\n");
            if (n_fields > ncol) printf("Number of fiedls exceeds ncol\n");
            return ncol; 
        }
        if (ch!=eol) return -1; //should be only reachable if sep & quote rule are invalid for this line
    }
    return ncol;
}


bool parse_int32(const char **pch, int fieldLen) {
    const char *ch = *pch;
    bool neg = false;
    
    // don't parse as int if leading zeros are kept  
    if (*ch == '0' && args.keepLeadingZeros && isdigit(ch[1])) return false;
    if (*ch == '-') {
        neg = true; ch++; fieldLen--;
    } 
    else if (*ch == '+') {
        ch++; fieldLen--;
    }
    for (int i = 0; i < fieldLen; i++) {
        if (isdigit(*(ch+i)) == 0) {
            return false;
        }
    }
    return true;
}

// TODO
void get_field_type(struct FieldParserContext *ctx) {
    const char *start = *(ctx->ch) - ctx->len;
    const char *end = start + ctx->len - 1;
    // if fields are quotes, ex <"hello", "3.33">, don't consider the quotes
    if (*start == quote) start++;
    if (*end == quote) end--;
    if (end < start) printf("blank field\n"); //TODO
    
    bool isint = parse_int32(&start, ctx->len);    

    for (int i = 0; i < (int)(end-start+1); ++i)
        printf("%c", *(start + i));
    printf("\t| int = %s\n", isint ? "true" : "false");
}


int detect_types(const char **pch, int ncols) {
    const char *ch = *pch;
    int ncol = 0;
    struct FieldParserContext ctx = {
            .ch = &ch, .off = 0, .len = 0,
        };
    while (ch < eol) {
        // parse_field(&ctx);
        get_field_type(&ctx);

         // if next character is end-of-line, advance
        if (ch[1] == '\r' || ch[1] == '\n' || (ch[1] == '\0' && ch+1 == eol)) {
            ch++;
        }
        // if sep, we have found a new field so count it and move to next
        if (*ch == sep) {
            ncol++; ch++;
            continue;
        }
        // if ch is an eol, we advance it to the next line and conclude
        // (this is what we want to happen for well behaved files)
        if (test_moveto_eol(&ch)) { 
            *pch = ch+1;
            return ncol; 
        }
        // if not sep or eol, we didn't/can't parse this field correctly
        if (ch != eol) return -1;
        break;        
    }
    if (ncol != ncols) return -1;
    *pch = ch;
    return ncol;
}


int read_file(char *filename) {

    if (readfile_cleanup()) {
        printf("Previous session was not cleaned up. Successfully cleaned now.");
    }
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
    whiteChar = (sep == ' ' ? '\t' : (sep == '\t' ? ' ' : 0));
    
    // Set some local args
    int64_t rowLimit = args.rowLimit;
    //=========================================================================
    // [2] Open file and determine where first row starts
    // Need to determine if we need to skip any lines or characters
    //=========================================================================
    printf("[2] Opening file %s...\n", args.filename);
    FILE *fp = fopen(args.filename, "rb");
    if (fp == NULL) {
        STOP("ERROR %d: %s. (File could not be read).\n", errno, strerror(errno));
    }
    int skiprows = 0;  //n rows to skip before we start processing txt
    int skipchars = 0; //in first row, n of chars to skip before we start processing txt

    size_t lsize = 0;
    char* line = NULL;
    ssize_t llen = 0;
    int delim = '\n';
    while ((llen = getdelim(&line, &lsize, delim, fp)) > -1) { 
        // getdelim returns n of chars read, or -1 if fail
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
        // parse n cols & header
        int ncols = countfields(sol);
        if (ncols < 0) STOP("ERROR: In 'countfields', could not parse number of cols.\n");
        if (ncols == 0) STOP("ERROR: Zero columns found.\n"); //these cases should be skipped above already
        printf("NUMBER OF COLS: %d\n", ncols);

        struct Field fields[ncols];
        if (iterfields(sol, ncols, fields) < 1) STOP("No column names found\n");
        // TODO: 
        // Store the column names in something (might just need to memcpy them) - assuming there is a header row, otherwise give names like V1, V2, etc
        // Next: figure out how we're going to iterate through all the data and pluck out the cols we're interested in.
        // maybe, 

        // if (args.header) {
        //     // this line we've reached is the column names
        // }
        for (int i = 0; i < ncols; i++) {
            printf("Field: ");
            for (int j = 0; j < fields[i].len; ++j) printf("%c", *(sol + fields[i].off + j));
            printf("\t(off = %d | len = %d)\n", fields[i].off, fields[i].len);
        }

        for (int i = 0; *(sol + i); i++) {
            printf("%c ", *(sol + i));
        }
        printf("\nline: %s\n", line);
        printf("processed line: %s\n", sol);
        printf("nchars: %ld | bufsize : %ld\n", llen, lsize);
        printf("skiprows: %d | skipchar %d\n", skiprows, skipchars);
        break;
    } 
    free(line);



    if (fclose(fp) == EOF) {
        STOP("ERROR %d: %s. (Failed in closing file).", errno, strerror(errno));
    }
    readfile_cleanup();
    return 1;
    // rewind(fp);
    // while ((llen = getdelim(&line, &lsize, delim, fp)) > -1) { 
    //     printf("%s\n", line);
    //     break;
    // }
    //=========================================================================
    // [3] Handle Byte-Order-Mark
    //     Files could contain start (and end) bytes based on their encoding 
    //     which need to be skipped. Will only UTF-8 be supported? Should be 
    //     the most common scenario, although BOMs in general should be uncommon.
    //=========================================================================
    // printf("[3] Detecting & Skipping BOMs.\n");
    // UTF-8 encoding
    // if (fileSize >= 3 && memcmp(sof, "\xEF\xBB\xBF", 3) == 0) {
    //     sof += 3;
    //     printf("Found and skipped UTF-8 BOM.\n");
    // }
    // handle other encodings : https://en.wikipedia.org/wiki/Byte_order_mark

    //=========================================================================
    // [4] Convert mmap to be a null (\0) terminated string
    //     Now 'eof' is truly the end of our input, as represented by null ptr.
    //     Advantage - we don't need to constantly test for eof. 
    //=========================================================================
    
    //=========================================================================
    // [5] TODO: Jump 'sof' to some n of skipped rows, or to some given string
    // For now, just make sure to skip over any blank input at start of file 
    // !TODO! - really should implement the skipnrows so user can manually skip
    //          over any kind of header rows
    //=========================================================================
    // printf("[5] Skipping lines as needed.\n");
    // const char *pos = sof; // pointer to start of data
    // int row1line = 1;   // line number of start of data, for messages
    // {
    // ch = pos;
    // const char *lineStart = ch;
    // while (ch < eof && (isspace(*ch) || *ch == '\0')) {
    //     if (*ch == '\n') { ch++; lineStart=ch; row1line++; } else { ch++; }
    // }
    // if (ch >= eof) STOP("Input is either empty of fully whitespace.\n");
    // if (lineStart > pos) printf("Skipped some blank lines at start of file.\n");
    // ch = pos = lineStart;
    // }

    //=========================================================================
    // [6] Auto detect ncols
    // Sample first n rows to determine if there are inconsistencies, because
    // we will want to fill those if so.
    // TODO: could detect other args too (sep, dec, quote rule, etc.)
    //=========================================================================
    // printf("[6] Detecting number of columns.\n");
    // int ncol = 0;
    // int tst_ncol = 0;
    // int testRows = 100; //will be 100 or nrows if nrows is < 100
    // if (args.sep == '\n') {
    //     // user wants each line to be read as a single column
    //     sep = 127; // ascii del - won't be in data, not \n \r or \0
    //     ncol = 1;
    //     fill = true;
    // } 
    // else {
    //     ncol = countfields(&ch); //first line is expected to not be blank
    //     if (ncol < 0) STOP("Internal error parsing number of columns.\n"); //TODO: handle this?
    //     int thisRow = 0;
    //     while (ch < eof && thisRow++ < testRows) {
    //         tst_ncol = countfields(&ch);
    //         ncol = tst_ncol > ncol ? tst_ncol : ncol;
    //     }
    // }
    // printf("Number of columns: %d\n", ncol);
    // ch = pos;
    //=========================================================================
    // [7] Auto detect column types & whether first row is column names
    //=========================================================================
    // printf("[7] Detecting column types.\n");
    // type = (int8_t *)malloc( (size_t)ncol * sizeof(int8_t) );
    // tmpType = (int8_t *)malloc( (size_t)ncol * sizeof(int8_t) );
    // if (!type || !tmpType) STOP("ERROR: Failed to allocate type buffers: %s",
    //                                                             strerror(errno));
    // int8_t type0 = 1;
    // for (int j = 0; j < ncol; j++) 
    //     tmpType[j] = type[j] = type0;
    
    // // if (args.header != false) {
    // //     countfields(&ch); //skip header row (likely colnames)
    // // } else {
    // //     // if all string, take to be colnames
    // //     // else, assume no header exists

    // // }
    // detect_types(&ch, ncol);
    // detect_types(&ch, ncol);

}

// TODO:
//
// figure out how data is going to be stored once i start copying from the txt file into memory
//      simple: copy values and convert them to their appropriate type and store in some dataframe schema
//      however: I don't need to be storing data as some specific type, I really just want a mapping that says, for any given field:
//          you belong to this column, you are of this type, so if the user tries some operation which requires you to be 
//          numeric, can you be?
// This data is not going to be living in memory for an extended period like it would if we were scripting w/ it in R or Python
// We _do not need_ to map it into some dataframe format
// We simply want to:
// - quickly access the data
// - determine where to start and stop processing the text
// - figure out how to parse each field (based on sep, dec, quote, etc)
// - process user's command (sort, unique, mean, etc)
// - prep the appropriate data (ie which cols and rows) for the command, converting types as needed
// - execute the command & redirect the output to stdout or some file
// 
// - don't really need to figure out types do we? let the user's desired command or extra flags 
//   determine what type each column should be processed as, then do our best to parse the column as that type (this may generate NAs, whatever) 
//


// set it up so that the desired commands/operators (such as 'unique' or 'mean') dictate what type the data needs to be interpreted as at runtime
// - main fn receives info on desired command's expected data-type
// - it calls the approriate type parser on the data associated with the field(s) which will be operated on
// - this creates an array of data of some type T
// - this array is passed to the command function (unique) which doesn't have to worry about type conversion itself (just has to deal with potential NAs caused by type conversion)
//
//
//  struggles:
//   how to store all of the meta info on where individual fields begin and end?
//   how to access a specific column when the input is one giant text string?
//   do we really want to copy the data into an in memory array? Or just convert it on the fly from the file?
//      - look at what sort and uniq do


/*
    Program Structure:

    Main File:
        1. Receive input text & prep for parsing
            - remove unwanted start & end bytes
            - determine how to parse fields - sep, quote, dec, whitespace
            - determine n of cols & col names
        2. Determine what operation to perform on the data & which columns to use
            - the operation could require 1 column, in which case that col must be user specified
                - send to function for dispatching 1 column operations
            - the operation could require >1 column, in which case the cols must be user specified or all cols will be used
                - send to function for dispatching multi-column operations
        3. Dispatch data to the operation function.
            - Data is still of type char at this point, no type conversion.
            - Operation functions could perform type conversion based on the type they require
            - Operation funcions could call shared type-conversion functions, for ex, "charToInt", at the beginning of their procedure
            - Type conversions will try their best on the given input, but will rely on users to specify appropriate columns
            - Type conversions will convert non-parsable data to 'NA', and this info will be delivered to the user.



    readfile:
        1. extract & check arguments
        2. open and start reading file
        3. handle byte-order mark / remove encoding bytes at beginnged & end (TODO: add more checks, like fread)
        5. skip rows as needed - if starts w/ blank, or if some nrows to skip is specified
        6. Auto-detect number of columns (could try to infer sep, dec, and quote too).
        7. parse column names
        8. determine what operation to send data to
        9. send relevant information about columns to skip, rows to skip, etc. & execute operation
        10. exit
*/

