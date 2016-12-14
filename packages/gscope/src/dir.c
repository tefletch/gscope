/*	Gscope - interactive C symbol cross-reference
 *
 *	directory searching functions
 */

#ifdef __ghs__      // hack to make ftw() work when using (32-bit) GHS tool chain [fixes size conflict]
#define _FILE_OFFSET_BITS 64
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <ftw.h>
#include <stdint.h>
#include <errno.h>

#include "global.h"
#include "app_config.h"
#include "utils.h"
#include "dir.h"
#include "build.h"
#include "scanner.h"
#include "search.h"
#include "display.h"
#include "auto_gen.h"


#define ANALYZE_HASH    2      /* set to 1 to enable hash analysis */
#define OLD_HASH        1


//#define	DIRSEPS	" ,:"	/* directory list separators */
#define	DIRINC	40	        /* directory list size increment */

/* As of Jan-2016:  Largest known database had 85000 files                             */
/* For a data set of this size, and composition (med-to-long pathname strings), a hash */
/* table size of 2^14 is a sweet spot for the Bob Jenkins lookup3 hash functions.      */

#define HASH_SIZE_EXPONENT  14                                      /* Hash table size (power of 2) size = 2^n */
#if (OLD_HASH == 1)
#define HASH_SIZE     2003
#else
#define HASH_SIZE           ( (uint32_t) 1 << (HASH_SIZE_EXPONENT)) /* New hash function -- best if a power of 2 */
#endif
#define HASH_MASK           ( (HASH_SIZE) - 1)
#define	SRC_GROW_INCREMENT  10000   	                            /* source file list size increment */

//#define HASHMOD	2003	    /* (old hash function -- mod prime) must be a prime number - original cscope value */


/*
 * My best guess at if you are big-endian or little-endian.  This may
 * need adjustment.
 */
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
     __BYTE_ORDER == __LITTLE_ENDIAN) || \
    (defined(i386) || defined(__i386__) || defined(__i486__) || \
     defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL))
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
       __BYTE_ORDER == __BIG_ENDIAN) || \
      (defined(sparc) || defined(POWERPC) || defined(mc68000) || defined(sel))
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 0
#endif


#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose 
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}


//=====================================================================================
//  Public (Program-Global) Variables
//=====================================================================================

char      **DIR_src_files;                /* source file list */
int       DIR_max_src_files = 100000;     /* maximum number of source files (scalable) */
uint32_t  nsrcfiles;




//=====================================================================================
//  Private (File-Global) Variables
//=====================================================================================

//static int     nsrcdirs;                /* number of source directories */


//static char    currentdir[PATHLEN + 1];  /* current directory */
static char     **include_dirs;             /* List of directories to search for #include files */
static int      num_include_dirs;           /* number of #include directories */
static int      max_include_dirs;           /* maximum number of #include directories */

static char     *master_ignored_list = NULL;
static char     master_ignored_delim;

int     msrcdirs;                 /* maximum number of source directories */
char    *namefile;                /* file of file names */
FILE    *rlogfile;


#if (ANALYZE_HASH == 1)
static  int hash_collisions;
#endif


static  struct  listitem
{  /* source file names */
    char    *text;
    struct  listitem *next;
} *src_names_hash_tbl[HASH_SIZE];


static  struct  offset_listitem
{
    char    *text;          /* source file name */
    char    *offset;        /* Offset into Cross-Reference for "source file's" data */
    struct  offset_listitem *next;
} *offset_hash_tbl[HASH_SIZE];


//=====================================================================================
// Private Function Prototypes
//=====================================================================================

static void       find_srcfiles_in_tree(gchar *src_dir);
static gboolean   infilelist(const char *file);
static int        list(const char *name, const struct stat *status, int type);
static gboolean   issrcfile(const char *file);
static gboolean   path_check_ok(const char *file);
static char *     compress_path(char *pathname);
static void       _make_src_file_list(void);
static void       _init_include_dir_list(void);
static void       _alloc_src_file_list(void);
static gboolean   is_protobuf_file(const char *filename);
static void       add_src_primitive(char *name);

#if ( OLD_HASH == 1 )
static int  hash(const char *ss);
#else
static uint32_t   hashlittle( const void *key, size_t length, uint32_t initval);
static uint32_t   hash(const char *hash_string);
#endif



void DIR_init(dir_init_e init_type)
{
    if (init_type == NEW_CREF)
    {
        DIR_get_path(DIR_INITIALIZE);
        _alloc_src_file_list();
        _make_src_file_list();
        _init_include_dir_list();
    }
    else
    {
        _alloc_src_file_list();
    }
}



