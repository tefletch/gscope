/*
 *  gscope searching functions
 *
 */

#include <gtk/gtk.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <regex.h>
#include <string.h>

#include "build.h"
#include "scanner.h"        /* for token definitions */
#include "dir.h"
#include "search.h"
#include "lookup.h"
#include "crossref.h"
#include "utils.h"
#include "display.h"
#include "app_config.h"


//===============================================================
//       Defines
//===============================================================
#define     MSGLEN                  PATHLEN + 80    /* displayed message length */
#define     TMPDIR                  "/tmp"
#define     MAX_SYMBOL_SIZE         PATHLEN         /* a more readable name for PATLEN & PATHLEN */


//===============================================================
//       Local Type Definitions
//===============================================================

typedef enum    {       /* Search result codes */
    NOERROR,
    NOTSYMBOL,
    REGCMPERROR
} search_result_t;


//===============================================================
//       Public Global Variables
//===============================================================

FILE     *refsfound;


//===============================================================
//       Private Global Variables
//===============================================================

static char         *cref_file_buf = NULL;  /* Buffer the holds the entire cross reference database */
static char         global[] = "<global>";  /* dummy global function name */
static uint32_t     starttime;              /* start time for progress messages */
static char         temp1[PATHLEN + 1];     /* temporary file name */
static char         temp2[PATHLEN + 1];     /* temporary file name */
static FILE         *nonglobalrefs;
static gboolean     cancel_search = FALSE;  /* UI hook to abort a lengthy search */
static gboolean     cref_status   = TRUE;   /* Cross reference up-to-date status */

//===============================================================
//      Local Functions
//===============================================================
static void             putline  (FILE *output, char **src_ptr);
static void             putref   (char *file, char *func, char **src_ptr);
static gboolean         putsource(FILE *output, char **src_ptr);

static void             progress      (char *format, uint32_t n1, uint32_t n2);
static void             initprogress  (void);
static search_result_t  find_regexp   (char *pattern);
static search_result_t  find_string   (char *pattern);
static search_result_t  find_symbol   (char *pattern);
static search_result_t  find_def      (char *pattern);
static search_result_t  find_called_by(char *pattern);
static search_result_t  find_calling  (char *pattern);
static search_result_t  find_file     (char *pattern);
static search_result_t  find_include  (char *pattern);
static search_result_t  find_all_functions(void);
static void             find_called_by_sub(char *file, char **src);

static gboolean         writerefsfound(void);
static void             get_string(char *dest, char **src);
static char             *html_copy(FILE *output_file, char *read_ptr, char match_char);
static char             *open_results_file(char *results_file, off_t *size);
static FILE             *open_out_file(gchar *full_filename);
static gboolean         is_regexp(char *pattern);
static void             match_file(char *infile_name, regex_t regex_ptr, char *format);
static gboolean         match_regex(char **src, regex_t regex_ptr);
static gboolean         match_bytes(char **src_ptr, char *cpattern);
static void             strip_anchors(char *pattern);
static gboolean         compress_search_pattern(char *cpattern, char *pattern);

static search_result_t  configure_search(char *pattern,   gboolean *use_regexp, regex_t *regex_ptr,       char *cpattern);
static gboolean         mega_match(      char **read_ptr, gboolean use_regexp,  const regex_t *regex_ptr, char *cpattern);



/* find the symbol in the cross-reference */
static search_result_t find_symbol(char *pattern)
{
    char        file[MAX_SYMBOL_SIZE + 1];     /* source file name */
    char        function[MAX_SYMBOL_SIZE + 1];  /* function name */
    char        macro[MAX_SYMBOL_SIZE + 1];     /* macro name */
    char        match_string[MAX_SYMBOL_SIZE + 1];
    char        *read_ptr;
    char        *tmp_ptr;

    gboolean    in_macro    = FALSE;
    gboolean    in_function = FALSE;
    uint32_t    fcount      = 0;
    gboolean    done        = FALSE;

    regex_t     regex_ptr;
    char        cpattern[MAX_SYMBOL_SIZE + 1];   /* compressed version of symbol pattern */
    gboolean    use_regexp;
    search_result_t error;


    /*** Perform search initialization ***/
    error = configure_search(pattern, &use_regexp, &regex_ptr, cpattern);

    if (error != NOERROR) return(error);


    /*** Start the searching the cross-reference data ***/

    read_ptr = cref_file_buf;

    while (*read_ptr++ != '\t');            /* Skip the header, scan past the next tab char */
    read_ptr++;                             /* Skip the file marker */
    get_string(file, &read_ptr);            /* Get the file name */

    *macro    = '\0';
    *function = '\0';

    while (!done)
    {
        /* find the next symbol */
        while (*read_ptr != '\n')
        {
            ++read_ptr;
        }

        ++read_ptr;

        /* look for a source file, function, or macro name */
        if (*read_ptr == '\t')
        {
            switch ( *(++read_ptr) )
            {

                case NEWFILE:       /* file name */

                    /* save the name */
                    read_ptr++;
                    get_string(file, &read_ptr);

                    /* check for the end of the symbols */
                    if (*file == '\0')
                    {
                        done = TRUE;
                        continue;
                    }
                    fcount++;
                    progress("Searched %d of %d files", fcount, nsrcfiles);
                    /* FALLTHROUGH */

                case FCNEND:        /* function end */
                    *function = '\0';
                    in_function = FALSE;
                    continue;    /* don't match name */

                case FCNDEF:        /* function name */
                    tmp_ptr = read_ptr;
                    tmp_ptr++;
                    get_string(function, &tmp_ptr);  // get the string without advancing read_ptr
                    in_function = TRUE;
                break;

            case DEFINE:        /* macro name */
                    tmp_ptr = read_ptr;
                    tmp_ptr++;
                    get_string(macro, &tmp_ptr);    // get the string without advancing read_ptr
                    in_macro = TRUE;
                break;

                case DEFINEEND:     /* macro end */
                    *macro = '\0';
                    in_macro = FALSE;
                    continue;

                case INCLUDE:           /* #include file */
                    continue;    /* don't match name */

                default:        /* other symbol */
                    /* do nothing */
                break;
            }
            /* save the name */
            read_ptr++;
        }

        tmp_ptr = read_ptr;  // Keep a pointer the the current symbol.  We'll need it if there is a match

        /*** Compare the search pattern to the selected symbol in the cross-reference.
             If a match is found, output the matching symbol info to the results file. ***/

        if ( mega_match(&read_ptr, use_regexp, &regex_ptr, cpattern) )
        {
            get_string(match_string, &tmp_ptr);

            /* output the file, function or macro, and source line */
            if ( in_function && strcmp(function, match_string) )
            {
                putref(file, function, &read_ptr);
            }
            else
            {
                if ( in_macro && strcmp(macro, match_string) )
                {
                    putref(file, macro, &read_ptr);     /* everthing else within the macro def */
                }
                else
                {
                    putref(file, global, &read_ptr);
                }
            }
        }

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }

    /* Free memory allocated to the pattern buffer by the regcom() compiling process (performed in configure_search() */
    if (use_regexp) regfree(&regex_ptr);

    return(NOERROR);
}



