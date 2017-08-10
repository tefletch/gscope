
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "app_config.h"
#include "interface.h"
#include "support.h"
#include "callbacks.h"
#include "search.h"
#include "display.h"
#include "build.h"
#include "utils.h"


//  ======= #defines ========


//  ==== Global Variables ====


int main(int argc, char *argv[])
{
    GtkWidget   *gscope_main;
    GtkWidget   *gscope_splash;

    GError      *error = NULL;
    gchar       *home;

    static gboolean option_error = FALSE;
    static gchar *refFile = NULL;
    static gchar *nameFile = NULL;
    static gchar *includeDir = NULL;
    static gchar *rcFile = NULL;
    static gchar *srcDir = NULL;

#define G_OPTION_FLAG_NONE 0

    GOptionEntry options[] = {
        { 
            "refOnly", 'b', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &settings.refOnly,
            "Build the cross-reference only.  (No GUI)", NULL
        },
        {
            "compressOff", 'c', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &settings.compressDisable,
            "Use only ASCII characters in the cross-reference file (don't compress).", NULL
        },
        {
            "noBuild", 'd', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &settings.noBuild,
            "Do not update the cross-reference file.", NULL
        },
        {
            "refFile", 'f', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, &refFile,
            "Use the filename specified as the cross reference [output] file name instead of 'cscope_db.out`", "FILE"
        },
        {
            "nameFile", 'i', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, &nameFile,
            "Use the filename specified as the list of source files to cross-reference", "FILE"
        },
        {
            "includeDir", 'I', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &includeDir,
            "Use the specified directory search path to find #include files. (:dir1:dir2:dirN:)", "PATH"
        },
        {
            "rcFile", 'r', G_OPTION_FLAG_NONE, G_OPTION_ARG_FILENAME, &rcFile,
            "Start Gscope using the preferences info from FILE.", "FILE"
        },
        {
            "recurseDir", 'R', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &settings.recurseDir,
            "Recursively search all subdirecties [Default = search below <current-dir>] for source files.", NULL
        },
        {
            "srcDir", 'S', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &srcDir,
            "Search the specified directory for source files. When used with -R, set search-root = DIRECTORY.", "DIRECTORY"
        },
        {
            "truncSymbols", 'T', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &settings.truncateSymbols,
            "Use only the first eight characters to match against C symbols.", NULL
        },
        {
            "updateAll", 'u', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &option_error,
            "***DEPRICATED option do not use*** Unconditionally [re]build the cross-reference file.", NULL
        },
        {
            /* Debugging tool, not for end-user consumption */
            "UpdateAll", 'U', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &settings.updateAll,
            "Unconditionally [re]build the cross-reference file.", NULL
        },
        {
            "version", 'v', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, &settings.version,
            "Show version information", NULL
        },
        { NULL }

    };

    // Depricated since GTK 2.24 (now automatically called by gtk_init()
    //gtk_set_locale();

    if (!gtk_init_with_args(&argc, &argv, "[source files]", options, NULL, &error))
    {
        fprintf(stderr, "\nError: %s\n", error->message);

        if ((error->code == G_OPTION_ERROR_UNKNOWN_OPTION) && (error->domain == G_OPTION_ERROR))
        {
            fprintf(stderr, "Type '%s --help' for a list of valid options\n\n", argv[0]);
        }

        exit(EXIT_FAILURE);
    }

    if (option_error) fprintf(stderr, "Warning:  Ignoring depricated option '-u'.\n");

    if (settings.updateAll) fprintf(stderr, "Warning: Unless you are debugging Gscope, you should probably not be using the '-U' option.\n");

    /* Copy any dynamically allocated [argument] strings to the settings structure (and free them) */
    /* Buffer overflow protection:  Any [argument] string that exceeds the maximum allowed size is truncated */
    if (refFile)
    {strncpy(settings.refFile,    refFile,    MAX_STRING_ARG_SIZE); settings.refFile[MAX_STRING_ARG_SIZE - 1] = 0; g_free(refFile);    }
    if (nameFile)
    {strncpy(settings.nameFile,   nameFile,   MAX_STRING_ARG_SIZE); settings.nameFile[MAX_STRING_ARG_SIZE - 1] = 0; g_free(nameFile);   }
    if (includeDir)
    {strncpy(settings.includeDir, includeDir, MAX_STRING_ARG_SIZE); settings.includeDir[MAX_STRING_ARG_SIZE - 1] = 0; g_free(includeDir); }
    if (rcFile)
    {strncpy(settings.rcFile,     rcFile,     MAX_STRING_ARG_SIZE); settings.rcFile[MAX_STRING_ARG_SIZE - 1] = 0; g_free(rcFile);     }
    if (srcDir)
    {strncpy(settings.srcDir,     srcDir,     MAX_STRING_ARG_SIZE); settings.srcDir[MAX_STRING_ARG_SIZE - 1] = 0; g_free(srcDir);     }

    //Process the arguments that don't require GUI functionality

    if (settings.version)
    {
        printf("GSCOPE version %s\n", VERSION);
        exit(EXIT_SUCCESS);
    }


    /* save the filename arguments */
    BUILD_init_cli_file_list(argc, argv);

    if (settings.refOnly)
    {
        APP_CONFIG_init(NULL);
        BUILD_initDatabase();
    }
    else
    {
        g_set_application_name("G-Scope");

        /* Support optional/fall-back "local" pixmap files under $HOME/gscope/pixmaps */
        home = getenv("HOME");
        if (home == NULL)
            home = "";

        {
            char  *path;
            my_asprintf(&path, "%s%s", home, "/.gscope/pixmaps");
            add_pixmap_directory(path);  // Support for "private" installs (as a fall-back, not an override)
            add_pixmap_directory("../pixmaps");
            g_free(path);
        }

        /* Standard location for this application's pixmap files */
        add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");  // The pixmap directory "added" last is the firt to be checked

        gscope_splash = create_gscope_splash();
        gtk_widget_show(gscope_splash);
        DISPLAY_message_set_transient_parent(gscope_splash);

        // Instead of rendering the splash screen right away, wait until the first progress bar update.
        // Process pending gtk events
        //while (gtk_events_pending() )
        //    gtk_main_iteration();

        APP_CONFIG_init(gscope_splash);

        /* Save references to top-level interface object(s) */
        gscope_main  = create_gscope_main();

        /* Perform initial configuration for all application callbacks */
        CALLBACKS_init(gscope_main);

        {
            char  *program_name;
            my_asprintf(&program_name, "<span weight=\"bold\">Version %s</span>", VERSION);
            gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(gscope_main), "label1")), program_name);
            g_free(program_name);
        }


        // Initialize the Quick-option checkbox settings
        //==============================================

        if (settings.ignoreCase)
        {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), "ignorecase_checkmenuitem")),
                                           TRUE);
            settings.ignoreCase = TRUE;    // undo the value change caused by the "set_active" callback
        }
        if (settings.useEditor)
        {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), "useeditor_checkmenuitem")),
                                           TRUE);
            settings.useEditor = TRUE;     // undo the value change caused by the "set_active" callback
        }
        if (settings.reuseWin)
        {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), "reusewin_checkmenuitem")),
                                           TRUE);
            settings.reuseWin = TRUE;     // undo the value change caused by the "set_active" callback
        }
        if (settings.retainInput)
        {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), "retaininput_checkmenuitem")),
                                           TRUE);
            settings.retainInput = TRUE;  // undo the value change caused by the "set_active" callback
        }

        gtk_widget_hide(lookup_widget(GTK_WIDGET(gscope_main), "progressbar1"));

        DISPLAY_set_active_progress_bar( lookup_widget(gscope_splash, "splash_progressbar") );
        BUILD_initDatabase();
        DISPLAY_set_active_progress_bar( lookup_widget(gscope_main, "rebuild_progressbar") );
    }

    if (!settings.refOnly)
    {
        gtk_widget_destroy(gscope_splash);  // Kill the splash screen
        gtk_widget_show(gscope_main);
        DISPLAY_message_set_transient_parent(gscope_main);

        gtk_main();
    }
    return 0;
}