char *DIR_get_path(get_method_e method)
{
    // Methods:
    //      DIR_INITIALIZE
    //      DIR_CURRENT_WORKING
    //      DIR_DATA
    //      DIR_SOURCE
    //      FILE_NEW_CREF

    char      *working_ptr;

    static char *cwd      = NULL;
    static char *data_dir = NULL;
    static char *src_dir  = NULL;
    static char *new_cref = NULL;

    static char      new_cref_buf[MAX_STRING_ARG_SIZE + 4];   /* Added space is for ".new" suffix */
    static char full_new_cref_buf[PATHLEN + 1];
    static char      data_dir_buf[PATHLEN + 1];
    static char      src_dir_buf [PATHLEN + 1];
    static char      autogen_cache[PATHLEN + 1];

    switch (method)
    {
        case DIR_INITIALIZE:
            // Derive the paths of several key run-time directories and files
            // Including:  
            //   1. Current-Working-Directory (the directory that G-Scope was run from)
            //   2. Data-Directory (the directory where cross-reference data is stored -- default is CWD)
            //   3. Source-directory (the directory where source files are found -- default is CWD)
            //   4. The full path name of the "new" cross-reference files


            /*** Set CWD ***/
            /***************/
            if (cwd) free(cwd);  /* avoid memory leak if we DIR_INITIALIZE more than once */

            cwd = getcwd(NULL,0);  /* Get a dynamically allocated CWD string */
            if (!cwd)
            {
                fprintf(stderr, "Error: cannot get current working directory name.\n");
                exit(EXIT_FAILURE);
            }


            /*** Set new cref file name ***/
            /******************************/
            /* Note: refFile could be a simple file name or a path */
            strcpy(new_cref_buf, settings.refFile);
            strcat(new_cref_buf, ".new");
            new_cref = new_cref_buf;


            /*** Set the Data-Directory ***/
            /********************************/
            /* if new_cref_file specifies a path, that is our data_dir path.  If none specified, use CWD */
            working_ptr = my_basename(new_cref);

            if (working_ptr == new_cref)  /* no data-dir path specified, use CWD */
            {
               data_dir = cwd;

               /* prepend data_dir to the cref file name */
               strcpy(full_new_cref_buf, data_dir);
               strcat(full_new_cref_buf, "/");
               strcat(full_new_cref_buf, new_cref_buf);
               new_cref = full_new_cref_buf;
            }
            else    /* a data-dir path is specified, use it */
            {
               uint32_t size;

               size = working_ptr - new_cref - 1;       // -1 to drop the trailing '//
               strncpy(data_dir_buf, new_cref, size);
               data_dir_buf[size] = '\0';   /* Null-terminate the string */
               data_dir = data_dir_buf;
            }

            /*** Set the Source-Directory ***/
            /********************************/
            /* if no user-specified value for srcDir, use CWD */
            if ( strcmp(settings.srcDir, "") == 0)
                src_dir = cwd;
            else
            {
                strcpy(src_dir_buf, settings.srcDir);
                src_dir = src_dir_buf;
            }


            /*** Set the autogen_cache Directory ***/
            /***************************************/
            snprintf(autogen_cache, PATHLEN, "%s/%s", settings.autoGenPath, getenv("USER"));


            if ( !settings.refOnly )    // Only update when in GUI mode.
                DISPLAY_update_path_label(src_dir);     /* Update the "path label" on the main window */
        break;


        case DIR_CURRENT_WORKING:
            return(cwd);

        case DIR_DATA:
            return(data_dir);

        case DIR_SOURCE:
            return(src_dir);

        case FILE_NEW_CREF:
            return(new_cref);

        case DIR_AUTOGEN_CACHE:
            return(autogen_cache);

        default:
            /* do nothing */
        break;
    }
    
    return(NULL);
}


static void _init_include_dir_list()
{
   int  i;
   char *extract_ptr;
   char *extract_end_ptr;
   char *working_ptr;
   char  include_dir[MAX_STRING_ARG_SIZE];


   if (include_dirs != NULL)   /* If the include-directory list is NOT empty */
   {
      /* Free the pre-existing include_dirs list */

      for (i = 0; i < max_include_dirs; i++)
      {
         /* Stop when we encounter a NULL entry in the table */
         if ( include_dirs[i] == NULL) break;

         g_free(include_dirs[i]);     /* Free the old directory list string */
      }

      /* Re-initialze the table */
      memset(include_dirs, 0, sizeof(*include_dirs) * max_include_dirs);

      /* Re-use the existing, empty, include_dirs table */
      num_include_dirs = 0;
   }
   else
   {
      num_include_dirs = 0;
      max_include_dirs = DIRINC;
      include_dirs = g_malloc(max_include_dirs * sizeof(char *));

      /* Initialize the table */
      memset(include_dirs, 0, sizeof(*include_dirs) * max_include_dirs);
   }


   /* Update the Include file search list [From command line '-I <list>' or startup preferences] */
   extract_ptr = settings.includeDir;

   if ( *extract_ptr != '\0')
   {
       extract_end_ptr = extract_ptr + strlen(extract_ptr);

       /* Skip over the initial delimiter(s) */
       while (*extract_ptr == settings.includeDirDelim)
           extract_ptr++;

       while (extract_ptr < extract_end_ptr)
       {
           working_ptr = include_dir;
           /* Now extract the include directory*/
           while (*extract_ptr != settings.includeDirDelim)
           {
               *working_ptr++ = *extract_ptr++;
           }
           *working_ptr = '\0';    /* Nulll-terminate the string */
           extract_ptr++;    /* skip the delimiter */

           /* For now, we only accept absolute paths in the include file search path */
           if (include_dir[0] == '/')
           {
               DIR_addincdir(include_dir);
           }
           else
           {
               (void) fprintf(stderr, "Warning:  Relative 'Include Search Path' directories are not supported, skipping entry: %s\n", include_dir);
           }
       }
   }
}


/* Create a file-name keyed hash table that contains the offset into the (old) cross-reference file buffer
   for the named file's data section.  This is used by the incremental cref builder for data re-use. */
