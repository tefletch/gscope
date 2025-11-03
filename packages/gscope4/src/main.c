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

#include "gtk4_aux.h"


// Test callback
void print_hello (GtkWidget *widget, gpointer   data)
{
  printf ("Hello World\n");
}

static GActionEntry app_entries[] = {
    {"rebuild", on_rebuild_database1_activate, NULL, NULL, NULL },
    {"save_results", on_save_results_activate, NULL, NULL, NULL },
    {"clear_query", on_clear_query_activate, NULL, NULL, NULL },
    {"save_query", on_save_query_activate, NULL, NULL, NULL },
    {"load_query", on_load_query_activate, NULL, NULL, NULL },
    {"delete_history", on_delete_history_file_activate, NULL, NULL, NULL },
    {"quit", on_quit1_activate, NULL, NULL, NULL },
    {"ignorecase", on_ignorecase_activate, NULL, "false", NULL},
    {"useeditor", on_useeditor_activate, NULL, "false", NULL },
    {"reusewin", on_reusewin_activate, NULL, "false", NULL },
    {"retaininput", on_retaininput_activate, NULL, "false", NULL },
    {"preferences", on_preferences_activate, NULL, "false", NULL },
    {"smartquery", on_smartquery_activate, NULL, "true", NULL},
    {"session_statistics", on_session_statistics_activate, NULL, NULL, NULL},
    {"list_all_functions", on_list_all_functions1_activate, NULL, NULL, NULL },
    {"list_autogen_errors", on_list_autogen_errors_activate, NULL, NULL, NULL },
    {"usage_activate", on_usage1_activate, NULL, NULL, NULL },
    {"setup", on_setup1_activate, NULL, NULL, NULL },
    /* wiki sub-menu entries*/
    {"overview_wiki", on_overview_wiki_activate, NULL, NULL, NULL },
    {"usage_wiki", on_usage_wiki_activate, NULL, NULL, NULL },
    {"configure_wiki", on_configure_wiki_activate, NULL, NULL, NULL },
    {"release_wiki", on_release_wiki_activate, NULL, NULL, NULL },
    {"about", on_about1_activate, NULL, NULL, NULL }
};




// set this value to TRUE to utilize GTK builder XML file ./gscope4.cmb
// set this value to FALSE to utilize the built-in string array created by xxd -i gscope3.glade > interface.c
#define BUILD_WIDGETS_FROM_FILE     TRUE

// Revist: Possible use manually created widgets for GTK4
#if (!BUILD_WIDGETS_FROM_FILE)
#include    "interface.c"
#endif

gchar *refFile = NULL;
gchar *nameFile = NULL;
gchar *includeDir = NULL;
gchar *rcFile = NULL;
gchar *srcDir = NULL;
gchar *geometry = NULL;
gboolean option_error = FALSE;

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
        /*
        g_application_command_line_print (cmd_line, 
                                          "refOnly=%s\ncompress=%s\nnoBuld=%s\nrefFile=%s\n",
                                          settings.refOnly ? "TRUE" : "FALSE",
                                          settings.compressDisable ? "Disabled" : "Enabled",
                                          settings.noBuild ? "TRUE" : "FALSE",
                                          refFile
                                         );
        */
        g_application_command_line_set_exit_status (cmd_line, 0);       
    }

    /* Copy any dynamically allocated [argument] strings to the settings structure (and free them) */
    /* Buffer overflow protection:  Any [argument] string that exceeds the maximum allowed size is truncated */
    if (refFile)
    {strncpy(settings.refFile,    refFile,    MAX_STRING_ARG_SIZE); settings.refFile[MAX_STRING_ARG_SIZE - 1] = 0; g_free(refFile);}
    if (nameFile)
    {strncpy(settings.nameFile,   nameFile,   MAX_STRING_ARG_SIZE); settings.nameFile[MAX_STRING_ARG_SIZE - 1] = 0; g_free(nameFile);}
    if (includeDir)
    {strncpy(settings.includeDir, includeDir, MAX_STRING_ARG_SIZE); settings.includeDir[MAX_STRING_ARG_SIZE - 1] = 0; g_free(includeDir);}
    if (rcFile)
    {strncpy(settings.rcFile,     rcFile,     MAX_STRING_ARG_SIZE); settings.rcFile[MAX_STRING_ARG_SIZE - 1] = 0; g_free(rcFile);}
    if (srcDir)
    {strncpy(settings.srcDir,     srcDir,     MAX_STRING_ARG_SIZE); settings.srcDir[MAX_STRING_ARG_SIZE - 1] = 0; g_free(srcDir);}
    if (geometry)
    {strncpy(settings.geometry,   geometry,   MAX_STRING_ARG_SIZE); settings.geometry[MAX_STRING_ARG_SIZE - 1] = 0; g_free(geometry);}


    g_strfreev(args);
    g_option_context_free(context);

    printf("%s: done\n", __func__);

    return (G_SOURCE_REMOVE);   // Revisit: should no longer be needed since we aren't processing options in another thread.
}





