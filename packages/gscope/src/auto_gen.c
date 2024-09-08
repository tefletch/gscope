// Gscope auto source file generation routines
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <ftw.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "dir.h"
#include "utils.h"
#include "auto_gen.h"
#include "search.h"
#include "display.h"
#include "app_config.h"

//===============================================================
//       Defines
//===============================================================

#define GEN_PREFIX      "autogen_"
#define BLD_PREFIX      "autobld_"
#define MAX_USER_SIZE   40          /* My condolences if your have a username anywhere near 40 chars */

//===============================================================
//       Local Type Definitions
//===============================================================

typedef struct
{
    char        *true_basename;     //Real basename
    char        *unique_basename;   //Modified basename if basename already exists. Same as true_basename if name is unique
    char        *unique_dirname;    //Directory path to the unique file
    gboolean    exists;             //The proto file is still present in the file system
} file_info_t;

//===============================================================
//       Public Global Variables
//===============================================================

time_t      autogen_elapsed_sec;                    //Holds the elapsed seconds
suseconds_t autogen_elapsed_usec;                   //Holds the elapsed milliseconds

//===============================================================
//       Private Global Variables
//===============================================================

static int          autogen_src_grow_incrmt = 200;  //Size to grow source file list if needed
static int          autogen_max_src_files = 400;    //Max number of proto files (scalable)

static file_info_t  *file_info_list;                //Holds file info for all proto files. Look at file_info_t structure.
static int          nfile_info;                     //Number of file_info structures in the struct list;
static proto_compile_stats_t pc_stats;              //Proto Buffer Header auto-gen compiler statistics

struct      timeval autogen_time_start, autogen_time_stop; //For timing of the autogen functions

//===============================================================
//       Private Function Prototypes
//===============================================================

static void     _mkdir_all(const char *dir);
static gboolean _protobuf_csrc(const char *filename, char *data_dir);
static gboolean _is_protobuf_file(const char *filename);
static void     _remove_old_symlinks(char *data_dir);
static void     _do_garbage_collection(void);
static int      _my_compare();      // for qsort
static int      recursive_remove(char *path);

//===============================================================
//      Public Functions
//===============================================================

/**
 * Creates symlinks to build(symlinks to uncompiled files) and generation(compiled files) directories.
 * Then initializes storage as neccessary and populates file_info_list with compiled files from
 * previous sessions.
 */