void DIR_create_offset_hash(char *buf_ptr)
{
    struct      offset_listitem *list_ptr;
    uint32_t    i;
    uint32_t    name_len;  
    char        *file_name;
    char        *offset_ptr;

    DIR_free_offset_hash(); /* (re)initialize the offset hash table */

    while ( TRUE )
    {
        /* Find the next file and offset - By definition, there are no duplicate file names */
        do
        {
            while ( *buf_ptr++ != '\t' );
        }
        while ( *buf_ptr++ != NEWFILE );

        if ( *buf_ptr == '\n')
        {
            /* we have reached the end of data */
            break;
        }

        offset_ptr = buf_ptr - 2;   /* Get the offset for this cref section */

        name_len = 0;
        while ( *buf_ptr++ != '\n')
        {
            name_len++;
        }

        buf_ptr -= (name_len + 1);  /* reset the pointer to the beginning of the file name string */

        /* Malloc a string buffer */
        file_name = g_malloc(name_len + 1);

        /* Malloc a list item */
        list_ptr = (struct offset_listitem *) g_malloc(sizeof(struct offset_listitem));

        list_ptr->text = file_name;

        /* Copy the filename string into the list item->text field */
        while ( *buf_ptr != '\n')
        {
            *file_name++ = *buf_ptr++;
        }
        *file_name = '\0';  /* Null terminate the string */


        /* Set the offset value */
        list_ptr->offset = offset_ptr;

        /* Insert the newly created listitem into the hash table. */
        i = hash(list_ptr->text);

        list_ptr->next = offset_hash_tbl[i];
        offset_hash_tbl[i] = list_ptr;
    }
}



void DIR_free_offset_hash()
{
    struct offset_listitem *p, *nextp;
    int i;

    for (i = 0; i < HASH_SIZE; ++i)
    {
        for (p = offset_hash_tbl[i]; p != NULL; p = nextp)
        {
            nextp = p->next;
            g_free(p->text);
            g_free(p);
        }
        offset_hash_tbl[i] = NULL;
    }
}



void DIR_free_src_names_hash()
{
    struct listitem *p, *nextp;
    int    i;

    for (i = 0; i < HASH_SIZE; ++i)
    {
        for (p = src_names_hash_tbl[i]; p != NULL; p = nextp)
        {
            nextp = p->next;
            /* free(p->text);       Do not free() the file-name strings, they are part of the
                                    master source file list which is utilized by search functions. */
            g_free((char *) p);
        }
        src_names_hash_tbl[i] = NULL;
    }
}


char *DIR_get_old_offset(char *filename)
{
    struct      offset_listitem *item_ptr;

    for (item_ptr = offset_hash_tbl[hash(filename)]; item_ptr != NULL; item_ptr = item_ptr->next)
    {
        if ( STREQUAL(filename, item_ptr->text) )
        {
            return(item_ptr->offset);
        }
    }
    return(NULL);
}



/* add a #include directory to the list */
void DIR_addincdir(char *path)
{
    struct  stat    statstruct;
    int     i;

    char *clean_path;

    clean_path = strdup( (char *) path);   // Malloc a duplicate 'path' string.
    compress_path(clean_path);             // Sanitize the 'path' string.

    /* make sure it is a directory */
    if ( (stat(clean_path, &statstruct) == 0) && (statstruct.st_mode & S_IFDIR) )
    {
        
        if (num_include_dirs == max_include_dirs)   /* If the include-directory list is full, make it larger */
        {
            max_include_dirs += DIRINC;
            include_dirs = (char **) g_realloc((char *) include_dirs, 
                                           max_include_dirs * sizeof(char *));
        }

        /* Check if 'clean_path' is already in the list */
        i = 0;
        while (i < num_include_dirs)
        {
            if ( strcmp(include_dirs[i++], clean_path) == 0)   /* duplicate found */
            {    
                free(clean_path); // Prevent memeory leak
                return;
            }
        }

        /* Not a duplicate entry, add it to the list */
        include_dirs[num_include_dirs++] = strdup(clean_path);
    }

    free(clean_path);
}



/* General method for joining user-configured and built-in lists */
/* usr_list is assumed to be a properly delimited gscope list (e.g. :entry1:entry2:) */

void DIR_list_join(char *usr_list, dir_list_e dir_list)
{
    guint usr_len;

    switch (dir_list)
    {
        case MASTER_IGNORED_LIST:
            if (master_ignored_list)
                free(master_ignored_list);  // Free any pre-existing list.

            usr_len = strlen(usr_list);

            if ( usr_len == 0 )  // usr_list is "empty" (null string)
            {
                master_ignored_list = g_malloc(sizeof(CSCOPE_BLD_DIR) + 2); // +2 for delimiters
                sprintf(master_ignored_list,":%s:", CSCOPE_BLD_DIR);        // Always ignore the autogen BUILD directory.
            }
            else  // usr_list is (a valid) not-empty list, append to it using the user-selected delimiter
            {
                master_ignored_list = g_malloc(usr_len + sizeof(CSCOPE_BLD_DIR) + 1);                   // +1 for delimiter
                sprintf(master_ignored_list,"%s%s%c", usr_list, CSCOPE_BLD_DIR, settings.ignoredDelim); // Always ignore the autogen BLD directory.
            }

            if ( !APP_CONFIG_valid_list("Master Ignored List", master_ignored_list, &master_ignored_delim) )
                 exit(EXIT_FAILURE);
        break;

        default:
            // do nothing - ignore unkown list types
        break;
    }
}





