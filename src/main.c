#include <stdio.h>
#include <stdlib.h>
#include <getopt.h> //getopt_long, struct option
#include <string.h> //strcpy
#include "readfile.h" //readfile
#include "common.h" //args, clean_globals
#include "simplestats.h"

#define BUFSIZE 8192 //eventually this will have to be much bigger or dynamic 
#define END_OF_LINE -1

extern struct mainArgs args;
extern char *optarg; //getopt.h
extern int optind; //getopt.h

// TODO: header, rowLimit, dec, quote
static struct option const longopts[] =
{
  {"delim", required_argument, NULL, 'd'}, //, . ; | \t (ie, sep/fieldsep)
  {"fields", required_argument, NULL, 'f'},
  {"quotehead", no_argument, NULL, 'q'}, //flag, keep quotes on header names
//   {GETOPT_HELP_OPTION_DECL},
//   {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

// https://stackoverflow.com/questions/3939157/c-getopt-multiple-value
// TODO:
// problems: this method will stop on any '-', even if it is the beginning of a
// column name and not the next option. (ex: main -c var1name -var2name -s,)
int count_arg_list(int argc, char **argv, int _optind) {
	_optind--; //optind left at beginning of next arg, decrement
	int list_len = 0;
	for (; _optind < argc && !(*argv[_optind] == '-'); ++_optind) { 
		list_len++;
		printf("%s ", argv[_optind]);  
	}
	putchar('\n');
	return list_len;
}


void parse_selected_cols(int argc, char **argv, int _optind) {
	int n = count_arg_list(argc, argv, _optind);
	printf("list len = %d\n", n);
	_optind--; //optind left at beginning of next arg, decrement
    args.n_colselect = n;
    args.colselect = malloc(sizeof(char*) * n);
	if (args.colselect) {
    	for (int i = 0; i < n; ++i) {
        	args.colselect[i] = malloc( strlen(argv[_optind + i]) + 1 );
        	strcpy(args.colselect[i], argv[_optind + i]);
        	// printf("Selected col: %s\n", args.colselect[i]);
    	}
	}
}

void parse_fieldsep(char user_sep) {
	// TODO: shell strips '\' char, is this the best way to account for this?
	if (user_sep == 't') user_sep = '\t';
	if (!(user_sep == ',' || 
	     user_sep == '\t' || user_sep == '|' || 
	     user_sep == ';'  || user_sep == ':')) {
			STOP("provided delim is not implemented\n");
	   }
	args.sep = user_sep;
}


void argparse(int argc, char **argv) {
	// TODO: implement piping input from command line https://www.delftstack.com/howto/c/pipe-in-c/
	// - there may not be a filename, so need to add some control flow here
	int optc;
	// int parsing_vals = 0;
	if (argc == 1) {
		// usage()
		STOP("ERROR: Usage: ./main [OPTIONS]...[FILENAME]\n");
	}
	printf("Program=%s. ArgCount=%d. File=%s\n", argv[0], argc, argv[argc-1]);
	// Get target file
	args.filename = malloc(strlen(argv[argc-1]) + 1);
	strcpy(args.filename, argv[argc-1]);
	// If there is a filename provided and we've grabbed it, rm it from argv
	argv[argc] = NULL; argc--;

	while ((optc = getopt_long(argc, argv, "d:f:q", longopts, NULL)) != -1) {
		switch (optc) {
			case 'd':
				printf("Arg = d\n");
				printf(" - val = %c\n", optarg[0]);
				if (optarg[0]) { //if char is not null
					args.sep = optarg[0];
					parse_fieldsep(args.sep);
				} else {
					STOP("the delimeter must be one character\n");
				}
				break;
			case 'f':
				printf("Arg = f\n");
				printf(" - val = ");
				parse_selected_cols(argc, argv, optind);
				break;
			case 'q':
				printf("Arg = q\n - (flag true)\n");
				args.header_keep_quotes = true;
				break;
		}
	}
}





int main(int argc, char **argv) {
	printf("[ PROCESS ARGUMENTS ]\n");
	int ae;
	if ((ae = atexit(clean_globals)) != 0)
		STOP("Internal error: main.c: Cannot register 'clean_globals' for atexit.\n");
	// parse command line
	argparse(argc, argv);
	// open file (FILE *fp) and prep it for use
	// fills out mainArgs with info needed to parse the file 
	printf("[ READFILE ]\n");
	read_file();
	if (!fp) STOP("File was lost.\n"); // hopefully unreachable
	
	// execute operation
	printf("[ OPERATION ]\n");
	file_info();
	column_mean();
}



// selected_col_inds is sorted based on order cols appear in the data (from l to r),
// which makes things simpler but prevents the user from specifying the order of
// output. maybe spefic print fns can attempt that functionality 
// for (int i = 0; i < args.n_colselect; i++){
//     printf("args.selected_col_inds[%d] = %d\n", i, args.selected_col_inds[i]);
// }