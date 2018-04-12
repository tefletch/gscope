
#define GSCOPE_GEN_DIR  ".gscope_gen"
#define GSCOPE_BLD_DIR  ".gscope_bld"

// defines
#define     AUTOGEN_ERR_PATTERN     "/* AutoGen Error in compiling .c file */"

// auto_gen public type definitions

typedef struct
{
    guint num_proto_changed;
    guint num_proto_files;
    guint proto_build_success;
    guint proto_build_failed;
} proto_compile_stats_t;


// auto_gen public interfaces
void                    AUTOGEN_init(char *data_dir);
void                    AUTOGEN_run(char *data_dir);
void                    AUTOGEN_addproto(char* name);
proto_compile_stats_t   *AUTOGEN_get_file_count(void);
unsigned int            AUTOGEN_get_cache_count(char *cache_path);
