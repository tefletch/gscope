/* -*-C-*-
*******************************************************************************
*
* File:         build.c
* Author:       David Lowe, HP/VCD
* Created:      Mon Apr  8 13:55:17 1991
* Description:  Functions to initialize and rebuild the cscope database
*
*
*******************************************************************************
*
*/

/*
Symbol database (cscope_db.out file) format

    header
    first file symbol data
    ...
    last file symbol data

The header is a single line

    cscope <format version> <current dir> [-T] <trailer offset>    Note: offset is obsolete.

The format version is the first number in the cscope version that wrote
the database, e.g. the format version is 9 for cscope version 9.14.
When the format version number in the cscope program is greater than that
in the database, the entire database will be rebuilt when any part of it is
out-of-date.  The current directory is either a full path or prefixed by
$HOME, allowing the user's login to be moved to a different file system
without rebuilding the database.  The (obsolete) trailer offset is the fseek(3)
offset of the trailer.

The header is followed by the symbol data for each file in alphabetical
order.  This allows fast updating of the database when only a few files
have changed, because cscope just copies the old database to the new up to
the point of the changed file. The the changed file's symbol data is added
to the new database, the old database is copied up to the next changed file,
and so on.

Two data compression techniques are used on the symbol data: (1) keywords
and trailing syntax are converted to control characters, and (2) common
digraphs (character pairs) are compressed to meta-characters (characters
with the eight bit set).

The symbol data for each file starts with

    <file mark><file path>
    <empty line>

and for each source line containing a symbol

    <line number><blank><non-symbol text>
    <optional mark><symbol>
    <non-symbol text>
    repeat above 2 lines as necessary
    <empty line>

Leading and trailing white space in the source line is removed. Tabs are
changed to blanks, and multiple blanks are squeezed to a single blank,
even in character and string constants. The symbol data for the last file
ends with

    <file mark>

A mark is a tab followed by one of these characters:

    Char    Meaning
    @   new file
    $   function definition
    `   function call
    }   function end
    #   #define
    )   #define end
    ~   #include
    c   class definition
    e   enum definition
    g   other global definition
    m   global enum/struct/union member definition
    s   struct definition
    t   typedef definition
    u   union definition

A #include mark is followed by '<' or '"' to determine if the current
directory should be searched. An untagged enum/struct/union definition
has a mark without a symbol name so it's beginning can be found.

Historical note: The first three mark characters were chosen because
they are not use by the C language, but since they can appear in a string
constant, the tab prefix was added. They were never changed to more
meaningful characters because other programs had been written to read
the database file.


============================= begin obsolete section =============================
Note:  The trailer is now obsolete (2/10/13 TF)

The trailer contains lists of source directories, include directories, and
source files; its format is

    <number of source directories> .
    <first source directory path (always .)>
    ...
    <last source directory path>
    <number of include directories>
    <first include directory path>
    ...
    /usr/include
    <number of source and include files>
    <first file path>
    ...
    <last file path>

The trailer was part of the header prior to cscope version 8.  The problem
was that the source file list did not have #include'd files because they
weren't known until the files were read and the symbol data output.  This
list is used by findstring() and findregexp(), so the #include'd files were
not searched if the database was up-to-date.  Outputting the source file
list after the symbol data solved this problem.  The directory lists could
have remained part of the header; it was just easier move all but the first
line of the header to the trailer.
============================== end obsolete section =============================
*/


/*** Includes ***/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>     /* stat */
#include <sys/time.h>
#include <unistd.h>
#include <glib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "dir.h"
#include "utils.h"
#include "crossref.h"
#include "search.h"
#include "build.h"
#include "lookup.h"
#include "scanner.h"
#include "display.h"
#include "app_config.h"
#include "auto_gen.h"



//===============================================================
//      Defines
//===============================================================
#define         FILEVERSION         14  /* symbol database file format version */
#define         OPTIONS_LEN         40


//===============================================================
//      Typedefs
//===============================================================

typedef struct
{
    time_t          reftime;
    char            *start;      /* Pointer to the beginning of the buffer */
    char            *end;        /* Pointer to the last byte of the buffer.  (buffer_start + buffer_length -1) */
} old_buf_descriptor_t;




