

//===============================================================
// Defines
//===============================================================


// Define a default value for each application setting
#define ignoreCaseDef      FALSE
#define refOnlyDef         FALSE
#define noBuildDef         FALSE
#define updateAllDef       FALSE
#define truncateSymbolsDef FALSE
#define compressDisableDef FALSE
#define autoGenEnableDef   TRUE
#define recurseDirDef      FALSE
#define versionDef         FALSE
#define useEditorDef       FALSE
#define retainInputDef     FALSE
#define retainFailedDef    FALSE
#define searchLoggingDef   FALSE
#define reuseWinDef        FALSE
#define exitConfirmDef     TRUE
#define menuIconsDef       TRUE
#define singleClickDef     FALSE
#define showIncludesDef    FALSE
#define refFileDef         "cscope_db.out"
#define nameFileDef        ""
#define includeDirDef      ""
#define includeDirDelimDef 0
#define srcDirDef          ""
#define rcFileDef          ""
#define fileEditorDef      "/bin/vi"
#define autoGenPathDef     "/sirius/work"
#define autoGenSuffixDef   ".proto"
#define autoGenCmdDef      "protoc-c_latest"
#define autoGenRootDef     ""
#define autoGenIdDef       ".pb-c"
#define autoGenThreshDef   10
#define searchLogFileDef   "cscope_srch.log"
#define suffixListDef      ":c:h:cpp:hpp:arm:fml:mf:l:y:s:ld:lnk:"
#define suffixDelimDef     0
#define typelessListDef    ":Makefile:"
#define typelessDelimDef   0
#define ignoredListDef     ":validation:"
#define ignoredDelimDef    0
#define histFileDef        ""
#define terminalAppDef     "gnome-terminal --working-directory=%s"
#define fileManagerDef     "nautilus %s" 
#define trackedVersionDef  1000


//===============================================================
// Public Functions
//===============================================================

void        APP_CONFIG_init        (GtkWidget *gscope_splash);
void        APP_CONFIG_set_boolean (const gchar *key, gboolean value);
void        APP_CONFIG_set_integer (const gchar *key, gint value);
void        APP_CONFIG_set_string  (const gchar *key, const gchar *value);
gboolean    APP_CONFIG_valid_list  (const char *list_name, char *list_ptr, char *delim_char);