void AUTOGEN_init(char *data_dir)
{
    ssize_t num_bytes;
    char    *extract_ptr;
    char    gen_symlink_path[PATHLEN + 10];                     // +10 for literal chars and null-terminator
    char    bld_symlink_path[PATHLEN + 10];
    char    euid[MAX_STRING_ARG_SIZE];                          // Per session/direAUTOGEN_initctory unique ID.
    char    username[MAX_USER_SIZE + 1];
    char    link_dest[PATHLEN + PATHLEN + MAX_USER_SIZE + sizeof(BLD_PREFIX) + sizeof(euid) + 20];   // +20 for literal chars and null-terminator

    DIR     *dir;
    struct  dirent *ent;
    char    file_path[PATHLEN]; //Working copy of full path
    char    *real_path;

    gettimeofday(&autogen_time_start, NULL);
    snprintf(username, MAX_USER_SIZE + 1, "%s", getenv("USER"));     // Truncate any username > MAX_USER_SIZE

    /*** Initialize "auto generated" symlink and destination ***/
    /***********************************************************/
    sprintf(gen_symlink_path, "%s/%s", data_dir, GSCOPE_GEN_DIR);
    num_bytes = readlink(gen_symlink_path, link_dest, PATHLEN - 1);
    if (num_bytes < 0)  // Symlink does not exist
    {
        // Create new EUID
        sprintf(euid, "%ld", time(NULL));

        // Create new "auto generated" symlink and destination
        sprintf (link_dest,"%s/%s/%s%s", settings.autoGenPath, username, GEN_PREFIX, euid);
        _mkdir_all(link_dest);
        if ( symlink (link_dest , gen_symlink_path) < 0 )
        {
            fprintf(stderr, "Fatal Error: Symlink symlnk() failed: old=%s new=%s\n%s\n", link_dest, gen_symlink_path, strerror(errno));
            //exit(EXIT_FAILURE);
        }

        _do_garbage_collection();  // Every time we create a new cache directory, manage the overall collection of caches.
    }
    else            // Symlink exists
    {
        link_dest[num_bytes] = '\0';

        // Extract the pre-existing EUID
        extract_ptr = strrchr(link_dest, '_');
        extract_ptr++;
        strcpy(euid, extract_ptr);

        if ( access(link_dest, X_OK) < 0 )       // Symlink exists, but symlink destination does not exist
        {
            switch (errno)
            {
                case ENOENT:
                    // (Re)Create destination
                    _mkdir_all(link_dest);
                break;

                default:
                    // Handle (unlikely) permission errors (and other errors)
                    fprintf(stderr, "1Fatal Error: Cannot access autogen cache directory: %s\n%s\n", link_dest, strerror(errno));
                    exit(EXIT_FAILURE);
                break;
            }
        }
    }


    /*** Initialize "auto build" symlink and destination ***/
    /*******************************************************/
    sprintf(bld_symlink_path, "%s/%s", data_dir, GSCOPE_BLD_DIR);
    num_bytes = readlink(bld_symlink_path, link_dest, PATHLEN - 1);
    if (num_bytes < 0)  // Symlink does not exist
    {
        // euid (new, or extracted) has already been set

        // Create new "auto build" symlink and destination
        sprintf (link_dest,"%s/%s/%s%s", settings.autoGenPath, username, BLD_PREFIX, euid);
        _mkdir_all(link_dest);
        if ( symlink (link_dest, bld_symlink_path) < 0 )
        {
            fprintf(stderr, "2Fatal Error: Symlink symlnk() failed: old=%s new=%s\n%s\n", link_dest, gen_symlink_path, strerror(errno));
            //exit(EXIT_FAILURE);
        }
    }
    else            // Symlink exists
    {
        link_dest[num_bytes] = '\0';

        if ( access(link_dest, X_OK) < 0 )       // Symlink exists, but symlink destination does not exist
        {
            switch (errno)
            {
                case ENOENT:
                    // (Re)Create destination
                    _mkdir_all(link_dest);
                break;

                default:
                    // Handle (unlikely) permission errors (and other errors)
                    fprintf(stderr, "Fatal Error: Cannot access autogen cache directory: %s\n%s\n", link_dest, strerror(errno));
                    exit(EXIT_FAILURE);
                break;
            }
        }
    }


    /*** Allocate file-info list or re-initialize the already allocated storage ***/
    /******************************************************************************/
    if (file_info_list != NULL)         /* If storage for the file-info list is already allocated */
    {
        /* Free the existing file-info entries */
        while (nfile_info > 0)
        {
            --nfile_info;
            g_free(file_info_list[nfile_info].unique_basename);
            g_free(file_info_list[nfile_info].true_basename);
            g_free(file_info_list[nfile_info].unique_dirname);
        }
        /* Re-use the existing source file list storage */
    }
    else    // No storage allocated yet.
    {
        file_info_list = (file_info_t *) g_malloc(autogen_max_src_files * sizeof(file_info_t));  // Get some
    }

    /* ALWAYS initialize the file-info list storage (g_malloc() does not guarantee zero'd memory) */
    memset(file_info_list, 0, autogen_max_src_files * sizeof(file_info_t));
    nfile_info = 0;
    gettimeofday(&autogen_time_start, NULL);


    /*** Update the file-info list with information extracted from a pre-existing build directory ***/
    /************************************************************************************************/
    if ((dir = opendir(bld_symlink_path)) == NULL)
    {
        /* could not open directory */
        fprintf(stderr, "ERROR: Failed to open auto_gen BUILD directory[%s]: %s\n", bld_symlink_path, strerror(errno));
        exit(EXIT_FAILURE);
    }

    pc_stats.num_proto_files = 0;

    /*** Loop through all the files in the BUILD directory ***/
    /* If "force rebuild" is checked, unlink all symlinks. Else, repopulate file_info_list with current files */
    while ((ent = readdir (dir)) != NULL)
    {
        if (_is_protobuf_file(ent->d_name))
        {
            pc_stats.num_proto_files++;

            sprintf(file_path, "%s/%s/%s", data_dir, GSCOPE_BLD_DIR, ent->d_name);
            real_path = realpath(file_path, NULL);
            if (settings.updateAll) // Unlink all symlinks
            {
                if (unlink(file_path) != 0)
                {
                    fprintf(stderr, "Error: Cannot remove symlink for force rebuild [%s]", file_path);
                    exit(EXIT_FAILURE);
                }
            }
            else // Repopulate file_info_list with current symlinks
            {
                // If out of space, grow the file-info list
                if (nfile_info == autogen_max_src_files)
                {
                    autogen_max_src_files += autogen_src_grow_incrmt;
                    file_info_list = (file_info_t *) g_realloc(file_info_list, autogen_max_src_files * sizeof(file_info_t));
                }

                if (!real_path){ // Skips file if error in finding symlink target. Likely due to file removed
                    file_info_list[nfile_info].unique_basename = strdup(ent->d_name); // Need unique basename in _remove_old_symlinks()
                    file_info_list[nfile_info].true_basename   = strdup("");
                    file_info_list[nfile_info].unique_dirname  = strdup("");
                    file_info_list[nfile_info].exists = FALSE; // Causes symlink to be removed later
                    nfile_info++;
                    free(real_path);

                    continue;
                }

                file_info_list[nfile_info].unique_basename = strdup(ent->d_name);
                file_info_list[nfile_info].true_basename   = strdup(my_basename(real_path));
                file_info_list[nfile_info].unique_dirname  = strdup(my_dirname(real_path + strlen(data_dir) + 1));

                nfile_info++;
            }
            free(real_path);
        }
    }
    closedir (dir);

    gettimeofday(&autogen_time_stop, NULL);

    /* Timing calculations: calculate milliseconds */
    if (autogen_time_stop.tv_usec >= autogen_time_start.tv_usec)
        autogen_elapsed_usec += autogen_time_stop.tv_usec - autogen_time_start.tv_usec;
    else
    {
        autogen_elapsed_usec += (1000000 + autogen_time_stop.tv_usec) - autogen_time_start.tv_usec;
        autogen_time_stop.tv_sec--;
    }
    autogen_elapsed_sec += autogen_time_stop.tv_sec - autogen_time_start.tv_sec;
}