//===============================================================
//      Local Functions
//===============================================================
static gboolean old_crossref_is_compatible(char *file_buf);
static void     initialize_using_old_cref(void);
static void     initialize_for_new_cref(void);
static void     build_new_cref(void);
static void     make_new_cref(old_buf_descriptor_t *old_descriptor);
static void     initcompress(void);
static void     putheader(char *dir);
static char     *get_old_file(char *dest_ptr, char *src_ptr);
static void     copydata(char *src_ptr);
static void     movefile(char *new, char *old);
static void     get_decompressed_string(char *dest, char *src);
static int      compare();   /* for qsort */


//===============================================================
//      Public Globals Variables
//===============================================================

int     fileversion;        /* cross-reference file version */


//===============================================================
//      Private Global Variables
//===============================================================


/* Note: these digraph character frequencies were calculated from possible
   printable digraphs in the cross-reference for the C compiler */
char        dichar1[] = " teisaprnl(of)=c"; /* 16 most frequent first chars */
char        dichar2[] = " tnerpla";         /* 8 most frequent second chars using the above as first chars */
char        dicode1[256];           /* digraph first character code */
char        dicode2[256];           /* digraph second character code */

FILE        *newrefs;           /* new cross-reference */


struct timeval overall_time_start,  overall_time_stop;
struct timeval src_list_time_start, src_list_time_stop;
struct timeval cref_time_start,     cref_time_stop;

static char build_stats_msg[1024];

//====================================================================
//
// Open up the cross reference database.  This database will be
// constructed by one of three methods:
//
//    1) Nobuild: An old, pre-existing, database will be opened
//       without any modifications to the original contents.
//       This is the default behavior if, and only if, absolutely
//       none of the source files, and include files have changed.
//       This mode can be explicity requested by the user, but
//       doing so could create a data coherency problem if target
//       source file content (or file existance) have changed since
//       the original cross-reference was generated.
//
//    2) Full: A completely new, built from scratch, cross reference
//       database is created. (Slowest mode of open - source file
//       list is constructed and cross reference is built from
//       scratch, every detected source file is parsed).  This is
//       the default behavior if no previous cross-reference is
//       detected (or the user requests a 'forced' rebuild).
//
//    3) Incremental:  An out-of-date, but "compatible" old cross-
//       reference is found. A completely new, source file list is
//       constructed.  When the cross-reference database is
//       populated, only new files and modifed files are parsed.
//       Existing files that have not been touched are not re-parsed
//       and their existing data is simply copied to the new database.
//       This is the default behavior if a previous, compatible,
//       cross-reference is detected.  (Assuming 'forced' and
//       'nobuild' are not specified)
//
//====================================================================

