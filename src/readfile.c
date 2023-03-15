#include <sys/mman.h> //mmap
#include <sys/stat.h> //fstat
#include <fcntl.h> //open
#include <unistd.h> //close

#include <stdio.h> //printf
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
static const char *sof, *eof;
static char sep; //ie, the delimiter
static char whiteChar; //what is considered whitespace - ' ', '\t', or 0 for both
static char quote, dec; //quote style and decimal style
static bool stripWhite = true; //strip whitespacde
static bool fill = true; //fill ragged rows
static int8_t header = INT8_MIN;

// To clean up on error or return
static void *mmp = NULL;
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
    int8_t header; //yes 1, no 0, autodetect int 8 min -128,
    bool keepLeadingZeros; 
};

struct mainArgs args = {
    .rowLimit = INT64_MAX,
    .sep = ',',
    .dec = '.',
    .quote = '"',
    .stripWhite = true,
    .header = INT8_MIN,
    .keepLeadingZeros = true,
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
    bool requires_clean = (mmp);
    if (mmp != NULL) {
        if (munmap(mmp, fileSize)) {
            printf("System error %d when unmapping file: %s\n", 
                    errno, strerror(errno));
        mmp = NULL;
        }
    }
    fileSize = 0;
    sof = eof = NULL;
    sep = whiteChar = quote = dec = '\0';
    stripWhite = fill = true;

    printf("\nFile successfully unmapped.\n");
    return requires_clean;
}

// Recast a 'const char' as a 'char/
static char* _const_cast(const char *ptr) {
    union {const char *a; char *b;} tmp = { ptr };
    return tmp.b;
}

// Move to the next non whitespace character
void skip_whitechar(const char **pch) {
    const char *ch = *pch;
    if (whiteChar == 0) {
        while (*ch == ' ' || *ch == '\t' || (*ch == '\0' && ch < eof)) ch++;
    } else {
        while (*ch==whiteChar || (*ch == '\0' && ch < eof)) ch++;
    }
    *pch = ch;
}

// Given a position, this tests if any common (and not so) line endings occur at 
// the position and moves to the end of the line if they do:
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
                                        && (ch==eof || test_moveto_eol(&ch))));
}

// Given a FieldParserContext pointing to the start of a potential field, 
// advance its 'ch' ptr to the actual start of the field and calculate the 
// length to the proper end of the field.
// Field can be obtained immeditely after by ch - len
void parse_field(struct FieldParserContext *ctx) {
    const char *ch = *(ctx->ch);
    const char *fieldStart;
    int32_t fieldOff = 0;
    int32_t fieldLen = 0;
    // skip leading spaces
    if ((*ch == ' ' && stripWhite) || (*ch == '\0' && ch < eof)) {
        while(*(++ch) == ' ' || (*ch == '\0' && ch < eof)) fieldOff++;
    } 
    ctx->off = fieldOff; //give context a clue to actual start of field
    fieldStart = ch;
    // TODO: implement alternative quote rules. For now, just keep all quotes.
    // if (*ch != quote ) or it does but we want to keep them:  
    {
        while (!test_end_of_field(ch)) ch++; //will end on sep, \n, \r, or eof
        *(ctx->ch) = ch; //give context the final parsed position 
        fieldLen = (int32_t)(ch - fieldStart);
        // remove any lagging spaces 
        while (fieldLen > 0 && ((ch[-1]==' ' && stripWhite) || ch[-1]=='\0')) {
            fieldLen--; ch--;
        }
        if (fieldLen == 0) fieldLen = INT32_MIN; //blanks are NAs
        ctx->len = fieldLen; // give context the length of field after start
    }
}