/* WARNING: Do not call any functions from this handler that are affected by command line arguments 
            Place command-line dependent function calls in the 'activate' handler. */
static void startup (GApplication *app, gpointer *user_data)
{
    GtkBuilder  *builder;
    GSList      *list;
    GError      *error = NULL;
    GAction     *action;

    printf("** %s **\n", __func__);

    /* App-Standard location for pixmap files */
    add_pixmap_directory("../pixmaps");
    add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");  // The pixmap directory "added" last is the firt to be checked

    // Parse the application UI file(s) and add the requested object(s) to the widget database "builder"
    //==================================================================================================
    #if (BUILD_WIDGETS_FROM_FILE)
    {
        gchar ui_file_path[256];    // Revisit: Buffer overrun risk
        gchar ui_file[256];
        
        // Read to core UI definitions
        //============================
        if (readlink("/proc/self/exe", ui_file_path, sizeof(ui_file_path)) == -1)
        {
            fprintf(stderr, "Application abort: Could not get location of Gscope binary.\nUnable to load UI...\n");
            exit(EXIT_FAILURE);
        }
        ui_file_path[255] = '\0';        // Ensure path string is null terminated -- readlink() does not append a null if it truncates.
        my_dirname(ui_file_path);
        sprintf(ui_file, "%s%s", ui_file_path, "/gscope4.ui");
        builder = gtk_builder_new_from_file(ui_file);

        // Read supplemental UI definitions
        //=================================
        const char *splash_objects[] = {"gscope_splash", ""};
        sprintf(ui_file, "%s%s", ui_file_path, "/splash.ui");
        if (!gtk_builder_add_objects_from_file(builder, ui_file, splash_objects, &error))
        {
            fprintf(stderr, "UI object merge error: File: %s. Error: %s\n", ui_file, error ? error->message : "Unknown");
            if (error) g_error_free(error);
        }

        const char *prefs_objects[] = {"gscope_preferences", ""};
        sprintf(ui_file, "%s%s", ui_file_path, "/preferences.ui");
        if (!gtk_builder_add_objects_from_file(builder, ui_file, prefs_objects, &error))
        {
            fprintf(stderr, "UI object merge error: File: %s. Error: %s\n", ui_file, error ? error->message : "Unknown");
            if (error) g_error_free(error);
        }

        const char *quit_objects[] = {"quit_confirm_dialog", ""};
        sprintf(ui_file, "%s%s", ui_file_path, "/quit.ui");
        if (!gtk_builder_add_objects_from_file(builder, ui_file, quit_objects, &error))
        {
            fprintf(stderr, "UI object merge error: File: %s. Error: %s\n", ui_file, error ? error->message : "Unknown");
            if (error) g_error_free(error);
        }
    }
    #else
        builder = gtk_builder_new_from_string(gscope3_glade, gscope3_glade_len);
    #endif

    //Create a list of all builder objects
    //====================================
    list = gtk_builder_get_objects(builder);
    g_slist_foreach(list, my_add_widget, NULL);

    // Instantiate the main window 'gscope_main' and
    // connect it to the top-level application window
    //======================================================
    GObject *gscope_main = gtk_builder_get_object (builder, "gscope_main");
    gtk_window_set_application (GTK_WINDOW(gscope_main), GTK_APPLICATION(app));
    gtk_widget_set_visible (GTK_WIDGET(gscope_main), FALSE);

    // Instantiate the splash screen 'gscope_splash'
    //==============================================
    GObject *gscope_splash = gtk_builder_get_object(builder, "gscope_splash");
    gtk_window_set_transient_for(GTK_WINDOW(gscope_splash), GTK_WINDOW(gscope_main));
    gtk_widget_set_visible(GTK_WIDGET(gscope_splash), TRUE);

    // Instantiate the preferences dialog 'gscope_preferences'
    //========================================================
    GObject *gscope_preferences = gtk_builder_get_object(builder, "gscope_preferences");
    gtk_window_set_transient_for(GTK_WINDOW(gscope_preferences), GTK_WINDOW(gscope_main));
    gtk_widget_set_visible(GTK_WIDGET(gscope_preferences), FALSE);
    gtk_window_set_hide_on_close(GTK_WINDOW(gscope_preferences), TRUE);

    // Instantiate the 'quit_confirm_dialog'
    //=====================================
    GObject *quit_confirm_dialog = gtk_builder_get_object(builder, "quit_confirm_dialog");
    gtk_window_set_transient_for(GTK_WINDOW(quit_confirm_dialog), GTK_WINDOW(gscope_main));
    gtk_widget_set_visible(GTK_WIDGET(quit_confirm_dialog), FALSE);
    gtk_window_set_hide_on_close(GTK_WINDOW(quit_confirm_dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(quit_confirm_dialog),TRUE);

    
    // Configure Menu Actions
    //=======================
    g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries), app);

    //You can disable a menu item by removing its entry from the action map
    //g_action_map_remove_action(G_ACTION_MAP(app), "quit");


    g_object_unref(builder);
}