/* find the function definition or #define */
static search_result_t find_def(char *pattern)
{
    char        file[MAX_SYMBOL_SIZE + 1];  /* source file name */

    char        *read_ptr;
    uint32_t    fcount = 0;
    gboolean    done = FALSE;
    regex_t     regex_ptr;
    char        cpattern[MAX_SYMBOL_SIZE + 1];   /* compressed version of symbol pattern */
    gboolean    use_regexp;
    search_result_t error;


    /*** Perform search initialization ***/
    error = configure_search(pattern, &use_regexp, &regex_ptr, cpattern);

    if (error != NOERROR) return(error);


    /*** Start the searching the cross-reference data ***/

    read_ptr = cref_file_buf;

    /* find the next file name */
    while (*read_ptr++ != '\t');        /* Skip the header.  Scan past the next tab char */
    read_ptr++;                         /* Skip the file marker */
    get_string(file, &read_ptr);        /* Get the first file name */

    while (!done)
    {
        /* find the next scan token */
        while (*read_ptr++ != '\t');

        switch (*read_ptr)
        {

            case NEWFILE:
                /* save the file name */
                read_ptr++;
                get_string(file, &read_ptr);

                /* check for the end of the symbols */
                if (*file == '\0')
                {
                    done = TRUE;
                    continue;
                }
                fcount++;
                progress("Searched %ld of %ld files", fcount, nsrcfiles);
            break;

            case DEFINE:        /* could be a macro */
            case FCNDEF:
            case CLASSDEF:
            case ENUMDEF:
            case MEMBERDEF:
            case STRUCTDEF:
            case TYPEDEF:
            case UNIONDEF:
            case GLOBALDEF:     /* other global definition */
                read_ptr++;     /* match name to pattern */
                if ( mega_match(&read_ptr, use_regexp, &regex_ptr, cpattern) )
                {
                    /* output the file, function and source line */
                    putref(file, pattern, &read_ptr);
                }
            break;

            default:
                /* do nothing */
            break;
        }

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }

    /* Free memory allocated to the pattern buffer by the regcom() compiling process (performed in configure_search() */
    if (use_regexp) regfree(&regex_ptr);

    return(NOERROR);
}



/* find all function definitions */
static search_result_t find_all_functions()
{
    char        file[MAX_SYMBOL_SIZE + 1];  /* source file name */
    char        function[MAX_SYMBOL_SIZE + 1];   /* function name */

    char        *read_ptr;
    uint32_t    fcount = 0;
    gboolean    done = FALSE;

    read_ptr = cref_file_buf;

    /* find the next file name */
    while (*read_ptr++ != '\t');        /* Skip the header.  Scan past the next tab char */
    read_ptr++;                         /* Skip the file marker */
    get_string(file, &read_ptr);        /* Get the first file name */

    while (!done)
    {
        /* find the next scan token */
        while (*read_ptr++ != '\t');

        switch (*read_ptr)
        {

            case NEWFILE:
                read_ptr++;  /* save file name */
                get_string(file, &read_ptr);

                /* Check for end-of-symbols */
                if (*file == '\0')
                {
                    done = TRUE;
                    continue;
                }
                fcount++;
                progress("Searched %ld of %ld files", fcount, nsrcfiles);
                /* FALLTHROUGH */

            case FCNEND:        /* function end */
                (void) strcpy(function, global);
                break;

            case FCNDEF:
            case CLASSDEF:
                get_string(function, &read_ptr);

                /* output the file, function and source line */
                putref(file, function, &read_ptr);
                break;
        }

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }
    return(NOERROR);
}



/* find the functions called by this function */
static search_result_t find_called_by(char *pattern)
{
    char        file[MAX_SYMBOL_SIZE + 1];  /* source file name */

    char        *read_ptr;
    uint32_t    fcount = 0;
    gboolean    done = FALSE;
    regex_t     regex_ptr;
    char        cpattern[MAX_SYMBOL_SIZE + 1];   /* compressed version of symbol pattern */
    gboolean    use_regexp;
    search_result_t error;


    /*** Perform search initialization ***/
    error = configure_search(pattern, &use_regexp, &regex_ptr, cpattern);

    if (error != NOERROR) return(error);

    /* Note: User provided regular expression and/or ignoreCase (use_regexp == TRUE) might match more than a */
    /*       single calling function. TF - 8/5/13 */

    /*** Start the searching the cross-reference data ***/

    read_ptr = cref_file_buf;

    /* find the next file name */
    while (*read_ptr++ != '\t');    /* Skip the header */
    read_ptr++;                     /* Skip the file marker */
    get_string(file, &read_ptr);    /* Get the first file name */

    while (!done)
    {
        /* find the next symbol */
        while (*read_ptr != '\n')
        {
            ++read_ptr;
        }

        ++read_ptr;

        if (*read_ptr == '\t')
        {
            switch ( *(++read_ptr) )
            {
                case NEWFILE:
                    read_ptr++;  /* save file name */
                    get_string(file, &read_ptr);

                    /* Check for the end of the symbols */
                    if (*file == '\0')
                    {
                        done = TRUE;
                        continue;
                    }
                    fcount++;
                    progress("Searched %ld of %ld files", fcount, nsrcfiles);
                break;

                case FCNDEF:
                    read_ptr++;  /* match name to pattern */
                    if ( mega_match(&read_ptr, use_regexp, &regex_ptr, cpattern) )
                    {
                        find_called_by_sub(file, &read_ptr);
                    }
                break;

                default:
                    /* do nothing */
                break;
            }
        }

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }

    /* Free memory allocated to the pattern buffer by the regcom() compiling process (performed in configure_search() */
    if (use_regexp) regfree(&regex_ptr);

    return(NOERROR);
}



static void find_called_by_sub(char *file, char **src)
{
    gboolean done = FALSE;
    char     function[MAX_SYMBOL_SIZE + 1];

    while (!done)
    {
        /* find the next function call or the end of this function */
        while ( *(*src)++ != '\t');

        switch ( *(*src) )
        {

            case FCNCALL:       /* function call */
                (*src)++;
                get_string(function, src);
                putref(file, function, src);
            break;

            case FCNEND:        /* function end */
            case NEWFILE:       /* file end */
                done = TRUE;
            break;

            default:
                /* do nothing */
            break;
        }
    }
}



/* find the functions calling this function */
static search_result_t find_calling(char *pattern)
{
    char        file[MAX_SYMBOL_SIZE + 1];     /* source file name */
    char        function[MAX_SYMBOL_SIZE + 1];  /* function name */
    char        macro[MAX_SYMBOL_SIZE + 1];     /* macro name */

    char        *read_ptr;
    uint32_t    fcount = 0;
    gboolean    done = FALSE;
    regex_t     regex_ptr;
    char        cpattern[MAX_SYMBOL_SIZE + 1];   /* compressed version of symbol pattern */
    gboolean    use_regexp;
    search_result_t error;


    /*** Perform search initialization ***/
    error = configure_search(pattern, &use_regexp, &regex_ptr, cpattern);

    if (error != NOERROR) return(error);


    /*** Start the searching the cross-reference data ***/

    read_ptr = cref_file_buf;

    /* If the function call is from a macro, report the host 'macro' as the calling function */
    *macro = '\0';

    /* find the next file name */
    while (*read_ptr++ != '\t');    /* Skip the header */
    read_ptr++;                     /* Skip the file marker */
    get_string(file, &read_ptr);    /* Get the first file name */


    while (!done)
    {
        /* Find the next scan token */
        while (*read_ptr++ != '\t');

        switch (*read_ptr)
        {
            case NEWFILE:       /* save file name */
                read_ptr++;
                get_string(file, &read_ptr);

                /* Check for the end of the symbols */
                if (*file == '\0')
                {
                    done = TRUE;
                    continue;
                }
                fcount++;
                progress("Searched %ld of %ld files", fcount, nsrcfiles);
                (void) strcpy(function, global);
            break;

            case DEFINE:        /* could be a macro */
                read_ptr++;
                get_string(macro, &read_ptr);
            break;

            case DEFINEEND:
                *macro = '\0';
            break;

            case FCNDEF:        /* save calling function name */
                read_ptr++;
                get_string(function, &read_ptr);
            break;

            case FCNEND:
                function[0] = '\0';   /* Clear the function name - Will not always be cleared when parsing
                                         (non-C code can trigger FCNDEF without matching FCNEND) */
            break;

            case FCNCALL:       /* match function called to pattern */
                read_ptr++;
                if ( mega_match(&read_ptr, use_regexp, &regex_ptr, cpattern) )
                {
                    /* output the file, calling function or macro, and source */
                    if (*macro != '\0')
                    {
                        putref(file, macro, &read_ptr);
                    }
                    else
                    {
                        putref(file, function, &read_ptr);
                    }
                }
            break;

            default:
                /* do nothing */
            break;
        }

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }

    /* Free memory allocated to the pattern buffer by the regcom() compiling process (performed in configure_search() */
    if (use_regexp) regfree(&regex_ptr);

    return(NOERROR);
}



