/*
 * main.c
 *
 * Reference info:
 *
 *  https://developer.gnome.org/gtk3/stable/ch01s04.html  (Bulding GTK3 Applications) Lots of "bootstrapping" examples.
 *
 *  https://developer.gnome.org/gtk3/3.0/ch25s02.html#gtk-migrating-GtkStyleContext (General GTK2 to GTK3 migration topics)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "app_config.h"

#include "support.h"
#include "callbacks.h"
#include "search.h"
#include "display.h"
#include "build.h"
#include "utils.h"


// set this value to TRUE to utilize GTK builder XML file ./gscope3.glade
// set this value to FALSE to utilize the built-in string array created by xxd -i gscope3.glade > interface.c
#define BUILD_WIDGETS_FROM_FILE     TRUE

#if (!BUILD_WIDGETS_FROM_FILE)
#include    "interface.c"
#endif

//  ==== Global Variables ====


int main(int argc, char *argv[])
{
    GtkWidget   *gscope_main;
    GtkWidget   *gscope_splash;

    GError      *error = NULL;

    static gboolean option_error = FALSE;
    static gchar *refFile = NULL;
    static gchar *nameFile = NULL;
    static gchar *includeDir = NULL;
    static gchar *rcFile = NULL;
    static gchar *srcDir = NULL;
    static gchar *geometry = NULL;

    GtkBuilder      *builder;       // For GTK3

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
            "geometry", 'g', G_OPTION_FLAG_NONE, G_OPTION_ARG_STRING, &geometry,
            "Override the default window geometry [Width x Heigth] in pixels", "WxH"
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
    if (geometry)
    {strncpy(settings.geometry,   geometry,   MAX_STRING_ARG_SIZE); settings.geometry[MAX_STRING_ARG_SIZE - 1] = 0; g_free(geometry); }

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

        #if 0
        gchar       *home;
        char        path[PATHLEN + 1];

        /* Support optional/fall-back "local" pixmap files under $HOME/gscope/pixmaps */
        home = getenv("HOME");
        if (home == NULL) home = "";
        sprintf(path, "%s%s", home, "/.gscope/pixmaps");

        add_pixmap_directory(path);  // Support for "private" installs (as a fall-back, not an override)

        #else
        /* App-Standard location for pixmap files */
        add_pixmap_directory("../pixmaps");
        add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");  // The pixmap directory "added" last is the firt to be checked
        #endif

        #if 0 // Manual/additive widget creation method (selective extraction from xml file)
        {
            gchar *toplevel[] = {"gscope_splash", "gscope_main",  "image1",  "image2",  "image3",
                                 "image4",  "image5",  "image6",  "image7",  "image8",  /*image9*/
                                 "image10", "image11", "image12", "image13", "image14", "image15",
                                 "image16", "accelgroup1", "aboutdialog1", "stats_dialog",
                                 "autogen_active_cache_path_label", "rc_filename_label",
                                 "src_mode_status_button", NULL};

            builder = gtk_builder_new();
            gtk_builder_add_objects_from_file(builder, "../gscope3.glade", toplevel, NULL);
        }
        #else

        // Eventually, we want to use gtk_builder_new_from_string() with the string initialized to the contents of gscope3.glade
        // This will allow the application to be distributed as a single executable file [no .glade file needed].

        #if (BUILD_WIDGETS_FROM_FILE)
        {
            gchar ui_file_path[256];

            if (readlink("/proc/self/exe", ui_file_path, sizeof(ui_file_path)) == -1)
            {
                fprintf(stderr, "Application abort: Could not get location of Gscope binary.\nUnable to load UI...\n");
                exit(EXIT_FAILURE);
            }
            ui_file_path[255] = '\0';        // Ensure path string is null terminated -- readlink() does not append a null if it truncates.
            my_dirname(ui_file_path);
            strcat(ui_file_path, "/gscope3.glade");
            builder = gtk_builder_new_from_file(ui_file_path);
        }
        #else
            builder = gtk_builder_new_from_string(gscope3_glade, gscope3_glade_len);
        #endif

        #endif

        {
            GSList  *list;

            list = gtk_builder_get_objects(builder);
            g_slist_foreach(list, my_add_widget, NULL);
        }




        // Store a list of references for all widgets that are manupulated by the application at runtime.
        //instance.gscope_main            = GTK_WIDGET(gtk_builder_get_object(builder, "gscope_main"));


        gscope_splash = GTK_WIDGET(gtk_builder_get_object(builder, "gscope_splash"));
        gtk_widget_show(gscope_splash);
        DISPLAY_message_set_transient_parent(gscope_splash);

        // Instead of rendering the splash screen right away, wait until the first progress bar update.
        // Process pending gtk events
        //while (gtk_events_pending() )
        //    gtk_main_iteration();


        gtk_builder_connect_signals(builder, NULL);

        APP_CONFIG_init(gscope_splash);

        /* Get a reference to the top-level application window */
        gscope_main  = my_lookup_widget("gscope_main");
        if (settings.geometry[0])
        {
            char *e_ptr;
            gint width, height;

            width  = strtol(settings.geometry, &e_ptr, 10);
            height = strtol(e_ptr + 1, NULL, 10);
            gtk_window_resize(GTK_WINDOW(gscope_main), width, height);
        }

        /* Perform initial configuration for all application callbacks */
        CALLBACKS_init(gscope_main);

        {
            char *program_name;
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

        DISPLAY_set_active_progress_bar( GTK_WIDGET(gtk_builder_get_object(builder, "splash_progressbar")) );  // build progress meter
        BUILD_initDatabase();
        DISPLAY_set_active_progress_bar( GTK_WIDGET(gtk_builder_get_object(builder, "rebuild_progressbar")) );  // build progress meter
        g_object_unref(G_OBJECT(builder));
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