void BUILD_initDatabase()
{
    suseconds_t elapsed_usec;
    char working_buf[500];

    build_stats_msg[0] = '\0';   /* Initialize build stats to null string */

    gettimeofday(&overall_time_start, NULL);

    /* if the database path is relative and it can't be created */
    if (settings.refFile[0] != '/' && access(".", W_OK) != 0)
    {
        (void) fprintf(stderr, "Cannot create Cross Reference file, no write permission for [%s] in CWD\n", settings.refFile);
        exit(EXIT_FAILURE);
    }

    if (settings.noBuild)
    {
        /* We want to re-use an existing cross-reference */
        printf("\nWARNING: The '--no_build' option is active.\n");
        printf("         The pre-existing Cross-Reference will be coherent only if no source\n");
        printf("         files have changed since the last time it was built.\n\n");

        /* Re-use an existing cross-reference */
        initialize_using_old_cref();
    }
    else
    {
        /* Build a new cross-reference */
        initialize_for_new_cref();
    }

    // Now that we have a valid cross-reference database,
    // Initialize the "search" sub-system
    SEARCH_init();

    /* Free the source_name hash table (no longer needed) */
    DIR_free_src_names_hash();

    /* Free the offset_hash table (no longer needed) */
    DIR_free_offset_hash();

    gettimeofday(&overall_time_stop, NULL);

    /* Show (optional) autogen stats */
    if (settings.autoGenEnable)
    {
        proto_compile_stats_t *stats_ptr =  AUTOGEN_get_file_count();

        sprintf(working_buf, "Updated auto-generated header files for %d of %d\ndetected (%s) files (%d Succeeded, %d Failed).\n",
                stats_ptr->num_proto_changed,
                stats_ptr->num_proto_files,
                settings.autoGenSuffix,
                stats_ptr->proto_build_success,
                stats_ptr->proto_build_failed);
        strcat(build_stats_msg, working_buf);
    }


    strcat(build_stats_msg,"\n");

    /* Show elapsed times */

    if (src_list_time_stop.tv_usec >= src_list_time_start.tv_usec)
        elapsed_usec = src_list_time_stop.tv_usec - src_list_time_start.tv_usec;
    else
    {
        elapsed_usec = (1000000 + src_list_time_stop.tv_usec) - src_list_time_start.tv_usec;
        src_list_time_stop.tv_sec--;
    }

    sprintf(working_buf, "Source Search Time:    %ld.%6.6ld seconds\n",
           src_list_time_stop.tv_sec - src_list_time_start.tv_sec,
           elapsed_usec );
    strcat(build_stats_msg, working_buf);


    if (cref_time_stop.tv_usec >= cref_time_start.tv_usec)
        elapsed_usec = cref_time_stop.tv_usec - cref_time_start.tv_usec;
    else
    {
        elapsed_usec = (1000000 + cref_time_stop.tv_usec) - cref_time_start.tv_usec;
        cref_time_stop.tv_sec--;
    }

    sprintf(working_buf, "Cross Ref Time:            %ld.%6.6ld seconds\n",
           cref_time_stop.tv_sec - cref_time_start.tv_sec,
           elapsed_usec );
    strcat(build_stats_msg, working_buf);


    if (overall_time_stop.tv_usec >= overall_time_start.tv_usec)
        elapsed_usec = overall_time_stop.tv_usec - overall_time_start.tv_usec;
    else
    {
        elapsed_usec = (1000000 + overall_time_stop.tv_usec) - overall_time_start.tv_usec;
        overall_time_stop.tv_sec--;
    }

    if (settings.autoGenEnable)
    {
        sprintf(working_buf, "Autogen time:              %ld.%6.6ld seconds\n",
               autogen_elapsed_sec,
               autogen_elapsed_usec );
        strcat(build_stats_msg, working_buf);
    }

    sprintf(working_buf, "Overall Time:               %ld.%6.6ld seconds",
           overall_time_stop.tv_sec - overall_time_start.tv_sec,
           elapsed_usec );
    strcat(build_stats_msg, working_buf);

    if ( !settings.refOnly )
    {
        DISPLAY_update_stats_tooltip(build_stats_msg);
        DISPLAY_set_cref_current(TRUE);
    }
    else
        printf("\n%s\n", build_stats_msg);
}



void BUILD_init_cli_file_list(int argc, char *argv[])
{
    DIR_init_cli_file_list(argc, argv);
}