/* find the text in the source files */

static search_result_t find_string(char *pattern)
{
    uint32_t    i;
    regex_t     regex_ptr;
    char        new_pattern[MAX_SYMBOL_SIZE * 2];

    char        *file;
    char        *write_ptr;
    char        *read_ptr;


    /*** Set up the search ***/

    if ( is_regexp(pattern) )
    {
        /* Escape all regular expression special charactes */
        write_ptr = new_pattern;
        for (read_ptr = pattern; *read_ptr != '\0'; ++read_ptr)
        {
            if (strchr("^.*[]{}()\\$?+|", *read_ptr) != NULL)
            {
                *write_ptr++ = '\\';
            }
            *write_ptr++ = *read_ptr;
        }
        *write_ptr = '\0';
    }
    else
    {
        strcpy(new_pattern, pattern);
    }

    /* This search utilizes regexec() even if there are no metacharacters in the user-provided search pattern. */
    /* allow a match anywhere inside the string */
    if (regcomp (&regex_ptr, new_pattern, REG_EXTENDED | REG_NOSUB | (settings.ignoreCase ? REG_ICASE : 0) ) != 0)
        return(REGCMPERROR);

    /*** Perform the search ***/

    for (i = 0; i < nsrcfiles; ++i)
    {
        file = DIR_src_files[i];
        progress("%ld of %ld files searched", i, nsrcfiles);

        match_file(file, regex_ptr, "%s|<unknown> %ld %s\n");

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }

    regfree(&regex_ptr);    /* Avoid memory leak, free memory allocated to the pattern buffer by regcomp() compiling process */
    return(NOERROR);
}





/* find this regular expression in the source files */

static search_result_t find_regexp(char *pattern)
{
    uint32_t    i;
    regex_t     regex_ptr;
    char        *file;

    /* This search utilizes regexec() even if there are no metacharacters in the user-provided search pattern. */
    /* allow a match anywhere inside the string */
    if (regcomp (&regex_ptr, pattern, REG_EXTENDED | REG_NOSUB | (settings.ignoreCase ? REG_ICASE : 0) ) != 0)
        return(REGCMPERROR);

    /*** Perform the search ***/

    for (i = 0; i < nsrcfiles; ++i)
    {
        file = DIR_src_files[i];
        progress("%ld of %ld files searched", i, nsrcfiles);

        match_file(file, regex_ptr, "%s|<unknown> %ld %s\n");

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }

    regfree(&regex_ptr);    /* Avoid memory leak, free memory allocated to the pattern buffer by regcomp() compiling process */
    return(NOERROR);
}



/* find matching file names */
static search_result_t find_file(char *pattern)
{
    uint32_t    i;
    regex_t     regex_ptr;
    char        *s;

    /* remove trailing white space */
    for (s = pattern + strlen(pattern) - 1; isspace(*s); --s) *s = '\0';

    /* This searches utilize regexec() even if there are no metacharacters in the search pattern. */
    /* allow a match anywhere inside the string */
    if (regcomp (&regex_ptr, pattern, REG_EXTENDED | REG_NOSUB | (settings.ignoreCase ? REG_ICASE : 0) ) != 0)
        return(REGCMPERROR);


    for (i = 0; i < nsrcfiles; ++i)
    {
        s = DIR_src_files[i];
        if (regexec (&regex_ptr, s, (size_t)0, NULL, 0) == 0)
        {
            fprintf(refsfound, "%s|<unknown> 1 <unknown>\n", DIR_src_files[i]);
        }

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }

    regfree(&regex_ptr);    /* Avoid memory leak, free memory allocated to the pattern buffer by regcomp() compiling process */
    return(NOERROR);
}



/* find files #including this file */
static search_result_t find_include(char *pattern)
{
    char        file[MAX_SYMBOL_SIZE + 1];  /* source file name */
    uint32_t    searchcount = 0;
    char        *read_ptr;
    gboolean    done = FALSE;
    char        *s;
    regex_t     regex_ptr;

    /* remove trailing white space */
    for (s = pattern + strlen(pattern) - 1; isspace(*s); --s) *s = '\0';

    /* This search utilizes regexec() for all search patterns */
    /* allow a match anywhere inside the string */
    if (regcomp (&regex_ptr, pattern, REG_EXTENDED | REG_NOSUB | (settings.ignoreCase ? REG_ICASE : 0) ) != 0)
        return(REGCMPERROR);

    read_ptr = cref_file_buf;

    /* find the next source file name or #include */
    while (*read_ptr++ != '\t');    /* Skip the header, Scan past the next tab char */
    read_ptr++;                     /* Skip the file marker */
    get_string(file, &read_ptr);    /* Get the first file name */

    while (!done)
    {
        /* Find the next scan token */
        while (*read_ptr++ != '\t');

        switch (*read_ptr)
        {

            case NEWFILE:       /* save file name */
                read_ptr++;
                get_string(file, &read_ptr);

                /* Check for end of symbols */
                if (*file == '\0')
                {
                    done = TRUE;
                    continue;
                }
                searchcount++;
                progress("Searched %ld of %ld files", searchcount, nsrcfiles);
            break;

            case INCLUDE:
                read_ptr++;
                read_ptr++;  /* skip global or local #include marker '<' or '"' */
                if (match_regex(&read_ptr, regex_ptr))
                {
                    /* output the file and source line */
                    putref(file, global, &read_ptr);
                }
            break;

            default:
                /* do nothing */
            break;
        }

        if (cancel_search)
        {
            cancel_search = FALSE;
            break;
        }
    }

    regfree(&regex_ptr);    /* Avoid memory leak, free memory allocated to the pattern buffer by regcomp() compiling process */
    return(NOERROR);
}



static gboolean is_regexp(char *pattern)
{
    char *work_ptr;
    gboolean isregexp = FALSE;

    work_ptr = pattern;

    while ( (!isregexp) && (*work_ptr != '\0') )
    {
        isregexp = (strchr("^.*[{(\\$?+|", *work_ptr) != NULL );
        work_ptr++;
    }

    return(isregexp);
}



/* match the pattern to the string */
static gboolean match_regex(char **src, regex_t regex_ptr)
{
    char    string[MAX_SYMBOL_SIZE + 1];

    get_string(string, src);
    if (*string == '\0')
    {
        return(FALSE);
    }

    return(regexec (&regex_ptr, string, (size_t)0, NULL, 0) ? FALSE : TRUE);
}



static gboolean match_bytes(char **src_ptr, char *cpattern)
{
    int i = 1;
    char *match_ptr = *src_ptr;

    while (*match_ptr == cpattern[i])
    {
        ++match_ptr;
        ++i;
    }

    *src_ptr = match_ptr;
    if (*match_ptr == '\n' && cpattern[i] == '\0')
    {
        return(TRUE);
    }
    return(FALSE);
}