static void _alloc_src_file_list()
{
    if (DIR_src_files != NULL)      /* If the source file list is NOT empty*/
    {
        /* Free the existing source file list */
        while (nsrcfiles > 0)
        {
            g_free(DIR_src_files[--nsrcfiles]);
        }

        /* Re-initialize the source file list */
        memset(DIR_src_files, 0, sizeof(*DIR_src_files) * DIR_max_src_files);

        /* Re-use the existing, empty, source file list storage */
    }
    else
    {
        /* allocate storage for the source file list */
        DIR_src_files = (char **) g_malloc(DIR_max_src_files * sizeof(char *));
    }
    nsrcfiles = 0;


    /* Always (re)initialize the src_file hash table */
    DIR_free_src_names_hash();

}



//======================================================================
// Name:  _make_src_file_list()
//
// Description:  Create the Source-File list.
// 
//   Create the source file list using the following precedence:
//      1) Use the list of files supplied on the command line.
//      2) Use the list of files contained in a text file specifiec
//         on the command line (-i option).
//      3) Use the files present in srcDir (-S option, default is CWD)
//              if -R option is set
//                 Recursively search the srcDir
//              else
//                 flat-search srcDir
//       
//======================================================================

static void _make_src_file_list()
{
    DIR     *dirfile;                     /* directory file descriptor */
    struct  dirent  *entry;               /* directory entry pointer */
    FILE    *names;                       /* name file pointer */
    char    path[PATHLEN + 1];
    char    *file;
    int     i;
    char    *src_dir;

    #if (ANALYZE_HASH == 1)
    struct listitem *item_ptr;
    int depth;

    hash_collisions = 0;
    #endif


    if (settings.autoGenEnable)
        AUTOGEN_init( DIR_get_path(DIR_DATA) );


    /* If there are source file arguments */
    /*====================================*/
    if (fileargc > 0)
    {
        /* put them in a list that can be expanded */
        for (i = 0; i < fileargc; ++i)
        {
            file = strdup(fileargv[i]);     // protect fileargv[i] contents
            compress_path( (char *) file);  // file may be modified.

            if ( !infilelist(file) )
            {
                if (access(file, R_OK) == 0)    /* if file is found */
                    // Add the new filename string to the source file list
                    DIR_addsrcfile(file);
                else
                    fprintf(stderr, "Cannot find file %s\n", file);
            }
            free(file);
        }
        return;
    }



    /* Otherwise, if there is a file of source file names */
    /*====================================================*/
    if ( (settings.nameFile[0] != 0) && (!settings.recurseDir) )
    {
        if ((names = fopen(settings.nameFile, "r")) == NULL)
        {
            my_cannotopen(namefile);
            exit(EXIT_FAILURE);
        }
        /* get the names in the file */
        while (fscanf(names, "%s", path) == 1)      // Revisit:  Possible buffer overrun using fscanf()
        {
            compress_path(path);    // path may be modified.

            if (infilelist(path) == FALSE)
            {
                if (access(path, R_OK) == 0)    /* if file is found */
                    DIR_addsrcfile(path);
                else
                    fprintf(stderr, "Cannot find file %s\n", path);
            }
        }
        (void) fclose(names);
        return;
    }



    /*   Lastly, search for files in specified directories        */
    /*   =================================================        */
    /* Default:  Directory = CWD, Recursive Search = OFF          */
    /* Optional: Use specified directory and/or Recursive Search  */
    /*============================================================*/
    
    src_dir = DIR_get_path(DIR_SOURCE);

    /*** Now Perform the source file search ***/

    if (settings.recurseDir)    /* if recursive source file search is set   */
    {

        /* Walk the tree under <currentdir>, add "valid" source files
           to source file list - just like "filtered" -i processing */
        //printf("Recursively searching for source files under:\n    %s/...\nThis may take a moment.\n", src_dir);

        /* set up the recursive search log [if configured] */
        if (settings.searchLogging && (strlen(settings.searchLogFile) > 0) )
            if ( (rlogfile = fopen(settings.searchLogFile, "w")) == NULL )
                fprintf(stderr, "Error: Could not open Recursive Search log '%s' for writing\n", settings.searchLogFile);

        find_srcfiles_in_tree(src_dir);

        if ( (settings.searchLogging) && (rlogfile != NULL) )
            fclose(rlogfile);

    }
    else    /* non-recursive search of src_dir */
    {
        /* Open the directory.  Note: failure is allowed because srcdirs may not exist */
        if ( (dirfile = opendir(src_dir)) != NULL )
        {
            /* read each entry in the directory */
            while ((entry = readdir(dirfile)) != NULL)
            {
                /* if it is a source file not already found */
                file = entry->d_name;

                if (entry->d_ino != 0 && issrcfile(file) && infilelist(file) == FALSE)
                {

                    /* add it to the list */
                    (void) sprintf(path, "%s/%s", src_dir, file);
                    compress_path(path);
                    DIR_addsrcfile(path);
                }
            }
            closedir(dirfile);
        }
    }


    #if (ANALYZE_HASH == 1)
    printf("\n");
    for ( i = 0; i < HASH_SIZE; i++ )
    {
        depth = 0;

        if (src_names_hash_tbl[i])
        {
            depth++;

            item_ptr = src_names_hash_tbl[i];
            while (item_ptr)
            {
                item_ptr = item_ptr->next;
                depth++;
            }
        }
        printf("%d,", depth);
    }

    printf("File Count: %d  Hash Collisions: %d\n", nsrcfiles, hash_collisions);
    #endif
}





