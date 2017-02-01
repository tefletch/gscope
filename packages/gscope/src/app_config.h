

//===============================================================
// Defines
//===============================================================

#define     MAX_STRING_ARG_SIZE     256             /* Maximum supported length of any single command-line string-type argument */
#define     PATHLEN                 500             /* file pathname length */


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
// typedefs
//===============================================================

/* Application "Settings" Structure Layout */
typedef struct
  {
      // Command argument [boolean] settings
      gboolean   refOnly;
      gboolean   noBuild;
      gboolean   updateAll;
      gboolean   truncateSymbols;
      gboolean   compressDisable;
      gboolean   recurseDir;
      gboolean   version;
      // Non-command-argument [boolean]settings
      gboolean   ignoreCase;
      gboolean   useEditor;
      gboolean   retainInput;
      gboolean   retainFailed;
      gboolean   searchLogging;
      gboolean   reuseWin;
      gboolean   exitConfirm;
      gboolean   menuIcons;
      gboolean   singleClick;
      gboolean   showIncludes;
      gboolean   autoGenEnable;
      // Command agrument [string] settings
      gchar     refFile[MAX_STRING_ARG_SIZE];
      gchar     nameFile[MAX_STRING_ARG_SIZE];
      gchar     includeDir[MAX_STRING_ARG_SIZE];
      gchar     includeDirDelim;
      gchar     srcDir[MAX_STRING_ARG_SIZE];
      gchar     rcFile[MAX_STRING_ARG_SIZE];
      gchar     fileEditor[MAX_STRING_ARG_SIZE];
      // Non-command-argument [string] settings
      gchar     autoGenPath[MAX_STRING_ARG_SIZE];
      gchar     autoGenSuffix[MAX_STRING_ARG_SIZE];
      gchar     autoGenCmd[MAX_STRING_ARG_SIZE];
      gchar     autoGenRoot[MAX_STRING_ARG_SIZE];
      gchar     autoGenId[MAX_STRING_ARG_SIZE];
      guint     autoGenThresh;
      gchar     searchLogFile[MAX_STRING_ARG_SIZE];
      gchar     suffixList[MAX_STRING_ARG_SIZE];
      gchar     suffixDelim;
      gchar     typelessList[MAX_STRING_ARG_SIZE];
      gchar     typelessDelim;
      gchar     ignoredList[MAX_STRING_ARG_SIZE];
      gchar     ignoredDelim;
      gchar     histFile[MAX_STRING_ARG_SIZE];
      gchar     terminalApp[MAX_STRING_ARG_SIZE];
      gchar     fileManager[MAX_STRING_ARG_SIZE];
      // Non-command-argument [integer] settings
      gint      trackedVersion;
      // Non "sticky" settings [Not configurable from command line or config file]
      gboolean  smartQuery;
  } settings_t;


  typedef struct
{
    gboolean    ignoreCase;
    gboolean    useEditor;
    gboolean    reuseWin;
    gboolean    retainInput;
} sticky_t;


//===============================================================
// Global Variables
//===============================================================

// Application-global settings structure
extern settings_t settings;
extern sticky_t   sticky_settings;


//===============================================================
// Public Functions
//===============================================================

void        APP_CONFIG_init        (GtkWidget *gscope_splash);
void        APP_CONFIG_set_boolean (const gchar *key, gboolean value);
void        APP_CONFIG_set_integer (const gchar *key, gint value);
void        APP_CONFIG_set_string  (const gchar *key, const gchar *value);
gboolean    APP_CONFIG_valid_list  (const char *list_name, char *list_ptr, char *delim_char);
