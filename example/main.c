#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aho_corasick.h"

#define NO_OF_THREADS 2
#define MAX_PATTERN_SIZE 128

void read_file_to_search (const char *filename, STRING *buffer);
int read_patterns (const char *filename, STRING *patterns);
void print_usage (const char *exec_file);
int match_handler(MATCH * m, int automata_num, int thread_num);

int main(int argc, char **argv)
{
	AC_AUTOMATA *aca;
	STRING *patterns, input_buffer;
	unsigned int i, j, no_of_patterns;
	int clopt;

	/* Command line config*/
	const char *pattern_file;
	const char *input_file;
	short verbosity = 0;

	if (argc < 4) {
		print_usage(argv[0]);
		exit(1);
	}

	while ((clopt = getopt(argc, argv, "P:vh?")) != -1) {
		switch (clopt) {
			case 'P':
				pattern_file = optarg;
				break;
			case 'v':
				verbosity = 1;
				break;
			case 'h':
			case '?':
			default:
				print_usage(argv[0]);
				exit(1);
		}
	}
	input_file = *(argv + optind);

	if (verbosity)
		printf("Loading patterns from file - %s\n", pattern_file);
	patterns = (STRING *) malloc(12 * sizeof(STRING));	
	no_of_patterns =  read_patterns (pattern_file, patterns);

	aca = (AC_AUTOMATA *) malloc(sizeof(AC_AUTOMATA)*NO_OF_THREADS);
	
	if (verbosity)
		printf("Initialising automata\n");
	
	#pragma omp parallel for private(i) shared(aca)
	for (i = 0; i < NO_OF_THREADS; i++)
		ac_automata_init(&aca[i], match_handler);
	
	if (verbosity)
		printf("Adding strings\n");

	#pragma omp parallel for collapse(2)
	for (i = 0; i < NO_OF_THREADS; i++)
		for (j = 0; j < no_of_patterns; j++)
			ac_automata_add_string(&aca[i], patterns + j);

	read_file_to_search(input_file, &input_buffer);

	if (verbosity)
		printf("Searching\n");

	#pragma omp parallelfor private(i)
	for (i = 0; i < NO_OF_THREADS; i++) {
		int id = omp_get_thread_num();
		if (verbosity) {
			printf("In thread: %d, Automata: %d\n", id, i);
		}

		ac_automata_search(&aca[i], &input_buffer, i, id);
	}
	
	if (verbosity)
		printf("Freeing resources\n");

	for (i = 0; i < NO_OF_THREADS; i++)
		ac_automata_release (&aca[i]);

	return 0;
}


void read_file_to_search (const char *filename, STRING *buffer)
{
	FILE *fp;

	fp = fopen(filename, "r");

    fseek(fp, 0, SEEK_END);
    buffer->length = ftell(fp) + 1;
    rewind(fp);

    buffer->str = (ALPHA *) malloc((buffer->length) * sizeof(ALPHA));
    fread(buffer->str, buffer->length - 1, 1, fp);
    buffer->str[buffer->length - 1] = '\0';

    fclose(fp);
}


int read_patterns (const char *filename, STRING *patterns)
{
	unsigned int no_of_patterns, i;
	char *buffer = (char *) malloc(MAX_PATTERN_SIZE * sizeof(char));
	FILE *fp;
	
	fp = fopen(filename, "r");

	fscanf(fp, "%u\n", &no_of_patterns);

	for (i = 0; i < no_of_patterns; i++) {
		fscanf(fp, "%s\n", buffer);

		patterns[i].str = (ALPHA *) malloc((strlen(buffer)+1) * sizeof(ALPHA));
		
		strcpy(patterns[i].str, buffer);
		patterns[i].length = strlen(buffer) + 1;
		patterns[i].id = i;
	}

	return no_of_patterns;
}


void print_usage (const char *exec_file)
{
    printf("Usage: %s [-v] -P pattern_file file1\n", exec_file);
}


int match_handler(MATCH * m, int automata_num, int thread_num)
{
	unsigned int j;

	printf ("@ Thread %ld Automata %ld position %ld string(s) ", thread_num, automata_num, m->position);

	for (j=0; j < m->match_num; j++)
		printf("%ld (%s), ", m->matched_strings[j].id, m->matched_strings[j].str);
	/*
	CAUTION: be carefull about using m->matched_strings[j].str
	if 'str' has permanent string allocation inside your program 
	memory area, you can use this form. otherwise it will point to
	an incorrect memory place. in this case you must reconstruct
	the recognized pattern from the input string.
	*/
	printf("matched\n");

	/* to find all matches alwas return 0 */
	return 0;
	/* 
		return 0 : contiue searching
		return none zero : stop searching
		
		as soon as you satisfied with search results, you can stop search and 
		exit from ac_automata_search() and return to the rest of your program.
		
		as an example if you only need first N matches,
		define a counter and return none zero after the counter exceeds N.
	*/
}