static void initialize_using_old_cref()
{
    FILE      *old_file;           /* old cross-reference file */
    char      *format_string;
    char      options[OPTIONS_LEN + 1];
    char      *options_ptr = options;
    char      *old_file_buf = NULL;     /* Buffer that holds the entire old crossref file contents */
    char      *oldbuf_ptr;
    struct    stat statstruct;          /* file status */
    char      src_file[PATHLEN +1];
    int       file_count;

    gettimeofday(&src_list_time_start, NULL);

    /* If there is no pre-existing cross-reference */
    if ( (stat(settings.refFile, &statstruct) != 0 ) || ((old_file = fopen(settings.refFile, "rb")) == NULL) )
    {
        fprintf(stderr, "Fatal Error: No pre-existing cross-reference file [%s] available\n", settings.refFile);
        exit(EXIT_FAILURE);
    }

    old_file_buf = g_malloc(statstruct.st_size);    /* malloc a buffer to hold the entire old-file */
    if (old_file_buf == NULL)
    {
        fprintf(stderr, "Fatal Error: Unable to allocate memory for old cross-reference file\n");
        exit(EXIT_FAILURE);
    }


    if ( fread(old_file_buf, 1, statstruct.st_size, old_file) != statstruct.st_size )
    {
        fprintf(stderr, "Fatal Error: Unable to to read old cross-reference file\n");
        exit(EXIT_FAILURE);
    }
    fclose(old_file);


    /* Construct a format_string that protects us from buffer overflows */
    my_asprintf(&format_string, "cscope %%d %%*s %%%ds", OPTIONS_LEN);

    /* get the crossref file version but skip the current directory */
    if ( (sscanf(old_file_buf, format_string, &fileversion, options) != 2) || (fileversion < FILEVERSION) )
    {
        fprintf(stderr, "Fatal Error: Incompatible pre-existing cross-reference file [%s]\n", settings.refFile);
        exit(EXIT_FAILURE);
    }
    g_free(format_string);


    /* Process the options in the cross-reference (the options set in the cref file set the configuration) */
    while ( *options_ptr )
    {
        switch ( *options_ptr )
        {
            case 'c':   /* Compression disabled setting */
                options_ptr++;
                settings.compressDisable = (*options_ptr++ == '1');
            break;

            case 'T':   /* truncate symbols to 8 characters */
                options_ptr++;
                settings.truncateSymbols = (*options_ptr++ == '1');
            break;

            default:
                /* Unrecognized option, ignore */
            break;
        }
    }

    /* Initialize the digraph character tables for text compression */
    initcompress();

    gettimeofday(&src_list_time_start, NULL);

    /* Extract the source file list from the existing cross-reference data */
    /***********************************************************************/
    DIR_init(OLD_CREF);

    file_count = 0;
    oldbuf_ptr = old_file_buf;
    oldbuf_ptr = get_old_file(src_file, oldbuf_ptr);

    while (*src_file != '\0')
    {
        file_count++;
        DIR_addsrcfile(src_file);
        oldbuf_ptr = get_old_file(src_file, oldbuf_ptr);
    }

    g_free(old_file_buf);

    gettimeofday(&src_list_time_stop, NULL);
    gettimeofday(&cref_time_start, NULL);
    cref_time_stop = cref_time_start;

    {
        char *working_buf;
        my_asprintf(&working_buf, "Using %d Old (noBuild) Cross-reference files\n\n", file_count);
        strcat(build_stats_msg, working_buf);
        g_free(working_buf);
    }
}


static void initialize_for_new_cref()
{
    gettimeofday(&src_list_time_start, NULL);

    if ( !settings.refOnly )  // Only update if we are in GUI mode.
    {
        /* Bring up the splash screen prior to searching for source files (the search can take a while) */
        DISPLAY_update_build_progress(0, 100);   /* Show (essentially) no progress */
    }

    /* Create a fresh Source-File list.
       Initialize the Include-Directory list.
       Initialize key path variables and file name */
    DIR_init(NEW_CREF);

    if (settings.autoGenEnable)
        AUTOGEN_run( DIR_get_path(DIR_DATA));


    if (nsrcfiles == 0)
    {
        if ( !settings.refOnly )
            DISPLAY_msg(GTK_MESSAGE_ERROR, "<span weight=\"bold\"> No source files found</span>\nGscope will exit.");
        else
            fprintf(stderr,"No source files found. Gscope will exit.\n");

        exit(EXIT_FAILURE);
    }

    /* initialize the C keyword table */
    initsymtab();

    /* Initialize the digraph character tables for text compression */
    initcompress();

    gettimeofday(&src_list_time_stop, NULL);

    // At this point, we have:
    //      1) A new (core) source file list (with length) - this list doesn't have any 'included' files yet.
    //      2) A source file hash table
    //      3) An (optional) include file search path
    //      4) A C-keyword hash table
    //      5) A digraph character compression table
    // We are now ready parse the source files and build the cross-reference database
    gettimeofday(&cref_time_start, NULL);

    build_new_cref();

    gettimeofday(&cref_time_stop, NULL);
}




/* set up the digraph character tables for text compression */

static void initcompress()
{
    int    i;

    if (settings.compressDisable == FALSE)
    {
        for (i = 0; i < 16; ++i)
        {
            dicode1[(unsigned) (dichar1[i])] = i * 8 + 1;
        }
        for (i = 0; i < 8; ++i)
        {
            dicode2[(unsigned) (dichar2[i])] = i + 1;
        }
    }
}



