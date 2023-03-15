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
#include <ctype.h> //isspace

#include "readfile.h"

extern int errno;

/* Private globals */
// To save passing to many functions
static const char *sof, *eof;
static char sep; //ie, the delimiter
static char whiteChar; //what is considered whitespace - ' ', '\t', or 0 for both
static char quote, dec;
static bool stripWhite = true;

// To clean on error and return
static void *mmp = NULL;
// static void *mmp_copy = NULL;
static size_t fileSize;
// static int8_t *type = NULL, *tmpType = NULL, *size = NULL;
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
    int8_t header; //yes 1, no 0, autodetect int 8 min -128
};

struct mainArgs args = {
    .rowLimit = INT64_MAX,
    .sep = ',',
    .dec = '.',
    .quote = '"',
    .stripWhite = true,
    .header = true,
}; 

struct FieldParserContext {
    const char **ch;
    int32_t len; //signed, negative is NA, 0 is empty ("")
} FieldParser;

/* Utils */

bool readfile_cleanup(void) {
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
    stripWhite = true;
    
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

// Given a FieldParser pointing to the start of a potential field, advance its
// 'ch' ptr to the actual start of the field and calculate the length to the 
// proper end of the field.
void parse_field(struct FieldParserContext *ctx) {

    const char *ch = *(ctx->ch);
    const char *fieldStart;
    int32_t fieldLen;
    // skip leading spaces
    if ((*ch == ' ' && stripWhite) || (*ch == '\0' && ch < eof)) {
        while(*(++ch) == ' ' || (*ch == '\0' && ch < eof));
    }
    fieldStart = ch;
    // TODO: implement alternative quote rules. For now, just keep all quotes.
    // if (*ch != quote ) or it does but we want to keep them:  
    {
        while (!test_end_of_field(ch)) {ch++;} //will end on sep, \n, \r, or eof
        *(ctx->ch) = ch;
        fieldLen = (int32_t)(ch - fieldStart);
        // remove any lagging spaces 
        while (fieldLen > 0 && ((ch[-1]==' ' && stripWhite) || ch[-1]=='\0')) {
            fieldLen--; ch--;
        }
        if (fieldLen == 0) fieldLen = INT32_MIN; //blanks are NAs
        ctx->len = fieldLen;
    }
}

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
        .ch = &ch, .len = 0,
    };
    while (ch < eof) {
        parse_field(&ctx); //advances ch to sep, \n, \r, or eof 
        if (ch[1] == '\r' || ch[1] == '\n' || (ch[1] == '\0' && ch+1 == eof)) {
            ch++; //advance to eol char
        }
        // if sep, we have a new column so count it and move to next field
        if (*ch == sep) {
            ch++; ncol++;
            printf("Field: ");
            for (int i = 0; i < ctx.len; i++ ) {
                printf("%c", *(ch + i));
            }
            printf(" (len = %d)\n", ctx.len);

            continue;
        }
        // if reaching end of line
        if (test_moveto_eol(&ch)) { 
            *pch = ch+1;
            return ncol; 
        }
        // if not sep or eol, we can't parse this field correctly
        if (ch != eof) return -1;
        break;        
    }
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
    // header = args.header

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

    for (int i = 0; i < 100; i++) {
        printf("%c", *(sof + i));
    }
    putchar('\n');   

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
    // [6] Auto detect separator, quoting rule, first line, and ncol
    //=========================================================================
    // int ncol;  
    int ncol = countfields(&sof);
    printf("Fields: %d\n", ncol);

    
    readfile_cleanup();
    return 1;
}



