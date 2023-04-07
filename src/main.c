#include <stdio.h>
#include <stdlib.h>
#include <getopt.h> //getopt_long, struct option
#include <string.h> //strcpy

#include "readfile.h" //readfile
#include "common.h" //args, clean_globals


#define BUFSIZE 8192 //eventually this will have to be much bigger or dynamic 
#define END_OF_LINE -1

extern struct Args args;
extern char *optarg; //getopt.h
extern int optind; //getopt.h


static struct option const longopts[] =
{
  {"sep", required_argument, NULL, 's'}, //, . ; |
  {"cols", required_argument, NULL, 'c'},
//   {GETOPT_HELP_OPTION_DECL},
//   {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

// https://stackoverflow.com/questions/3939157/c-getopt-multiple-value
int count_arg_list(int argc, char **argv, int _optind) {
	_optind--;
	int list_len = 0;
	for (; _optind < argc-1 && //don't count the file name TODO: temp fix, what if input is piped?
			  !(*argv[_optind] == '-' && strlen(argv[_optind]) == 2);
		 ++_optind
		 ) { ++list_len;
		    printf("%s ", argv[_optind]);  
			}
	putchar('\n');
	return list_len;
}

void parse_selected_cols(int argc, char **argv, int _optind) {
	int n = count_arg_list(argc, argv, _optind);
	printf("list len = %d\n", n);
	_optind--;
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



void argparse(int argc, char **argv) {
	// TODO: implement piping input from command line https://www.delftstack.com/howto/c/pipe-in-c/
	int optc;
	// int parsing_vals = 0;
	if (argc == 1) {
		STOP("ERROR: No file provided.\n");
	}
	printf("Program=%s. ArgCount=%d. File=%s\n", argv[0], argc, argv[argc-1]);
	// Get target file
	args.filename = malloc(strlen(argv[argc-1]) + 1);
	strcpy(args.filename, argv[argc-1]);
	// printf("args.filename=%s\n", args.filename);

	while ((optc = getopt_long(argc, argv, "s:c:", longopts, NULL)) != -1) {
		switch (optc) {
			case 's':
				printf("Arg = s\n");
				printf(" - val = %c\n", optarg[0]);
				if (optarg[0]) {
					args.sep = optarg[0];
					if (!(args.sep == ','  || 
					     args.sep == '\t' || args.sep == '|' || 
					     args.sep == ';'  || args.sep == ':')) {
							STOP("Provide proper sep\n");
					   }
				}
				break;
			case 'c':
				printf("Arg = c\n");
				printf(" - val = ");
				parse_selected_cols(argc, argv, optind);
				break;
		}
	}


	// for (int i = 2; i < argc; ++i) {
	// 	ch = argv[i];		
	// 	while (*ch=='-') {
	// 		ch++; parsing_vals=0;
	// 	}
	// 	if (!parsing_vals) {
	// 		printf("arg: %s\n", ch);
		
	// 	} else {
	// 		printf("- value: %s\n", ch);
	// 	}
	// 	parsing_vals = 1;
	// }

	args.whiteChar = (args.sep == ' ' ? '\t' : (args.sep == '\t' ? ' ' : 0)); //0: both
}





int main(int argc, char **argv) {
	printf("[ MAIN ]\n");
	int ae;
	if ((ae = atexit(clean_globals)) != 0) {
		STOP("Internal error: main.c: Cannot register 'clean_globals' for atexit.\n");
	};
	
	argparse(argc, argv);

	read_file();
}