/* put the reference into the file */
static void putref(char *file, char *func, char **src)
{
    FILE    *output;

    if (strcmp(func, global) == 0)
    {
        output = refsfound;
    }
    else
    {
        output = nonglobalrefs;
    }
    (void) fprintf(output, "%s|%s ", file, func);

    if ( !putsource(output, src) )
    {
        fprintf(stderr,"\nGscope Internal error: cannot get source line from database\n\n");
        fprintf(stderr,"This failure is typically caused by a file using non-UNIX newline format.\n");
        fprintf(stderr,"Problem file: %s\n", file);
        fprintf(stderr,"Fix the newline format of this file to correct this failure.\n");
        SEARCH_cleanup();
        exit(EXIT_FAILURE);
    }
}



/* put the source line into the file */
static gboolean putsource(FILE *output, char **src_ptr)
{
    char     *cp;
    char     nextc = '\0';

    cp = *src_ptr;

    /* scan back to the beginning of the source line (double-newline chars) */
    while (*cp != '\n' || nextc != '\n')
    {
        nextc = *cp;
        --cp;
    }
    if (*cp != '\n' || *(++cp) != '\n' || !isdigit(*(++cp)) )
    {
        return(FALSE);
    }

    /* A double newline followed by a single digit [0..9] is found */
    do
    {
        /* skip a symbol type */
        if (*cp == '\t')
        {
            cp += 2;;
        }
        *src_ptr = cp;

        /* output a piece of the source line */
        putline(output, &cp);
    } while (*(++cp) != '\n');  /* until a double newline is found */

    (void) putc('\n', output);
    *src_ptr = cp;

    return(TRUE);
}



/* put the rest of the cross-reference line into the file */
static void putline(FILE *output, char **src_ptr)
{
    char    *line_ptr = *src_ptr;
    unsigned c;

    while ((c = (unsigned)(*line_ptr)) != '\n')
    {

        /* check for a compressed digraph */
        if (c > 0x7f)
        {
            c &= 0x7f;
            (void) putc(dichar1[c / 8], output);
            (void) putc(dichar2[c & 7], output);
        }
        /* check for a compressed keyword */
        else if (c < ' ')
        {
            (void) fputs(keyword[c].text, output);
            if (keyword[c].delim != '\0')
            {
                (void) putc(' ', output);
            }
            if (keyword[c].delim == '(')
            {
                (void) putc('(', output);
            }
        }
        else
        {
            (void) putc((int) c, output);
        }
        ++line_ptr;
    }
    *src_ptr = line_ptr;    /* Return the new read-pointer value */
}



/* put the rest of the cross-reference line into the string */
static void get_string(char *dest, char **src)
{
    uint8_t byte;
    char *src_ptr = *src;
    unsigned int byte_count = 0;
    char *start = dest;

    while ( (byte = (unsigned) (*src_ptr)) != '\n' )
    {
        if (byte > 0x7f)
        {
            byte   &= 0x7f;
            *dest++ = dichar1[byte / 8];
            *dest++ = dichar2[byte & 7];
        }
        else
        {
            *dest++ = byte;
        }
        src_ptr++;
        byte_count++;
        if (byte_count == MAX_SYMBOL_SIZE) break;
    }
    *dest++ = '\0';    // Null-terminate the extracted string

    if (byte_count == MAX_SYMBOL_SIZE)
    {
        fprintf(stderr, "Warning: Unusually large (> %d bytes) symbol encountered.  Symbol Truncated to:\n    %s\n"
                       "Pattern Matches for the non-truncated version of this symbol will fail.\n"
                       "If this is a problem, request a new Gscope build with a larger MAX_SYMBOL_SIZE limit.\n",
               MAX_SYMBOL_SIZE,
               start);
    }
    *src = src_ptr;    // Update the caller's copy of the source pointer.
}



/* initialize the progress message */
static void initprogress()
{
    starttime = time((time_t *) NULL);
}



/* display the progress every three seconds */
static void progress(char *format, uint32_t n1, uint32_t n2)
{
    uint32_t    now;
    char    msg[MSGLEN];

    /* Update every 1 second */
    now = time((time_t *) NULL);
    if ( (now - starttime) >= 1 )
    {
        starttime = now;
        (void) sprintf(msg, format, n1, n2);
        DISPLAY_status(msg);
        DISPLAY_update_progress_bar(n1, n2);

        // Process any pending gtk events
        while (gtk_events_pending() )
            gtk_main_iteration();
    }
}



/* Perform a periodic cross-reference update check */
static void periodic_check_cref()
{
    static gboolean     initialized = FALSE;
    static time_t       last_check_time = 0;

    time_t now;

    if ( !initialized )
    {
        last_check_time = time( (time_t *) NULL);
        initialized = TRUE;
    }

    now = time( (time_t *) NULL);

    if (now - last_check_time > (15 * 60) )  // if we haven't checked the cref in the last 15 minutes
    {
        last_check_time = time( (time_t *) NULL);
        SEARCH_check_cref();
    }
}



/* open the references found file for writing */
static gboolean writerefsfound()
{
    if (refsfound == NULL)
    {
        if ((refsfound = fopen(temp1, "w")) == NULL)
        {
            my_cannotopen(temp1);
            return(FALSE);
        }
    }
    else if (freopen(temp1, "w", refsfound) == NULL)
    {
        fprintf(stderr, "Cannot open temporary file\n");
        return(FALSE);
    }
    return(TRUE);
}



/*****************************************************************************/
/*** Open the search results file and read the entire contents into memory ***/
/***                                                                       ***/
/*** This function mallocs memory, caller is responsible for freeing the   ***/
/*** allocated buffer.                                                     ***/
/*****************************************************************************/
static char *open_results_file(char *full_filename, off_t *size)
{
    FILE     *results_file;
    struct   stat statstruct;
    char     *results_buf;

    if ( (stat(full_filename, &statstruct) != 0 ) || ((results_file = fopen(full_filename, "rb")) == NULL) )
    {
        /* There is no search-results file */
        return(NULL);
    }

    results_buf = g_malloc(statstruct.st_size);    /* malloc a buffer to hold the entire old-file */
    if (results_buf == NULL)
    {
        /* Malloc failed */
        fprintf(stderr, "Error: Unable to allocate memory for search results file\n");
        *size = 0;
        fclose(results_file);
        return(NULL);
    }

    if ( fread(results_buf, 1, statstruct.st_size, results_file) != statstruct.st_size )
    {
        /* Read failed */
        fprintf(stderr, "Error: Unable to to read entire search results file\n");
        *size = 0;
        fclose(results_file);
        return(NULL);
    }
    fclose(results_file);

    *size = statstruct.st_size;
    return(results_buf);
}



static FILE *open_out_file(gchar *full_filename)
{
    FILE *output_file;

    output_file = fopen(full_filename, "w");
    if (output_file == NULL)
    {
        my_cannotopen(full_filename);
    }

    return(output_file);
}



/* Prepare to perform a Case Sensitive, non-regexp search */
gboolean symbol_search_init(char *cpattern, char *pattern)
{
    uint32_t i;
    char    *read_ptr;
    char    c;

    if (settings.truncateSymbols) pattern[8] = '\0';    /* if requested, try to truncate a C symbol pattern */

    read_ptr = pattern;

    /* check for a valid C symbol */
    if (!isalpha(*read_ptr) && *read_ptr != '_')
    {
        return(FALSE);
    }
    while (*++read_ptr != '\0')
    {
        if (!isalnum(*read_ptr) && *read_ptr != '_')
        {
            return(FALSE);
        }
    }

    /* compress the string pattern for matching */
    for (i = 0; (c = pattern[i]) != '\0'; ++i)
    {
        if (IS_A_DICODE(c, pattern[i + 1]))
        {
            c = DICODE_COMPRESS(c, pattern[i + 1]);
            ++i;
        }
        /*(unsigned char)*/ *cpattern++ = c;
    }
    *cpattern = '\0';

    return(TRUE);
}



