typedef struct {
    uint32_t    lineoffset; /* source line database offset */
    uint32_t    fcnoffset;  /* function name database offset */
    uint32_t    fileindex : 24; /* source file name index */
    uint32_t    type : 8;   /* reference type (mark character) */
} POSTING;


typedef enum  {
    FIND_SYMBOL = 0,
    FIND_DEF,
    FIND_CALLEDBY,
    FIND_CALLING,
    FIND_STRING,
    FIND_REGEXP,
    FIND_FILE,
    FIND_INCLUDING,
    FIND_ALL_FUNCTIONS,
    FIND_NULL
} search_t;


typedef enum {
    COUNT_SET = 0,
    COUNT_GET = 1,
} count_method_t;


typedef struct
{
    guint define_cnt;
    guint identifier_cnt;
    guint fn_calls_cnt;
    guint fn_cnt;
    guint class_cnt;
    guint include_cnt;
} stats_struct_t;


typedef struct
{
    gchar       *start_ptr;
    gchar       *end_ptr;
    uint32_t    match_count;
} search_results_t;


//===============================================================
//      Public Interface Functions
//===============================================================

void                SEARCH_init     (void);
search_results_t *  SEARCH_lookup   (search_t search_operation, gchar *pattern);
void                SEARCH_stats    (stats_struct_t *sptr);
void                SEARCH_cleanup  (void);
gboolean            SEARCH_save_html(gchar *filename);
gboolean            SEARCH_save_text(gchar *filename);
gboolean            SEARCH_save_csv (gchar *filename);
void                SEARCH_cancel   (void);
void                SEARCH_check_cref     (void);
void                SEARCH_set_cref_status(gboolean status);
gboolean            SEARCH_get_cref_status(void);
void                SEARCH_free_results   (search_results_t *results);

//===============================================================
//      Public Global Variables
//===============================================================

extern FILE     *refsfound;    /* references found file */