/* build the cross-reference */

void build_new_cref()
{
    FILE    *old_file;
    char    *old_file_buf = NULL;   /* Buffer that holds the entire old crossref file contents */
    struct  stat statstruct;        /* file status */
    size_t  num_bytes;

    gboolean force_rebuild;
    old_buf_descriptor_t     old_buf_descriptor;

    /*
        The cross reference build algorithm is as follows:
            1) If absolutely nothing has changed re-use the old cross-reference as-is (no rebuild)
            2) If the "force rebuild" option is set, or there is no 'old' cross-reference file, build
               a full cross reference from scratch.  (full rebuild)
            3) If the existing cross ref file format is "compatible", program options are compatible
               and only some files have been modified/added/deleted, re-use what we can, generate
               the rest (incremental rebuild)
    */


    /* sort the source file names list (needed for rebuilding) */
    qsort((char *) DIR_src_files, nsrcfiles, sizeof(char *), compare);

    force_rebuild = FALSE;


    /* If there is no pre-existing cross-reference OR we are configured to ignore it */
    if ( ( stat(settings.refFile, &statstruct) != 0 ) || ( settings.updateAll ) )
    {
        force_rebuild = TRUE;
    }
    else    /* There is a pre-existing cross-reference present AND we are _NOT_ ignoring it */
    {
        if ( (old_file = fopen(settings.refFile, "rb")) == NULL)
        {
            fprintf(stderr, "Error opening old cross-reference file.  Assuming old file is out-of-date.\n");
            force_rebuild = TRUE;
        }
        else    /* The old cross-reference file has been successfully opened */
        {
            old_file_buf = g_malloc(statstruct.st_size + 1);  /* malloc a buffer to hold the entire old-file */
            if ( old_file_buf == NULL )
            {
                fprintf(stderr, "Error allocating memory to read old cross-reference file.  Assuming old file is out-of-date.\n");
                force_rebuild = TRUE;
            }
            else
            {
                num_bytes = fread(old_file_buf, 1, statstruct.st_size, old_file);
                if ( num_bytes != statstruct.st_size )
                {
                    fprintf(stderr, "Error reading old cross-reference file.  Assuming old file is out-of-date.\n");
                    force_rebuild = TRUE;
                }
                else
                {
                    old_file_buf[num_bytes] = '\0';
                    if ( !old_crossref_is_compatible(old_file_buf) )
                    {
                        printf("Pre-existing cross-reference file is incompatible.  Building New database...\n");
                        force_rebuild = TRUE;
                    }
                    else
                    {
                        /* Looks like a useable "old" cross-reference, initialize the descriptor */
                        /*************************************************************************/

                        /* Get the modification time of the old cross-reference file */
                        old_buf_descriptor.reftime = statstruct.st_mtime;
                        old_buf_descriptor.start   = old_file_buf;
                        old_buf_descriptor.end     = old_file_buf + statstruct.st_size - 1;
                    }
                }
            }
            fclose(old_file);
        }
    }


    if ( force_rebuild )
        make_new_cref(NULL);                /* Create a full cross reference */
    else
        make_new_cref(&old_buf_descriptor); /* Create an incremental cross-reference */


    if (old_file_buf) g_free(old_file_buf);

    return;
}