void match_file(char *infile_name, regex_t regex_ptr, char *format)
{
    FILE        *in_file;
    struct      stat statstruct;
    uint32_t    linenum = 0;
    char        *end_ptr;
    char        *string_ptr;
    char        *work_ptr;

    static uint32_t     buf_size = (1024 * 1024);
    static char         *buf_ptr = NULL;

    if (!buf_ptr) buf_ptr = g_malloc(buf_size);   // Perform the initial buffer malloc

    // open the input file

    if ( (stat(infile_name, &statstruct) == 0) && ((in_file = fopen(infile_name, "r")) != NULL) )
    {
        if ( (statstruct.st_size + 1) > buf_size )        // grow the buffer as needed
        {
            g_free(buf_ptr);
            buf_size = statstruct.st_size + 1;    // +1 needed for files that do not end with LF character
            buf_ptr = g_malloc(buf_size);

            if (!buf_ptr)
            {
                fprintf(stderr,"Malloc Error in match_file().\n");
                fclose(in_file);
                return;
            }
        }

        if ( fread(buf_ptr, 1, statstruct.st_size, in_file) != statstruct.st_size )
        {
            fclose(in_file);
            return;
        }
        fclose(in_file);

        end_ptr = buf_ptr + statstruct.st_size;
        string_ptr = buf_ptr;
        work_ptr = buf_ptr;

        //*** search entire file line-by-line ***/

        while (work_ptr < end_ptr)
        {
            while ( (*work_ptr != '\n') && (work_ptr < end_ptr) ) work_ptr++;       // Find then next newline char

            linenum++;
            *work_ptr++ = '\0';

            // if match found fprintf(refsfound, format, line_number)
            if ( regexec (&regex_ptr, string_ptr, (size_t)0, NULL, 0) == 0 )
            {
                fprintf(refsfound, format, infile_name, linenum, string_ptr);
            }

            string_ptr = work_ptr;  // Advance to the next string.
        }
    }
    else
    {
        DISPLAY_set_cref_current(FALSE);    /* Set the out-of-date indicator */
        fprintf(stderr, "File open error: %s\n", infile_name);
    }
}



/* remove leading ^ and trailing $ (if present) */
static void strip_anchors(char *pattern)
{
    int i;
    char *wptr;

    wptr = pattern;

    if (*wptr == '^')
    {
        while ( *wptr != '\0')
        {
            *wptr = *(wptr + 1);
            wptr++;
        }
    }

    i = strlen(pattern) - 1;
    if (pattern[i] == '$')
    {
        pattern[i] = '\0';
    }

    return;
}



static gboolean compress_search_pattern(char *cpattern, char *pattern)
{
    char *s;
    char c;
    int i;

    s = pattern;

    if (settings.truncateSymbols)
        s[8] = '\0';    /* if requested, try to truncate a C symbol pattern */

    /* check for a valid C symbol */
    if (!isalpha(*s) && *s != '_')
    {
        return(FALSE);
    }
    while (*++s != '\0')
    {
        if (!isalnum(*s) && *s != '_')
        {
            return(FALSE);
        }
    }

    /* compress the string pattern for matching */
    s = cpattern;
    for (i = 0; (c = pattern[i]) != '\0'; ++i)
    {
        if (IS_A_DICODE(c, pattern[i + 1]))
        {
            c = DICODE_COMPRESS(c, pattern[i + 1]);
            ++i;
        }
        /*(unsigned char)*/ *s++ = c;
    }
    *s = '\0';

    return(TRUE);
}



static search_result_t configure_search(char *pattern, gboolean *use_regexp, regex_t *regex_ptr, char *cpattern)
{
    char        *s_ptr;
    char        buf[MAX_SYMBOL_SIZE + 3];

    /* remove trailing white space */
    for (s_ptr = pattern + strlen(pattern) - 1; isspace(*s_ptr); --s_ptr) *s_ptr = '\0';

    /* remove leading white space */
    for (s_ptr = pattern; isspace(*s_ptr); ++s_ptr);
    if (s_ptr > pattern)        // leading whitespace present, squash it..
        memmove(pattern, s_ptr, strlen(s_ptr) + 1);


    /* This search utilizes regexec() ONLY if there are metacharacters in the search pattern */
    /* The match must be an exact match */
    if (is_regexp(pattern) || settings.ignoreCase)          // Configure regex search
    {
        /* remove leading ^ and trailing $ (if present) */
        strip_anchors(pattern);

        (void) sprintf(buf, "^%s$", pattern);
        if (regcomp (regex_ptr, buf, REG_EXTENDED | REG_NOSUB | (settings.ignoreCase ? REG_ICASE : 0) ) != 0)
            return(REGCMPERROR);

        *use_regexp = TRUE;
    }
    else                                                    // Configure byte-matching search
    {
        if ( !compress_search_pattern(cpattern, pattern) )
            return(NOTSYMBOL);

        *use_regexp = FALSE;
    }
    return(NOERROR);
}



static gboolean mega_match(char **read_ptr, gboolean use_regexp, const regex_t *regex_ptr, char *cpattern)
{
    char     firstchar;             /* first character of a potential symbol */
    char     symbol[MAX_SYMBOL_SIZE + 1];    /* symbol name */
    gboolean match_found;


    match_found = FALSE;

    if (use_regexp)  /* Perform a regular expression pattern match */
    {

        /* if this is a symbol */

        /**************************************************
         * The first character may be a digraph'ed char, so
         * unpack it into firstchar, and then test that.
         *
         * Assume that all digraphed chars have the 8th bit
         * set (0200).
         **************************************************/
        if (**read_ptr & 0x80)
        {   /* digraph char? */
            firstchar = dichar1[(**read_ptr & 0x7f) / 8];
        }
        else
        {
            firstchar = **read_ptr;
        }

        if (isalpha((unsigned char)firstchar) || firstchar == '_')
        {
            get_string(symbol, read_ptr);

            /* match the symbol to the regular expression */
            if (regexec (regex_ptr, symbol, (size_t)0, NULL, 0) == 0)
            {
                match_found = TRUE;
            }
        }
    }
    else    /* Not a regexp, perform a direct byte-for-byte (compressed text) pattern match */
    {
        if (**read_ptr == cpattern[0])
        {
            (*read_ptr)++;
            /* match the rest of the symbol to the text pattern */
            if (match_bytes(read_ptr, cpattern))
            {
                match_found = TRUE;
            }
        }
    }

    return(match_found);
}





//===================================================================================================
//          Public Functions
//===================================================================================================

