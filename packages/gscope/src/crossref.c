
/*  cscope - interactive C symbol cross-reference
 *
 *  build cross-reference file
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdint.h>

#include "crossref.h"
#include "scanner.h"
#include "build.h"
#include "lookup.h"
#include "utils.h"
#include "app_config.h"


/* convert long to a string */
#define ltobase(value)  n = value; \
            s = buf + (sizeof(buf) - 1); \
            *s = '\0'; \
            digits = 1; \
            while (n >= BASE) { \
                ++digits; \
                i = n; \
                n /= BASE; \
                *--s = i - n * BASE + '!'; \
            } \
            *--s = n + '!';

#define SYMBOLINC   20  /* symbol list size increment */

uint32_t    dboffset;       /* new database offset */
gboolean    errorsfound;    /* prompt before clearing messages */
uint32_t    lineoffset;     /* source line database offset */
int         nsrcoffset;     /* number of file name database offsets */
uint32_t    *srcoffset;     /* source file name database offsets */
int         symbols;        /* number of symbols */

static  char    *filename;  /* file name for warning messages */
//static  uint32_t    fcnoffset;  /* function name database offset */
//static  uint32_t    macrooffset;    /* macro name database offset */
static  int msymbols = SYMBOLINC;   /* maximum number of symbols */
struct  symbol
{    /* symbol data */
    int type;       /* type */
    int first;      /* index of first character in text */
    int last;       /* index of last+1 character in text */
    int length;     /* symbol length */
    int fcn_level;  /* function level of the symbol */
};
static struct symbol *symbol;

/* Local Functions */

static  void     putcrossref(void);
static  void     savesymbol(int token, int num);
static  void     writestring(char *s);
static  void     putfilename(char *srcfile);
static  gboolean file_is_ascii_text(FILE *filename);

gboolean crossref(char *srcfile)
{
    int i;
    int length;     /* symbol length */
    int entry_no;       /* function level of the symbol */
    int token;          /* current token */
    struct stat st;

    if (! ((stat(srcfile, &st) == 0)
           && S_ISREG(st.st_mode)))
    {
        my_cannotopen(srcfile);
        errorsfound = TRUE;
        return(FALSE);
    }

    entry_no = 0;
    /* open the source file */
    if ((yyin = fopen(srcfile, "r")) == NULL)
    {
        my_cannotopen(srcfile);
        errorsfound = TRUE;
        return(FALSE);
    }
    filename = srcfile; /* save the file name for warning messages */

    if ( file_is_ascii_text(yyin) )
    {
        putfilename(srcfile);   /* output the file name */
        dbputc('\n');
        dbputc('\n');

        /* read the source file */
        initscanner(srcfile);
        //fcnoffset = 0;
        //macrooffset = 0;
        symbols = 0;
        if (symbol == (struct symbol *) NULL)
        {
            symbol = (struct symbol *) g_malloc(msymbols * sizeof(struct symbol));
        }
        for (;;)
        {

            /* get the next token */
            switch (token = yylex())
            {
                default:
                    /* if requested, truncate C symbols */
                    length = last - first;

                    /* see if the token has a symbol */
                    if (length == 0)
                    {
                        savesymbol(token, entry_no);
                        break;
                    }
                    /* update entry_no if see function entry */
                    if (token == FCNDEF)
                    {
                        entry_no++;
                    }
                    /* see if the symbol is already in the list */
                    for (i = 0; i < symbols; ++i)
                    {
                        if (length == symbol[i].length
                            && strncmp(my_yytext + first,
                                       my_yytext + symbol[i].first,
                                       length) == 0 
                            && entry_no == symbol[i].fcn_level
                            && token == symbol[i].type
                           )
                        { /* could be a::a() */
                            break;
                        }
                    }
                    if (i == symbols)
                    { /* if not already in list */
                        savesymbol(token, entry_no);
                    }
                    break;

                case NEWLINE:   /* end of line containing symbols */
                    entry_no = 0;   /* reset entry_no for each line */
    #ifdef USING_LEX
                    --yyleng;   /* remove the newline */
    #endif
                    putcrossref();  /* output the symbols and source line */
                    lineno = myylineno; /* save the symbol line number */
    #ifndef USING_LEX
                    /* HBB 20010425: replaced yyleng-- by this chunk: */
                    if (my_yytext)
                        *my_yytext = '\0';
                    my_yyleng = 0;
    #endif
                    break;

                case LEXEOF:    /* end of file; last line may not have \n */

                    /* if there were symbols, output them and the source line */
                    if (symbols > 0)
                    {
                        putcrossref();
                    }
                    (void) fclose(yyin);    /* close the source file */

                    /* output the leading tab expected by the next call */
                    dbputc('\t');
                    return(TRUE);
            }
        }
    }
    else
    {
        fprintf(stderr,"WARNING: Skipping Source File: %s\n    It is not an ASCII text file.\n", srcfile);
        fclose(yyin);    /* close the source file */
    }
    return(FALSE);
}