static gboolean old_crossref_is_compatible(char *file_buf)
{
    char    olddir[PATHLEN + 1];
    char    options[OPTIONS_LEN + 1];
    char    *options_ptr = options;
    gboolean retval = TRUE;
    char    *data_dir;
    gboolean option_val;
    char    *format_string;

    char    *cref_header;
    guint   header_size;

    data_dir = DIR_get_path(DIR_DATA);

    // Since sscanf expects a null-terminated string for it's input stream, we need
    // to extract the cross-reference header line into a standalone, null-terminate string.
    header_size = strstr(file_buf, "\n") - file_buf;
    cref_header = g_malloc(header_size + 1);
    strncpy(cref_header, file_buf, header_size);
    cref_header[header_size] = '\0';

    /* Check the file Version */
    if ( (sscanf(cref_header, "cscope %d", &fileversion) == 1) &&  (fileversion == FILEVERSION) ) /* File version match */
    {
        /* Construct a format_string that protects us from buffer overflows */
        my_asprintf(&format_string, "cscope %%*d %%%ds %%%ds", PATHLEN, OPTIONS_LEN);

        /* Get the path and options-list from the old cross-reference file */
        if (( sscanf(cref_header, format_string, olddir, options) == 2) &&    /* File format OK */
            ( strlen(olddir) == strlen(data_dir) ) &&                    /* Path string length matches */
            ( strncmp(olddir, data_dir, strlen(data_dir)) == 0 ))        /* Path String content mmatches */
        {
            /* Verify the old config and current config options are compatible (not necessarily identical) */
            while ( *options_ptr )
            {
                switch ( *options_ptr )
                {
                    case 'c':   /* Compression disabled setting */
                        options_ptr++;
                        option_val = (*options_ptr++ == '1');
                        if (settings.compressDisable != option_val)  retval = FALSE;
                    break;

                    case 'T':   /* truncate symbols to 8 characters */
                        options_ptr++;
                        option_val = (*options_ptr++ == '1');
                        if (settings.truncateSymbols != option_val) retval = FALSE;
                    break;

                    default:
                        /* Unrecognized option, ignore */
                    break;
                }
            }
        }
        else
        {
            retval = FALSE;
        }
        g_free(format_string);
    }
    else
    {
        retval = FALSE;
    }

    g_free(cref_header);
    return(retval);
}