void SEARCH_init()
{
    FILE    *cref_file;
    struct  stat statstruct;
    char    *tmpdir;    /* temporary directory */
    pid_t   pid;

    if (cref_file_buf != NULL)
    {
        g_free(cref_file_buf);    /* Avoid a memory leak.  Free any old file buffer first */
        cref_file_buf = NULL;
    }

    /* How big is the file?  And can we acces it? Should always succeed */
    if ( stat(settings.refFile, &statstruct) != 0 )
    {
        fprintf(stderr, "Fatal Error: Unable to stat() cross-reference file.\n");
        exit(EXIT_FAILURE);
    }

    /* Open the file for reading.   Should always succeed */
    if ( (cref_file = fopen(settings.refFile, "rb")) == NULL)
    {
        fprintf(stderr, "Fatal Error: Unable to open() cross-reference file.\n");
        exit(EXIT_FAILURE);
    }

    /* Allocate a buffer to hold the entire file.  A really big cross reference
       (like: ~1 GBbyte) on a memory-constrained host (like 2GB) MIGHT fail. */
    cref_file_buf = g_malloc(statstruct.st_size);  /* malloc a buffer to hold the entire file */
    if ( cref_file_buf == NULL )
    {
        fprintf(stderr, "Fatal Error: Unable to allocate memory to load cross-reference file.\n");
        fprintf(stderr, "             Add more memory to your system.\n");
        exit(EXIT_FAILURE);
    }

    /* Read the entire file into memory.  Sould always succeed */
    if ( fread(cref_file_buf, 1, statstruct.st_size, cref_file) != statstruct.st_size )
    {
        fprintf(stderr, "Fatal Error: Unable to load cross-reference file.\n");
        exit(EXIT_FAILURE);
    }

    /* At this point we have a valid, memory-resident, cross-reference database available
       (cref_file_buf) for use by the various functions of the SEARCH component */

    /*** create the temporary file names ***/
    tmpdir = getenv("TMPDIR");
    if (tmpdir == NULL) tmpdir = TMPDIR;

    pid = getpid();
    sprintf(temp1, "%s/cscope%d.1", tmpdir, pid);
    sprintf(temp2, "%s/cscope%d.2", tmpdir, pid);

    /*** Initialize the Cross-Reference "periodic check" timer ***/
    periodic_check_cref();

    fclose(cref_file);
}



/*
 * search for the symbol or text pattern
 * Note: need to return an ERR status too
 *
 * Returns a pointer to a search_results_t structure that contains:
 *      start_ptr - A pointer to a dynamically allocated buffer containing all results data
 *      end_ptr   - A pointer to the first byte following the last byte of the results data
 *    match_count - The number of matches produced by the lookup operation.
 *
 * Callers are responsible for calling SEARCH_free_results() as soon as they are finished using the results data.
 * As a failsafe, to prevent memory leaks, this function will make the "free results" call if any old results are
 * still present when a new SEARCH_lookup() call is made.
 *
 */
search_results_t *SEARCH_lookup(search_t search_operation, gchar *pattern)
{
    int       c;
    char      msg[MSGLEN];
    struct    stat statstruct;

    search_result_t         result = NOERROR;          /* findinit return code */
    static search_results_t results;

    /* open the references found (search results) file for writing */
    if ( !writerefsfound() ) return(0);

    if ((nonglobalrefs = fopen(temp2, "w")) == NULL)
    {
        my_cannotopen(temp2);
        return(0);
    }

    /* find the pattern */
    initprogress();
    DISPLAY_status("Searching ...");


    switch (search_operation)
    {
        case FIND_STRING:
            result = find_string(pattern);
        break;

        case FIND_REGEXP:
            result = find_regexp(pattern);
        break;

        case FIND_SYMBOL:
            result = find_symbol(pattern);
        break;

        case FIND_DEF:
            result = find_def(pattern);
        break;

        case FIND_CALLEDBY:
            result = find_called_by(pattern);
        break;

        case FIND_CALLING:
            result = find_calling(pattern);
        break;

        case FIND_FILE:
            result = find_file(pattern);
        break;

        case FIND_INCLUDING:
            result = find_include(pattern);
        break;

        case FIND_ALL_FUNCTIONS:
            result = find_all_functions();
        break;

        default:
            result = NOERROR;
        break;
    }

    /* append the non-global references */
    freopen(temp2, "r", nonglobalrefs);
    while ((c = getc(nonglobalrefs)) != EOF)
    {
        putc(c, refsfound);
    }

    fclose(nonglobalrefs);

    periodic_check_cref();

    // Avoid memory leaks - Free any old "results" - This should not be needed.
    if (results.start_ptr != NULL)
    {
        g_free(results.start_ptr);
        results.start_ptr = NULL;
        results.end_ptr = NULL;
        results.match_count = 0;
        fprintf(stderr, "Warning: SEARCH_lookup: Found old lookup data that should have already been freed.\n");
    }


    /* reopen the references found file for reading */
    (void) freopen(temp1, "r", refsfound);

    if ( stat(temp1, &statstruct) != 0 )
    {
        fprintf(stderr, "Error in SEARCH_lookup: Unexpected stat() failure.\n");
        exit(EXIT_FAILURE);
    }

    if ( statstruct.st_size > 0)
    {
        // malloc a buffer to hold the entire "results" file
        results.start_ptr = g_malloc(statstruct.st_size);
        if ( results.start_ptr == NULL )
        {
            fprintf(stderr, "Error in SEARCH_lookup: malloc() failure.\n");
            exit(EXIT_FAILURE);
        }

        // Load the entire results file into a memory buffer
        if ( fread(results.start_ptr, 1, statstruct.st_size, refsfound) != statstruct.st_size)
        {
            fprintf(stderr, "Error in SEARCH_lookup: File loading error.\n");
            exit(EXIT_FAILURE);
        }
        results.end_ptr = results.start_ptr + statstruct.st_size;
    }
    else
    {
        results.start_ptr = NULL;
        results.end_ptr   = NULL;
    }

    results.match_count = 0;

    if (results.end_ptr == results.start_ptr )      // Handle the no-results case
    {
        if (result == NOTSYMBOL)
        {
            (void) sprintf(msg, "<span foreground=\"red\">This is not a C symbol:</span> %s", pattern);
        }
        else if (result == REGCMPERROR)
        {
            (void) sprintf(msg,
                           "<span foreground=\"red\">Error in this regcmp(3X) regular expression:</span> %s",
                           pattern);
        }
        else
        {
            sprintf(msg, "<span foreground=\"red\">Could not find:</span> %s", pattern);
        }
        DISPLAY_status(msg);
    }
    else    // We have some results - count the newlines to determine match_count
    {
        gchar *work_ptr;

        work_ptr = results.start_ptr;

        while (work_ptr != results.end_ptr)
        {
            if (*work_ptr++ == '\n') results.match_count++;
        }
    }

    return( &results );
}



uint32_t get_results_count(count_method_t method)
{
    static uint32_t reference_count = 0;
    int    ch;

    if ( method == COUNT_SET )
    {
        // Just count the newlines in the results file and save that value.
        while ( !feof(refsfound))
        {
            ch = fgetc(refsfound);
            if (ch == '\n')
            {
                reference_count++;
            }
        }
        rewind(refsfound);

        return(reference_count);
    }

    if ( method == COUNT_GET )
    {
        return(reference_count);
    }

    fprintf(stderr, "Error in SEARCH_results_count: Should never reach here: Notify tool maintainer.\n");
    return(0);
}



void SEARCH_cancel()
{
    cancel_search = TRUE;
}



/* Remove all temp files when Gscope exits */
void SEARCH_cleanup()
{
    // Clean up temp files
    if (temp1[0] != '\0')
    {
        unlink(temp1);
        unlink(temp2);
    }
}




