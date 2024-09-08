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

//#include "support.h"
//#include "callbacks.h"
//#include "search.h"
//#include "display.h"
#include "build.h"
#include "utils.h"


// set this value to TRUE to utilize GTK builder XML file ./gscope4.cmb
// set this value to FALSE to utilize the built-in string array created by xxd -i gscope3.glade > interface.c
#define BUILD_WIDGETS_FROM_FILE     TRUE

// Revist: Possible use manually created widgets for GTK4
#if (!BUILD_WIDGETS_FROM_FILE)
#include    "interface.c"
#endif

//  ==== Global Variables ====
settings_t settings = {
    /*.refOnly            =*/refOnlyDef,
    /*.noBuild            =*/noBuildDef,
    /*.updateAll          =*/updateAllDef,
    /*.truncateSymbols    =*/truncateSymbolsDef,
    /*.compressDisable    =*/compressDisableDef,
    /*.recurseDir         =*/recurseDirDef,
    /*.version            =*/versionDef,
    /*.ignoreCase         =*/ignoreCaseDef,
    /*.useEditor          =*/useEditorDef,
    /*.retainInput        =*/retainInputDef,
    /*.retainFailed       =*/retainFailedDef,
    /*.searchLogging      =*/searchLoggingDef,
    /*.reuseWin           =*/reuseWinDef,
    /*.exitConfirm        =*/exitConfirmDef,
    /*.menuIcons          =*/menuIconsDef,
    /*.singleClick        =*/singleClickDef,
    /*.showIncludes       =*/showIncludesDef,
    /*.autoGenEnable      =*/autoGenEnableDef,
    /*.refFile            =*/refFileDef,
    /*.nameFile           =*/nameFileDef,
    /*.includeDir         =*/includeDirDef,
    /*.includeDirDelim    =*/includeDirDelimDef,
    /*.srcDir             =*/srcDirDef,
    /*.rcFile             =*/rcFileDef,
    /*.fileEditor         =*/fileEditorDef,
    /*.autoGenPath        =*/autoGenPathDef,
    /*.autoGenSuffix      =*/autoGenSuffixDef,
    /*.autoGenCmd         =*/autoGenCmdDef,
    /*.autoGenRoot        =*/autoGenRootDef,
    /*.autoGenId          =*/autoGenIdDef,
    /*.autoGenThresh      =*/autoGenThreshDef,
    /*.searchLogFile      =*/searchLogFileDef,
    /*.suffixList         =*/suffixListDef,
    /*.suffixDelim        =*/suffixDelimDef,
    /*.typelessList       =*/typelessListDef,
    /*.typelessDelim      =*/typelessDelimDef,
    /*.ignoredList        =*/ignoredListDef,
    /*.ignoredDelim       =*/ignoredDelimDef,
    /*.histFile           =*/histFileDef,
    /*.terminalApp        =*/terminalAppDef,
    /*.fileManager        =*/fileManagerDef,
    /*.geometry           =*/geometryDef,
    /*.trackedVersion     =*/trackedVersionDef,
    /*.smartQuery         =*/TRUE
};


   gboolean option_error = FALSE;
    gchar *refFile = NULL;
    gchar *nameFile = NULL;
    gchar *includeDir = NULL;
    gchar *rcFile = NULL;
    gchar *srcDir = NULL;
    gchar *geometry = NULL;

static gboolean cmd_line_handler(gpointer data)
{
    GApplicationCommandLine *cmd_line = data;
    gchar **GskShaderArgsBuilder;
    gchar **args;
    gint  argc;
    GOptionContext  *context;
    GError *error = NULL;

 
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

    printf("%s\n", __func__);

    /* Process the command line options */
    args = g_application_command_line_get_arguments(cmd_line, &argc);
    context = g_option_context_new (NULL);
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, options, NULL);         // Load the argument definitions

    if ( !g_option_context_parse_strv (context, &args, &error) ) // Parse the command line argument list
    {
        g_application_command_line_printerr (cmd_line, "%s\n", error->message);
        g_error_free (error);
        g_application_command_line_set_exit_status (cmd_line, 1);
    }
    else
    {
        g_application_command_line_print (cmd_line, 
                                          "refOnly=%s\ncompress=%s\nnoBuld=%s\nrefFile=%s\n",
                                          settings.refOnly ? "TRUE" : "FALSE",
                                          settings.compressDisable ? "Disabled" : "Enabled",
                                          settings.noBuild ? "TRUE" : "FALSE",
                                          refFile
                                         );
        g_application_command_line_set_exit_status (cmd_line, 0);       
    }

    /* Copy any dynamically allocated [argument] strings to the settings structure (and free them) */
    /* Buffer overflow protection:  Any [argument] string that exceeds the maximum allowed size is truncated */
    if (refFile)
    {strncpy(settings.refFile,    refFile,    MAX_STRING_ARG_SIZE); settings.refFile[MAX_STRING_ARG_SIZE - 1] = 0; g_free(refFile);    }


    g_strfreev(args);
    g_option_context_free(context);
    g_object_unref(cmd_line);

    printf("%s: done\n", __func__);

    return (G_SOURCE_REMOVE);
}

static void startup (GApplication *app, gpointer *user_data)
{
    printf("** %s **\n", __func__);
}

