//***************************************************************************
//
// app_config.c - Initialize the application settings
//
// Settings Managment Architecture/Rules
// =====================================
//
// 1. All settings have a hard-coded (built-in) default [base] value.
//
// 2. Some settings are 'persistent' across sessions, others are not.
//
// 3. Non-persistent settings use the hard-coded (built-in) default value,
//    or they are constructed during the application initialization process.
//
// 4. Command line arguments override hard-coded and/or persistent settings but
//    are not "sticky" [e.g. The stored value of a persistent setting is not
//    altered].
//
// 5. "Quick Pick" settings override all other settings sources [hard-coded,
//    command-line-argument, persistent] but are not "sticky".  The Quick
//    Pick settings are settings that a user is likely to want to actively
//    modify during a session.
//
// 6. By definition, all settings presented/modified in the apllication
//    "Preferences" dialog are persistent (sticky).
//
// Rules of Persistence.
// =====================
//
// 1. All persistent settings are stored in an application-specific plain
//    text configuration file(s) using the GLib Key-value file parser utility
//    format.
//
// 2. The name and or location of the configuration file(s) is application
//    defined.
//
// 3. If no application configuration file is found at the designated
//    location(s), a new, fully populated configuration file is created
//    at the application-specific location.
//
// 4. Configuration file content evolution/mutation:  If a newer version
//    of an application adds new persistent configuration values, the
//    hard-coded defaults for those new values are stored in the
//    configuration file for subsequent sessions.  If an existing value
//    in the configuration file is obsoleted by the new application
//    version, the old, obsolete value is ignored.  The user's prior
//    persistent settings that are still viable are always preserved.
//
// 5. End users can "factory reset" their configuration by deleting
//    the configuration file and re-staring the application.
//
//***************************************************************************


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <stdint.h>
#include <errno.h>

#include "app_config.h"
#include "version.h"
#include "dir.h"
#include "utils.h"

#ifdef GTK4_BUILD
#include "search.h"
#include "gtk4_aux.h"
#endif


//===============================================================
// Defines
//===============================================================
#define     MAX_UPDATE_MSG  1000
#define     MAX_HOME_ROOT   256
#define     MAX_APP_DIR     20
#define     MAX_HOME_PATH   MAX_HOME_ROOT + MAX_APP_DIR

/*** Version Checking Defines ***/
#define     VCHECK_SIZE     30

#define     MAX_LIST_SIZE           1023            /* Max size for all lists.  All lists must be the same size */
#define     MAX_OVERRIDE_PATH_SIZE  256

#if defined(GTK3_BUILD) || defined(GTK4_BUILD)  // GTK3/GTK4 (gscope.css)
#define     CURRENT_CONFIG_VERSION   "005"
#define     CONFIG_VERSION_TAG       "/*Version="
#else               // GTK2 (gtkrc)
#define     CURRENT_CONFIG_VERSION   "005"
#define     CONFIG_VERSION_TAG       "#!Version="
#endif




//===============================================================
// Private Function Prototypes
//===============================================================
static gboolean  create_app_config_file(const char *filename);
static void      parse_app_config        (const char *filename);
static gboolean  create_template_file    (const char *filename, gchar *template);
static void      rewrite_config_file     (void);
static gboolean  gtk_config_version_check(char *filename, char *version);
static gboolean  app_version_check       (char *old_string, char *new_string);
static void      string_trunc_warn       (gchar *string_name);
static void      pixmap_path_fixup       (char *filename, char *path, GtkWidget *parent);
static void      gtk_config_parse        (char *gtk_config_file);

static gboolean  create_gtk_config_file  (const char *filename);  /* GTK V2/V3 specific config file */


//===============================================================
// Global Variables
//===============================================================

// Applicaton settings (with defaults initialized)

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


// Settings (initialized from the config file) that can be temporarily overriden
// via the "options menu" need to be saved in order to keep the preferences
// dialog and options menu in-sync.
//
// A preference dialog" change
//          - updates the application behavior
//          - updates the options menu
//          - is remembered across sessions
//
// An "options menu" change
//          - temporarily alters the application behavior
//          - does not alter the preferences dialog
//          - is NOT remembered across sessions

sticky_t sticky_settings;


void helper_app_missing(gchar *app, gchar *app_type)
{
    char *message;

    asprintf(&message,
    "Missing helper App [%s]: " BRIGHT_RED(%s) "\n"
    "Install missing App or Change configured %s.\n"
    "G-Scope [%s] operations will fail until this is corrected.\n\n", app_type, app, app_type, app_type);

    printf("%s", message);
    free(message);
}

//===============================================================
// File-Private Globals
//===============================================================
GKeyFile *key_file;
gchar app_config_file[256] = {0};
gchar app_snapshot_file[256] = {0};



//===============================================================
// Public Functions
//===============================================================

