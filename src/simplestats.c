#define _GNU_SOURCE
#include<stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> //memset
#include <unistd.h> //ssize_t
#include "common.h"

extern int errno;
extern FILE *fp;


void basic_mean(void) {
    printf("\n__basic_mean__\n");
    // if (args.n_colselect != 1) STOP("Select one column to get its mean value.\n");
    float sums[args.ncols]; memset(sums, 0, args.ncols*sizeof(float));
    int nans[args.ncols]; memset(nans, 0, args.ncols*sizeof(int));
    int ns[args.ncols]; memset(ns, 0, args.ncols*sizeof(int));

    rewind(fp);
    int32_t row = 0;
    int delim = '\n';
    // ITER ROWS
    char* line = NULL; size_t lsize = 0; ssize_t llen = 0;
    while ((llen = getdelim(&line, &lsize, delim, fp)) > -1) {
        // skip over rows, including header row if it exists.
        if (row < args.skiprows) { row++; continue; }
        if (row == args.skiprows && args.header) { 
            row++; args.skipchars = 0; //skipchars would have been for the header
            continue; 
        }
        sol = line + args.skipchars;
        eol = line + llen;
        struct FieldContext fields[args.ncols];
        int it = iterfields(sol, args.ncols, fields, 1);
        if (it == -1) STOP("simplestats.c: basic_mean: Fields could not be parsed.\n"); //iterfields stores discovered field info in FieldContext   
        if (it == 0) { row++; continue; } //empty line

        // ITER COLS & ADD TO RUNNING TOTALS
        for (int i = 0; i < args.ncols; ++i) {
            if (!args.col_is_selected[i])
                continue;
            float val = field_to_float(sol, fields[i]);
            if (val == NA_FLOAT) {
                nans[i]++;
            } else {
                printf("val=%f\n", val);
                sums[i] += val;
                ns[i]++;
            }
        }
    }
    // Display results
    float mean;
    for (int i = 0; i < args.ncols; ++i) {
        if (!args.col_is_selected[i]) continue;
        if (ns[i] == 0) { 
            mean = 0; 
        } else { 
            mean = sums[i] / ns[i];
        }
        printf("%s - MEAN: %f | NA: %d\n", colnames[i], mean, nans[i]);
    }
}


// For next time : can we do this without iterating over all of the columns, 
// instrad just iterating over the selected cols? this will be important for 
// high dim datasets