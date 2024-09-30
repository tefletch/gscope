extern  FILE    *newrefs;       /* new cross-reference */
extern char     dicode2[];      /* digraph second character code */

extern int      fileversion;    /* cross-reference file version */
extern char     dichar1[];      /* 16 most frequent first chars */
extern char     dichar2[];      /* 8 most frequent second chars using the above as first chars */
extern char     dicode1[];      /* digraph first character code */
extern char     dicode2[];      /* digraph second character code */

extern time_t       autogen_elapsed_sec;
extern suseconds_t  autogen_elapsed_usec;

void  BUILD_initDatabase(GtkWidget *progress_bar);
void  BUILD_init_cli_file_list(int argc, char *argv[]);

