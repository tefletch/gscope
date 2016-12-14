#define dbfputs(s)          (dboffset += strlen(s), fputs(s, newrefs))

extern uint32_t	    dboffset;	    /* new database offset */
extern uint32_t	    fileindex;	    /* source file name index */
extern uint32_t     lineoffset;     /* source line database offset */
extern int          symbols;        /* number of symbols */

gboolean crossref(char *srcfile);
void warning(char *text);
