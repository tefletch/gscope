#define	    PATHLEN	                500		        /* file pathname length */
#define     PATLEN                  PATHLEN         /* symbol pattern length */
#define     MAX_SYMBOL_SIZE         PATHLEN         /* a more readable name for PATLEN & PATHLEN */
#define     MSGLEN                  PATHLEN + 80    /* displayed message length */
#define     MAX_LIST_SIZE           1023            /* Max size for all lists.  All lists must be the same size */
#define     MAX_STRING_ARG_SIZE     256             /* Maximum supported length of any single command-line string-type argument */
#define     STMTMAX                 5000            /* maximum source statement length */
#define     MAX_SUFFIX              60              /* Support up to a 60 character file suffix */
#define     TMPDIR                  "/tmp"

/* database output macros that update it's offset */
#define     dbputc(c)           (++dboffset, (void) putc(c, newrefs))

/* fast string equality tests (avoids most strcmp() calls) - test second char to avoid common "leading-/" strings */
#define     STREQUAL(s1, s2)    (*(s1+1) == *(s2+1) && strcmp(s1, s2) == 0)
#define     STRNOTEQUAL(s1, s2) (*(s1) != *(s2) || strcmp(s1, s2) != 0)

/* Check if a given pair of chars is compressable as a dicode: */
#define IS_A_DICODE(inchar1, inchar2)					   \
  (dicode1[(unsigned char)(inchar1)] && dicode2[(unsigned char)(inchar2)])

/* Combine the pair into a dicode */
#define DICODE_COMPRESS(inchar1, inchar2)		\
  ((0200 - 2) + dicode1[(unsigned char)(inchar1)]	\
   + dicode2[(unsigned char)(inchar2)])

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

// main.c global variables
extern settings_t settings;
extern sticky_t   sticky_settings;
extern int        fileargc;        /* file argument count */
extern char       **fileargv;      /* file argument values */

// Top-level widgets
extern GtkWidget  *gscope_main;    /* top level window widget pointer */
extern GtkWidget  *quit_dialog;
extern GtkWidget  *aboutdialog1;
extern GtkWidget  *gscope_preferences;
extern GtkWidget  *stats_dialog;
extern GtkWidget  *folder_chooser_dialog;
extern GtkWidget  *open_file_chooser_dialog;
extern GtkWidget  *output_file_chooser_dialog;
extern GtkWidget  *save_results_file_chooser_dialog;

extern GtkWidget  *build_progress; /* Progress bar widget for Splash Screen and/or xref Rebuild */

#ifdef GTK3_BUILD

#define lookup_widget(wiget, name) my_lookup_widget(name)

#endif