/**
 * Main function to run compilation process of the user specified files */
void AUTOGEN_run(char *data_dir)
{
    struct  stat statstruct;
    int     src_file_index;     //Index of autogen_src_files
    time_t  compile_time;       //Last time file was compiled
    gboolean no_compile_flag;   //Indicates if compiled .pb-c.c can not be found

    char    full_path_buf[PATHLEN + sizeof(GSCOPE_BLD_DIR) + PATHLEN + 10];     // +10 for literal chars and null-terminator
    char    file_path_buf[PATHLEN + PATHLEN + 10];
    char    compiledBuf  [PATHLEN + sizeof(GSCOPE_GEN_DIR) + PATHLEN + PATHLEN + 10]; //Path to the compiled .pb-c.c file

    gettimeofday(&autogen_time_start, NULL);

    no_compile_flag = FALSE;
    src_file_index = 0;
    pc_stats.proto_build_success = 0;
    pc_stats.proto_build_failed = 0;
    pc_stats.num_proto_changed = 0;

    /* Remove symlinks to files deleted by users in between sessions */
    _remove_old_symlinks(data_dir);

    /*** Run compilation on each .proto file if not up to date ***/
    /*************************************************************/
    while (src_file_index < nfile_info)
    {
        /* <directory>/<file.proto> */
        sprintf(file_path_buf, "%s/%s", file_info_list[src_file_index].unique_dirname, file_info_list[src_file_index].unique_basename);

        /* .../GSCOPE_BLD_DIR/<symlink to .proto> */
        sprintf(full_path_buf, "%s/%s/%s",
                               data_dir,
                               GSCOPE_BLD_DIR,
                               file_info_list[src_file_index].unique_basename);

        /* .../GSCOPE_GEN_DIR/<directory>/<file.proto> */
        sprintf(compiledBuf, "%s/%s/%s/%s",
                             data_dir,
                             GSCOPE_GEN_DIR,
                             file_info_list[src_file_index].unique_dirname,
                             file_info_list[src_file_index].true_basename);

        /* .../GSCOPE_GEN_DIR/<directory>/<file.pb-c.c> */
        sprintf(strstr(compiledBuf, settings.autoGenSuffix), "%s%s", settings.autoGenId,".c");

        /* Checks to not compile file if it is already up to date */
        if ( (stat(compiledBuf, &statstruct) != 0) )
        {
            no_compile_flag = TRUE;
            //fprintf(stderr, "Could not find compiled file: %s\n", compiledBuf);
        }
        compile_time = statstruct.st_mtime;

        if ( (stat(full_path_buf, &statstruct) != 0) )
        {
            fprintf(stderr, "Fatal Error: Meta-source file [%s] is missing.  Try running Gscope again.\n", full_path_buf);
            exit(EXIT_FAILURE); // File disappeared during processing???
        }

        /*** proto file compile phase ****/
        // If the proto file is newer than the cross reference
        if (difftime(statstruct.st_mtime, compile_time) > 0 || no_compile_flag || settings.updateAll)
        {
            pc_stats.num_proto_changed++;

            if ( _protobuf_csrc(file_path_buf, data_dir) )
                pc_stats.proto_build_success++;
            else
                pc_stats.proto_build_failed++;
        }
        src_file_index++;
    }
    gettimeofday(&autogen_time_stop, NULL);

    /* Timing calculations: calculate milliseconds */
    if (autogen_time_stop.tv_usec >= autogen_time_start.tv_usec)
        autogen_elapsed_usec += autogen_time_stop.tv_usec - autogen_time_start.tv_usec;
    else
    {
        autogen_elapsed_usec += (1000000 + autogen_time_stop.tv_usec) - autogen_time_start.tv_usec;
        autogen_time_stop.tv_sec--;
    }
    autogen_elapsed_sec += autogen_time_stop.tv_sec - autogen_time_start.tv_sec;

    return;
}