/* see if this is a source file */

static gboolean issrcfile(const char *file)
{
    char    pattern[MAX_SUFFIX + 3] = "";
    char    *pat_ptr;
    const char    *s;
    int     i, pattern_len;

    /* if there is a file suffix */
    if ((s = strrchr(file, '.')) != NULL && *++s != '\0')
    {
        /* construct a delimited pattern to search with */
        pat_ptr = &(pattern[0]);
        pattern_len = strlen(s);
        if (pattern_len > MAX_SUFFIX)
        {
            /* if the suffix pattern is longer than MAX_SUFFIX (gasp!), truncate it */
            pattern_len = MAX_SUFFIX;
        }
        *pat_ptr++ = settings.suffixDelim;
        for (i=0; i < pattern_len; i++)
        {
            *pat_ptr++ = *s++;
        }
        *pat_ptr++ = settings.suffixDelim;
        *pat_ptr++ = 0;

        /* search the suffix list */
        if ( (settings.suffixList[0] != '\0') && (strstr(settings.suffixList, pattern) ) )
        {
            return(TRUE);        /* suffix present, match */
        }
        else
            return(FALSE);         /* suffix present, no match */
    }

    /* no suffix - "typeless" file */
    
    s = &(file[0]);
    pat_ptr = &(pattern[0]);
    pattern_len = strlen(file);
    *pat_ptr++ = settings.typelessDelim;
    for (i=0; i < pattern_len; i++)
    {
        *pat_ptr++ = *s++;
    }
    *pat_ptr++ = settings.typelessDelim;
    *pat_ptr++ = 0;

    /* search the typeless file list */
    if ( (settings.typelessList[0] != '\0') && (strstr(settings.typelessList, pattern) ) )
    {
        return(TRUE);
    }
    else
        return(FALSE);
}




/* Walk the directory tree rooted at "." <current-dir> */

// **** WARNING:  The Green Hills version of ftw() on Linux is buggy [misses entire sub-directories]
//                Do not expect comprehensive source file detection when testing under Multi.
//
// Revisit:  ftw() will eventually be obsolete.  Need to migrate our 'Tree Walker' to either nftw() or fts().
//           As of mid 2015 - fts() seems to be the more favored approach.

static void find_srcfiles_in_tree(gchar *src_dir)
{
    /* Temporarily set the CWD for this process to match the user-specified source "root" directory */
    /* It seems a little silly to do this, but things stay much simpler if we only use the "." form */
    /* of the ftw() function call */
    chdir(src_dir);

    /* walk the tree & build source file list */
    if ( ftw(".", list, 1) < 0 )
    {
        char message[512];

        sprintf(message,"\nG-Scope Error: Recursive File Tree Walk Error: %s", strerror(errno));

        if ( !settings.refOnly )  // If we are in GUI mode
            DISPLAY_msg(GTK_MESSAGE_ERROR, message);
        else
            fprintf(stderr, "%s\n", message);
    }

    /* Pop back to the original CWD */
    chdir( DIR_get_path(DIR_CURRENT_WORKING) );

    return;
}


static int list(const char *name, const struct stat *status, int type) 
{
    char *base_name;

    if(type == FTW_NS)     /* non stat-able file */
    {
        /* Quietly skip any nan stat-able file */
        // fprintf(stderr, "Error: Cannot stat: %s ignored [probably a broken symlink]\n", name);
        return 0;
    }

    if(type == FTW_F || type == FTW_SL)  /* if we have a file, or symlink */
    {
        if  ( path_check_ok( name ) )   /* if path is not on the exclusion list */
        {
            base_name = my_basename( (char *) name );
            if ( issrcfile( base_name ) )   /* is this a source file */
            {
                 if (settings.searchLogging) fprintf(rlogfile,"%s\n", name);
                 DIR_addsrcfile( (char *) name);
            }
        }
    }
    
    /* ignore readable & unreadable directories */
    return 0;
}



/* Given a file or symlink path of ./dir/... return TRUE if 'dir'
   or any subdirectory of 'dir' is not on the directory exclusion
   list.  Return FALSE if 'dir', or any subdirectory is found in the
   exclusion list.
 
   Examples: 

   Assuming ignoredirList= ":root:root1:root2:root3:"
   
   ./root2 = no match (file - not a dir),  return TRUE
   ./root3/... = root3  match,  return FALSE
   ./root/sub1/sub2 = root match, return FALSE
   ./subdir/root3 = match  (subdir root3 in match-table), return FALSE   */

static gboolean path_check_ok(const char *file)
{
    #define MAX_DIRNAME 160
    char pattern[MAX_DIRNAME + 3] = "";
    char *pat_ptr;

    const char *src;
    int i;

    src = &(file[2]);   /* skip the leading "./" chars */

    if (master_ignored_list[0] != '\0')    // Scan the (non-empty) master ignore list  (User list + built-in list)
    {
        while (TRUE) 
        {
            i = 0;
            pat_ptr = &(pattern[0]);
            *pat_ptr++ = master_ignored_delim;

            /* Get the next dirname [or filename] */
            while ( (*src != '/') && (*src != 0) && (i++ < MAX_DIRNAME) )
                *pat_ptr++ = *src++;

            if (*src != '/')  /* top level file [or DIRNAME too large] */
                break;

            /* If we make it here, we have a dirname to check */
            *pat_ptr++ = master_ignored_delim;
            *pat_ptr++ = 0;

            if ( strstr(master_ignored_list, pattern) )
            {
                /* We found a dirname match in the exclusion list */
                return(FALSE);
            }
            src++;   /* skip the '/' char */
        }
    }

    return (TRUE); 
}