/* Perform a quick check to verify this is only a text file .              */
/* Check is limited to 16 bytes [as opposed to the entire file contents]   */
/* for performance reasons.  Most non-text files can be detected this way .*/

#define TEXT_CHECK_SIZE     16      /* The number of bytes to check */

static gboolean file_is_ascii_text(FILE *filename)
{
    char check_buf[TEXT_CHECK_SIZE];
    size_t bytes_read;
    int i;
    char *check_ptr = check_buf;

    bytes_read = fread(check_buf, 1, TEXT_CHECK_SIZE, filename);
    rewind(filename);
    
    for (i = 0; i < bytes_read; i++)
    {
        if (*check_ptr < 9)
        {
            // Check for UTF-8 encoded Unicode BOM (special "text file" exception)
            if (i == 0              &&  // This is the first byte in the file
                bytes_read > 2      &&  // The file has at least 3 bytes and they are 0xef, 0xbb and 0xbf
                check_buf[0] == -17 &&  // 0xef
                check_buf[1] == -69 &&  // 0xbb
                check_buf[2] == -65 )   // 0xbf
            {
                // Warning:  Ugly, INTENTIONAL, function side effect ahead!
                //=========================================================
                // Advance the FILE position indicator past the first three bytes
                // Besides being ugly, this behavior is a tad dangerous.  If we encounter a real UTF8 file that contains
                // a variety of 8-bit extended ASCII characters [instead of just one or two copyright symbols] Gscope will probably
                // behave badly and maybe even crash.  If this happens, the BOM detect-and-skip logic might need to
                // be normally-off [Default = Treat any file with BOM as binary] and assign a command line argurment to enable
                // BOM detect-and-skip on a per session basis.
                if (fread(check_buf, 1, 3, filename) < 3)
                    return (FALSE);          // Something is seriously wrong if we cannot advance the pointer
                else
                    return(TRUE);           // _Assume_ the remaining (TEXT_CHECK_SIZE - 3) bytes are ASCII
            }
            else
                return(FALSE);
        }
        if (*check_ptr > 126)
            return(FALSE);
        if ( (*check_ptr > 13) && (*check_ptr < 32) )
            return(FALSE);

        check_ptr++;
    }
    return(TRUE);
}


/* save the symbol in the list */
static void savesymbol(int token, int num)
{
    /* make sure there is room for the symbol */
    if (symbols == msymbols)
    {
        msymbols += SYMBOLINC;
        symbol = (struct symbol *) g_realloc( (char *) symbol,
                                             msymbols * sizeof(struct symbol));
    }
    /* save the symbol */
    symbol[symbols].type = token;
    symbol[symbols].first = first;
    symbol[symbols].last = last;
    symbol[symbols].length = last - first;
    symbol[symbols].fcn_level = num;
    ++symbols;
}

/* output the file name */

static void putfilename(char *srcfile)
{
    #if 0
    /* check for file system out of space */
    /* note: dbputc is not used to avoid lint complaint */
    if (putc(NEWFILE, newrefs) == EOF)
    {
       char new_ref[MAX_STRING_ARG_SIZE + 4];   // add room for the ".new"

       sprintf(new_ref, "%s%s", settings.refFile, ".new");
       cannotwrite(new_ref);
       /* NOTREACHED */
    }
    ++dboffset;
    #endif

    dbputc(NEWFILE);
    dbfputs(srcfile);

    #if 0
    fcnoffset = 0;
    macrooffset = 0;
    #endif
}

/* output the symbols and source line */