/* Collect Search Statistics*/
void SEARCH_stats(stats_struct_t *sptr)
{
    char        file[MAX_SYMBOL_SIZE + 1];  /* source file name */

    char        *read_ptr;
    gboolean    done = FALSE;

    /* Initialize the statistics structure */
    memset(sptr, 0, sizeof(stats_struct_t));

    read_ptr = cref_file_buf;   /* Initialize the read pointer to the beginning of the database */

    /* find the next file name or definition */
    do
    {
        while (*read_ptr++ != '\t');    /* Scan past the next tab */

        switch (*read_ptr)
        {
            case CLASSDEF:          // C++ class definitions
                sptr->define_cnt++;
                sptr->class_cnt++;
            break;

            case DEFINE:
                sptr->define_cnt++;
                sptr->identifier_cnt++;
            break;

            case DEFINEEND:
            case FCNEND:
                // do nothing
            break;

            case NEWFILE:
                read_ptr++;  /* save file name */
                get_string(file, &read_ptr);

                /* Check for end-of-symbols */
                if (*file == '\0')
                {
                    done = TRUE;
                    continue;
                }
            break;

            case FCNCALL:
                sptr->fn_calls_cnt++;
            break;

            case FCNDEF:
                sptr->define_cnt++;
                sptr->identifier_cnt++;
                sptr->fn_cnt++;
            break;

            case INCLUDE:
                sptr->include_cnt++;
            break;

            case ENUMDEF:
            case GLOBALDEF:
            case MEMBERDEF:
            case STRUCTDEF:
            case TYPEDEF:
            case UNIONDEF:
                sptr->define_cnt++;
            break;

            default:
                fprintf(stderr, "Error in statistics collection: should never reach here: char = %c\n", *read_ptr);

         }
    }
    while (!done);
    return;
}




#define BORDER_COLOR_AND_SIZE   "#7BA0CD 1.0pt;\n"
#define ROW_HIGHLIGHT_COLOR     "#D3DFEE"
#define ROW_NORMAL_COLOR        ""
#define HEADER_BACKGROUND_COLOR "#4F81BD"
#define HEADER_TEXT_COLOR       "white"


gboolean SEARCH_save_html(gchar *filename)
{
    typedef struct
    {
        uint8_t   num_columns;
        char      *column_header[4];
    } format_struct_t;

    FILE     *output_file;
    char     *results_buf = NULL;
    char     *results_ptr;
    char     *end_ptr;
    char     *work_ptr;
    char     *full_filename;
    off_t    size;
    uint8_t  format_index;
    uint8_t  i;
    gboolean row = FALSE;
    const    format_struct_t *format;


    static const format_struct_t format_table[3] =
    {
        {4, "File", "Function", "Line",   "Source"},
        {3, "File", "Line",     "Source", ""      },
        {1, "File", "",         "",       ""      },
    };


    full_filename = g_malloc(strlen(filename) + 6);
    if (full_filename == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory for new file name.\n");
        return(FALSE);
    }

    /* Add an appropriate file extension, if neccessary */
    work_ptr = strrchr(filename, '.');

    if (work_ptr == NULL)
    {
        /* Filename has no extension, add ".html" */
        strcpy(full_filename, filename);
        strcat(full_filename, ".html");
    }
    else
    {
        /* Filename has an extension, check for .htm or .html */
        if ( (strcmp(work_ptr, ".htm") == 0) || (strcmp(work_ptr, ".html") == 0) )
        {
            /* We already have a valid html extension, don't add one */
            strcpy(full_filename, filename);
        }
        else
        {
            /* Not a valid html extension, add ".html" */
            strcpy(full_filename, filename);
            strcat(full_filename, ".html");
        }
    }

    /*** See if a non-zero results file exists.  If it does, open the results file and the output file ***/
    /*****************************************************************************************************/

    results_buf = open_results_file(temp1, &size);
    if ( results_buf == NULL )
    {
        return(FALSE);
    }

    output_file = open_out_file(full_filename);
    if ( output_file == NULL )
    {
        my_cannotopen(full_filename);
        return(FALSE);
    }

    /*** Determine the results format ***/
    /************************************/

    // Looking for one of three formats:
    //  1) file, function, linenumber, text
    //  2) file, linenumber, text
    //  3) file
    //
    //  Discrimination algorithm:
    //      Scan past file (always present, scan past '|')
    //      If next entry is <unknown>
    //          format is 2 or 3.
    //      else
    //          format is 1 (done).
    //
    //      if (format 2 or 3) scan past line number (<uknown> # text)
    //         if text = <unknown>
    //              format is 3 (done)
    //          else
    //              format is 2 (done)
    //

    /* Get a pointer to the beginning of the file buffer */
    results_ptr = results_buf;
    end_ptr = results_buf + size;

    /* Scan past the file name */
    while ( *results_ptr++ != '|');

    if ( strncmp(results_ptr, "<unknown> ", 10) == 0)
    {
        results_ptr += 10;  // Scan past the dummy function name;

        while (*results_ptr++ != ' ');  // Scan past the line number

        if ( strncmp(results_ptr, "<unknown>", 9) == 0 )
            format_index = 2;
        else
            format_index = 1;
    }
    else
    {
        format_index = 0;
    }

    /* Reset the read pointer */
    results_ptr = results_buf;


    /*** Emit the HTML table ***/
    /***************************/

    // Emit fixed HTML preamble data
    fprintf(output_file, "\n<html>\n<table class=SrcTable border=1 cellspacing=0 cellpadding=0 style='border-collapse:collapse;border:none'>\n");
    format = &(format_table[format_index]);

    // Emit column number specific header row
    fprintf(output_file, "<tr>\n");
    for (i = 0; i < format->num_columns; i++)
    {
        fprintf(output_file, "<td nowrap=\"nowrap\"\nstyle='background:%s'>\n<b><span style='color:%s'>%s</span></b>\n</td>\n\n",
                    HEADER_BACKGROUND_COLOR,
                    HEADER_TEXT_COLOR,
                    format->column_header[i]
                );
    }
    fprintf(output_file, "</tr>\n");

    while ( results_ptr < end_ptr )
    {
        /* build row data */
        /******************/

        // build row preambles
        fprintf(output_file, "<tr>");

        for (i = 0; i < format->num_columns; i++)
        {
            // build cell preamble
            fprintf(output_file, "<td nowrap=\"nowrap\"\nstyle='border-top:none;\n");

            // Check for left border
            if (i == 0)
            {
                fprintf(output_file, "border-left:solid %s", BORDER_COLOR_AND_SIZE);  // Turn on left border
            }
            else
            {
                fprintf(output_file, "border-left:none;\n");
            }

            fprintf(output_file, "border-bottom:solid %s", BORDER_COLOR_AND_SIZE);

            // Check for right border
            if (i == format->num_columns - 1)
            {
                fprintf(output_file, "border-right:solid %s", BORDER_COLOR_AND_SIZE); // Turn on right border
            }
            else
            {
                fprintf(output_file, "border-right:none;\n");
            }

            fprintf(output_file, "padding:0in 0.2in 0in 0in;\nbackground:");

            if (row)
                fprintf(output_file, "%s", ROW_HIGHLIGHT_COLOR);
            else
                fprintf(output_file, "%s", ROW_NORMAL_COLOR);

            fprintf(output_file, "'>\n");

            // enter cell data

            switch (i)
            {
                case 0:
                    /* The first column is always a 'file' column, and it is uniquely delimited */
                    fprintf(output_file, "<b>");

                    results_ptr = html_copy(output_file, results_ptr, '|');

                    if (format->num_columns == 1)
                    {
                        /* skip over dummy data */
                        while (*results_ptr++ != '\n');
                    }

                    fprintf(output_file, "</b>\n");
                break;

                case 1:
                    if (format->num_columns == 3)
                    {
                        /* this is 3-column output, skip the dummy info in the function field. */
                        while (*results_ptr++ != ' ');
                    }

                    results_ptr = html_copy(output_file, results_ptr, ' ');

                break;

                case 2:
                    if (format->num_columns == 3)
                    {
                        results_ptr = html_copy(output_file, results_ptr, '\n');
                    }
                    else
                    {
                        results_ptr = html_copy(output_file, results_ptr, ' ');
                    }
                break;

                case 3:
                    results_ptr = html_copy(output_file, results_ptr, '\n');
                break;
            }

            // emit cell postamble
            fprintf(output_file, "\n</td>\n");
        }
        // emit row postamble
        fprintf(output_file, "\n</tr>\n");
        row = !row;
    }

    // emit file postamble
    fprintf(output_file, "\n</table>\n</html>\n");

    fclose(output_file);

    if (full_filename) g_free(full_filename);
    if (results_buf)   g_free(results_buf);

    return(TRUE);
}