/* add an include file to the source file list */

void DIR_incfile(char *file)
{
    char    path[PATHLEN + 1];
    int     i;
    char    *src_dir;
    char    *clean_name;

    clean_name = strdup(file);
    compress_path(clean_name);    // warning: compress_path might modify 'file'


    if ( infilelist(clean_name) ) 
    {
        free(clean_name);
        return;   // If the file is already in the list, no further action is required.
    }

    // Find the file using this algorithm"
    // If 'file' specifies an absolute path, check that path only.
    // If 'file' specifies a relative path:
    //      1)  First, check for the relative file name based off of src_dir
    //      2)  otherwise, search for the relative file name based of of each entry
    //          in the include-file search-list.

    if ( *clean_name == '/')      // Is this an absolute path?
    {
        if (access(clean_name, R_OK) == 0)  // File found
        {
            /* We already know that 'file' is not in the list, so just add it */
            DIR_addsrcfile(clean_name);
        }
    }
    else        // 'file' is a relative path
    {
        /* First look in source_dir */
        src_dir = DIR_get_path(DIR_SOURCE);
        sprintf(path, "%s/%s", src_dir, clean_name);
        if ( (access(compress_path(path), R_OK) == 0) && (!infilelist(clean_name)) )
        {
            DIR_addsrcfile(clean_name);   // yes, use 'file', not 'path' -- keep the name "relative"
        }
        else
        {
            /* Nothing found in source_dir, check the "include" search path */
            for (i = 0; i < num_include_dirs; ++i)
            {
                sprintf(path, "%s/%s", include_dirs[i], clean_name);
                if ( access(compress_path(path), R_OK) == 0 )
                {
                     if (!infilelist(path))
                     {
                         DIR_addsrcfile(path);     // Must use 'path', not 'file'
                         break;
                     }
                     else
                     {
                         break;
                     }
                }
            }
        }
    }
    free(clean_name);
}

/* see if the file is already in the list */

static gboolean infilelist(const char *file)
{
    struct  listitem *p;

    for (p = src_names_hash_tbl[hash(file)]; p != NULL; p = p->next)
    {
        if ( STREQUAL(file, p->text) )
        {
            return(TRUE);
        }
    }
    return(FALSE);
}


/**
 * Checks to see if filename inputted is a proto file
 */
static gboolean is_protobuf_file(const char *filename)
{
    char *ptr;

    ptr = strrchr(filename, (int) '.');

    if (ptr == NULL) return (FALSE);

    // Performance trick - check the first char after the '.' with the second char of pattern to avoid
    //                     unneccessary strcmp() calls
    if ( (*(ptr + 1) == settings.autoGenSuffix[1]) && (strcmp(ptr, settings.autoGenSuffix) == 0) )
        return (TRUE);
    else
        return(FALSE);
}



//********************************************************************
//
// Wrapper function to add a single, or multiple (iterated name files)
// to the source file list
// 
//********************************************************************
void DIR_addsrcfile(char *name)
{
    char    *clean_name;
    char    *tmp_name; // Modified version of "name".
    char    synthetic_name[PATHLEN + sizeof(CSCOPE_GEN_DIR) + sizeof(settings.autoGenId) + 3];   // +2 for '.c' or '.h', +1 for null
    char    full_path[PATHLEN*2 + sizeof(CSCOPE_GEN_DIR) + sizeof(settings.autoGenId) + 3]; 
    char    *work_ptr;
    struct stat statstruct;

    clean_name = strdup( (char *) name);    // Malloc a duplicate 'name' string
    compress_path(clean_name);              // Sanitize the name string (not compression).  Warning: This call may alter clean_name

    /* add the file to the list */
    add_src_primitive(clean_name);

    // Add Proto-buffer generated source files  <filename><genId>.[ch]
    if ( !settings.noBuild && settings.autoGenEnable && is_protobuf_file(clean_name) )
    {
        // add file to auto_gen
        AUTOGEN_addproto(clean_name);

        // Remove ".proto" from name
        tmp_name = strdup(clean_name);
        work_ptr = strrchr(tmp_name, '.');
        if (work_ptr)
            *work_ptr = '\0';

        sprintf(synthetic_name, "%s/%s%s.c", CSCOPE_GEN_DIR, tmp_name, settings.autoGenId);
        sprintf(full_path, "%s/%s", DIR_get_path(DIR_DATA), synthetic_name);
        
        // Adds compiled output files before they are created if they do not already exist
        if (stat(full_path, &statstruct) != 0)
        {
            // add .c file
            add_src_primitive( strdup(synthetic_name) );

            // add .h file
            work_ptr = strrchr(synthetic_name,'.');
            if (work_ptr)
            {
                *(++work_ptr) = 'h'; 
                add_src_primitive( strdup(synthetic_name) );
            }
        }
        free(tmp_name);
    }
}