static void print_hello (GtkWidget *widget, gpointer   data)
{
  printf ("Hello World\n");
}


static void window_removed(GtkWidget *widget, gpointer   data)
{
    printf("** %s **\n", __func__);
    g_application_release(G_APPLICATION(widget));
}


static void activate (GApplication *app, gpointer *user_data)
{
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *box;

    printf("** %s **\n", __func__);

    g_application_hold (app);
    window = gtk_application_window_new (GTK_APPLICATION(app));
    gtk_window_set_title (GTK_WINDOW (window), "Window");
    gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

    gtk_window_set_child (GTK_WINDOW (window), box);

    button = gtk_button_new_with_label ("Hello World");

    g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);
    //g_signal_connect_swapped (button, "clicked", G_CALLBACK (gtk_window_destroy), window);
    g_signal_connect(app, "window-removed", G_CALLBACK(window_removed), NULL);

    gtk_box_append (GTK_BOX (box), button);

    gtk_window_present (GTK_WINDOW (window));

    BUILD_initDatabase();
}

static int command_line(GApplication *app, GApplicationCommandLine *cmdline)
{
    printf("\n%s: enter\n", __func__);

    #if 1   // Add a (idle-priority) defferred function call to the main loop
    /* keep the application running until we are done with this commandline */
    g_application_hold (app);     // Increase the use count of 'application'

    // Load 'cmdline' object into a key/value table (release the application when cmdline object is destroyed)
    g_object_set_data_full (G_OBJECT (cmdline), "application", app, (GDestroyNotify)g_application_release);

    g_object_ref (cmdline);                  // Increase the reference count of 'cmdline' (refcount decremented by handler)
    g_idle_add (cmd_line_handler, cmdline);  // Call command line handler from THE main-loop('default-idle' priority)
    printf("%s: handler request submitted\n", __func__);

    #else   // Process the command line options here
    g_application_hold (app);
    g_object_set_data (G_OBJECT (cmdline), "application", app);
    g_object_ref (cmdline); 
    cmd_line_handler (cmdline);
    g_object_unref(cmdline);
   
    #endif

    // When command_line option is processed, the activate signal is not automatically issued
    activate(app, NULL);

    printf("%s: done\n", __func__);
    return 0;
}


static void shutdown(GApplication *app, gpointer *user_data)
{
    printf("** Shutdown **\n");
    // Perform all shutdown cleanup (save/close files)
}



int main(int argc, char *argv[])
{
    GtkApplication *app;
    int status;

    //app = gtk_application_new("gscope.gscope4", G_APPLICATION_DEFAULT_FLAGS);
    app = gtk_application_new("gscope.gscope4", G_APPLICATION_DEFAULT_FLAGS | G_APPLICATION_HANDLES_COMMAND_LINE);
   
    // *** startup ***
    g_signal_connect(app, "startup", G_CALLBACK(startup), NULL);
    /*
        When your application starts, the startup signal will be fired. This gives you a chance to perform initialisation
        tasks that are not directly related to showing a new window. After this, depending on how the application is started, 
        either activate or open will be called next.   
    */
    
    // *** activate ***
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    /*
        GtkApplication defaults to applications being single-instance. If the user attempts to start a second instance
        of a single-instance application then GtkApplication will signal the first instance and you will receive additional
        activate or open signals. In this case, the second instance will exit immediately, without calling startup or shutdown.

        All startup initialisation should be done in startup. This avoids wasting work in the second-instance case where the 
        program just exits immediately.

        The application will continue to run for as long as it needs to. This is usually for as long as there are any open windows.
        You can additionally force the application to stay alive using g_application_hold().
    */
    
    // *** command-line ***
    g_signal_connect(app, "command-line", G_CALLBACK(command_line), NULL);
    //g_application_set_inactivity_timeout (G_APPLICATION(app), 10000);

    // *** shutdown ***
    g_signal_connect(app, "shutdown", G_CALLBACK(shutdown), NULL);
    /*
        On shutdown, you receive a shutdown signal where you can do any necessary cleanup tasks (such as saving files to disk).
        
        The main entry point for your application should only create a GtkApplication instance, set up the signal handlers,
        and then call g_application_run().   
    */

     
    // *** Run the application instance ***
    printf("\n%s: run application\n", __func__);
    status = g_application_run (G_APPLICATION(app), argc, argv);

    printf("%s: application_run complete\n", __func__);
    g_object_unref (app);

    printf("%s: done\n", __func__);

#if 0 // GTK4 bootstrap: Migrate later (build spash screen next -- see below)
    GtkWidget   *gscope_main;
    GtkWidget   *gscope_splash;

    GError      *error = NULL;


    GtkBuilder      *builder;       // For GTK3




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
            strcat(ui_file_path, "/gscope4.cmb");

            // gtk4 migration:
            // If needed, use gtk_builder_set_current_object() to add user data to signals.
            // If gtk_builder_set_current_object() is used then we can no longer use gtk_builder_new_from_file() or gtk_builder_new_from_string().
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

        // gtk4 migration: gtk_builder_connect_signals() no longer exists. Instead, signals are always connected automatically
        //gtk_builder_connect_signals(builder, NULL);

        /* Initialze the application settings */
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


        // Initialize the Quick-option checkbox settings -- Revisit: Can this be moved to app initialization app_config
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
#endif

    return status;
}

