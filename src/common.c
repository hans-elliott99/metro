#include <stdbool.h> //true, false
#include <stdint.h> //int32_t, int64_t, etc
#include <ctype.h> //isspace, isdigit, 
#include <string.h> //strlen, memset
#include <stdio.h> //FILE, fclose
#include <errno.h>

#include "common.h"

extern int errno;
char **colnames = NULL;
FILE *fp = NULL;

// Default values for args
struct mainArgs args = {
    .filename = NULL,
    .header = true,
    .header_keep_quotes = false,
    .rowLimit = INT64_MIN, //all rows
    .sep = 0, //guess sep
    .dec = '.',
    .quote = '"',
    .colselect = NULL,
    //
    .skiprows = 0,
    .skipchars = 0,
    .ncols = 0,
    .n_colselect = 0,
    .col_is_selected = NULL,
    .selected_col_inds = NULL,
    .whiteChar = 0,
    .stripWhite = true
};


/////////////////////////////// HELPERS ////////////////////////////////////////
void clean_globals(void) {
    printf("\n__clean_globals__\n");
    if (fp) {
        printf("closing file\n");
        if (fclose(fp) == EOF) 
            fprintf(stderr, "ERROR %d: %s. (Failed in closing file).", errno, strerror(errno));
        fp = NULL;
    }
    if (args.colselect) {
        printf("freeing args.colselect\n");
        for (int i = 0; i < args.n_colselect; ++i) {
            free(args.colselect[i]);
        }
        free(args.colselect); args.colselect = NULL;
    }
    if (args.col_is_selected) {
        printf("freeing args.col_is_selected\n");
        free(args.col_is_selected); args.col_is_selected = NULL;
    }
    if (args.selected_col_inds) { 
        printf("freeing args.selected_col_inds\n");
        free(args.selected_col_inds); args.selected_col_inds = NULL;
    }
    if (colnames) {
        printf("freeing colnames\n");
        for (int i = 0; i < args.ncols; ++i) {
            free(colnames[i]);
        }
        free(colnames); colnames = NULL;
    }
} 



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


// // Recast a const char* as a char*
// static char* _const_cast(const char *ptr) {
//     union {const char *a; char *b;} tmp = { ptr };
//     return tmp.b;
// }

///////////////////////////// FIELD PARSING ////////////////////////////////////