/**
 * Add .proto files found from ftw() to file_info_list
 */
void AUTOGEN_addproto(char* name)
{
    char    *baseName;
    char    *dirName;
    char    *cwd;

    char    build_link_src [PATHLEN + 10];      // +10 for literal chars and null-terminator
    char    build_link_dest[PATHLEN + 10];

    int     uid;
    int     file_info_index;


    baseName = my_basename(name);
    dirName  = my_dirname(strdup(name));

    gettimeofday(&autogen_time_start, NULL);

    /*** If a valid symlink for the file already exists, do nothing ***/
    /******************************************************************/
    for (file_info_index = 0; file_info_index < nfile_info; file_info_index++)
    {
        //If directories are equal...
        if (strcmp(dirName, file_info_list[file_info_index].unique_dirname) == 0)
        {
            //...check if names are equal
            if (strcmp(baseName, file_info_list[file_info_index].true_basename) == 0)
            {
                /* Symlink already exists */
                file_info_list[file_info_index].exists = TRUE;

                free(dirName);  /* Don't leak... */
                return;
            }
        }
    }


    /*** If a valid symlink does not already exist, we need to add the new file ***/
    /******************************************************************************/

    pc_stats.num_proto_files++;

    if (nfile_info == autogen_max_src_files)    // If the file-info storage area is full, grow the table.
    {
        autogen_max_src_files += autogen_src_grow_incrmt;
        file_info_list = (file_info_t *) g_realloc(file_info_list, autogen_max_src_files * sizeof(file_info_t));
        // Reminder:  Any new memory allocated is un-initialized.
    }

    // Record the true basename and unique dirname.
    file_info_list[nfile_info].true_basename = strdup(baseName);    // Real basename
    file_info_list[nfile_info].unique_dirname = dirName;            // dirName is already malloc'd


    // If this bassename already exists in the build directory, create a synthetic
    // "unique" base name and use it as the name for the new symlink.
    sprintf(build_link_src, "%s/%s", GSCOPE_BLD_DIR, baseName);

    if ( access(build_link_src, F_OK) == -1 )
    {
        /* File (link) doesn't exist - create symlink (unique_basename == true_basename) */

        file_info_list[nfile_info].unique_basename = strdup(baseName);  // Record the unique basename
    }
    else
    {
        /* File (link) exists, synthesize a new, unique basename */
        uid = 0;
        do
        {
            sprintf(build_link_src, "%s/%s", GSCOPE_BLD_DIR, baseName);
            sprintf(strstr(build_link_src, settings.autoGenSuffix), "__%d%s", ++uid, settings.autoGenSuffix);
        }
        while ( access(build_link_src, F_OK) == 0 );

        file_info_list[nfile_info].unique_basename = strdup(my_basename(build_link_src));
    }

    // If file is in the "root" of the source tree, do not include directory in path
    if( strcmp(file_info_list[nfile_info].unique_dirname, "") == 0 )
    {
        sprintf(build_link_dest, "%s/%s", cwd = getcwd(NULL, 0), baseName);
        free(cwd);
    }
    else
    {
        if (dirName[0] == '/')
        {
            // dirName is an absolute path, use it as-is
            sprintf(build_link_dest, "%s/%s", dirName, baseName);
        }
        else
        {
            // dirName is a relative path, make it absolute
            sprintf(build_link_dest, "%s/%s/%s", cwd = getcwd(NULL, 0), dirName, baseName);
            free(cwd);
        }
    }

    if ( symlink(build_link_dest, build_link_src) < 0 )       // Create the new "build file" symlink
    {
        fprintf(stderr, "Fatal Error: 3Symlink symlnk() failed: old=%s new=%s\n%s\n", build_link_dest, build_link_src, strerror(errno));
        //exit(EXIT_FAILURE);
    }

    file_info_list[nfile_info].exists  = TRUE;      // Symlink and target file exists

    nfile_info++;

    gettimeofday(&autogen_time_stop, NULL);

    /* Timing calculations: calculate milliseconds */
    if (autogen_time_stop.tv_usec >= autogen_time_start.tv_usec)
        autogen_elapsed_usec += autogen_time_stop.tv_usec - autogen_time_start.tv_usec;
    else
    {
        autogen_elapsed_usec += (1000000 + autogen_time_stop.tv_usec) - autogen_time_start.tv_usec;
        autogen_time_stop.tv_sec--;
    }
    autogen_elapsed_sec += autogen_time_stop.tv_sec - autogen_time_start.tv_sec;
}