//********************************************************************
//
// Do not call this function directly.  It exist for the sole purpose
// of allowing DIR_addsrcfile() to iterate over a given filename.  If
// you want to add a source file to the list, call DIR_addsrcfile()
// NOT THIS function.
// 
//********************************************************************
static void add_src_primitive(char *name)
{
    struct  listitem *p;
    int i;

    /* make sure there is room for the file */
    if (nsrcfiles == DIR_max_src_files)
    {
        DIR_max_src_files += SRC_GROW_INCREMENT;
        DIR_src_files = (char **) g_realloc((char *) DIR_src_files, DIR_max_src_files * sizeof(char *));
    }

    p = (struct listitem *) g_malloc(sizeof(struct listitem));


    DIR_src_files[nsrcfiles++] = name;


    p->text = name;

    i = hash(p->text);

    #if (HASH_ANALASYS == 1)
    if (src_names_hash_tbl[i] != NULL) 
        hash_collisions++;
    #endif

    /* insert the newly created listitem into the hash table. */
    p->next = src_names_hash_tbl[i];
    src_names_hash_tbl[i] = p;
}



/* 
 * Determine if the file (passed via parameter *srcfile) is on the include file search
 *  path (by using the global include file directories information).
 */
gboolean DIR_file_on_include_search_path(gchar *srcfile)
{
    // num_include_dirs:            Global.  This is a count of include file directories.
    // include_dirs[num_include_dirs]:  Global,  An array of include file directory names.
    int i;

    for (i = 0; i < num_include_dirs; i++)
    {
        if ( strncmp(srcfile, include_dirs[i], strlen(include_dirs[i])) == 0 )
            return(TRUE);
    }
    
    return(FALSE);
}



/*
 *	compress_path(pathname)
 *
 *	This function compresses pathname.  All strings of multiple slashes are
 *	changed to a single slash.  All occurrences of "./" are removed.
 *	Whenever possible, strings of "/.." are removed together with
 *	the directory names that they follow.
 *
 *  WARNING: The string passed via parameter 'pathname' is altered by
 *           this function.  The caller should save a duplicate copy
 *           of the original string if that string needs to be
 *           preserved.
 */
static char * compress_path(char *pathname)
{
	register char	*nextchar;
	register char	*lastchar;
	char	*sofar;
	char	*pnend;

	int	pnlen;

		/*
		 *	do not change the path if it has no "/"
		 */

	if (strchr(pathname, '/') == 0)
		return(pathname);

		/*
		 *	find all strings consisting of more than one '/'
		 */

	for (lastchar = pathname + 1; *lastchar != '\0'; lastchar++)
		if ((*lastchar == '/') && (*(lastchar - 1) == '/'))
		{

			/*
			 *	find the character after the last slash
			 */

			nextchar = lastchar;
			while (*++lastchar == '/')
			{
			}

			/*
			 *	eliminate the extra slashes by copying
			 *	everything after the slashes over the slashes
			 */

			sofar = nextchar;
			while ((*nextchar++ = *lastchar++) != '\0')
				;
			lastchar = sofar;
		}

		/*
		 *	find all strings of "./"
		 */

	for (lastchar = pathname + 1; *lastchar != '\0'; lastchar++)
		if ((*lastchar == '/') && (*(lastchar - 1) == '.') &&
		    ((lastchar - 1 == pathname) || (*(lastchar - 2) == '/')))
		{

			/*
			 *	copy everything after the "./" over the "./"
			 */

			nextchar = lastchar - 1;
			sofar = nextchar;
			while ((*nextchar++ = *++lastchar) != '\0')
				;
			lastchar = sofar;
		}

		/*
		 *	find each occurrence of "/.."
		 */

	for (lastchar = pathname + 1; *lastchar != '\0'; lastchar++)
		if ((lastchar != pathname) && (*lastchar == '/') &&
		    (*(lastchar + 1) == '.') && (*(lastchar + 2) == '.') &&
		    ((*(lastchar + 3) == '/') || (*(lastchar + 3) == '\0')))
		{

			/*
			 *	find the directory name preceding the "/.."
			 */

			nextchar = lastchar - 1;
			while ((nextchar != pathname) &&
			    (*(nextchar - 1) != '/'))
				--nextchar;

			/*
			 *	make sure the preceding directory's name
			 *	is not "." or ".."
			 */

			if ((*nextchar == '.') &&
			    (*(nextchar + 1) == '/') ||
			    ((*(nextchar + 1) == '.') && (*(nextchar + 2) == '/')))
				/* EMPTY */;
			else
			{

				/*
				 * 	prepare to eliminate either
				 *	"dir_name/../" or "dir_name/.."
				 */

				if (*(lastchar + 3) == '/')
					lastchar += 4;
				else
					lastchar += 3;

				/*
				 *	copy everything after the "/.." to
				 *	before the preceding directory name
				 */

				sofar = nextchar - 1;
				while ((*nextchar++ = *lastchar++) != '\0');
					
				lastchar = sofar;

				/*
				 *	if the character before what was taken
				 *	out is '/', set up to check if the
				 *	slash is part of "/.."
				 */

				if ((sofar + 1 != pathname) && (*sofar == '/'))
					--lastchar;
			}
		}

	/*
 	 *	if the string is more than a character long and ends
	 *	in '/', eliminate the '/'.
	 */

	pnlen = strlen(pathname);
	pnend = strchr(pathname, '\0') - 1;

	if ((pnlen > 1) && (*pnend == '/'))
	{
		*pnend-- = '\0';
		pnlen--;
	}

	/*
	 *	if the string has more than two characters and ends in
	 *	"/.", remove the "/.".
	 */

	if ((pnlen > 2) && (*(pnend - 1) == '/') && (*pnend == '.'))
		*--pnend = '\0';

	/*
	 *	if all characters were deleted, return ".";
	 *	otherwise return pathname
	 */

	if (*pathname == '\0')
		(void) strcpy(pathname, ".");

	return(pathname);
}