static char *html_copy(FILE *output_file, char *read_ptr, char match_char)
{
    while (*read_ptr != match_char)
    {
        switch (*read_ptr)
        {
            case '<':
                fprintf(output_file, "&lt;");
                read_ptr++;
            break;

            case '>':
                fprintf(output_file, "&gt;");
                read_ptr++;
            break;

            case '"':
                fprintf(output_file, "&quot;");
                read_ptr++;
            break;

            case '\'':
                fprintf(output_file, "&apos;");
                read_ptr++;
            break;

            case '&':
                fprintf(output_file, "&amp;");
                read_ptr++;
            break;

            default:
                fputc( *read_ptr++, output_file );
            break;
        }

    }

    fputc( '\0', output_file);
    read_ptr++;      // skip match char

    return(read_ptr);
}



gboolean SEARCH_save_text(gchar *filename)
{
    FILE     *output_file;
    char     *results_buf = NULL;
    char     *results_ptr;
    char     *end_ptr;
    char     *work_ptr;
    char     *full_filename;
    off_t    size;

    /* Add an appropriate file extension, if neccessary */
    full_filename = g_malloc(strlen(filename) + 5);
    if (full_filename == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory for new file name.\n");
        return(FALSE);
    }

    work_ptr = strrchr(filename, '.');

    if (work_ptr == NULL)
    {
        /* Filename has no extension, add ".txt" */
        strcpy(full_filename, filename);
        strcat(full_filename, ".txt");
    }
    else
    {
        /* Filename has an extension, check for .txt */
        if ( strcmp(work_ptr, ".txt") == 0 )
        {
            /* We already have a valid .txt extension, don't add one */
            strcpy(full_filename, filename);
        }
        else
        {
            /* Not a valid .txt extension, add ".txt" */
            strcpy(full_filename, filename);
            strcat(full_filename, ".txt");
        }
    }

    /*** See if a non-zero results file exists.  If it does, open the results file and the output file ***/
    /*****************************************************************************************************/

    results_buf = open_results_file(temp1, &size);
    if ( results_buf == NULL )
    {
        return(FALSE);
    }

    output_file = open_out_file(full_filename);
    if ( output_file == NULL )
    {
        my_cannotopen(full_filename);
        return(FALSE);
    }

    /* Get a pointer to the beginning of the file buffer */
    results_ptr = results_buf;
    end_ptr = results_buf + size;

    while ( results_ptr < end_ptr )
    {
        fputc(*results_ptr++, output_file);
    }

    fclose(output_file);

    if (full_filename) g_free(full_filename);
    if (results_buf)   g_free(results_buf);

    return(TRUE);
}



gboolean SEARCH_save_csv(gchar *filename)
{
    FILE     *output_file;
    char     *results_buf = NULL;
    char     *results_ptr;
    char     *end_ptr;
    char     *work_ptr;
    char     *full_filename;
    off_t    size;

    /* Add an appropriate file extension, if neccessary */
    full_filename = g_malloc(strlen(filename) + 5);
    if (full_filename == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory for new file name.\n");
        return(FALSE);
    }

    work_ptr = strrchr(filename, '.');

    if (work_ptr == NULL)
    {
        /* Filename has no extension, add ".csv" */
        strcpy(full_filename, filename);
        strcat(full_filename, ".csv");
    }
    else
    {
        /* Filename has an extension, check for .txt */
        if ( strcmp(work_ptr, ".csv") == 0 )
        {
            /* We already have a valid .txt extension, don't add one */
            strcpy(full_filename, filename);
        }
        else
        {
            /* Not a valid .txt extension, add ".txt" */
            strcpy(full_filename, filename);
            strcat(full_filename, ".csv");
        }
    }

    /*** See if a non-zero results file exists.  If it does, open the results file and the output file ***/
    /*****************************************************************************************************/

    results_buf = open_results_file(temp1, &size);
    if ( results_buf == NULL )
    {
        return(FALSE);
    }

    output_file = open_out_file(full_filename);
    if ( output_file == NULL )
    {
        my_cannotopen(full_filename);
        return(FALSE);
    }

    /* Get a pointer to the beginning of the file buffer */
    results_ptr = results_buf;
    end_ptr = results_buf + size;

    // Create header entries
    fprintf(output_file, "File, Function, Line Number, Source Text,\n");

    while ( results_ptr < end_ptr )
    {
        // Copy the "filename"
        while (*results_ptr != '|')
        {
            fputc(*results_ptr++, output_file);
        }
        fputc(',', output_file);
        results_ptr++;

        // Copy the "function"
        while (*results_ptr != ' ')
        {
            fputc(*results_ptr++, output_file);
        }
        fputc(',', output_file);
        results_ptr++;

        // Copy the "Line Number"
        while (*results_ptr != ' ')
        {
            fputc(*results_ptr++, output_file);
        }
        fputc(',', output_file);
        results_ptr++;

        // Copy the "Source Text"
        fputc('"', output_file);   // Since source text can contain ',' delimiter, we need to quot the whole entry
        while ( (*results_ptr == ' ') || (*results_ptr == '\t') )
        {
            results_ptr++;      // Squash leading white space
        }

        while (*results_ptr != '\n')
        {
            if (*results_ptr == '"') fputc('"', output_file);   // Change all source text '"' to double-quote
            fputc(*results_ptr++, output_file);
        }
        fputc('"', output_file);
        fputc('\n', output_file);
        results_ptr++;
    }

    fclose(output_file);

    if (full_filename) g_free(full_filename);
    if (results_buf)   g_free(results_buf);

    return(TRUE);
}


/* Check for missing our updated source files */
void SEARCH_check_cref()
{
    uint32_t    i;
    char        *src_file;
    struct      stat statstruct;
    time_t      ref_time;
    gboolean    ref_status = TRUE;

    if ( cref_status )     // Don't bother checking if we already know the cref is out-of-date
    {
        /* Get the update time for the current cross-reference file */
        if ( stat(settings.refFile, &statstruct) != 0 )
        {
            /* Cross reference file doesn't exist, must have been deleted out from under us */
            DISPLAY_set_cref_current(FALSE);
            ref_status = FALSE;
        }
        else
        {
            ref_time = statstruct.st_mtime;

            for (i = 0; i < nsrcfiles; ++i)
            {
                src_file = DIR_src_files[i];

                if ( stat(src_file, &statstruct) != 0 )
                {
                    DISPLAY_set_cref_current(FALSE);
                    ref_status = FALSE;
                    break;
                }

                if ( statstruct.st_mtime > ref_time )
                {
                    DISPLAY_set_cref_current(FALSE);
                    ref_status = FALSE;
                    break;
                }
            }
        }

        DISPLAY_set_cref_current(ref_status);
        SEARCH_set_cref_status(ref_status);
    }
}



void SEARCH_set_cref_status(gboolean status)
{
    cref_status = status;
}



gboolean SEARCH_get_cref_status()
{
    return(cref_status);
}


void SEARCH_free_results(search_results_t *results)
{
    g_free(results->start_ptr);
    results->start_ptr = NULL;
    results->end_ptr = NULL;
    results->match_count = 0;
}