unsigned int AUTOGEN_get_cache_count(char *cache_path)
{
    struct dirent *entry;
    unsigned int  count = 0;
    DIR           *fd;

    if ((fd = opendir(cache_path)) == NULL)
    {
        fprintf(stderr, "Fatal Error: Cannot open autogen cache directory: %s.\n", cache_path);
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(fd)) != NULL)
    {
        if (entry->d_type == DT_DIR)    // If the entry is a directory
        {
            if ( strstr(entry->d_name, GEN_PREFIX) || strstr(entry->d_name, BLD_PREFIX) )
                count++;
        }
    }

    closedir(fd);
    return(count);
}


proto_compile_stats_t *AUTOGEN_get_file_count(void)
{
    return(&pc_stats);
}

//===============================================================
//      Local Functions
//===============================================================

/**
 * Checks if each symlink's target still exists, and removes the
 * symlink if the target was deleted since the last session
 */
static void _remove_old_symlinks(char *data_dir)
{
    int     file_info_index;
    char    remove_buf[PATHLEN + sizeof(GSCOPE_BLD_DIR) + PATHLEN + 10];    // +10 for literal chars and null-terminator

    for (file_info_index = 0; file_info_index < nfile_info; file_info_index++)
    {
        if (!file_info_list[file_info_index].exists) //If target proto file is no longer found in directory
        {
            sprintf(remove_buf, "%s/%s/%s", data_dir, GSCOPE_BLD_DIR, file_info_list[file_info_index].unique_basename);
            (void) remove(remove_buf);

            g_free(file_info_list[file_info_index].unique_basename);
            g_free(file_info_list[file_info_index].true_basename);
            g_free(file_info_list[file_info_index].unique_dirname);

            file_info_list[file_info_index] = file_info_list[nfile_info - 1]; //Replace removed file with last element in array
            file_info_index--; //Makes sure loop checks the element moved in
            nfile_info--;
        }
    }
}