// Move to the next non whitespace character
void skip_whitechar(const char **pch) {
    char whiteChar = args.whiteChar;
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
bool check_moveto_eol(const char **pch) {
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
            ch++;
        } else {
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
bool check_end_of_field(const char *ch) {
    return ((*ch==args.sep) || ((*ch=='\n' || *ch=='\r' || *ch=='\0') 
                                        && (ch==eol || check_moveto_eol(&ch))));
}



// Advance the char pointer to the end of the current field, while incrementing
// the offset from the start of the line to equal the start of this field, and 
// calculating the field len (n chars) that correspond to actual data.
// keepQuotes: if 0, skip over " or ' chars at the beginning/end of each field. 
void parse_field(const char **pch, int32_t *pFieldOff, int32_t *pFieldLen, 
                 int keepQuotes) {
    bool stripWhite = args.stripWhite;
    const char *ch = *pch;
    int32_t fieldOff = *pFieldOff; // may be greater than 0
    int32_t fieldLen = 0;
    // skip leading spaces
    if ((*ch == ' ' && stripWhite) || (*ch == '\0' && ch < eol)) {
        while (*ch == ' ' || (*ch == '\0' && ch < eol)) { ch++; fieldOff++; }
    }
    // skip quotes at field start
    // TODO: implement alternative quote rules. For now, just keep all quotes.
    if (!keepQuotes) {
        while(*ch == '"' || *ch == '\'') { ch++; fieldOff++; }
    }
    *pFieldOff = fieldOff; //offset from sol to start of field
    const char *fieldStart = ch;
    {
        //advance ch to one of: next sep, \n, \r, or eol
        while (!check_end_of_field(ch)) ch++;
        *pch = ch; //returning pointer to end of field.
        fieldLen = (int32_t)(ch - fieldStart);
        // remove any white spaces which follow the data
        while (fieldLen > 0 && ((ch[-1]==' ' && stripWhite) || ch[-1]=='\0')) {
            fieldLen--; ch--;
        }
        // remove quotes from field end
        while (fieldLen > 0 && (!keepQuotes && (ch[-1]=='"' || ch[-1]=='\''))) {
            fieldLen--; ch--;
        }
        if (fieldLen == 0) fieldLen = NA_INT32; //blanks are NAs
    }
    *pFieldLen = fieldLen; //length from fieldOff to end of field
}


// Counts the number of fields in a line, given pointer to start of line
int countfields(const char *ch) {
    // skip starting spaces even if sep==space, since they don't break up fields,
    // (skip_whitechar ignores them since it doesn't skip seps)
    int ncol = 1;
    int32_t fieldOff = 0;
    int32_t fieldLen = 0;
    if (args.sep == ' ') while (*ch == ' ') ch++;
    skip_whitechar(&ch);
    if (check_moveto_eol(&ch)) {
        return 0; //empty line, 0 fields
    }
    while (ch < eol) {
        parse_field(&ch, &fieldOff, &fieldLen, 1); //1 : keep quotes 
        // parse_field advances ch to either sep or eol (\n, \r)
        if (args.sep==' ' && *ch==args.sep) {
            while (ch[1]==' ') ch++; //skip over multiple spaces
            // if next character is an end-of-line char, advance to it
            if (ch[1] == '\r' || ch[1] == '\n' || (ch[1] == '\0' && ch+1 == eol)) {
                ch++;
            }
        }
       // if sep, we have found a new field so count it and move to next
        if (*ch == args.sep) {
            ncol++; ch++;
            continue;
        }
        // if ch is an eol, we conclude
        // (this is what we want to reach for well behaved rows)
        if (check_moveto_eol(&ch)) { 
            return ncol; 
        }
        if (ch!=eol) return -1; //only reachable if sep & quote rule are invalid for this line
    }
    return ncol;
}

// Takes char pointing to start of line, the desired ncols, and the 
// FieldContext array - which it will populate.
// keepQuotes: whether to keep quotes when parsing the fields (0 False, 1 True).
// Returns: the number of found fields, 0 if none, -1 if error
int iterfields(const char *ch, const int ncol, 
               struct FieldContext *fields, int keepQuotes) {
    //TODO: implement checking if n of parsed fields == ncol, + error handling
    const char *start = ch;
    int32_t fieldOff = 0;
    int32_t fieldLen = 0;
    int n_fields = 1;
    // skip starting spaces even if sep==space, since they don't break up fields,
    // (skip_whitechar ignores them since it doesn't skip seps)
    if (args.sep == ' ') while (*ch == ' ') ch++;
    skip_whitechar(&ch);
    if (check_moveto_eol(&ch)) {
        return 0; //empty line, 0 fields
    }
    while (ch < eol) {
        fieldOff = ch - start;
        parse_field(&ch, &fieldOff, &fieldLen, keepQuotes);
        // parse_field advances ch to either sep or eol (\n, \r)
        fields[n_fields-1].off = fieldOff;
        fields[n_fields-1].len = fieldLen;
        if (args.sep == ' ' && *ch==args.sep) {
            while (ch[1]==' ') ch++;
            // ignore any spaces at end of line, advance to eol
            if (ch[1] == '\r' || ch[1] == '\n' || (ch[1] == '\0' && ch+1 == eol)) {
                ch++;
            }
        }
       // if sep, we have found a new field so count it and move to next
        if (*ch == args.sep) {
            n_fields++; ch++;
            continue;
        }
        // if ch is at eol, conclude
        // (this is what we want to happen for well behaved files)
        if (check_moveto_eol(&ch)) {
            //TODO: handle cases of inconsistent row length
            if (n_fields < ncol) STOP("Internal error: iterfields: Number of fields (%d) did not reach ncol (%d)\n", n_fields, ncol);
            if (n_fields > ncol) STOP("Internal error: iterfields: Number of fields (%d) exceeds ncol (%d)\n", n_fields, ncol);
            return n_fields; 
        }
        if (ch!=eol) return -1; //should be only reachable if sep & quote rule are invalid for this line
    }
    return n_fields;
}

//////// FIELD CONVERTERS

// characters that can be skipped over when checking if a string is numeric
int isfloatskip(const char *ch) {
    // TODO: https://github.com/Rdatatable/data.table/blob/a4c2b01720afa94dae69344c9889167122790e91/src/fread.c#L813
    if (*ch == ' ' || *ch == '"' || *ch == '\'' || *ch == 'f' || *ch == '$' ||
        (*ch == '\0' && ch < eol)
        ) {
        return 1;
    }
    return 0;
}


// TODO:
// For self implementation: https://github.com/ochafik/LibCL/blob/master/src/main/resources/LibCL/strtof.c
// Need to accomodate both '.' and ',' decimals, this might be most efficiently
// done by messing with the locale settings: 
// https://stackoverflow.com/questions/7951019/how-to-convert-string-to-float#:~:text=Use%20atof()%20or%20strtof()%20but,setlocale()%20and%20restore%20it%20after%20you%27re%20done. 
// Can we do this without copying the string? Not with strtof, would need another
// method. 
float field_to_float(const char *sol, struct FieldContext field) {
    float val;
    int off = (int)field.off;
    int len = (int)field.len;
    const char *sof = sol + off;
    // skip any spaces or quotes at beginning
    while (isfloatskip(sof) && sof < eol) { sof++; len--; }
    if (!isdigit(*sof)) return NA_FLOAT;
    
    // rm any undesired chars at end
    while (isfloatskip(sof + len - 1) && len > 0) len--;
    if ( !isdigit(*(sof + len - 1)) ) return NA_FLOAT;

    // copy the parsed string into its own buffer
    char str[len + 1];
    strncpy(str, sof, len);
    str[len] = '\0';
    // convert str to float
    char *endptr;
    val = strtof(str, &endptr);
    if (*endptr != '\0') printf("Internal wanring: field_to_float: error creating null terminated string.\n");

    // printf("float found: str=_%s_ == %f\n", str, val);
    return val;
}

