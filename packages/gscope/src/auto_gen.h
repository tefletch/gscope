
#define CSCOPE_GEN_DIR  "cscope_gen"
#define CSCOPE_BLD_DIR  "cscope_bld"

// auto_gen puplic interfaces
void AUTOGEN_init(char *data_dir);
void AUTOGEN_run(char *data_dir);
void AUTOGEN_addproto(char* name);
unsigned int AUTOGEN_get_file_count(void);
unsigned int AUTOGEN_get_cache_count(char *cache_path);