/**
 * Takes a proto file path as the input and parses it to create the protoc
 * command string that is then inputted into a protoc-c system call.
 * Will compile the proto file and place the output in the specified
 * outputPath directory.
 */
static gboolean _protobuf_csrc (const char *full_filename, char *data_dir)
{
    int     system_res;     //Result of system call to protoc-c
    FILE    *file_ptr;

    char    *dirname_ptr;
    char    *proto_ptr;     // Used to remove ".proto" from file name
    char    *realpath_ptr;  //Holds the target path of the symlink
    gboolean retval = TRUE;

    char    output_dir  [PATHLEN + sizeof(GSCOPE_GEN_DIR) + PATHLEN + 10];  // +10 for literal chars and null-terminator
    char    symlink_buf [PATHLEN + sizeof(GSCOPE_BLD_DIR) + PATHLEN + 10];
    char    isearch_buf [PATHLEN + sizeof(GSCOPE_BLD_DIR) + 10];
    char    command_buf [sizeof(settings.autoGenCmd) +
                         sizeof(output_dir) +
                         PATHLEN +
                         PATHLEN +
                         sizeof(isearch_buf) +
                         PATHLEN + 100]; // +100 for literal chars in fmt string and null-terminator

    // Build the protobuf compiler include search path
    sprintf(isearch_buf, "%s/%s", data_dir, GSCOPE_BLD_DIR);

    // Get the target of the symlink as input to compiler. Used to remove unique identifier from compiled output
    sprintf(symlink_buf, "%s/%s/%s", data_dir, GSCOPE_BLD_DIR, my_basename(full_filename));
    realpath_ptr = realpath(symlink_buf, NULL);

    // Exit if target of symlink is missing
    if (!realpath_ptr)
    {
        fprintf(stderr, "Error: Missing target to symlink [%s]", symlink_buf);
        exit(EXIT_FAILURE);
    }

    // Construct/re-use a session specific, output directory
    dirname_ptr = my_dirname(strdup(full_filename));
    sprintf(output_dir, "%s/%s/%s", data_dir, GSCOPE_GEN_DIR, dirname_ptr);
    _mkdir_all(output_dir);


    // Construct the complete protoc-c command string to be run.  Redirect error output to /dev/null
    sprintf(command_buf, "%s --c_out=%s -I=%s/%s:%s %s > /dev/null 2>&1",
                          settings.autoGenCmd,
                          output_dir,
                          data_dir,
                          dirname_ptr,
                          isearch_buf,
                          realpath_ptr);

    free(dirname_ptr);  // Note: This works because my_dirname() always returns a pointer to the beginning of the strdup() string.

    //fprintf(stderr, "Command: %s\n", command_buf);

    // Run the compilation command on one proto file
    system_res = system(command_buf);

    if (system_res != 0)
    {
        char    errfile_buf [PATHLEN * 3];

        retval = FALSE;

        /*** Write error message to compilation output file ***/
        sprintf(errfile_buf, "%s/%s", output_dir, my_basename(realpath_ptr));

        // Remove .proto from path
        proto_ptr = strrchr(errfile_buf, '.');
        if (proto_ptr)
        {
            sprintf(proto_ptr, "%s.c", settings.autoGenId);

            // Write error message to .c file
            file_ptr = fopen(errfile_buf, "w");
            if (file_ptr != NULL)
            {
                fprintf(file_ptr, AUTOGEN_ERR_PATTERN "\n");
                fclose(file_ptr);
            }
            else
            {
                my_cannotopen(errfile_buf);
                exit(EXIT_FAILURE);
            }

            // Write error message to .h file
            sprintf(proto_ptr, "%s.h", settings.autoGenId);
            file_ptr = fopen(errfile_buf, "w");
            if (file_ptr != NULL)
            {
                fprintf(file_ptr, "/* AutoGen Error in compiling .h file */\n");
                fclose(file_ptr);
            }
            else
            {
                my_cannotopen(errfile_buf);
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "WARNING: Invalid filename for Protobuf metafile source specified:  %s\n"
                           "File ignored.\n", errfile_buf);
        }
    }

    free(realpath_ptr);

    return(retval);
}