void APP_CONFIG_init(GtkWidget *gscope_splash)
{
    #ifndef GTK4_BUILD
    static      GtkWidget   *MsgDialog;
    #endif
    char        new_version_string[20];
    char        old_version_string[20];
    gchar       gtk_config_file[256] = {0};
    gchar       app_home[MAX_HOME_PATH + 1] = {0};
    gboolean    new_version_detected = FALSE;
    char        *home;

    home = getenv("HOME");

    if (home == NULL || strlen(home) > MAX_HOME_ROOT)
    {
        fprintf(stderr,"\nWarning: The $HOME environment variable is not defined (or path too long)\n"
                "G-scope cannot read or write configuration files\n\n");
        return;
    }

    snprintf(app_home, MAX_HOME_PATH + 1, "%s%s", home, "/.gscope/");

    strncpy(gtk_config_file, app_home, 235);
    #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
        strncat(gtk_config_file, "gscope.css", 20);
    #else   // GTK2
        strncat(gtk_config_file, "gtkrc", 20);
    #endif

    strncpy(settings.histFile, app_home, 235);
    strncat(settings.histFile, "history", 20);

    if ( strcmp(settings.rcFile, rcFileDef) == 0)       /* Default Behavior - no command line override */
    {
        if ( g_file_test("gscoperc", G_FILE_TEST_EXISTS) )
        {
            strncat(app_config_file, "gscoperc", 20);       /* Use the 'local' override config file */
        }
        else
        {
            strncpy(app_config_file, app_home, 235);
            strncat(app_config_file, "gscoperc", 20);       /* Use the default config file */
        }

        snprintf(settings.rcFile, MAX_STRING_ARG_SIZE, "%s", app_config_file);
    }
    else   // use settings.rcFile as-is
    {
        snprintf(app_config_file, MAX_STRING_ARG_SIZE, "%s", settings.rcFile);
    }

    //printf("home=%s\napp=%s\ngtk=%s\n", app_home, app_config_file, gtk_config_file);

    // if the $HOME/.gscope directory doesn't exist, create it
    if ( !g_file_test(app_home, G_FILE_TEST_IS_DIR) )
    {
        if ( g_mkdir(app_home,0755) )
        {
            fprintf(stderr, "\nError:  Unable to create configuration directory: %s\n"
                            "Fix the problem and try again.\nStartup aborted.\n\n", app_home);
            exit(EXIT_FAILURE);
        }
    }

    /*** Applicaton Configuration File Processing ***/
    /************************************************/

    // if the application config file exists
    if ( g_file_test(app_config_file, G_FILE_TEST_EXISTS ) )
    {
        // process the application config file
        parse_app_config(app_config_file);
        new_version_detected = app_version_check(old_version_string, new_version_string);
    }
    else
    {
        // create an app-config file and populate it with the program defaults
        if ( create_app_config_file(app_config_file) )
        {
            parse_app_config(app_config_file);
            new_version_detected = app_version_check(old_version_string, new_version_string);
        }
        else
        {
            if (gscope_splash)  // if we are in GUI mode
            {
                #ifndef GTK4_BUILD
                MsgDialog = gtk_message_dialog_new_with_markup (
                        GTK_WINDOW (gscope_splash),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_WARNING,
                        GTK_BUTTONS_CLOSE,
                        "Unable to create default G-Scope configuration file:\n"
                        "%s.\n\n"
                        "Starting program using factory defaults.\n"
                        "G-Scope will not retain configuration changes.", app_config_file);

                gtk_dialog_run (GTK_DIALOG (MsgDialog));
                gtk_widget_destroy (GTK_WIDGET (MsgDialog));
                #else   // GTK4 Build
                    char *message;
                    asprintf(&message,
                        "Unable to create default G-Scope configuration file\n"
                        "Filename: %s\n\n"
                        "Starting program using factory defaults.\n"
                        "G-Scope will not retain configuration changes.", app_config_file);

                    GTK4_message_dialog(GTK_MESSAGE_WARNING, message);

                    free(message);
                #endif
            }
            else        // not in GUI mode
            {
                fprintf(stderr,
                        "Unable to create default G-Scope configuration file:\n"
                        "%s.\n\n"
                        "Starting program using factory defaults.\n"
                        "G-Scope will not retain configuration changes.\n", app_config_file);
            }
        }
    }


    /*** Sanity Check Application Helper App Configuration Settings ***/
    if (gscope_splash)
    {
        // These chacks are only relevent for GUI mode
        if ( !my_command_check(settings.fileEditor) )  helper_app_missing(settings.fileEditor, "Editor App");
        if ( !my_command_check(settings.terminalApp) ) helper_app_missing(settings.terminalApp, "Terminal App");
        if ( !my_command_check(settings.fileManager) ) helper_app_missing(settings.fileManager, "File Manager App");
    }

    /*** GTK Configuration File Processing ***/
    /*****************************************/

    if (gscope_splash)   // Only perform GTK configuration if we are in GUI mode
    {
        // if the application gtkrc config file exists
        if ( (g_file_test(gtk_config_file, G_FILE_TEST_EXISTS)) && (gtk_config_version_check(gtk_config_file, CURRENT_CONFIG_VERSION)) )
        {
            // We found a pre-existing application gtkrc (or gscope.css)
            // file.  Make sure the 'pixmap_path' is correct for this
            // gscope instance.
            // ==============================================
            pixmap_path_fixup(gtk_config_file, PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps", gscope_splash);

            // process the config file
            //=======================
            gtk_config_parse(gtk_config_file);
        }
        else
        {
            // No application gtkrc file found
            // create a default gtkrc file
            //================================

            if ( !create_gtk_config_file(gtk_config_file) )
            {
                #ifndef GTK4_BUILD
                MsgDialog = gtk_message_dialog_new_with_markup (
                        GTK_WINDOW (gscope_splash),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_WARNING,
                        GTK_BUTTONS_CLOSE,
                        "Unable to create default G-Scope button theme\ntemplate file: %s.\n\n"
                        "It may not be pretty, but\nG-Scope will still work.",
                        gtk_config_file);

                gtk_dialog_run (GTK_DIALOG (MsgDialog));
                gtk_widget_destroy (GTK_WIDGET (MsgDialog));
                #else
                    char *message;
                    asprintf(&message,
                        "Unable to create default G-Scope button theme\ntemplate file: %s.\n\n"
                        "It may not be pretty, but\nG-Scope will still work.",
                        gtk_config_file);


                    //GTK4_message_dialog_stub(message);
                    free(message);
                #endif
            }
            else
            {
                #ifndef GTK4_BUILD
                MsgDialog = gtk_message_dialog_new_with_markup (
                        GTK_WINDOW (gscope_splash),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_INFO,
                        GTK_BUTTONS_CLOSE,
                        "<span weight=\"bold\" size=\"large\">Updating obsolete (or missing)\n"
                        "G-Scope button theme.</span>\n\n"
                        "File: <span weight=\"bold\">%s</span>",
                        gtk_config_file);

                gtk_dialog_run (GTK_DIALOG (MsgDialog));
                gtk_widget_destroy (GTK_WIDGET (MsgDialog));
                #else   // GTK4_BUILD
                    char *message;
                    asprintf(&message,
                        "<span weight=\"bold\" size=\"large\">Updating obsolete (or missing)\n"
                        "G-Scope button theme.</span>\n\n"
                        "File: <span weight=\"bold\">%s</span>",
                        gtk_config_file);

                    GTK4_message_dialog(GTK_MESSAGE_INFO, message);
                    free(message);
                #endif

                // process the newly created config file
                //=====================================
                gtk_config_parse(gtk_config_file);
            }
        }

        if (new_version_detected)
        {
            GtkWidget *update_dialog;
            GtkWidget *update_dialog_hbox;
            GtkWidget *update_dialog_image;
            GtkWidget *update_dialog_notification_label;
            GtkWidget *release_notes_button;

            #ifdef GTK4_BUILD
            GtkWidget *update_dialog_vbox;
            #endif

            #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
            GtkWidget *update_dialog_content_area;
            #else
            GtkWidget *update_dialog_vbox;
            GtkWidget *update_dialog_action_area;
            #endif

            gchar     update_msg[MAX_UPDATE_MSG];

            snprintf(update_msg, MAX_UPDATE_MSG, "<span weight=\"bold\" size=\"large\">\nYour installation of Gscope has been updated.</span>\n\n"
                                 "Old Version:   %s\n"
                                 "New Version:  %s\n\n"
                                 "%s"
                                 "Follow the link below to view the latest release notes.",
                                 old_version_string,
                                 new_version_string,
                                 VERSION_ANNOUNCE);

            update_dialog_notification_label = gtk_label_new(NULL);
            gtk_label_set_markup (GTK_LABEL(update_dialog_notification_label), update_msg);
            gtk_widget_set_name (update_dialog_notification_label, "update_dialog_notification_label");
            gtk_widget_show (update_dialog_notification_label);

            #ifndef GTK4_BUILD
            update_dialog_image = gtk_image_new_from_icon_name ("gtk-dialog-info", GTK_ICON_SIZE_DIALOG);
            gtk_widget_set_name (update_dialog_image, "update_dialog_image");
            gtk_widget_show (update_dialog_image);

            update_dialog = gtk_dialog_new ();
            gtk_widget_set_name (update_dialog, "update_dialog");
            gtk_window_set_title(GTK_WINDOW (update_dialog), "");
            gtk_window_set_transient_for(GTK_WINDOW (update_dialog), GTK_WINDOW(gscope_splash));
            gtk_window_set_position (GTK_WINDOW (update_dialog), GTK_WIN_POS_CENTER_ON_PARENT);
            gtk_window_set_modal (GTK_WINDOW (update_dialog), TRUE);
            gtk_window_set_resizable (GTK_WINDOW (update_dialog), FALSE);
            gtk_window_set_destroy_with_parent (GTK_WINDOW (update_dialog), TRUE);

            #ifdef GTK3_BUILD

            update_dialog_content_area = gtk_dialog_get_content_area( GTK_DIALOG(update_dialog) );

            update_dialog_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_set_name (update_dialog_hbox, "update_dialog_hbox");
            gtk_container_add(GTK_CONTAINER(update_dialog_content_area), update_dialog_hbox);

            gtk_box_pack_start (GTK_BOX (update_dialog_hbox), update_dialog_image, FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (update_dialog_hbox), update_dialog_notification_label, TRUE, TRUE, 0);


            gtk_widget_set_hexpand(update_dialog_notification_label, TRUE);
            gtk_widget_set_halign(update_dialog_notification_label, GTK_ALIGN_FILL);

            update_dialog_image = gtk_image_new_from_icon_name ("gtk-dialog-info",GTK_ICON_SIZE_DIALOG);
            
            gtk_widget_set_name (update_dialog_image, "update_dialog_image");
            gtk_widget_set_hexpand(update_dialog_image, FALSE);
            gtk_widget_show (update_dialog_image);

            release_notes_button = gtk_link_button_new_with_label("https://github.com/tefletch/gscope/wiki/Gscope-Release-Notes",
                                                                   "Gscope Release Notes");
      
            gtk_widget_set_name (release_notes_button, "release_notes_button");
            gtk_dialog_add_action_widget(GTK_DIALOG(update_dialog), release_notes_button, 0);
            gtk_widget_show_all(update_dialog);

            #else   // GTK2

            gtk_window_set_type_hint (GTK_WINDOW (update_dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
            gtk_dialog_set_has_separator (GTK_DIALOG (update_dialog), FALSE);

            update_dialog_vbox = GTK_DIALOG (update_dialog)->vbox;
            gtk_widget_set_name (update_dialog_vbox, "update_dialog_vbox");
            gtk_widget_show (update_dialog_vbox);

            update_dialog_hbox = gtk_hbox_new (FALSE, 0);
            gtk_widget_set_name (update_dialog_hbox, "update_dialog_hbox");
            gtk_widget_show (update_dialog_hbox);
            gtk_box_pack_start (GTK_BOX (update_dialog_vbox), update_dialog_hbox, TRUE, TRUE, 0);

            gtk_box_pack_start (GTK_BOX (update_dialog_hbox), update_dialog_image, FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (update_dialog_hbox), update_dialog_notification_label, TRUE, TRUE, 0);

            update_dialog_action_area = GTK_DIALOG (update_dialog)->action_area;
            gtk_widget_set_name (update_dialog_action_area, "update_dialog_action_area");
            gtk_widget_show (update_dialog_action_area);
            gtk_button_box_set_layout (GTK_BUTTON_BOX (update_dialog_action_area), GTK_BUTTONBOX_SPREAD);

            release_notes_button = gtk_link_button_new_with_label("https://github.com/tefletch/gscope/wiki/Gscope-Release-Notes",
                                                                  "Gscope Release Notes");

            gtk_widget_set_name (release_notes_button, "release_notes_button");
            gtk_dialog_add_action_widget (GTK_DIALOG (update_dialog), release_notes_button, GTK_RESPONSE_APPLY);
            GTK_WIDGET_SET_FLAGS (release_notes_button, GTK_CAN_DEFAULT);

            gtk_widget_show_all(update_dialog);

            gtk_dialog_run (GTK_DIALOG (update_dialog));
            gtk_widget_destroy (GTK_WIDGET (update_dialog));
            #endif
            
            #else   // GTK4_BUILD
            update_dialog = gtk_window_new ();
            gtk_widget_set_name (update_dialog, "update_dialog");
            gtk_window_set_transient_for(GTK_WINDOW (update_dialog), GTK_WINDOW(gscope_splash));
            gtk_window_set_modal (GTK_WINDOW (update_dialog), TRUE);
            gtk_window_set_resizable (GTK_WINDOW (update_dialog), FALSE);
            gtk_window_set_destroy_with_parent (GTK_WINDOW (update_dialog), TRUE);
            gtk_window_set_title(GTK_WINDOW (update_dialog), "");
            //gtk_window_set_position (GTK_WINDOW (update_dialog), GTK_WIN_POS_CENTER_ON_PARENT); // This API not available in GTK4
            update_dialog_image = gtk_image_new_from_icon_name ("gtk-dialog-info");
            gtk_image_set_icon_size (GTK_IMAGE (update_dialog_image), GTK_ICON_SIZE_LARGE);
            

            gtk_widget_set_name (update_dialog_image, "update_dialog_image");
            gtk_widget_show (update_dialog_image);
            update_dialog_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_set_name (update_dialog_hbox, "update_dialog_hbox");
            
            gtk_box_prepend (GTK_BOX (update_dialog_hbox), update_dialog_image);
            gtk_box_prepend (GTK_BOX (update_dialog_hbox), update_dialog_notification_label);

            release_notes_button = gtk_link_button_new_with_label("https://github.com/tefletch/gscope/wiki/Gscope-Release-Notes",
                                                                  "Gscope Release Notes");
            gtk_widget_set_name (release_notes_button, "release_notes_button");

            update_dialog_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_window_set_child(GTK_WINDOW(update_dialog), update_dialog_vbox);
            gtk_box_prepend (GTK_BOX (update_dialog_vbox), release_notes_button);
            gtk_box_prepend (GTK_BOX (update_dialog_vbox), update_dialog_hbox);

            gtk_window_destroy(GTK_WINDOW(update_dialog));

            #endif
       }
    }
    return;
}


void APP_CONFIG_set_boolean(const gchar *key, gboolean value)
{
    //printf("CONFIG_SET: Key = %s, Value = %s\n", key, value ? "TRUE" : "FALSE");
    g_key_file_set_boolean(key_file, "Defaults", key, value);
    rewrite_config_file();
}



void  APP_CONFIG_set_integer(const gchar *key, gint value)
{
    //printf("app_config set integer = %d\n", value);
    g_key_file_set_integer(key_file, "Defaults", key, value);
    rewrite_config_file();
}



void  APP_CONFIG_set_string(const gchar *key, const gchar *value)
{
    //printf("app_config set string = %s\n", value);
    g_key_file_set_string(key_file, "Defaults", key, value);
    rewrite_config_file();
}


//**********************************************************************************************
// APP_CONFIG_valid_list
//
// Function:  Validate a pattern matching list according to the following criteria:
//    If the list length is <= MAX_LIST_SIZE characters long, and
//    If a valid delimiter character is defined [first and last char of pattern defines the
//      delimiter and they must be the same character].
//
// If the list passes the validation, set *delim_char equal to the derived delimiter value and
// return TRUE.
//
// If the list fails validation, print a critical error message using list_name+error and return
// FALSE.
//**********************************************************************************************

gboolean APP_CONFIG_valid_list(const char *list_name, char *list_ptr, char *delim_char)
{

    int pattern_len;
    char d_head, d_tail;

    pattern_len = strlen(list_ptr);

    if (pattern_len > MAX_LIST_SIZE)
    {
        fprintf(stderr, "List syntax error: '%s' pattern exceeds %d characters.\n", list_name, MAX_LIST_SIZE);
        return(FALSE);
    }

    d_head = *list_ptr;
    d_tail = *(list_ptr + pattern_len - 1);

    if (d_head != d_tail || pattern_len == 1 || pattern_len == 2)
    {
        fprintf(stderr, "List syntax error: Inconsistent delimiter char: '%c' != '%c'\nPattern name: '%s'\nPattern value: '%s'\n", d_head, d_tail, list_name, list_ptr);
        return(FALSE);
    }

    /* if we make it this far, we are good to go... */

    *delim_char = d_head;

    // printf("'%s' pattern: %s\n'%s' delimiter char is '%c' \n---------\n", list_name, list_ptr, list_name, *delim_char);

    return(TRUE);
}



//===================================
// Private Functions
//===================================

#ifdef GTK4_BUILD
static void parsing_error(GtkCssProvider* self, GtkCssSection* section, GError* error, gpointer user_data)
{
    GString     *sect_string;

    printf("*** %s ***\n", __func__);

    sect_string = g_string_new(NULL);
    
    gtk_css_section_print(section, sect_string);
    printf("CSS parsing error: Section: %s: ", sect_string->str);
    free(sect_string);
    if ( error )
        printf("%s\n", error->message);
    else
        printf("Unknown Parsing Error\n");
}
#endif


static void gtk_config_parse(char *gtk_config_file)
{
    printf("Parsing GTK config: %s\n", gtk_config_file);

    #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
    {
        GtkCssProvider  *provider;
        GdkDisplay      *display;
        #ifndef GTK4_BUILD
        GdkScreen       *screen;
        #endif

        provider = gtk_css_provider_new();

        #if 1 // applies to all widgets on the screen

        display  = gdk_display_get_default();

        #ifndef GTK4_BUILD
        screen   = gdk_display_get_default_screen(display);
        gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_path(provider, gtk_config_file, NULL);
        #else
        g_signal_connect(provider, "parsing-error", G_CALLBACK(parsing_error), NULL);
        gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        gtk_css_provider_load_from_path(provider, gtk_config_file);
        #endif

        g_object_unref(provider);       // This doesn't destrory the provider, the display still has a reference.

        #else  // affect only specific widgets  g_object_get_data(G_OBJECT(widget), name)
        GtkStyleContext *context;

        context = gtk_widget_get_style_context(GTK_WIDGET(gtk_builder_get_object(builder, "find_c_identifier_button")));
        gtk_css_provider_load_from_path(provider, "gscope.css", NULL);

        gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(provider);

        #endif
    }
    #else       // GTK2
        gtk_rc_parse(gtk_config_file);
    #endif
}




static gboolean gtk_config_version_check(char *filename, char *version)
{

    FILE *rc_file;
    char file_buf[VCHECK_SIZE];
    gboolean retval = FALSE;


    /* Check the version tag */
    rc_file = fopen(filename, "r");

    if (rc_file)
    {
        // grab the first VCHECK_SIZE bytes from the file
        if (fread(file_buf, 1, VCHECK_SIZE, rc_file) < VCHECK_SIZE)
        {
            fclose(rc_file);
            return(retval);
        }

        if (strncmp(file_buf, CONFIG_VERSION_TAG, 10) == 0)   // if we find the tag: #!Version=
        {
            /* Check the version number */
            if (strncmp(file_buf + 10, version, 3) >= 0)   // If version string in file is >= version string param.
            {
                retval = TRUE;
            }
        }

        fclose(rc_file);
    }

    return(retval);
}



static gboolean app_version_check(char *old_string, char *new_string)
{
    gint major_version, minor_version;
    gint my_version, old_version;
    GError *error=NULL;

    /* Convert the current Version to an integer */
    sscanf(VERSION, "%d.%d", &major_version, &minor_version);
    my_version = (major_version * 1000) + minor_version;

    if ( my_version > settings.trackedVersion)
    {
        old_version = g_key_file_get_integer(key_file, "Defaults", "trackedVersion", &error);
        if (old_version == 0) old_version = 1020;  /* if no version tracking info available, assume last version was 1.20 */

        sprintf(old_string,"%d.%d", old_version/1000, old_version%1000);

        strcpy(new_string, VERSION);

        APP_CONFIG_set_integer("trackedVersion", my_version);
        return(TRUE);
    }
    return(FALSE);
}



static void pixmap_path_fixup(char *filename, char *path, GtkWidget *parent)
{
    FILE    *config_file = NULL;
    char    *config_file_buf = NULL;
    char    *sub_string;
    struct  stat statstruct;
    gboolean path_ok = FALSE;
    size_t  num_bytes;

    // open gtk config file and locate pixmap path string
    if ( ((config_file = fopen(filename, "rwb")) != NULL) && (fstat(fileno(config_file), &statstruct) == 0) )
    {
        if ( (config_file_buf = g_malloc(statstruct.st_size + 1)) )                         // and we can alloc a buffer
        {
            num_bytes = fread(config_file_buf, 1, statstruct.st_size, config_file);
            if ( num_bytes == statstruct.st_size ) // and we can read the file into the buffer
            {
                config_file_buf[num_bytes] = '\0';  // Null terminate the fread buffer
                sub_string = strstr(config_file_buf, path);
                if (sub_string)      // Might be a good path, check delimiters
                {
                    #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
                    if ( *(sub_string - 1) == '\'')
                        if ( *(sub_string + strlen(path)) == '/' )
                            path_ok = TRUE;
                    #else       // GTK2
                        if ( *(sub_string - 1) == '"' || *(sub_string - 1) == ':' )
                            if ( *(sub_string + strlen(path)) == '"' || *(sub_string + strlen(path)) == ':' )
                                path_ok = TRUE;
                    #endif
                }

                // compare in-file pixmap_path with "path" parameter
                if ( !path_ok )
                {
                    char *new_filename;

                    // Preserve possible end-user customization
                    my_asprintf(&new_filename, "%s.sav", filename);
                    if ( rename(filename, new_filename) < 0 )
                        fprintf(stderr,"Warning: Unable to rename original GTK config file:\n%s\n", strerror(errno));
                    create_gtk_config_file(filename);

                    #ifndef GTK4_BUILD
                    GtkWidget *MsgDialog;
                    MsgDialog = gtk_message_dialog_new_with_markup (
                            GTK_WINDOW (parent),
                            GTK_DIALOG_DESTROY_WITH_PARENT,
                            GTK_MESSAGE_INFO,
                            GTK_BUTTONS_CLOSE,
                            "<span weight=\"bold\" size=\"large\">Fixed bad pixmap_path in G-Scope button theme.</span>\n\n"
                            "If you had customizations in <span weight=\"bold\">%s</span> you will need to merge them back"
                            "from your old config file"
                            "(now named %s)",
                            filename,
                            new_filename);

                    gtk_dialog_run (GTK_DIALOG (MsgDialog));
                    gtk_widget_destroy (GTK_WIDGET (MsgDialog));
                    #else   // GTK4 build
                    char *message;
                    asprintf(&message,
                            "<span weight=\"bold\" size=\"large\">Fixed bad pixmap_path in G-Scope button theme.</span>\n\n"
                            "If you had customizations in <span weight=\"bold\">%s</span>\nyou will need to merge them back "
                            "from your old config file.\n\n<b>Saved file name:</b> %s",
                            filename,
                            new_filename);
                    GTK4_message_dialog(GTK_MESSAGE_INFO, message);
                    free(message);
                    #endif
                    g_free(new_filename);
                }
            }
            g_free(config_file_buf);
        }
    }
    if (config_file) fclose(config_file);
}



static void rewrite_config_file(void)
{
    gchar *config_data;
    gsize length;
    GError *error = NULL;
    FILE *config_file;

    config_data = g_key_file_to_data(key_file, &length, &error);
    if ((error))
    {
        fprintf(stderr,"Gscope error: %s", error->message);
        g_error_free (error);
        return;  // Don't try to update the config file
    }

    config_file = fopen(app_config_file, "w");

    if (config_file)
    {
        fwrite(config_data, 1, length, config_file);
        fclose(config_file);
    }
    else
        fprintf(stderr, "Error: Could not update config file: %s\n", app_config_file);

    g_free(config_data);
}



static void string_trunc_warn(gchar *string_name)
{
    fprintf(stderr, "Warning: Configuration string [%s] was truncated.\n", string_name);
}


static void parse_app_config(const char *filename)
{
    GError *error = NULL;
    gchar *tmp_ptr;

    key_file = g_key_file_new();

    if ( !g_key_file_load_from_file( key_file, filename, G_KEY_FILE_KEEP_COMMENTS, &error) )
    {
        fprintf(stderr,"\nWarning:  Unable to parse config file: %s\n", filename);
        fprintf(stderr,"Error:  %s\n\n", error->message);
        g_error_free (error);
        return;
    }

    // gscoperc was parsed OK, now load the settings
    //===============================================

    // *** trackedVersion ***
    settings.trackedVersion = g_key_file_get_integer(key_file, "Defaults", "trackedVersion", &error);
    if (error)  {  /* revert to default */
        settings.trackedVersion = trackedVersionDef;
        error = NULL;
    }

    // *** useEditor ***
    settings.useEditor = g_key_file_get_boolean(key_file, "Defaults", "useEditor", &error);
    if (error)  {  /* revert to default */
        settings.useEditor = useEditorDef;
        error = NULL;
    }

    // *** fileEditor ***
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "fileEditor", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.fileEditor, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
            string_trunc_warn("settings.FileEditor");
        g_free(tmp_ptr);
    }

    // *** retainInput ***
    settings.retainInput = g_key_file_get_boolean(key_file, "Defaults", "retainInput", &error);
    if (error)  {
        settings.retainInput = retainInputDef;
        error = NULL;
    }

    // *** retainFailed ***
    settings.retainFailed = g_key_file_get_boolean(key_file, "Defaults", "retainFailed", &error);
    if (error)  {
        settings.retainFailed = retainFailedDef;
        error = NULL;
    }

    // *** searchLogging ***
    settings.searchLogging = g_key_file_get_boolean(key_file, "Defaults", "searchLogging", &error);
    if (error)  {
        settings.searchLogging = searchLoggingDef;
        error = NULL;
    }

    // *** reuseWin ***
    settings.reuseWin = g_key_file_get_boolean(key_file, "Defaults", "reuseWin", &error);
    if (error)  {
        settings.reuseWin = reuseWinDef;
        error = NULL;
    }

    // *** exitConfirm ***
    settings.exitConfirm = g_key_file_get_boolean(key_file, "Defaults", "exitConfirm", &error);
    if (error)  {
        settings.exitConfirm = exitConfirmDef;
        error = NULL;
    }

    // *** menuIcons ***
    settings.menuIcons = g_key_file_get_boolean(key_file, "Defaults", "menuIcons", &error);
    if (error)  {
        settings.menuIcons = menuIconsDef;
        error = NULL;
    }

    // *** menuIcons ***
    settings.singleClick = g_key_file_get_boolean(key_file, "Defaults", "singleClick", &error);
    if (error)  {
        settings.singleClick = singleClickDef;
        error = NULL;
    }

    // *** showIncludes ***
    settings.showIncludes = g_key_file_get_boolean(key_file, "Defaults", "showIncludes", &error);
    if (error)  {
        settings.showIncludes = showIncludesDef;
        error = NULL;
    }

    // *** searchLogFile ***
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "searchLogFile", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.searchLogFile, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
            string_trunc_warn("settings.searchLogFile");
        g_free(tmp_ptr);
    }

    // *** suffixList ***
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "suffixList", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.suffixList, tmp_ptr, MAX_GTK_ENTRY_SIZE) >= MAX_GTK_ENTRY_SIZE )
            string_trunc_warn("settings.suffixList");
        g_free(tmp_ptr);
    }
    if ( (settings.suffixList[0] != '\0') && ( !APP_CONFIG_valid_list("Suffix List", settings.suffixList, &settings.suffixDelim) ) )
        exit(EXIT_FAILURE);


    // *** typelessList ***
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "typelessList", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.typelessList, tmp_ptr, MAX_GTK_ENTRY_SIZE) >= MAX_GTK_ENTRY_SIZE )
            string_trunc_warn("settings.typelessList");
        g_free(tmp_ptr);
    }
    if ( (settings.typelessList[0] != '\0') && ( !APP_CONFIG_valid_list("Typeless List", settings.typelessList, &settings.typelessDelim) ) )
        exit(EXIT_FAILURE);


    // *** ignoredList ***
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "ignoredList", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.ignoredList, tmp_ptr, MAX_GTK_ENTRY_SIZE) >= MAX_GTK_ENTRY_SIZE )
            string_trunc_warn("settings.ignoredList");
        g_free(tmp_ptr);
    }
    if ( (settings.ignoredList[0] != '\0') && ( !APP_CONFIG_valid_list("Ignored List", settings.ignoredList, &settings.ignoredDelim) ) )
        exit(EXIT_FAILURE);

    DIR_list_join(settings.ignoredList, MASTER_IGNORED_LIST);

    // *** ignoreCase ***
    settings.ignoreCase = g_key_file_get_boolean(key_file, "Defaults", "ignoreCase", &error);
    if (error)  {
        settings.ignoreCase = ignoreCaseDef;
        error = NULL;
    }


    // *** autoGenEnable ***  (not available via command line argument)
    settings.autoGenEnable = g_key_file_get_boolean(key_file, "Defaults", "autoGenEnable", &error);
    if (error)  {
        settings.autoGenEnable = autoGenEnableDef;
        error = NULL;
    }

    // *** autoGenPath ***  (not available via command line argument)
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "autoGenPath", NULL);
    if (tmp_ptr)
    {
        if ( strlen(tmp_ptr) > 0 )  // a non-null string is required for this setting
        {
            if ( g_strlcpy(settings.autoGenPath, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
                string_trunc_warn("settings.autoGenPath");
        }
        else
        {
            // The user is not allowed to configure a "null" string for this value.  If they tried, fix it.
            if ( g_strlcpy(settings.autoGenPath, autoGenPathDef, MAX_STRING_ARG_SIZE) )
            {
                // Do nothing, the default value will always fit.  The 'if' is added to keep coverity happy
            }
        }
        g_free(tmp_ptr);
    }


    // *** autoGenSuffix ***  (not available via command line argument)
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "autoGenSuffix", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.autoGenSuffix, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
            string_trunc_warn("settings.autoGenSuffix");
        g_free(tmp_ptr);
    }


    // *** autoGenCmd ***  (not available via command line argument)
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "autoGenCmd", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.autoGenCmd, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
            string_trunc_warn("settings.autoGenCmd");
        g_free(tmp_ptr);
    }


    // *** autoGenRoot ***  (not available via command line argument)
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "autoGenRoot", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.autoGenRoot, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
            string_trunc_warn("settings.atoGenRoot");
        g_free(tmp_ptr);
    }


    // *** autoGenId ***  (not available via command line argument)
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "autoGenId", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.autoGenId, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
            string_trunc_warn("settings.autoGenId");
        g_free(tmp_ptr);
    }


    // *** autoGenThresh ***
    settings.autoGenThresh = g_key_file_get_integer(key_file, "Defaults", "autoGenThresh", &error);
    if (error)  {  /* revert to default */
        settings.autoGenThresh = autoGenThreshDef;
        error = NULL;
    }


    // *** terminalApp ***  (not available via command line argument)
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "terminalApp", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.terminalApp, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
            string_trunc_warn("settings.terminalApp");
        g_free(tmp_ptr);
    }


    // *** fileManager ***  (not available via command line argument)
    tmp_ptr = g_key_file_get_string(key_file, "Defaults", "fileManager", NULL);
    if (tmp_ptr)
    {
        if ( g_strlcpy(settings.fileManager, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
            string_trunc_warn("settings.fileManager");
        g_free(tmp_ptr);
    }


    // The remaining items can be overridden by the command line
    //----------------------------------------------------------

    // *** noBuild ***
    if (!settings.noBuild)
    {
        // if not set by the command line...
        settings.noBuild = g_key_file_get_boolean(key_file, "Defaults", "noBuild", &error);
        if (error)  {
            settings.noBuild = noBuildDef;
            error = NULL;
        }
        // noBuild is overriden by refOnly (-b) command line option
        if (settings.refOnly) settings.noBuild = FALSE;
    }

    // *** updateAll ***
    if (!settings.updateAll)
    {
        // if not set by the command line...
        settings.updateAll = g_key_file_get_boolean(key_file, "Defaults", "updateAll", &error);
        if (error)  {
            settings.updateAll = updateAllDef;
            error = NULL;
        }
    }

    // *** truncateSymbols ***
    if (!settings.truncateSymbols)
    {
        // if not set by the command line...
        settings.truncateSymbols = g_key_file_get_boolean(key_file, "Defaults", "truncateSymbols", &error);
        if (error)  {
            settings.truncateSymbols = truncateSymbolsDef;
            error = NULL;
        }
    }

    // *** compressDisable ***
    if (!settings.compressDisable)
    {
        // if not set by the command line...
        settings.compressDisable = g_key_file_get_boolean(key_file, "Defaults", "compressDisable", &error);
        if (error)  {
            settings.compressDisable = compressDisableDef;
            error = NULL;
        }
    }

    // *** recurseDir ***
    if (!settings.recurseDir)
    {
        // if not set by the command line...
        settings.recurseDir = g_key_file_get_boolean(key_file, "Defaults", "recurseDir", &error);
        if (error)  {
            settings.recurseDir = recurseDirDef;
            error = NULL;
        }
    }

    // *** refFile ***
    if ( strcmp(settings.refFile, refFileDef) == 0 )
    {
        // if the string has not been overridden by the command line, check the config file.
        tmp_ptr = g_key_file_get_string(key_file, "Defaults", "refFile", NULL);
        if (tmp_ptr)
        {
            if ( strcmp(tmp_ptr, "cscope.out") == 0 )
            {
                /*** Due to cref database format upgrade and conflict with cscope-vim integration,
                     cscope.out is no longer allowed as a cref file name ***/
                APP_CONFIG_set_string("refFile", refFileDef);
            }
            else
            {
                if ( g_strlcpy(settings.refFile, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
                    string_trunc_warn("settings.refFile");
            }
            g_free(tmp_ptr);
        }
    }

    // *** nameFile ***
    if ( strcmp(settings.nameFile, nameFileDef) == 0 )
    {
        // if the string has not been overridden by the command line, check the config file.
        tmp_ptr = g_key_file_get_string(key_file, "Defaults", "nameFile", NULL);
        if (tmp_ptr)
        {
            if ( g_strlcpy(settings.nameFile, tmp_ptr, MAX_STRING_ARG_SIZE) >= MAX_STRING_ARG_SIZE )
                string_trunc_warn("settings.nameFile");
            g_free(tmp_ptr);
        }
    }

    // *** includeDir ***    (no default, watch out for NULLs)
    if ( strcmp(settings.includeDir, includeDirDef) == 0 )
    {
        // if the string has not been overridden by the command line, check the config file.
        tmp_ptr = g_key_file_get_string(key_file, "Defaults", "includeDir", NULL);
        if (tmp_ptr)
        {
            if ( g_strlcpy(settings.includeDir, tmp_ptr, MAX_GTK_ENTRY_SIZE) >= MAX_GTK_ENTRY_SIZE)
                string_trunc_warn("settings.includeDir");
            g_free(tmp_ptr);
        }
        if ( (settings.includeDir[0] != '\0') && ( !APP_CONFIG_valid_list("Include Directory List", settings.includeDir, &settings.includeDirDelim) ) )
            exit(EXIT_FAILURE);
    }
    else
    {
        // the string was specified on the command line via -I <string>, validate it...
        if ( (strlen(settings.includeDir) < 3)  || (!APP_CONFIG_valid_list("Include Directory List", settings.includeDir, &settings.includeDirDelim) ) )
        {
            exit(EXIT_FAILURE);
        }
    }

    // *** srcDir ***    (no default, watch out for NULLs)
    if ( strcmp(settings.srcDir, srcDirDef) == 0 )
    {
        // if the string has not been overridden by the command line, check the config file.
        tmp_ptr = g_key_file_get_string(key_file, "Defaults", "srcDir", NULL);
        if (tmp_ptr)
        {
            if ( g_strlcpy(settings.srcDir, tmp_ptr, MAX_GTK_ENTRY_SIZE) >= MAX_GTK_ENTRY_SIZE)
            string_trunc_warn("settings.srcDir");
            g_free(tmp_ptr);
        }
    }


    // Initialize the transient settings to match configured (start-up) settings.
    sticky_settings.ignoreCase  = settings.ignoreCase;
    sticky_settings.useEditor   = settings.useEditor;
    sticky_settings.reuseWin    = settings.reuseWin;
    sticky_settings.retainInput = settings.retainInput;

    #if 0
    printf("Application Startup State\n"
           "=========================\n"
           "useEditor=%d\nfileEditor=%s\nretainInput=%d\nsearchLogFile=%s\n"
           "suffixList=%s\ntypelessList=%s\nignoredList=%s\n"
           "ignoreCase=%d\nnoBuild=%d\nupdateAll=%d\nrefFile=%s\nnameFile=%s\n"
           "includeDir=%s\nsrcDir=%s\ntruncSym=%d\ncompressDisable=%d\n"
           "autoGenEnable=%d\nautoGenPath=%s\n"
           "rcFile=%s\nrecurseDir=%d\n"
           "=========================\n\n\n",
           settings.useEditor, settings.fileEditor, settings.retainInput, settings.searchLogFile,
           settings.suffixList, settings.typelessList, settings.ignoredList,
           settings.ignoreCase, settings.noBuild,settings.updateAll,
           settings.refFile, settings.nameFile, settings.includeDir, settings.srcDir, settings.truncateSymbols, settings.compressDisable,
           settings.autoGenEnable, settings.autoGenPath,
           settings.rcFile, settings.recurseDir);
    #endif

    return;
}

#if defined(GTK3_BUILD) || defined(GTK4_BUILD)
static gboolean create_gtk_config_file(const char *filename)
{

gchar *template =
{
"/*Version=005 */"
"\n"
"\n/*"
"\nDocumentation for styling GTK+ using CSS is kind of scattered.  The following"
"\nlinks have been useful:"
"\n"
"\nhttps://thegnomejournal.wordpress.com/2011/03/15/styling-gtk-with-css/   (Styling GTK+ with CSS)"
"\n"
"\nhttp://www.gtkforums.com/viewtopic.php?f=3&t=988&p=72088=GTK3+with+CSS#p72088  (GTK forums 'GTK3 with CSS')-Lots of examples"
"\n"
"\nhttps://developer.gnome.org/gtk3/3.14/GtkCssProvider.html   (GTK 3.14 Reference - Gtk CssProvider)"
"\n"
"\nhttps://developer.gnome.org/gtk3/3.0/ch25s02.html#gtk-migrating-GtkStyleContext (Porting GTK2 to 3 in general, Migrating themes info)"
"\n"
"\nhttps://developer.gnome.org/gtk3/stable/GtkStyleContext.html  (GtkStyleContext - Rendering UI elements)"
"\n*/"
"\n"
"\n"
"\n/*--------- Blue Buttons------------*/"
"\n"
"\n#find_c_identifier_button,"
"\n#find_definition_of_button"
"\n{"
"\n    padding: 5px;"
"\n    /* border-radius: 15px; */"
"\n    /* border-style: solid; */"
"\n    /* border-width: 1px; */"
"\n    /* transition: 500ms linear; */"
"\n    /* font: Sans 16; */"
"\n    /* background-color: blue; */"
"\n    /* border-width: 3px 3px 3px 3px; */"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/blue-button-normal-wide.png');"
"\n    background-position: center;"
"\n    color: white;"
"\n    transition: background-image 0.5s ease-in-out;"
"\n}"
"\n"
"\n#find_c_identifier_button:hover,"
"\n#find_definition_of_button:hover"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/blue-button-prelight.png');"
"\n    color: cadetblue1;"
"\n    transition: background-image 0.5s ease-in-out;"
"\n}"
"\n"
"\n#find_c_identifier_button:active,"
"\n#find_definition_of_button:active"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/blue-button-pressed.png');"
"\n    color: yellow;"
"\n}"
"\n"
"\n"
"\n/*--------- Red Buttons ------------*/"
"\n"
"\n#find_functions_called_by_button,"
"\n#find_functions_calling_button"
"\n{"
"\n    padding: 5px;"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/red-button-normal-wide.png');"
"\n    background-position: center;"
"\n    color: white;"
"\n    transition: background-image 0.5s ease-in-out;"
"\n}"
"\n"
"\n#find_functions_called_by_button:hover,"
"\n#find_functions_calling_button:hover"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/red-button-prelight.png');"
"\n    color: lavenderblush;"
"\n    transition: background-image 0.5s ease-in-out;"
"\n}"
"\n"
"\n#find_functions_called_by_button:active,"
"\n#find_functions_calling_button:active"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/red-button-pressed.png');"
"\n    color: yellow;"
"\n}"
"\n"
"\n"
"\n/*--------- Green Buttons ------------*/"
"\n"
"\n#find_text_string_button,"
"\n#find_egrep_pattern_button"
"\n{"
"\n    padding: 5px;"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/green-button-normal-wide.png');"
"\n    background-position: center;"
"\n    color: white;"
"\n    transition: background-image 0.5s ease-in-out;"
"\n}"
"\n"
"\n#find_text_string_button:hover,"
"\n#find_egrep_pattern_button:hover"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/green-button-prelight.png');"
"\n    color: darkseagreen1;"
"\n    transition: background-image 0.5s ease-in-out;"
"\n}"
"\n"
"\n#find_text_string_button:active,"
"\n#find_egrep_pattern_button:active"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/green-button-pressed.png');"
"\n    color: yellow;"
"\n}"
"\n"
"\n"
"\n/*--------- Orange Buttons------------*/"
"\n"
"\n#find_files_button,"
"\n#find_files_including_button"
"\n{"
"\n    padding: 5px;"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/orange-button-normal-wide.png');"
"\n    background-position: center;"
"\n    color: white;"
"\n    transition: background-image 0.5s ease-in-out;"
"\n}"
"\n"
"\n#find_files_button:hover,"
"\n#find_files_including_button:hover"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/orange-button-prelight.png');"
"\n    color: lightgoldenrod;"
"\n    transition: background-image 0.5s ease-in-out;"
"\n}"
"\n"
"\n#find_files_button:active,"
"\n#find_files_including_button:active"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/orange-button-pressed.png');"
"\n    color: yellow;"
"\n}"
"\n"
"\n/*---------------------------------*/"
"\n"
"\n#blue_eventbox"
"\n{"
"\n    background-image: url('" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/blue-box-background.png');"
"\n}"
"\n"
"\n#header_button,"
"\n#horizontal_filler_header_button,"
"\n#adjuster_eventbox"
"\n{"
"\n    background-image: none;"
"\n    background-color: steelblue;"
"\n    color: white;"
"\n}"
"\n"
"\n#clear_query_button"
"\n{"
"\n    padding: 0px;"
"\n}"
"\n"
"\n"
"\nGtkNotebook"
"\n{"
"\n    padding: 5px;"
"\n}"
"\n"
"\n/*"
"\nGtkTreeView row:nth-child(even)"
"\n{"
"\n    background-color: shade(steelblue, 2.0);"
"\n}"
"\n"
"\nGtkTreeView row:nth-child(odd)"
"\n{"
"\n    background-color: shade(@base_color, 1.0);"
"\n}"
"\n*/"
"\n"
"\nGtkTreeView row:selected"
"\n{"
"\nbackground-color:steelblue;"
"\ncolor:#fff;"
"\n}"
"\n"
"\nGtkTreeView row:selected:focus"
"\n{"
"\nbackground-color:steelblue;"
"\ncolor:#fff;"
"\n}"
"\n"
};

    return( create_template_file(filename, template) );
}

#else   // GTK2

static gboolean create_gtk_config_file(const char *filename)
{

gchar *template =
{
"#!Version=005"
"\n# This is ""the initial gtkrc template file created automatically by Gscope."
"\n# Edit this file to make your own GTK customizations. If you would like"
"\n# gscope to create a fresh template just delete/rename this file and run gscope."
"\n"
"\n"
"\n# If you want to override some, or all, of the stock gscope button pixmaps, "
"\n# add the path to your pixmaps in front of the stock path.  For example"
"\n# pixmap_path \"/home/username/.gscope:/usr/local/share/gscope/pixmaps\""
"\n"
"\npixmap_path \"" PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps\""
"\n"
"\n"
"\nstyle \"blue-button\""
"\n{"
"\n"
"\n  engine \"pixmap\""
"\n  {"
"\n    image"
"\n    {"
"\n      function          = BOX"
"\n      state             = PRELIGHT"
"\n      recolorable       = TRUE"
"\n      file              = \"blue-button-prelight.png\""
"\n      border            = { 3, 3, 3, 3}"
"\n      stretch           = TRUE"
"\n    }"
"\n    image"
"\n    {"
"\n      function          = BOX"
"\n      state             = ACTIVE"
"\n      file              = \"blue-button-pressed.png\""
"\n      border            = { 3, 3, 3, 3 }"
"\n      stretch           = TRUE"
"\n    }"
"\n    image "
"\n    {"
"\n      function          = BOX"
"\n      state             = NORMAL"
"\n      file              = \"blue-button-normal.png\""
"\n      border            = { 3, 3, 3, 3 }"
"\n      stretch           = TRUE"
"\n    }"
"\n  }"
"\n"
"\n  fg[ACTIVE]      = \"yellow\""
"\n  fg[SELECTED]    = \"white\""
"\n  fg[NORMAL]      = \"white\""
"\n  fg[PRELIGHT]    = \"cadetblue1\""
"\n  fg[INSENSITIVE] = \"white\""
"\n"
"\n}"
"\n"
"\n"
"\n"
"\nstyle \"red-button\""
"\n{"
"\n"
"\n  engine \"pixmap\""
"\n  {"
"\n    image"
"\n    {"
"\n      function          = BOX"
"\n      state             = PRELIGHT"
"\n      recolorable       = TRUE"
"\n      file              = \"red-button-prelight.png\""
"\n      border            = { 3, 3, 3, 3}"
"\n      stretch           = TRUE"
"\n    }"
"\n    image"
"\n    {"
"\n      function          = BOX"
"\n      state             = ACTIVE"
"\n      file              = \"red-button-pressed.png\""
"\n      border            = { 3, 3, 3, 3 }"
"\n      stretch           = TRUE"
"\n    }"
"\n    image "
"\n    {"
"\n      function          = BOX"
"\n      state             = NORMAL"
"\n      file              = \"red-button-normal.png\""
"\n      border            = { 3, 3, 3, 3 }"
"\n      stretch           = TRUE"
"\n    }"
"\n  }"
"\n"
"\n  fg[ACTIVE]      = \"yellow\""
"\n  fg[SELECTED]    = \"white\""
"\n  fg[NORMAL]      = \"white\""
"\n  fg[PRELIGHT]    = \"lavenderblush\""
"\n  fg[INSENSITIVE] = \"white\""
"\n"
"\n}"
"\n"
"\n"
"\n"
"\nstyle \"green-button\""
"\n{"
"\n"
"\n  engine \"pixmap\""
"\n  {"
"\n    image"
"\n    {"
"\n      function          = BOX"
"\n      state             = PRELIGHT"
"\n      recolorable       = TRUE"
"\n      file              = \"green-button-prelight.png\""
"\n      border            = { 3, 3, 3, 3}"
"\n      stretch           = TRUE"
"\n    }"
"\n    image"
"\n    {"
"\n      function          = BOX"
"\n      state             = ACTIVE"
"\n      file              = \"green-button-pressed.png\""
"\n      border            = { 3, 3, 3, 3 }"
"\n      stretch           = TRUE"
"\n    }"
"\n    image "
"\n    {"
"\n      function          = BOX"
"\n      state             = NORMAL"
"\n      file              = \"green-button-normal.png\""
"\n      border            = { 3, 3, 3, 3 }"
"\n      stretch           = TRUE"
"\n    }"
"\n  }"
"\n"
"\n  fg[ACTIVE]      = \"yellow\""
"\n  fg[SELECTED]    = \"white\""
"\n  fg[NORMAL]      = \"white\""
"\n  fg[PRELIGHT]    = \"darkseagreen1\""
"\n  fg[INSENSITIVE] = \"white\""
"\n"
"\n}"
"\n"
"\n"
"\n"
"\nstyle \"orange-button\""
"\n{"
"\n"
"\n  engine \"pixmap\""
"\n  {"
"\n    image"
"\n    {"
"\n      function          = BOX"
"\n      state             = PRELIGHT"
"\n      recolorable       = TRUE"
"\n      file              = \"orange-button-prelight.png\""
"\n      border            = { 3, 3, 3, 3}"
"\n      stretch           = TRUE"
"\n    }"
"\n    image"
"\n    {"
"\n      function          = BOX"
"\n      state             = ACTIVE"
"\n      file              = \"orange-button-pressed.png\""
"\n      border            = { 3, 3, 3, 3 }"
"\n      stretch           = TRUE"
"\n    }   "
"\n    image "
"\n    {"
"\n      function          = BOX"
"\n      state             = NORMAL"
"\n      file              = \"orange-button-normal.png\""
"\n      border            = { 3, 3, 3, 3 }"
"\n      stretch           = TRUE"
"\n    }"
"\n  }"
"\n"
"\n  fg[ACTIVE]      = \"yellow\""
"\n  fg[SELECTED]    = \"white\""
"\n  fg[NORMAL]      = \"white\""
"\n  fg[PRELIGHT]    = \"lightgoldenrod\""
"\n  fg[INSENSITIVE] = \"white\""
"\n"
"\n}"
"\n"
"\n"
"\nstyle \"blue-background\""
"\n{"
"\n  bg_pixmap[NORMAL] = \"blue-box-background.png\""
"\n}"
"\n"
"\nstyle \"grey-background\""
"\n{"
"\n  bg[NORMAL]      = \"lightgrey\""
"\n}"
"\n"
"\nstyle \"pink-background\""
"\n{"
"\n  bg[NORMAL]      = \"lightpink\""
"\n}"
"\n"
"\nstyle \"dblue-background\""
"\n{"
"\n  bg[NORMAL]      = \"steelblue\""
"\n  bg[PRELIGHT]    = \"steelblue\""
"\n  bg[ACTIVE]      = \"steelblue\""
"\n  bg[SELECTED]    = \"lightsteelblue\""
"\n"
"\n  fg[NORMAL]      = \"white\""
"\n  fg[ACTIVE]      = \"white\""
"\n  fg[PRELIGHT]    = \"white\""
"\n  fg[SELECTED]    = \"yellow\""
"\n}"
"\n"
"\n"
"\n"
"\nwidget  \"*find_c_identifier_button*\"        style \"blue-button\""
"\nwidget  \"*find_definition_of_button*\"       style \"blue-button\""
"\n"
"\nwidget  \"*find_functions_called_by_button*\" style \"red-button\""
"\nwidget  \"*find_functions_calling_button*\"   style \"red-button\""
"\n"
"\nwidget  \"*find_text_string_button*\"         style \"green-button\""
"\nwidget  \"*find_egrep_pattern_button*\"       style \"green-button\""
"\n"
"\nwidget  \"*find_files_button*\"               style \"orange-button\""
"\nwidget  \"*find_files_including_button*\"     style \"orange-button\""
"\n"
"\nwidget \"*blue_eventbox*\"                    style \"blue-background\""
"\nwidget \"*grey_eventbox*\"                    style \"grey-background\""
"\nwidget \"*pink_eventbox*\"                    style \"pink-background\""
"\nwidget \"*dblue_eventbox*\"                   style \"dblue-background\""
"\n"
"\nwidget \"*hscale*\"                           style \"dblue-background\""
"\n"
"\nwidget \"*header_button*\"                    style \"dblue-background\""
"\nwidget \"*adjuster_eventbox*\"                style \"dblue-background\""
};

    return( create_template_file(filename, template) );
}
#endif


static gboolean create_app_config_file(const char *filename)
{

gchar *template =
{
"\n# This is the Gscope application configuration file."
"\n# It can be edited manually or updated by the Gscope application,"
"\n# so beware of file access conflicts.  "
"\n#"
"\n# If you are going to edit this file directly, it is strongly "
"\n# advised that you exit all running instances of Gscope."
"\n#"
"\n# Note: valid values for boolean variables are \"true\" and \"false\" [Case sensitive]."
"\n#"
"\n# This file is created the first time you run the Gscope application.  All"
"\n# available configuration parameters are listed, along with their default"
"\n# values.  To change a parameter, locate the desired option and"
"\n# set the it to a new value."
"\n"
"\n[Defaults]"
"\n"
"\n# Latest version tracker (Major version * 1000) + Minor version"
"\ntrackedVersion  = 1000"
"\n"
"\n# Use editor specified by 'fileEditor' to view source"
"\nuseEditor       = false"
"\n"
"\n# Use this editor instead of the built-in gscope file viewer."
"\n# See 'useEditor' value."
"\nfileEditor      =  gnome-terminal --execute vim"
"\n"
"\n# Leave search pattern in query field after performing a search"
"\nretainInput     = false"
"\n"
"\n# If retainInput is OFF, Leave search pattern in query field after a FAILED search"
"\n# This value has no effect if retainInput is ON (always keep input)."
"\nretainFailed     = false"
"\n"
"\n# Enable/Disable search logging"
"\nsearchLogging   = false"
"\n"
"\n# Reuse the built-in file-view window"
"\nreuseWin        = false"
"\n"
"\n# Confirm before exit"
"\nexitConfirm     = true"
"\n"
"\n# Show Menu Icons"
"\nmenuIcons        = true"
"\n"
"\n# Single Click Mode"
"\nsingleClick      = false"
"\n"
"\n# Show files found in include path on stdout (when session statistics are generated)"
"\nshowIncludes    = false"
"\n"
"\n# Trace the directories searched by a recursive source-file-search"
"\nsearchLogFile   = gscope_srch.log"
"\n"
"\n# Only files with the following suffixes are considered to be 'source' files."
"\nsuffixList      = :c:h:cpp:hpp:arm:fml:mf:l:y:s:ld:lnk:"
"\n"
"\n# Only the files without suffixes in this list are considered to be 'source' files."
"\ntypelessList    = :Makefile:"
"\n"
"\n# Ignore these directories when performing a recursive source-file-search."
"\nignoredList     = :validation:"
"\n"
"\n# Enable automatic generation of source files (e.g. Protobuf files)"
"\n# to include in cross reference."
"\nautoGenEnable   = false"
"\n"
"\n# Path to auto-generated source files cache."
"\nautoGenPath     = /tmp"
"\n"
"\n# The autogen meta-source file type (suffix) to search for."
"\nautoGenSuffix   = .proto"
"\n"
"\n# The meta-source compiler command."
"\nautoGenCmd      = protoc-c_latest"
"\n"
"\n# Path to alternative meta-source file search root."
"\nautoGenRoot     ="
"\n"
"\n# Path to alternative meta-source file search root."
"\nautoGenId       =.pb-c"
"\n"
"\n# Autogen cache garbage collection threshold."
"\nautoGenThresh   = 10"
"\n"
"\n# Terminal App Command (must include %s format specifier)"
"\nterminalApp   = gnome-terminal --working-directory=%s"
"\n"
"\n# File Manager App Command (must include %s format specifier)"
"\nfileManager   = nautilus %s"
"\n"
"\n# The items below can be overriden by the command line."
"\n#======================================================"
"\n"
"\n# Ignore character case when searching"
"\nignoreCase      = false"
"\n"
"\n# Do not update the cross-reference file at startup."
"\nnoBuild         = false"
"\n"
"\n# Force a rebuild of the cross-reference file at startup."
"\nupdateAll       = false"
"\n"
"\n# Use only the first eight characters to match against C symbols."
"\ntruncateSymbols = false"
"\n"
"\n# Use only ASCII characters in the cross-reference file (don't compress)"
"\n# Long ago compression saved memory space at the cost of a little search [and startup]"
"\n# performance.  Now it doesn't really matter which mode is used."
"\ncompressDisable = false"
"\n"
"\n# Recursively search all the subdirectories beneath the current working directory"
"\n# for source files.  See 'suffixList', 'ignoredList' and 'typelessList'"
"\nrecurseDir      = false"
"\n"
"\n# This is the name of the cross-reference file.  Sometimes it is useful to save"
"\n# it in another location besides the current working directory."
"\nrefFile         = cscope_db.out"
"\n"
"\n# A list of source file names to be used as input to the cross reference engine."
"\nnameFile        ="
"\n"
"\n# Look here for additonal include files."
"\nincludeDir      ="
"\n"
"\n# Use the specified directory [only] to search for source files."
"\nsrcDir          ="
"\n"
};

    gchar       override_path[MAX_OVERRIDE_PATH_SIZE + 1];
    const gchar config_file[] = "site_default_override";
    gchar       *buf;
    gboolean    retval = FALSE;
    struct stat statstruct;
    FILE        *override_file = NULL;
    ssize_t     num_chars;

    printf("Performing first time application configuration using: ");

    num_chars = readlink("/proc/self/exe", override_path, MAX_OVERRIDE_PATH_SIZE);

    if ( num_chars < 0)
    {
        fprintf(stderr, "Fatal Error: Unexpected readlink(\"/proc/self/exe\") failure\n%s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else
    {
        override_path[num_chars] = '\0';    // Null terminate the path.

        my_dirname(override_path);
        if ( strlen(override_path) + strlen(config_file) + 1 < MAX_OVERRIDE_PATH_SIZE )
        {
            strcat(override_path, "/");
            strcat(override_path, config_file);

            if ( g_file_test(override_path, G_FILE_TEST_EXISTS) )
            {
                if ( ((override_file = fopen(override_path, "rb")) != NULL) && (fstat(fileno(override_file), &statstruct) == 0) )   // Fix TOCTOU defect
                {
                    buf = g_malloc(statstruct.st_size + 1);
                    if ( buf )
                    {
                        if ( fread(buf, 1, statstruct.st_size, override_file) == statstruct.st_size )
                        {
                            printf("Site defaults: %s\n", override_path);
                            buf[statstruct.st_size] = '\0';     // Null terminate the file data.
                            retval = create_template_file(filename, buf);
                        }
                        else
                        {
                            printf("Built-in defaults\n");
                            retval = create_template_file(filename, template);
                        }

                        g_free(buf);
                    }
                }
                if (override_file) fclose(override_file);
            }
            else
            {
                printf("Built-in defaults\n");
                retval = create_template_file(filename, template);
            }
        }
    }

    return(retval);
}



static gboolean create_template_file(const char *filename, gchar *template)
{
FILE *out_file;

    out_file = g_fopen(filename, "w+");

    if (out_file != NULL)
    {
        fprintf(out_file,"%s\n", template);
        fclose(out_file);
        if ( g_chmod(filename, 0644) < 0 )
            fprintf(stderr, "Warning: Config file permissions error:  Unable to chmod() file: %s", filename);
    }
    else
    {
        return(FALSE);
    }

    return(TRUE);

}