static void make_new_cref(old_buf_descriptor_t *old_descriptor)
{
    uint32_t    firstfile;          /* first source file in pass */
    uint32_t    lastfile;           /* last source file in pass */
    uint32_t    num_original;       /* Count of original source files */
    uint32_t    fileindex;          /* source file name index */
    char        *new_file;          /* current file */
    int         built = 0;          /* built crossref for these files */
    int         skipped = 0;        /* number of invalid "source" files skipped */
    int         copied = 0;         /* copied crossref for these files */
    struct      stat statstruct;    /* file status */

    time_t      starttime;
    time_t      now;

    char        *old_offset_ptr;
    char        *new_cref_file;
    gboolean    full_update;
    char        working_buf[200];


    if (old_descriptor == NULL)
        full_update = TRUE;
    else
        full_update = FALSE;


    /* open the new cross-reference file */
    new_cref_file = DIR_get_path(FILE_NEW_CREF);
    if ((newrefs = fopen(new_cref_file, "w")) == NULL)
    {
        my_cannotopen(new_cref_file);
        exit(EXIT_FAILURE);
    }

    putheader( DIR_get_path(DIR_DATA) );

    /* output the leading tab expected by crossref() */
    dbputc('\t');

    /* make passes through the source file list until the last level of included files is processed */

    // The initial source file list is based on startup options (command line args and current config).
    // This list is sorted, and there are no duplicates.  It is further augmented when #include directives
    // are encountered in source files and the included file is not already on the source list (but found
    // via include file search path - as source files are parsed).

    firstfile = 0;
    lastfile = nsrcfiles;
    num_original = nsrcfiles;

    starttime = time((time_t *) NULL);  // Initialize the progress bar timer


    if (full_update)    /*** Start full update ***/
    {
        /*** Walk the NEW source file list and generate a new cross-reference section for every file. ***/
        /************************************************************************************************/
        for (;;)
        {
            /* get the next source file name from the NEW source file list*/
            for (fileindex = firstfile; fileindex < lastfile; fileindex++)
            {
                if ( !settings.refOnly )  // Only update if we are in GUI mode.
                {
                    now = time((time_t *) NULL);
                    if ( (now  - starttime) >= 1 )
                    {
                        starttime = now;
                        DISPLAY_update_build_progress(fileindex, nsrcfiles);
                    }
                }

                /* if srcDir is not NULL, temporarily cd to srcDir */
                if ( strcmp(settings.srcDir, "") != 0) my_chdir(settings.srcDir);

                new_file = DIR_src_files[fileindex];

                if ( crossref(new_file) )
                    built++;
                else
                    skipped++;

                /* if srcDir is not NULL, pop back to the original CWD */
                if ( strcmp(settings.srcDir, "") != 0) my_chdir( DIR_get_path(DIR_CURRENT_WORKING) );

            }  /* for (fileindex = firstfile; fileindex < lastfile; ++fileindex) */

            /* Process all include files detected during parsing */
            if (lastfile == nsrcfiles)
            {
                sprintf(working_buf, "Cross-referenced %d files\n(Source parsing found %d additional include files)\n",
                        nsrcfiles - skipped, nsrcfiles - skipped - num_original);
                strcat(build_stats_msg, working_buf);

                if (skipped > 0)
                {
                    sprintf(working_buf, "Skipped %d Non-ASCII text source files\n", skipped);
                    strcat(build_stats_msg, working_buf);
                }

                break;
            }

            firstfile = lastfile;
            lastfile = nsrcfiles;

            /* sort the included file names */
            qsort( (char *) &DIR_src_files[firstfile],
                   (unsigned) (lastfile - firstfile),
                   sizeof(char *),
                   compare
                 );

        }  /* for (;;) */
    }   /** End Full Update ***/
    else
    {
        /*** Start Incremental Update ***/

        DIR_create_offset_hash(old_descriptor->start);   /* Construct a hash table of old-cref file section offsets (for re-use lookup) */

        /*** Walk the NEW source file list and generate a new cross-reference using oldcross-reference data (if  ***/
        /*** it is still up-to-date). Otherwise, generate a new cross-reference section for the file.            ***/
        /***********************************************************************************************************/

        for (;;)
        {
            /* get the next source file name from the NEW source file list*/
            for (fileindex = firstfile; fileindex < lastfile; ++fileindex)
            {
                if ( !settings.refOnly )  // Only update if we are in GUI mode.
                {
                    now = time((time_t *) NULL);
                    if ( (now  - starttime) >= 1 )
                    {
                        starttime = now;
                        DISPLAY_update_build_progress(fileindex, nsrcfiles);
                    }
                }

                /* if srcDir is not NULL, temporarily cd to srcDir */
                if ( strcmp(settings.srcDir, "") != 0) my_chdir(settings.srcDir);

                new_file = DIR_src_files[fileindex];

                old_offset_ptr = DIR_get_old_offset(new_file);

                if (old_offset_ptr)     /* Old file match */
                {
                    /* If the file has been modified since it was last parsed. */
                    if (stat(new_file, &statstruct) == 0 && statstruct.st_mtime > old_descriptor->reftime)
                    {
                        if ( crossref(new_file) )
                            ++built;
                        else
                            skipped++;
                    }
                    else
                    {
                        /* copy (re-use) the old (and still valid) cross-reference data*/

                        // Yes, we re-use the old data if we can't stat the file in question.  It's just
                        // too obscure of a corner case to justify more complexity -- 2/8/13 TF

                        copydata(old_offset_ptr + 1);  // skip the leading '\t' character
                        ++copied;
                    }
                }
                else            // File not found in old CREF, this must be a new file
                {
                    if ( crossref(new_file) )
                        ++built;
                    else
                        skipped++;
                }

                /* if srcDir is not NULL, pop back to the original CWD */
                if ( strcmp(settings.srcDir, "") != 0) my_chdir( DIR_get_path(DIR_CURRENT_WORKING) );

            } /* for (fileindex = firstfile; fileindex < lastfile; ++fileindex) */

            /* Process all include files detected during parsing */
            if (lastfile == nsrcfiles)
            {
                sprintf(working_buf, "Cross-referenced %d files (%d New, %d Re-used)\nSource parsing found %d additional include files\n",
                        nsrcfiles - skipped, built, copied, nsrcfiles - skipped - num_original);
                strcat(build_stats_msg, working_buf);

                if (skipped > 0)
                {
                    sprintf(working_buf, "Skipped %d Non-ASCII text source files\n", skipped);
                    strcat(build_stats_msg, working_buf);
                }

                break;
            }
            firstfile = lastfile;
            lastfile = nsrcfiles;

            /* sort the included file names */
            qsort( (char *) &DIR_src_files[firstfile],
                   (lastfile - firstfile),
                   sizeof(char *),
                   compare
                 );

        } /* for(;;) */
    }  /*** End Incremental Update ***/

    /* add a null file name to the trailing tab */
    dbputc(NEWFILE);
    dbputc('\n');

    if (fflush(newrefs) == EOF)
    {
        /* fflush() failed - some sort of fatal file write error has occurred */

        fprintf(stderr, "%s\n", strerror(errno));    /* display the reason */
        (void) unlink(new_cref_file);
        fprintf(stderr, "Removed file %s because write failed\n", new_cref_file);
        exit(EXIT_FAILURE);
    }


    (void) fclose(newrefs);

    /* replace the old database file with the new database file */
    movefile(new_cref_file, settings.refFile);
//    printf("Cross-Reference build complete.\n\n");
}