/**
 * Makes all directories to the inputted path, including parent directories
 */
static void _mkdir_all(const char *dir)
{
    char    *tmp;
    char    *p = NULL;
    size_t  len;

    tmp = strdup(dir);

    len = strlen(tmp);

    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    // Parses individual directories of the path and creates them one by one
    for (p = tmp + 1; *p; p++)
        if (*p == '/')
        {
            *p = 0;
            (void) mkdir(tmp, ACCESSPERMS);
            *p = '/';
        }

    (void) mkdir(tmp, ACCESSPERMS);
    free(tmp);
}


/**
 * Checks to see if filename inputted is a proto file
 */
static gboolean _is_protobuf_file(const char *filename)
{
    char *ptr;

    ptr = strrchr(filename, (int) '.');

    if (ptr)
    {
        // Performance trick - check the first char after the '.' with the second char of pattern to avoid
        //                     unneccessary strcmp() calls
        if ( (*(ptr + 1) == settings.autoGenSuffix[1]) && (strcmp(ptr, settings.autoGenSuffix) == 0) )
            return (TRUE);
    }

    return (FALSE);
}



//============================================================================
//
// Name: _do_garbage_collection
//
// Input: new_cache - The path of the newly created Gscope auto-gen cache dir
//
// Description:
//
// Every time we create a new auto-ten cache directory, see if some clean-up
// should be performed:
//
//  1. Count the total number of user-specific local cache directories
//     present.
//
//  2. If the total number of cache directories meets (or exceeds) the
//     configured auto-cache threshold value:
//          a) Trim/Delete Threshold/2 old cache directories using an
//             oldest-directory-first algorithm.
//
//  3. If we deleted any old cache directoris:
//          a) Display an informational pop-up:
//                "Auto cache garbage collection activated if you see
//                 this message on a regular basis you may want to
//                 increase your auto-gen cache threshold.  See:
//                 Options-->preferences-->Cross Reference-->
//                 Autogen Cache Garbage-Collection threshold"
//          b)  Be careful not to display pop-up message when we are in
//              settings.refOnly mode (use stderr instead)
//
//  4. Garbage collection Threshold must be >= 10. Default is 10.
//     Max is 100 (arbitrary)
//
//  5. Do not attempt to clean up the "broken" symlinks caused by garbage
//     collection.  The user can delete them if desired.  These newly broken
//     symlinks can be "recovered" by running gscope in the future on the
//     corresponding data set.
//
//============================================================================