#if (OLD_HASH == 1)
static int  hash(const char *ss)
{
	int	i;
	unsigned char 	*s = (unsigned char *)ss;
	
	for (i = 0; *s != '\0'; )
		i += *s++;	/* += is faster than <<= for cscope */
	return(i % HASH_SIZE);
}

#else

#define HASH_INIT   0x8421      /* Any arbitrary 4-byte value */
static uint32_t hash(const char *key)
{
    uint32_t hash_val;

    hash_val = hashlittle( (const void *) key, strlen(key), HASH_INIT);

    return(hash_val & HASH_MASK);
}

#endif



/*
-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.

These are functions for producing 32-bit hashes for hash table lookup.
hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final() 
are externally useful functions.  Routines to test the hash are included 
if SELF_TEST is defined.  You can use this free for any purpose.  It's in
the public domain.  It has no warranty.

You probably want to use hashlittle().  hashlittle() and hashbig()
hash byte arrays.  hashlittle() is is faster than hashbig() on
little-endian machines.  Intel and AMD are little-endian machines.
On second thought, you probably want hashlittle2(), which is identical to
hashlittle() except it returns two 32-bit hashes for the price of one.  
You could implement hashbig2() if you wanted but I haven't bothered here.

If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
then use c as the hash value.  If you have a variable length array of
4-byte integers to hash, use hashword().  If you have a byte array (like
a character string), use hashlittle().  If you have several byte arrays, or
a mix of things, see the comments above hashlittle().  

Why is this so big?  I read 12 bytes at a time into 3 4-byte integers, 
then mix those integers.  This is fast (you can do a lot more thorough
mixing with 12*3 instructions on 3 integers than you can with 3 instructions
on 1 byte), but shoehorning those bytes into integers efficiently is messy.
-------------------------------------------------------------------------------
*/

/*
-------------------------------------------------------------------------------
hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Two keys differing by one or two bits will have
totally different hash values.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
-------------------------------------------------------------------------------
*/

#if ( OLD_HASH != 1 )

static uint32_t hashlittle( const void *key, size_t length, uint32_t initval)
{
  uint32_t a,b,c;                                          /* internal state */
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32_t *k = (const uint32_t *)key;         /* read 32-bit chunks */
    //const uint8_t  *k8;

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /* 
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef VALGRIND

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : return c;              /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    k8 = (const uint8_t *)k;
    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((uint32_t)k8[10])<<16;  /* fall through */
    case 10: c+=((uint32_t)k8[9])<<8;    /* fall through */
    case 9 : c+=k8[8];                   /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((uint32_t)k8[6])<<16;   /* fall through */
    case 6 : b+=((uint32_t)k8[5])<<8;    /* fall through */
    case 5 : b+=k8[4];                   /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((uint32_t)k8[2])<<16;   /* fall through */
    case 2 : a+=((uint32_t)k8[1])<<8;    /* fall through */
    case 1 : a+=k8[0]; break;
    case 0 : return c;
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const uint16_t *k = (const uint16_t *)key;         /* read 16-bit chunks */
    const uint8_t  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((uint32_t)k[1])<<16);
      b += k[2] + (((uint32_t)k[3])<<16);
      c += k[4] + (((uint32_t)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8_t *)k;
    switch(length)
    {
    case 12: c+=k[4]+(((uint32_t)k[5])<<16);
             b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 11: c+=((uint32_t)k8[10])<<16;     /* fall through */
    case 10: c+=k[4];
             b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* fall through */
    case 8 : b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 7 : b+=((uint32_t)k8[6])<<16;      /* fall through */
    case 6 : b+=k[2];
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* fall through */
    case 4 : a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 3 : a+=((uint32_t)k8[2])<<16;      /* fall through */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return c;                     /* zero length requires no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const uint8_t *k = (const uint8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((uint32_t)k[1])<<8;
      a += ((uint32_t)k[2])<<16;
      a += ((uint32_t)k[3])<<24;
      b += k[4];
      b += ((uint32_t)k[5])<<8;
      b += ((uint32_t)k[6])<<16;
      b += ((uint32_t)k[7])<<24;
      c += k[8];
      c += ((uint32_t)k[9])<<8;
      c += ((uint32_t)k[10])<<16;
      c += ((uint32_t)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=((uint32_t)k[11])<<24;
    case 11: c+=((uint32_t)k[10])<<16;
    case 10: c+=((uint32_t)k[9])<<8;
    case 9 : c+=k[8];
    case 8 : b+=((uint32_t)k[7])<<24;
    case 7 : b+=((uint32_t)k[6])<<16;
    case 6 : b+=((uint32_t)k[5])<<8;
    case 5 : b+=k[4];
    case 4 : a+=((uint32_t)k[3])<<24;
    case 3 : a+=((uint32_t)k[2])<<16;
    case 2 : a+=((uint32_t)k[1])<<8;
    case 1 : a+=k[0];
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}

#endif