// Counts the number of fields in a line - returns n fields, or -1 if it fails
// If successful, advances the input char from the beginning og the line to the
// start of the next line.
int countfields(const char **pch) {
    const char *ch = *pch;
    // skip starting spaces even if sep==space, since they don't break up fields,
    // (skip_whitechar would ignore them since it doesn't skip seps)
    if (sep == ' ') while (*ch == ' ') ch++;
    skip_whitechar(&ch);
    if (test_moveto_eol(&ch) || ch == eof) {
        *pch = ch+1;
        return 0;
    }
    int ncol = 1;
    struct FieldParserContext ctx = {
        .ch = &ch, .off = 0, .len = 0,
    };
    while (ch < eof) {
        parse_field(&ctx); //advances ch to either sep, \n, \r, or eof
        if (false) {
               printf("Field: ");
               for (int i = 0; i < ctx.len; ++i) printf("%c", *(*ctx.ch - ctx.len + i));
               printf("\t(len = %d)\n", ctx.len);
           }
        // if next character is end-of-line, advance
        if (ch[1] == '\r' || ch[1] == '\n' || (ch[1] == '\0' && ch+1 == eof)) {
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
        if (ch != eof) return -1;
        break;        
    }
    *pch = ch;
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
    while (ch < eof) {
        parse_field(&ctx);
        get_field_type(&ctx);

         // if next character is end-of-line, advance
        if (ch[1] == '\r' || ch[1] == '\n' || (ch[1] == '\0' && ch+1 == eof)) {
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
        if (ch != eof) return -1;
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
    // [2] Open and memory-map file
    // Need to set approriate sof and eof ponters 
    //=========================================================================
    const char *ch = NULL; //reusable pointer for storing current char
    printf("[2] Mapping input to memory.\n");
    {
    if (args.input) {
        printf("Reading text from input.");
        sof = args.input;
        fileSize = strlen(sof);
        eof = sof + fileSize;
        if (*eof != '\0') STOP("Input was not null-terminated.\n");
    } 
    else if (args.filename) {
        printf("Opening file %s...\n", filename);
        const char *fname = args.filename;
        
        // obtain filesize
        int fd = open(fname, O_RDONLY);
        if (fd == -1) STOP("File could not be read: %s\n", fname);
        struct stat sysStatBuf;
        if (fstat(fd, &sysStatBuf) == -1) {
            close(fd);
            STOP("Opened file but could not read its size: %s\n", fname);
        }
        fileSize = (size_t)sysStatBuf.st_size;
        if (fileSize == 0) {
            close(fd);
            STOP("File is empty: %s\n", fname);
        }
        printf("File opened successfully, size = %lu bytes.\n", fileSize);

        // map input into memory - provide no mapping so the kernel chooses,
        // and return the address of the new mapping
        mmp = mmap(NULL, fileSize, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        close(fd);
        if (mmp == MAP_FAILED) {
            STOP("Opened file but failed to map into memory.");
        }
    }

    // mmp now points to the the address of the file mapping, which is a void *
    // this is the start-of-file
    sof = (const char*) mmp;
    eof = sof + fileSize;

    if (true) {
        printf("first 100 chars: \n");
        for (int i = 0; i < 100; i++) {
            printf("%c", *(sof + i));
        }
        putchar('\n');   
    }
    }
    //=========================================================================
    // [3] Handle Byte-Order-Mark
    //     Files could contain start (and end) bytes based on their encoding 
    //     which need to be skipped. Will only UTF-8 be supported? Should be 
    //     the most common scenario, although BOMs in general should be uncommon.
    //=========================================================================
    printf("[3] Detecting & Skipping BOMs.\n");
    // UTF-8 encoding
    if (fileSize >= 3 && memcmp(sof, "\xEF\xBB\xBF", 3) == 0) {
        sof += 3;
        printf("Found and skipped UTF-8 BOM.\n");
    }
    // handle other encodings : https://en.wikipedia.org/wiki/Byte_order_mark

    //=========================================================================
    // [4] Convert mmap to be a null (\0) terminated string
    //     Now 'eof' is truly the end of our input, as represented by null ptr.
    //     Advantage - we don't need to constantly test for eof. 
    //=========================================================================
    printf("[4] Null terminating the memory mapped input.\n");
    ch = sof;
    while (ch < eof && *ch != '\n') ch++;
    if (ch == eof) STOP("ERROR: No 'newlines' detected in this file.\n");
    //TODO: handle special cases where no newlines, and only \r carriage returns
    //Otherwise, file has \n and may also contain \r\n cases, which are handled
    bool lastEOLreplaced = false;
    if (args.filename) { //if args.input, string already ends with \0
        ch = eof - 1; //eof was sof + fileSize, ie 1 byte past the file's last byte
        // Determine pos of last newline (\n) in the file, removing any spaces
        // also remove any leading \r to avoid a dangling \r 
        while (ch >= sof && *ch != '\n') ch--;
        while (ch > sof && ch[-1] == '\r') ch--;
        if (ch >= sof) {
            const char *lastNewLine = ch;
            // Confirm that only spaces followed the newline
            while (++ch < eof && isspace(*ch)) {};
            if (ch == eof) {
                eof = lastNewLine;
                lastEOLreplaced = true;
            }
        }
    }
    if (!lastEOLreplaced) {
        STOP("File has no final new-line character (\\n), please fix.\n");
    }
    // replace the last new-line with the null-byte
    *_const_cast(eof) = '\0';
 
    //=========================================================================
    // [5] TODO: Jump 'sof' to some n of skipped rows, or to some given string
    // For now, just make sure to skip over any blank input at start of file 
    // !TODO! - really should implement the skipnrows so user can manually skip
    //          over any kind of header rows
    //=========================================================================
    printf("[5] Skipping lines as needed.\n");
    const char *pos = sof; // pointer to start of data
    int row1line = 1;   // line number of start of data, for messages
    {
    ch = pos;
    const char *lineStart = ch;
    while (ch < eof && (isspace(*ch) || *ch == '\0')) {
        if (*ch == '\n') { ch++; lineStart=ch; row1line++; } else { ch++; }
    }
    if (ch >= eof) STOP("Input is either empty of fully whitespace.\n");
    if (lineStart > pos) printf("Skipped some blank lines at start of file.\n");
    ch = pos = lineStart;
    }

    //=========================================================================
    // [6] Auto detect ncols
    // Sample first n rows to determine if there are inconsistencies, because
    // we will want to fill those if so.
    // TODO: could detect other args too (sep, dec, quote rule, etc.)
    //=========================================================================
    printf("[6] Detecting number of columns.\n");
    int ncol = 0;
    int tst_ncol = 0;
    int testRows = 100; //will be 100 or nrows if nrows is < 100
    if (args.sep == '\n') {
        // user wants each line to be read as a single column
        sep = 127; // ascii del - won't be in data, not \n \r or \0
        ncol = 1;
        fill = true;
    } 
    else {
        ncol = countfields(&ch); //first line is expected to not be blank
        if (ncol < 0) STOP("Internal error parsing number of columns.\n"); //TODO: handle this?
        int thisRow = 0;
        while (ch < eof && thisRow++ < testRows) {
            tst_ncol = countfields(&ch);
            ncol = tst_ncol > ncol ? tst_ncol : ncol;
        }
    }
    printf("Number of columns: %d\n", ncol);
    ch = pos;
    //=========================================================================
    // [7] Auto detect column types & whether first row is column names
    //=========================================================================
    printf("[7] Detecting column types.\n");
    type = (int8_t *)malloc( (size_t)ncol * sizeof(int8_t) );
    tmpType = (int8_t *)malloc( (size_t)ncol * sizeof(int8_t) );
    if (!type || !tmpType) STOP("ERROR: Failed to allocate type buffers: %s",
                                                                strerror(errno));
    int8_t type0 = 1;
    for (int j = 0; j < ncol; j++) 
        tmpType[j] = type[j] = type0;
    
    // if (args.header != false) {
    //     countfields(&ch); //skip header row (likely colnames)
    // } else {
    //     // if all string, take to be colnames
    //     // else, assume no header exists

    // }
    detect_types(&ch, ncol);
    detect_types(&ch, ncol);

    readfile_cleanup();
    return 1;
}

// TODO:
// keep parsing types
// figure out how to store types for each column and how to check multiple lines for col types:
//      have a hierarchy, for ex - if most rows were int but some rows were numeric, row is numeric
//
// figure out how data is going to be stored once i start copying from the txt file into memory
//      simple: copy values and convert them to their appropriate type and store in some dataframe schema
//      however: I don't need to be storing data as some specific type, I really just want a mapping that says, for any given field:
//          you belong to this column, you are of this type, so if the user tries some operation which requires you to be 
//          numeric, can you be?
//          This keeps things more lightweight and realistically... 
// This data is not going to be living in memory for an extended period like it would if we were scripting w/ it in R or Python
// We _do not need_ to map it into some data.table format
// We simply want to:
// - quickly access it/get a view of the data 
// - determine where to start and stop processing the text
// - figure out how to parse each field (based on sep, dec, quote, etc)
// - process user's command (sort, unique, mean, etc)
// - prep the appropriate data (ie which cols and rows) for the command, converting types as needed
// - execute the command & redirect the output to stdout or some file
// 
//  struggles:
//   how to store all of the meta info on where individual fields begin and end?
//   how to access a specific column when the input is one giant text string?
//   