static void putcrossref(void)
{
    int i, j;
    unsigned char c;
    gboolean    blank;      /* blank indicator */
    int symput = 0; /* symbols output */
    int type;

    /* output the source line */
    lineoffset = dboffset;
    dboffset += fprintf(newrefs, "%d ", lineno);
#ifdef PRINTF_RETVAL_BROKEN
    dboffset = ftell(newrefs); /* fprintf doesn't return chars written */
#endif

    /* HBB 20010425: added this line: */
    my_yytext[my_yyleng] = '\0';

    blank = FALSE;
    for (i = 0; i < my_yyleng; ++i)
    {

        /* change a tab to a blank and compress blanks */
        if ((c = my_yytext[i]) == ' ' || c == '\t')
        {
            blank = TRUE;
        }
        /* look for the start of a symbol */
        else if (symput < symbols && i == symbol[symput].first)
        {

            /* check for compressed blanks */
            if (blank == TRUE)
            {
                blank = FALSE;
                dbputc(' ');
            }
            dbputc('\n');   /* symbols start on a new line */

            /* output any symbol type */
            if ((type = symbol[symput].type) != IDENT)
            {
                dbputc('\t');
                dbputc(type);
            }
            else
            {
                type = ' ';
            }
            /* output the symbol */
            j = symbol[symput].last;
            c = my_yytext[j];
            my_yytext[j] = '\0';
            writestring(my_yytext + i);
            dbputc('\n');
            my_yytext[j] = c;
            i = j - 1;
            ++symput;
        }
        else
        {
            /* Try to save some time by early-out handling of non-compressed mode */
            if (settings.compressDisable == TRUE)
            {
                if (blank == TRUE)
                {
                    dbputc(' ');
                    blank = FALSE;
                }
                j = i + strcspn(my_yytext+i, "\t ");
                if (symput < symbols
                    && j >= symbol[symput].first)
                    j = symbol[symput].first;
                c = my_yytext[j];
                my_yytext[j] = '\0';
                writestring(my_yytext + i);
                my_yytext[j] = c;
                i = j - 1;
                /* finished this 'i', continue with the blank */
                continue;
            }

            /* check for compressed blanks */
            if (blank == TRUE)
            {
                if (dicode2[c])
                {
                    c = DICODE_COMPRESS(' ', c);
                }
                else
                {
                    dbputc(' ');
                }
            }
            /* compress digraphs */
            else if (IS_A_DICODE(c, my_yytext[i + 1])
                     && symput < symbols
                     && i + 1 != symbol[symput].first
                    )
            {
                c = DICODE_COMPRESS(c, my_yytext[i + 1]);
                ++i;
            }
            dbputc((int) c);
            blank = FALSE;

            /* skip compressed characters */
            if (c < ' ')
            {
                ++i;

                /* skip blanks before a preprocesor keyword */
                /* note: don't use isspace() because \f and \v
                   are used for keywords */
                while ((j = my_yytext[i]) == ' ' || j == '\t')
                {
                    ++i;
                }
                /* skip the rest of the keyword */
                while (isalpha((unsigned char)my_yytext[i]))
                {
                    ++i;
                }
                /* skip space after certain keywords */
                if (keyword[c].delim != '\0')
                {
                    while ((j = my_yytext[i]) == ' ' || j == '\t')
                    {
                        ++i;
                    }
                }
                /* skip a '(' after certain keywords */
                if (keyword[c].delim == '('
                    && my_yytext[i] == '(')
                {
                    ++i;
                }
                --i;    /* compensate for ++i in for() */
            } /* if compressed char */
        } /* else: not a symbol */
    } /* for(i) */

    /* ignore trailing blanks */
    dbputc('\n');
    dbputc('\n');

    /* output any #define end marker */
    /* note: must not be part of #define so putsource() doesn't discard it
       so findcalledbysub() can find it and return */
    if (symput < symbols && symbol[symput].type == DEFINEEND)
    {
        dbputc('\t');
        dbputc(DEFINEEND);
        dbputc('\n');
        dbputc('\n');   /* mark beginning of next source line */
        //macrooffset = 0;
    }
    symbols = 0;
}



/* put the string into the new database */

static void writestring(char *s)
{
    unsigned char c;
    int i;

    if (settings.compressDisable == TRUE)
    {
        /* Save some I/O overhead by using puts() instead of putc(): */
        dbfputs(s);
        return;
    }
    /* compress digraphs */
    for (i = 0; (c = s[i]) != '\0'; ++i)
    {
        if (/* dicode1[c] && dicode2[(unsigned char) s[i + 1]] */
            IS_A_DICODE(c, s[i + 1]))
        {
            /* c = (0200 - 2) + dicode1[c] + dicode2[(unsigned char) s[i + 1]]; */
            c = DICODE_COMPRESS(c, s[i + 1]);
            ++i;
        }
        dbputc(c);  
    }
}

/* print a warning message with the file name and line number */

void warning(char *text)
{

    (void) fprintf(stderr, "cscope: \"%s\", line %d: warning: %s\n", filename, 
                   myylineno, text);
    errorsfound = TRUE;
}