static void activate (GtkApplication *app, gpointer *user_data)
{

    printf("** %s **\n", __func__);

    if (settings.version)
    {
        printf("GSCOPE version %s\n", VERSION);
        exit(EXIT_SUCCESS);
    }

    
    // Settings-conditional startup
    // (Must run AFTER command_line handler)
    //======================================

    if (settings.refOnly)
    {
        APP_CONFIG_init(NULL);
        BUILD_initDatabase(NULL);
    }
    else
    {
        APP_CONFIG_init(GTK_WIDGET(my_lookup_widget("gscope_splash")));
        CALLBACKS_init(GTK_WIDGET(my_lookup_widget("gscope_main")));
        CALLBACKS_register_app(app);    // Give a reference to callbacks.c for application shutdown.
        BUILD_initDatabase(GTK_WIDGET(my_lookup_widget("splash_progressbar")));
    }


    gtk_widget_set_visible(my_lookup_widget("gscope_splash"), FALSE);
    gtk_widget_set_visible (my_lookup_widget("gscope_main"), TRUE);


    // Comment out for standard theme, un-comment for stardard-dark theme -- Revisit: make this a view-->checkbox-dark-theme [on/off]
    // g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);

    //=====================================================
    // Reconcile start-up settings with stateful UI actions
    //=====================================================

    GAction     *my_action;
    GVariant    *action_state;

    // *** ignorecase ***
    my_action = g_action_map_lookup_action(G_ACTION_MAP(app), "ignorecase");
    action_state = g_action_get_state(my_action);
    if ( g_variant_get_boolean(action_state) != settings.ignoreCase )
        g_simple_action_set_state(G_SIMPLE_ACTION(my_action), g_variant_new_boolean(settings.ignoreCase));
    g_variant_unref(action_state);

    // *** useeditor ***
    my_action = g_action_map_lookup_action(G_ACTION_MAP(app), "useeditor");
    action_state = g_action_get_state(my_action);
    if ( g_variant_get_boolean(action_state) != settings.useEditor)
        g_simple_action_set_state(G_SIMPLE_ACTION(my_action), g_variant_new_boolean(settings.useEditor));
    g_variant_unref(action_state);

    // *** reusewin***
    my_action = g_action_map_lookup_action(G_ACTION_MAP(app), "reusewin");
    action_state = g_action_get_state(my_action);
    if ( g_variant_get_boolean(action_state) != settings.reuseWin)
        g_simple_action_set_state(G_SIMPLE_ACTION(my_action), g_variant_new_boolean(settings.reuseWin));
    g_variant_unref(action_state);

     // *** retaininput ***
    my_action = g_action_map_lookup_action(G_ACTION_MAP(app), "retaininput");
    action_state = g_action_get_state(my_action);
    if ( g_variant_get_boolean(action_state) != settings.retainInput)
        g_simple_action_set_state(G_SIMPLE_ACTION(my_action), g_variant_new_boolean(settings.retainInput));
    g_variant_unref(action_state);

     // *** smartquery ***
    my_action = g_action_map_lookup_action(G_ACTION_MAP(app), "smartquery");
    action_state = g_action_get_state(my_action);
    if ( g_variant_get_boolean(action_state) != settings.smartQuery)
        g_simple_action_set_state(G_SIMPLE_ACTION(my_action), g_variant_new_boolean(settings.smartQuery));
    g_variant_unref(action_state);


}


static int command_line(GApplication *app, GApplicationCommandLine *cmdline)
{
    printf("\n%s: enter\n", __func__);

    // Keep the application running until we are done with this commandline
    g_application_hold (app);     // Increase the use count of 'application'

    // Load 'cmdline' object into a key/value table (release the application when cmdline object is destroyed)
    g_object_set_data_full (G_OBJECT (cmdline), "application", app, (GDestroyNotify)g_application_release);

    // Why would we ever want command line handling to be handled in the main/idle loop????
    // g_object_ref (cmdline);                  // Increase the reference count of 'cmdline' (refcount decremented by handler)
    // g_idle_add (cmd_line_handler, cmdline);  // Call command line handler from THE main-loop('default-idle' priority)
    
    cmd_line_handler(cmdline);
    printf("%s: All options set\n\n", __func__);

    // When command_line option is processed, the activate signal is not automatically issued
    activate(GTK_APPLICATION(app), NULL);

    printf("%s: done\n", __func__);
    return 0;
}


static void shutdown(GApplication *app, gpointer *user_data)
{
    printf("** Main: GTK4 Shutdown **\nDoes nothing right now.\n");
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

    printf("hello from %s: application_run complete\n", __func__);
    g_object_unref (app);

    printf("hello from %s: done\n", __func__);

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
            strcat(ui_file_path, "/gscope4.");

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

