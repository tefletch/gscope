
typedef enum
{
    DIR_INITIALIZE,
    DIR_CURRENT_WORKING,
    DIR_DATA,
    DIR_SOURCE,
    FILE_NEW_CREF,
    DIR_AUTOGEN_CACHE,
} get_method_e;


typedef enum
{
    NEW_CREF,
    OLD_CREF,
} dir_init_e;


typedef enum
{
    MASTER_IGNORED_LIST,
} dir_list_e;


extern char     **DIR_src_files;    /* source file list*/
extern int      DIR_max_src_files;  /* maximum number of source files */
extern uint32_t nsrcfiles;          /* number of source files */


void     DIR_addincdir(char *path);
void     DIR_init(dir_init_e init_type);
void     DIR_incfile(char *file);
gboolean DIR_file_on_include_search_path(gchar *srcfile);
char *   DIR_get_path(get_method_e method);
void     DIR_addsrcfile(char *name);
void     DIR_create_offset_hash(char *buf_ptr);
void     DIR_free_offset_hash(void);
void     DIR_free_src_names_hash(void);
char     *DIR_get_old_offset(char *filename);
void     DIR_list_join(char *usr_list, dir_list_e dir_list);
void     DIR_init_cli_file_list(int argc, char *argv[]);

