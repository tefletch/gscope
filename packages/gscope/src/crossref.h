#define dbfputs(s)          (dboffset += strlen(s), fputs(s, newrefs))

extern uint32_t     dboffset;       /* new database offset */
extern uint32_t     fileindex;      /* source file name index */
extern uint32_t     lineoffset;     /* source line database offset */
extern int          symbols;        /* number of symbols */


//===============================================================
// Defines
//===============================================================

/* Check if a given pair of chars is compressable as a dicode: */
#define IS_A_DICODE(inchar1, inchar2)                      \
  (dicode1[(unsigned char)(inchar1)] && dicode2[(unsigned char)(inchar2)])

/* Combine the pair into a dicode */
#define DICODE_COMPRESS(inchar1, inchar2)       \
  ((0200 - 2) + dicode1[(unsigned char)(inchar1)]   \
   + dicode2[(unsigned char)(inchar2)])



//===============================================================
// Public Functions
//===============================================================

gboolean crossref(char *srcfile);
void warning(char *text);
