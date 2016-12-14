
#define GSCOPE_GEN_DIR  ".gscope_gen"
#define GSCOPE_BLD_DIR  ".gscope_bld"

// auto_gen puplic interfaces
void AUTOGEN_init(char *data_dir);
void AUTOGEN_run(char *data_dir);
void AUTOGEN_addproto(char* name);
unsigned int AUTOGEN_get_file_count(void);
unsigned int AUTOGEN_get_cache_count(char *cache_path);