/* string comparison function for qsort */

static int compare(char **s1, char **s2)
{
    return(strcmp(*s1, *s2));
}



static char *get_old_file(char *dest_ptr, char *src_ptr)
{
    uint32_t count = 0;

    if (src_ptr == NULL)
    {
        *dest_ptr = '\0';
        return(NULL);
    }

    // Scan to the next file entry "\t@"
    do
    {
        /* loop optimized to only one test */
        while ( *src_ptr++ != '\t');
    } while ( *src_ptr++ != NEWFILE );

    // Check for end of list marker
    if ( *src_ptr == '\0')
    {
        return(NULL);   // We have reached the end of the file list
    }
    else
    {
        // While not newline, copy (non-comressed) string to filename_ptr (watch for overflow PATHLEN + 1)
        while ( (*src_ptr != '\n') && (count < PATHLEN) )
        {
            *dest_ptr++ = *src_ptr++;
            count++;
        }

        // Null-terminate the string
        *dest_ptr++ = '\0';

        // Return "updated" source pointer address
        return(++src_ptr);
    }
}


/* output the cscope version, current directory, database format options, and
   the database trailer offset */

static void putheader(char *dir)
{
    dboffset = fprintf(newrefs, "cscope %d %s ", FILEVERSION, dir);

    /* When re-using a saved database, the application settings must track the settings used to create the original */

    dboffset += fprintf(newrefs, "%s", settings.compressDisable ? "c1" : "c0");

    dboffset += fprintf(newrefs, "%s", settings.truncateSymbols ? "T1" : "T0");

    /* Terminate the options field and add a dummy offset value (traileroffset value is no longer used) */
    dboffset += fprintf(newrefs, " %.10d\n", dboffset);
}



/* copy this file's symbol data */
static void copydata(char *src_ptr)
{
    char   symbol[PATHLEN + 1];

    for (;;)
    {
        /* copy up to the next 'tab' */
        while (*src_ptr != '\t')
        {
            dbputc(*src_ptr++);
        }

        dbputc(*src_ptr);   /* copy the tab, but don't increment the read pointer yet */

        /* exit if at the end of this file's cross-reference data */
        if (*(src_ptr + 1) == NEWFILE)
        {
            break;
        }

        src_ptr++;      /* Now update the read pointer */

        /* look for an #included file */
        if (*src_ptr == INCLUDE)
        {
            /* Get the (uncompressed) include file name */
            get_decompressed_string(symbol, src_ptr + 2);     /* Skip the '~<' or '~"' when extracting the filename */

            /* Add the include file to the source file list (if it isn't already there) */
            DIR_incfile(symbol);   // Revisit:  Lots of duplicates possible, maybe an optimization opportunity here.
        }
    }
    return;
}



/* replace the old file with the new file */

static void movefile(char *new, char *old)
{
    (void) unlink(old);
    if (link(new, old) == -1)
    {
        //logMessage(strerror(errno));
        fprintf(stderr, "Cannot link file %s to file %s", new, old);
        exit(EXIT_FAILURE);
    }
    if (unlink(new) == -1)
    {
        //logMessage(strerror(errno));
        fprintf(stderr, "Cannot unlink file %s", new);
    }
}



/* put the rest of the cross-reference line into the string */
static void get_decompressed_string(char *dest, char *src)
{
    uint8_t byte;

    while ( (byte = (unsigned) (*src)) != '\n' )
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
        src++;
    }
    *dest++ = '\0';    // Null-terminate the extracted string
}
