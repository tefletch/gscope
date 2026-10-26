#include <gtk/gtk.h>

#define PACKAGE_DATA_DIR "../packages"
#define PACKAGE "gscope"
#define     MAX_HOME_ROOT   256
#define     MAX_APP_DIR     20
#define     MAX_HOME_PATH   MAX_HOME_ROOT + MAX_APP_DIR


static GList *pixmaps_directories = NULL;



/* Use this function to set the directory containing installed pixmaps. */
void add_pixmap_directory (const gchar *directory)
{
    printf("Adding pixmap dir: %s\n", directory);
    pixmaps_directories = g_list_prepend (pixmaps_directories, g_strdup (directory));
}



char *my_dirname(char *path)
{
    char *base = strrchr(path, '/');
    if (base)
        *base = '\0';   // Trim off the basename
    else
        *path = '\0';   // No path, just a basename, return a null string.

    return (path);
}



static void parsing_error(GtkCssProvider* self, GtkCssSection* section, GError* error, gpointer user_data)
{
    GString     *sect_string;

    printf("*** %s ***\n", __func__);

    sect_string = g_string_new(NULL);
    
    gtk_css_section_print(section, sect_string);
    printf("CSS parsing error: Section: %s: ", sect_string->str);
    g_string_free(sect_string, TRUE);
    if ( error )
        printf("%s\n", error->message);
    else
        printf("Unknown Parsing Error\n");
}


static void gtk_config_parse(char *gtk_config_file)
{
    printf("Parsing GTK config: %s\n", gtk_config_file);

    {
        GtkCssProvider  *provider;
        GdkDisplay      *display;

        provider = gtk_css_provider_new();

        display  = gdk_display_get_default();

        g_signal_connect(provider, "parsing-error", G_CALLBACK(parsing_error), NULL);
        gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_path(provider, gtk_config_file);

        g_object_unref(provider);       // This doesn't destrory the provider, the display still has a reference.
    }
}




static gboolean cmd_line_handler(gpointer data)
{


    return (G_SOURCE_REMOVE);
}



/* WARNING: Do not call any functions from this handler that are affected by command line arguments 
            Place command-line dependent function calls in the 'activate' handler. */
static void startup (GApplication *app, gpointer *user_data)
{
    GtkBuilder  *builder;
    gchar ui_file[256];
    gchar ui_file_path[256];
    GError      *error = NULL;

    add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");  // The pixmap directory "added" last is the firt to be checked

    if (readlink("/proc/self/exe", ui_file_path, sizeof(ui_file_path)) == -1)
    {
        fprintf(stderr, "Application abort: Could not get location of Gscope binary.\nUnable to load UI...\n");
        exit(EXIT_FAILURE);
    }
    ui_file_path[255] = '\0';        // Ensure path string is null terminated -- readlink() does not append a null if it truncates.
    my_dirname(ui_file_path);
    snprintf(ui_file, sizeof(ui_file), "%s%s", ui_file_path, "/start_gui.ui");
    builder = gtk_builder_new_from_file(ui_file);



    // Instantiate the 'start_gui'
    //=====================================
    GObject *start_gui = gtk_builder_get_object (builder, "start_gui");
    gtk_window_set_application (GTK_WINDOW(start_gui), GTK_APPLICATION(app));
    gtk_widget_set_visible (GTK_WIDGET(start_gui), TRUE);

    g_object_unref(builder);
}


// 'Activated' Callback
static void activate (GtkApplication *app, gpointer user_data)
{
    GtkBuilder  *builder;
    char        *home;
    gchar       gtk_config_file[256] = {0};
    gchar       app_home[MAX_HOME_PATH + 1] = {0};

    // APP_CONFIG_ini()
    home = getenv("HOME");

    if (home == NULL || strlen(home) > MAX_HOME_ROOT)
    {
        fprintf(stderr,"\nWarning: The $HOME environment variable is not defined (or path too long)\n"
                "G-scope cannot read or write configuration files\n\n");
        return;
    }

    snprintf(app_home, MAX_HOME_PATH + 1, "%s%s", home, "/.gscope/");

    strncpy(gtk_config_file, app_home, 235);
    strncat(gtk_config_file, "gscope.css", 20);
 
    gtk_config_parse(gtk_config_file);    


    //CALLBACKS_init(GTK_WIDGET(my_lookup_widget("gscope_main")));
    //builder = gtk_builder_new_from_file("./start_gui.ui");
    printf("Exit: %s\n", __func__);
}


static int command_line(GApplication *app, GApplicationCommandLine *cmdline)
{
    printf("\n%s: enter\n", __func__);

    // Keep the application running until we are done with this commandline
    g_application_hold (app);     // Increase the use count of 'application'

    // Load 'cmdline' object into a key/value table (release the application when cmdline object is destroyed)
    g_object_set_data_full (G_OBJECT (cmdline), "application", app, (GDestroyNotify)g_application_release);

    cmd_line_handler(cmdline);
    //printf("%s: All options set\n\n", __func__);

    // When command_line option is processed, the activate signal is not automatically issued
    activate(GTK_APPLICATION(app), NULL);
    
    //printf("========== startup complete ========\n\n");
    return 0;
}


int main (int argc, char **argv) {
    GtkApplication *app;
    int status;

    // Create a new GtkApplication instance
    app = gtk_application_new("gscope.gui", G_APPLICATION_DEFAULT_FLAGS | G_APPLICATION_HANDLES_COMMAND_LINE);
    
    // *** startup ***
    g_signal_connect(app, "startup", G_CALLBACK(startup), NULL);

    // *** activate ***
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    // *** command-line ***
    g_signal_connect(app, "command-line", G_CALLBACK(command_line), NULL);


    // Run the application
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return status;
}