static void _do_garbage_collection()
{
    DIR           *fd;
    char          *cache_dir;
    unsigned int  count;
    gboolean      garbage_dumped = FALSE;

    /*** Count the number of user-specific local cache directories **/
    cache_dir = DIR_get_path(DIR_AUTOGEN_CACHE);
    count = AUTOGEN_get_cache_count(cache_dir);


    /*** If cache threshold is met (or exceeded), perform garbage collection ***/
    if ( (count >= settings.autoGenThresh * 2) && (count > 4) )  // Always leave at least 2 caches (4 cache-directories)
    {
        if ((fd = opendir(cache_dir)) != NULL)
        {
            unsigned int  entry_count = 0;
            unsigned int  i;
            struct        dirent *entry;
            char          **cache_list;
            char          full_cache_path[PATHLEN + 1];

            garbage_dumped = TRUE;      // For reporting purposes, assume that we successfully removed some cache(s).

            cache_list = malloc(sizeof(char *) * count);
            memset(cache_list, 0, sizeof(char *) * count);

            // Populate the cache_list
            while ((entry = readdir(fd)) != NULL)
            {
                if (entry->d_type == DT_DIR)    // If the entry is a directory
                {
                    if ( strstr(entry->d_name, GEN_PREFIX) || strstr(entry->d_name, BLD_PREFIX) )
                    {
                        cache_list[entry_count] = strdup(entry->d_name);
                        entry_count++;

                        if (entry_count == count) break;    // If we've filled the list, stop reading dirents
                    }
                }
            }
            closedir(fd);

            qsort((char *)cache_list, count, sizeof(char *), _my_compare);

            i = 0;
            while(entry_count > settings.autoGenThresh) // We want to delete half of the "old" cache directory pairs
            {
                sprintf(full_cache_path, "%s/%s", cache_dir, cache_list[i]);
                if (recursive_remove(full_cache_path) < 0)
                    fprintf(stderr,"Warning: Garbage Collection failed to remove cache directory: %s\n", cache_list[i]);
                i++;

                sprintf(full_cache_path, "%s/%s", cache_dir, cache_list[i]);
                if (recursive_remove(full_cache_path) < 0)
                    fprintf(stderr,"Warning: Garbage Collection failed to remove cache directory: %s\n", cache_list[i]);
                i++;

                entry_count -= 2;  // Even if we fail to delete the targeted directory, keep going.  Otherwise we have an infinite loop.
            }


            for (i = 0; i < count; i++)
            {
                if ( cache_list[i] )
                    free(cache_list[i]);
            }
            free(cache_list);
        }
        else
        {
            fprintf(stderr, "Garbage Collection Failed: Cannot open autogen cache directory.\n");
        }

    }

    // If garbage collection was activaed, display a informational message
    if (garbage_dumped)
    {
        char message[] = "G-Scope garbage collection has been run to reduce the amount of Autogen Cache data.\n\n"
                         "If you are seeing this message on a regular basis, you may want to increase the CACHE "
                         "GARBAGE COLLECTION THRESHOLD preference value.  See:\n\n(Options-->Preferences-->Cross Reference)";
        if ( !settings.refOnly )
            DISPLAY_msg(GTK_MESSAGE_INFO, message);
        else
            fprintf(stderr,"%s\n", message);
    }
}



static int _my_compare(char **s1, char **s2)
{
    return( strcmp(strrchr(*s1, '_'), strrchr(*s2, '_')) );
}



//============================================================================
//
// Name: recursive_remove
//
// Input: Path to file (or directory) to recursively remove
//
// Note that on many systems the d_type member in struct dirent is not
// supported; on such platforms, you will have to use stat() and
// S_ISDIR(stat.st_mode) to determine if a given path is a directory.
//
// Most other directory browsing functions in G-Scope utilize the
// d_type member from dirent.  So unless someting changes, d_type
// member support is REQUIRED by OTHER portions of G-Scope.  This
// function is left as an example of how to check for directories
// without using "d_type".
//
// Most, if not all, modern Linux distros support the "d_type" member,
// so this methodology might only be interesting for porting to
// non-Linux platforms.
//
//============================================================================

static int recursive_remove(char *path)
{
    DIR *fd = opendir(path);
    size_t path_len = strlen(path);
    int retval = -1;

    if (fd)
    {
        struct dirent *p;

        retval = 0;

        while (!retval && (p = readdir(fd)))
        {
            int r2 = -1;
            char *buf;
            size_t len;

            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
            {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);

            if (buf)
            {
                struct stat statbuf;

                snprintf(buf, len, "%s/%s", path, p->d_name);

                if (!stat(buf, &statbuf))
                {
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        r2 = recursive_remove(buf);      // Directory found, descend into it
                    }
                    else
                    {
                        r2 = unlink(buf);                // Regular File, delete
                    }
                }
                free(buf);
            }
            retval = r2;
        }
        closedir(fd);
    }

    if (!retval)
    {
        retval = rmdir(path);
    }

    return retval;
}
