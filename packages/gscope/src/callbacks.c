
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "version.h"
#include "support.h"

#include "callbacks.h"
#include "search.h"
#include "string.h"
#include "display.h"
#include "utils.h"
#include "fileview.h"
#include "dir.h"
#include "build.h"
#include "app_config.h"
#include "browser.h"
#include "auto_gen.h"


#if defined(GTK3_BUILD) || defined(GTK4_BUILD)
// Do nothing
#else
#include "interface.h"
#endif

#define MAX_STATS_PATH      50


//--------------------- Private Typedefs ----------------------------
typedef struct _SrcFile_stats
{
    char  *suffix;
    guint fcount;
    GtkWidget *col_label;
    GtkWidget *col_value;
    GtkWidget *vseparator_l;
    GtkWidget *vseparator_v;
    struct _SrcFile_stats *next;
} SrcFile_stats;


// ---- local function prototypes ----
static SrcFile_stats* create_stats_list(SrcFile_stats **si_stats);


//---------------- Private Globals ----------------------------------
static gboolean  editor_command_changed = FALSE;
static gboolean  suffix_entry_changed = FALSE;
static gboolean  typeless_entry_changed = FALSE;
static gboolean  ignored_entry_changed = FALSE;
static gboolean  include_entry_changed = FALSE;
static gboolean  source_entry_changed = FALSE;

static gboolean  autogen_cache_path_entry_changed = FALSE;
static gboolean  autogen_search_root_entry1_changed = FALSE;
static gboolean  autogen_suffix_entry_1_changed = FALSE;
static gboolean  autogen_cmd_entry1_changed = FALSE;
static gboolean  autogen_id_entry1_changed = FALSE;

static gboolean  name_entry_changed = FALSE;
static gboolean  cref_entry_changed = FALSE;
static gboolean  search_log_entry_changed = FALSE;
static gboolean  cancel_requested = FALSE;
static gboolean  ok_to_quit = FALSE;
static gboolean  stats_visible = FALSE;
static gboolean  search_button_lockout = FALSE;
static gboolean  cache_threshold_changed = FALSE;
static gboolean  terminal_app_entry_changed = FALSE;
static gboolean  file_manager_app_entry_changed = FALSE;


static unsigned int active_dir_entry;
static unsigned int active_input_entry;
static unsigned int active_output_entry;

// Top-level Widget global convenince pointers
static GtkWidget    *gscope_main = NULL;
static GtkWidget    *quit_dialog = NULL;
static GtkWidget    *aboutdialog1 = NULL;
static GtkWidget    *gscope_preferences = NULL;
static GtkWidget    *quit_confirm_dialog = NULL;
static GtkWidget    *stats_dialog = NULL;
static GtkWidget    *folder_chooser_dialog = NULL;
static GtkWidget    *open_file_chooser_dialog = NULL;
static GtkWidget    *output_file_chooser_dialog = NULL;
static GtkWidget    *save_results_file_chooser_dialog = NULL;
static GtkWidget    *browser_window = NULL;

static GtkApplication *gscope_app = NULL;  

//---Public Globals (Try to keep this section empty)----



//=============================================================================================
//         Private Callback-Support  Functions
//=============================================================================================


//---------------------------------------------------------------------------
// process_query (query_type)
//
// Perform one of the eight pre-defined queries [specified by <query_type>]
// using the text from the query_entry field [on the main application window]
// and display the results.
//
//---------------------------------------------------------------------------
static void process_query(search_t query_type)
{
#define MAX_COMPARISON          256
    gchar *pattern;
    #ifdef GTK4_BUILD
    GtkEntryBuffer *buffer;
    #endif

    static gchar *button_label[NUM_FIND_TYPES];
    static gchar *button_active_label[NUM_FIND_TYPES];
    static GtkWidget *buttons[NUM_FIND_TYPES];
    static GtkWidget *query_entry;
    static GtkWidget *cancel_button;
    static GtkWidget *progress_bar;
    static search_t  previous_button;

    static char previous_pattern[MAX_COMPARISON];
    static gboolean initialized = FALSE;
    static search_t previous_query = FIND_NULL;
    static const gchar *format_string = "=%s=";

    gchar msg[512];
    gchar plural[3];
    const gchar *str_ptr;

    if (!initialized)
    {
        initialized = TRUE;

        /* Create local copies of the button strings (active and idle) */
        buttons[FIND_SYMBOL] = lookup_widget(GTK_WIDGET(gscope_main), "find_c_identifier_button");
        str_ptr                             = gtk_button_get_label(GTK_BUTTON(buttons[FIND_SYMBOL]));
        button_label[FIND_SYMBOL] = strdup((char *)str_ptr);
        button_active_label[FIND_SYMBOL] = g_malloc(strlen(button_label[FIND_SYMBOL]) + 3);
        sprintf(button_active_label[FIND_SYMBOL], format_string, button_label[FIND_SYMBOL]);

        previous_button = FIND_SYMBOL;
        gtk_button_set_label(GTK_BUTTON(buttons[FIND_SYMBOL]), button_active_label[FIND_SYMBOL]);


        buttons[FIND_DEF] = lookup_widget(GTK_WIDGET(gscope_main), "find_definition_of_button");
        str_ptr                             = gtk_button_get_label(GTK_BUTTON(buttons[FIND_DEF]));
        button_label[FIND_DEF] = strdup((char *)str_ptr);
        button_active_label[FIND_DEF] = g_malloc(strlen(button_label[FIND_DEF]) + 3);
        sprintf(button_active_label[FIND_DEF], format_string, button_label[FIND_DEF]);


        buttons[FIND_CALLEDBY] = lookup_widget(GTK_WIDGET(gscope_main), "find_functions_called_by_button");
        str_ptr                             = gtk_button_get_label(GTK_BUTTON(buttons[FIND_CALLEDBY]));
        button_label[FIND_CALLEDBY] = strdup((char *)str_ptr);
        button_active_label[FIND_CALLEDBY] = g_malloc(strlen(button_label[FIND_CALLEDBY]) + 3);
        sprintf(button_active_label[FIND_CALLEDBY], format_string, button_label[FIND_CALLEDBY]);


        buttons[FIND_CALLING] = lookup_widget(GTK_WIDGET(gscope_main), "find_functions_calling_button");
        str_ptr                             = gtk_button_get_label(GTK_BUTTON(buttons[FIND_CALLING]));
        button_label[FIND_CALLING] = strdup((char *)str_ptr);
        button_active_label[FIND_CALLING] = g_malloc(strlen(button_label[FIND_CALLING]) + 3);
        sprintf(button_active_label[FIND_CALLING], format_string, button_label[FIND_CALLING]);


        buttons[FIND_STRING] = lookup_widget(GTK_WIDGET(gscope_main), "find_text_string_button");
        str_ptr                             = gtk_button_get_label(GTK_BUTTON(buttons[FIND_STRING]));
        button_label[FIND_STRING] = strdup((char *)str_ptr);
        button_active_label[FIND_STRING] = g_malloc(strlen(button_label[FIND_STRING]) + 3);
        sprintf(button_active_label[FIND_STRING], format_string, button_label[FIND_STRING]);


        buttons[FIND_REGEXP] = lookup_widget(GTK_WIDGET(gscope_main), "find_egrep_pattern_button");
        str_ptr                             = gtk_button_get_label(GTK_BUTTON(buttons[FIND_REGEXP]));
        button_label[FIND_REGEXP] = strdup((char *)str_ptr);
        button_active_label[FIND_REGEXP] = g_malloc(strlen(button_label[FIND_REGEXP]) + 3);
        sprintf(button_active_label[FIND_REGEXP], format_string, button_label[FIND_REGEXP]);


        buttons[FIND_FILE] = lookup_widget(GTK_WIDGET(gscope_main), "find_files_button");
        str_ptr                             = gtk_button_get_label(GTK_BUTTON(buttons[FIND_FILE]));
        button_label[FIND_FILE] = strdup((char *)str_ptr);
        button_active_label[FIND_FILE] = g_malloc(strlen(button_label[FIND_FILE]) + 3);
        sprintf(button_active_label[FIND_FILE], format_string, button_label[FIND_FILE]);


        buttons[FIND_INCLUDING] = lookup_widget(GTK_WIDGET(gscope_main), "find_files_including_button");
        str_ptr                             = gtk_button_get_label(GTK_BUTTON(buttons[FIND_INCLUDING]));
        button_label[FIND_INCLUDING] = strdup((char *)str_ptr);
        button_active_label[FIND_INCLUDING] = g_malloc(strlen(button_label[FIND_INCLUDING]) + 3);
        sprintf(button_active_label[FIND_INCLUDING], format_string, button_label[FIND_INCLUDING]);


        // Virtual button(s)
        button_label[FIND_AUTOGEN_ERRORS] = "AutoGen Errors";
        button_label[FIND_ALL_FUNCTIONS] = "Find All Functions";

        query_entry   = lookup_widget(GTK_WIDGET(gscope_main), "query_entry");
        cancel_button = lookup_widget(GTK_WIDGET(gscope_main), "cancel_button");
        progress_bar  = lookup_widget(GTK_WIDGET(gscope_main), "progressbar1");

    }

    if (query_type == FIND_NULL)
    {
        // Clear previous query info
        strcpy(previous_pattern, "");
        previous_query = FIND_NULL;
        return;
    }

    if (query_type <= FIND_INCLUDING) // if this is not a "virtual button" query
    {
        // Make the most recent query the default
        if (buttons[query_type] != buttons[previous_button])
        {
            gtk_button_set_label(GTK_BUTTON(buttons[previous_button]), button_label[previous_button]);
            gtk_button_set_label(GTK_BUTTON(buttons[query_type]), button_active_label[query_type]);
            previous_button = query_type;
        }

        #ifndef GTK4_BUILD
        gtk_widget_grab_default(buttons[query_type]);
        #else
        gtk_window_set_default_widget(GTK_WINDOW(gscope_main), buttons[query_type]);
        #endif

        pattern = strdup(my_gtk_entry_get_text(GTK_ENTRY(query_entry)));
    }
    else
    {
        if ( query_type == FIND_AUTOGEN_ERRORS )   // fixed "virtual" button pattern
        {
            pattern = strdup(AUTOGEN_ERR_PATTERN);
        }
        else    // no-pattern "virtual" button
            pattern = strdup(" ");  // Dummy search pattern
    }


    if (*pattern == '\0')
    {
        DISPLAY_status("<span foreground=\"red\" weight=\"bold\">Please enter a search pattern</span>");
        free(pattern);
        return;
    }

    if (settings.smartQuery && (query_type == previous_query) && (strncmp(pattern, previous_pattern, 256) == 0))
    {
        DISPLAY_status("<span foreground=\"blue\">The query requested is current (Turn off "
                       "</span>"
                       "<span foreground=\"blue\" weight=\"bold\">Smart Query "
                       "</span>"
                       "<span foreground=\"blue\">to force a new search)</span>");
    }
    else
    {
        search_results_t *search_results;

        // Enable the Cancel button
        gtk_widget_set_sensitive(cancel_button, TRUE);

        // Kick-off the search.
        gtk_widget_show(progress_bar);

        // Perform the search
        search_results = SEARCH_lookup(query_type, pattern);

        if (search_results->match_count > 0)
        {
            char *esc_pattern;

            // Disable the Cancel button
            gtk_widget_set_sensitive(cancel_button, FALSE);
            gtk_widget_hide(progress_bar);
            
            // remember our last successful query
            previous_query = query_type;
            // remember the last successful query pattern
            snprintf(previous_pattern, MAX_COMPARISON, "%s", pattern);

            if (search_results->match_count > 1)
                strcpy(plural, "es");
            else
                strcpy(plural, "");

            esc_pattern = g_markup_escape_text(pattern, -1);
            sprintf(msg, "%s:  <span foreground=\"blue\">%s</span>   [%d match%s]", button_label[query_type],
                    esc_pattern,
                    search_results->match_count,
                    plural);
            g_free(esc_pattern);

            if (cancel_requested)
            {
                cancel_requested = FALSE;
                strcat(msg, "<span foreground=\"red\">  - Search Aborted - Partial Results Shown!</span>");
            }
            DISPLAY_status(msg);

            DISPLAY_search_results(query_type, search_results);
            if (query_type <= FIND_INCLUDING)     // If this is not a "virtual button" query
                DISPLAY_history_update(pattern);  // Update the history log
        }
        else   // no match
        {
            // Disable the Cancel button
            gtk_widget_set_sensitive(cancel_button, FALSE);
            gtk_widget_hide(progress_bar);
            cancel_requested = FALSE;
        }


        if (!settings.retainInput)
        {

            if ((search_results->match_count == 0) && (settings.retainFailed))
            {
                /* Do nothing, leave the input text alone */
            }
            else
            {
                /* otherwise clear the entry text if we have a match */
                my_gtk_entry_set_text(GTK_ENTRY(query_entry), "");
            }
        }

        SEARCH_free_results(search_results);   /* Free the search results data */
    }

    free(pattern);

    // Place focus back onto the query_entry field
    gtk_widget_grab_focus(query_entry);
}


// Create a list of source file suffixes and counts for each suffix
static SrcFile_stats* create_stats_list(SrcFile_stats **si_stats)
{
    gchar *suffix_ptr;
    gchar *wptr;
    gchar delimiter;
    gchar working[80];       // Working buffer for constructing each individual suffix string.
    int f;
    guint noSuffixCnt = 0;
    guint si_noSuffixCnt = 0;

    SrcFile_stats *ListBegin = NULL;
    SrcFile_stats *entry;

    SrcFile_stats *si_ListBegin = NULL;
    SrcFile_stats *si_entry;

    SrcFile_stats *previous;
    SrcFile_stats * current,*temp;

    int counter;

    gboolean include_file_path_exists;

    if (strcmp(settings.includeDir, "") == 0)
        include_file_path_exists = FALSE;
    else
        include_file_path_exists = TRUE;


    suffix_ptr = &(settings.suffixList[0]);

    if (*suffix_ptr != '\0')    // If we have a non-null suffix list
    {
        delimiter = *suffix_ptr++;
        while (*suffix_ptr != '\0')
        {
            wptr = working;
            *wptr++ = '.';
            while (*suffix_ptr != delimiter)
            {
                *wptr++ = *suffix_ptr++;
            }
            suffix_ptr++;
            *wptr = 0;  // Null-terminate the suffix.

            /* Create the application specific suffix list */
            entry = g_malloc(sizeof(SrcFile_stats));  // Create the suffix entry
            entry->next = ListBegin;                  // Add it to the linked list
            ListBegin = entry;
            entry->fcount = 0;
            entry->suffix = strdup(working);

            /* Create the (optional) system include suffix list (/usr/include/...)  */
            si_entry = g_malloc(sizeof(SrcFile_stats));  // Create the suffix entry
            si_entry->next = si_ListBegin;               // Add it to the linked list
            si_ListBegin = si_entry;
            si_entry->fcount = 0;
            si_entry->suffix = strdup(working);
        }

        counter = 0;

        // Now that the list is created, collect the per-suffix statistics
        for (f = 0; f < nsrcfiles; f++)
        {
            if ((include_file_path_exists) && (DIR_file_on_include_search_path(DIR_src_files[f])))
            {
                /* Count this file in the system include source stats */
                if (settings.showIncludes)
                    printf("%d) %s\n", ++counter, DIR_src_files[f]);

                // if the file has a suffix
                if ((wptr = strrchr(DIR_src_files[f], '.')) != NULL)
                {
                    si_entry = si_ListBegin;
                    // increment the suffix counter corresponding to wptr
                    while (si_entry != NULL)
                    {
                        if (strcmp(si_entry->suffix, wptr) == 0)
                        {
                            si_entry->fcount++;
                            break;
                        }
                        si_entry = si_entry->next;
                    }
                }
                else
                {
                    // increment the no-suffix counter
                    si_noSuffixCnt++;
                }
            }
            else /* This is a user source file, not a System Includes (/usr/include...) file */
            /* Count this file in the user program source stats */
            {
                // if the file has a suffix
                if ((wptr = strrchr(DIR_src_files[f], '.')) != NULL)
                {
                    entry = ListBegin;
                    // increment the suffix counter corresponding to wptr
                    while (entry != NULL)
                    {
                        if (strcmp(entry->suffix, wptr) == 0)
                        {
                            //if ( strcmp(wptr, DELSOL_ISINK_FID_INIT) == 0) printf("%s\n", DIR_src_files[f]);
                            entry->fcount++;
                            break;
                        }
                        entry = entry->next;
                    }
                }
                else
                {
                    // increment the no-suffix counter
                    noSuffixCnt++;
                }
            }
        }
    }

    // -------- Add the <no-suffix> count to the user sourcelist --------
    entry = g_malloc(sizeof(SrcFile_stats));
    entry->next = ListBegin;
    ListBegin = entry;
    entry->fcount = noSuffixCnt;
    entry->suffix = strdup("No Suffix");

    // Add the <no-suffix> count to the system includes sourcelist
    si_entry = g_malloc(sizeof(SrcFile_stats));
    si_entry->next = si_ListBegin;
    si_ListBegin = si_entry;
    si_entry->fcount = si_noSuffixCnt;
    si_entry->suffix = strdup("No Suffix");


    // -------- Add the TOTAL count to the user list --------
    entry = g_malloc(sizeof(SrcFile_stats));
    entry->next = ListBegin;
    ListBegin = entry;
    entry->fcount = nsrcfiles;
    entry->suffix = strdup("Total");

    // Now reverse the entries in the linked list so that the
    // Suffix counts are displayed in the same order as the suffix
    // list pattern.

    current = ListBegin;
    previous = NULL;
    while (current != NULL)
    {
        temp = current->next;
        current->next = previous;
        previous = current;
        current = temp;
    }
    ListBegin = previous;

    current = si_ListBegin;
    previous = NULL;
    while (current != NULL)
    {
        temp = current->next;
        current->next = previous;
        previous = current;
        current = temp;
    }
    si_ListBegin = previous;


    *si_stats = si_ListBegin;    // Return the pointer to the si_List here
    return (ListBegin);
}



//=============================================================================================
//          Public Callback Functions
//=============================================================================================

// Provide a catalog of prominent widgets that enables components with no knowledge
// of the widget hierarchy to fetch a reference for that widget.
//
// The widgets in this catalog are primarily (if not exclusively):
//      Top level widgets:  Parent windows for messages
//      Progress Bars:      Progress widgets for incremental status

GtkWidget *CALLBACKS_get_widget(gchar *widget_name)
{
    if ( strcmp(widget_name, "gscope_main") == 0 )
        return(gscope_main);
    if ( strcmp(widget_name, "gscope_splash") == 0)
        return(lookup_widget("gscope_splash", widget_name));
    if ( strcmp(widget_name, "progressbar1") == 0 )
        return(lookup_widget(gscope_main, widget_name));

    return(NULL);
}





//====================================================
//    Non GTK-version dependent callbacks
//====================================================

void CALLBACKS_init(GtkWidget *main)
{
    gscope_main = main;     // Initialize a private global for all callbacks to use.

    DISPLAY_init(main);

    // Instantiate the widgets defined by the GUI Builders
    //====================================================

#if defined(GTK3_BUILD) || defined(GTK4_BUILD)

    quit_dialog  = my_lookup_widget("quit_confirm_dialog");
    aboutdialog1 = my_lookup_widget("aboutdialog1");
    gscope_preferences = my_lookup_widget("gscope_preferences");
    quit_confirm_dialog = my_lookup_widget("quit_confirm_dialog");
    stats_dialog = my_lookup_widget("stats_dialog");
    folder_chooser_dialog = my_lookup_widget("folder_chooser_dialog");
    open_file_chooser_dialog = my_lookup_widget("open_file_chooser_dialog");
    output_file_chooser_dialog = my_lookup_widget("output_file_chooser_dialog");
    save_results_file_chooser_dialog = my_lookup_widget("save_results_file_chooser_dialog2");

#else   // GTK2

    quit_dialog  = create_quit_confirm_dialog();
    aboutdialog1 = create_aboutdialog1();
    gscope_preferences = create_gscope_preferences();
    stats_dialog = create_stats_dialog();
    folder_chooser_dialog = create_folder_chooser_dialog();
    open_file_chooser_dialog = create_open_file_chooser_dialog();
    output_file_chooser_dialog = create_output_file_chooser_dialog();
    save_results_file_chooser_dialog = create_save_results_file_chooser_dialog();
 
#endif

    // Reconcile widget states with startup configuration settings
    //============================================================

    #ifndef GTK4_BUILD
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(quit_confirm_dialog),
                                "confirm_exit_checkbutton")), settings.exitConfirm);
    #else
    if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(my_lookup_widget("confirm_exit_checkbutton")))!= settings.exitConfirm )
    {
        printf("%s: Synchronize confirm_exit_checkbutton to settings.exitConfirm = %s\n", __func__, settings.exitConfirm ? "TRUE" : "FALSE");
        gtk_check_button_set_active(GTK_CHECK_BUTTON(my_lookup_widget("confirm_exit_checkbutton")), settings.exitConfirm);
    }
    #endif
}


#if defined(GTK4_BUILD)
// Needed by callbacks For g_application_quit() when processing shutdown
void CALLBACKS_register_app(GtkApplication *app)
{
    gscope_app = app;
}
#endif



void on_cref_update_button_clicked(GtkButton *button, gpointer user_data)
{
    if ( SEARCH_get_cref_status() )   // If the cref is marked up-to-date, check for changes
        SEARCH_check_cref();
    else
        #ifndef GTK4_BUILD
        on_rebuild_database1_activate(NULL, NULL);
        #else   // GTK2 and GTK3
        on_rebuild_database1_activate(NULL, NULL, NULL);  // Otherwise, rebuild the cross reference
        #endif
}


//===================================================================================================
//  Application exit logic
//
//  Immediate application exit logic can be simple (and straightforward).  Close the app window or
//  select the menue item Commands-->quit. Either transaction performs the same functionality:
//  Perform any application cleanup and quit-the-app.
//
//  Things get more convoluted when an "exit confirmation dialog" is introduced.
//  Pre-GTK4:
//  A relatively simple gtk_dialog_run() is inserted into the exit logic execution path and the
//  choice of exit pathway is followed based on in-dialog selections and subsequent to the 
//  completion of the dialog run.
//  GTK4:
//  The gods of GTK4 decided that nested/recursive run loops (like gtk_dialog_run) are bad and have
//  removed the gtk_dialog_run() API.  I'm not sure I buy their logic, but it is what it is.
//  For GTK4 (and beyond) the exit logic execution path must be gtk_main_loop event driven.  This
//  is not a bad thing, but it is hostile to Multi-GTK version backwards compatibility (which is 
//  clearly not a priority of the GTK gods)
//===================================================================================================

static gboolean exit_confirmed()
{
    if (settings.exitConfirm)
    {
        #ifndef GTK4_BUILD
        if ( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(quit_dialog), "confirm_exit_checkbutton"))) != settings.exitConfirm )
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(quit_dialog), "confirm_exit_checkbutton")), settings.exitConfirm);
        gtk_dialog_run(GTK_DIALOG(quit_dialog));
        gtk_widget_hide(GTK_WIDGET(quit_dialog));
        #else

        // Show the 'quit_confirm_dialog'
        //=====================================
        //gtk_window_set_modal(GTK_WINDOW(quit_confirm_dialog),TRUE);  --set in main.c
        gtk_widget_set_visible(quit_confirm_dialog, TRUE);
        printf("finish exit\n-");  //---- need to collect response from dialog before deciding ok_to_quit
        #endif
    }
    else
        ok_to_quit = TRUE;

    printf("%s returns: %s\n", __func__, ok_to_quit ? "TRUE" : "FALSE");
    return (ok_to_quit);
}


static void app_shutdown()
{
    printf("hello from: %s\n",__func__);
    SEARCH_cleanup();

    // Good-bye !!!
    #ifndef GTK4_BUILD
    gtk_main_quit();
    #else   //gtk4
    g_application_quit(G_APPLICATION(gscope_app));
    #endif
}


#ifndef GTK4_BUILD
void on_quit1_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_quit1_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    printf("hello from: %s\n", __func__);
    if ( exit_confirmed() )
        app_shutdown();
    else
        return;
}


void on_window1_destroy(GtkWidget *widget, gpointer user_data)
{
    // Callback is called when on_window1_delete_event returns FALSE
    app_shutdown();
}


// Top-level window "Close" or "X" button on window frame pressed.
// In GTK4, the ::delete-event signal from GTK3 is replaced by the GtkWindow::close-request signal
#ifndef GTK4_BUILD
gboolean on_window1_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
#else
gboolean on_window1_close_request(GtkWidget *widget, GdkEvent *event, gpointer user_data)
#endif
{
    // Normal Window destroy callback sequence
    // 1) delete_event
    // 2) if delete_event() returns FALSE then on_window_destroy()

    if ( exit_confirmed() )
        return FALSE;    // Return FALSE to destroy the widget.
    else
        return TRUE;     // Return TRUE to cancel the delete_event.
}


// Top-level window "Close" or "X" button on window frame pressed.
// In GTK4, the ::delete-event signal from GTK3 is replaced by the GtkWindow::close-request signal
#ifndef GTK4_BUILD
gboolean on_quit_confirm_dialog_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
#else
gboolean on_quit_confirm_dialog_close_request(GtkWidget *widget, GdkEvent *event, gpointer user_data)
#endif
{
    gtk_widget_hide(widget);
    //return TRUE;     // Do not destroy the widget
}


void on_quit_confirm_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            ok_to_quit = TRUE;
            break;

        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            ok_to_quit = FALSE;
            break;

        default:
        {
            char *error_string;

            my_asprintf(&error_string, "\nG-Scope Warning: Unexpected response: [%d] from: \"Quit Confirmation\" dialog.", response_id);
            DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_WARNING, error_string, TRUE);
            g_free(error_string);
        }

            break;
    }
    
    #if defined(GTK4_BUILD)
    if ( ok_to_quit )
        app_shutdown();
    else
       gtk_widget_set_visible(GTK_WIDGET(dialog), FALSE);

    #endif

    return;
}


// Exit confirm setting toggled from quit_confirm_dialog
#ifndef GTK4_BUILD
void on_confirm_exit_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_confirm_exit_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.exitConfirm )
    {
        settings.exitConfirm = !settings.exitConfirm;
        APP_CONFIG_set_boolean("exitConfirm", settings.exitConfirm);

        // Synch the checkbutton in the preferences dialog.
        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(preferences_dialog),
                                    "confirmation_checkbutton"), settings.exitConfirm);
    }
}

//===================================================================================================
//  End Application exit logic
//===================================================================================================


//===================================================================================================
//  Help-->Wiki activation logic
//===================================================================================================

#ifndef GTK4_BUILD
void on_overview_wiki_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki", GDK_CURRENT_TIME, NULL);
}
#else
void on_overview_wiki_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki", GDK_CURRENT_TIME);
}
#endif


#ifndef GTK4_BUILD
void on_usage_wiki_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Using-Gscope", GDK_CURRENT_TIME, NULL);
}
#else
void on_usage_wiki_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Using-Gscope", GDK_CURRENT_TIME);
}
#endif


#ifndef GTK4_BUILD
void on_configure_wiki_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope", GDK_CURRENT_TIME, NULL);
}
#else
void on_configure_wiki_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope", GDK_CURRENT_TIME);
}
#endif


#ifndef GTK4_BUILD
void on_release_wiki_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Gscope-Release-Notes", GDK_CURRENT_TIME, NULL);
}
#else
void on_release_wiki_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Gscope-Release-Notes", GDK_CURRENT_TIME);
}
#endif


//===================================================================================================
//  End Wiki activation logic
//===================================================================================================



#ifndef GTK4_BUILD  // GtkMenu evolution


void on_fileview_start_editor_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    ViewWindow *windowPtr;
    gchar *line_str;

    windowPtr = user_data;

    // Get the current line number from the ViewWindow structure
    my_asprintf(&line_str, "%d", windowPtr->line);
    my_start_text_editor(windowPtr->filename, line_str);
    g_free(line_str);

    gtk_widget_destroy(GTK_WIDGET(windowPtr->topWidget));
}


void on_fileview_close_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    gtk_widget_destroy(GTK_WIDGET(((ViewWindow *)user_data)->topWidget));
}

#endif


//====================================================
//      GTK multi-variant MENU ITEM handling
//====================================================

#ifndef GTK4_BUILD
void on_rebuild_database1_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_rebuild_database1_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    static gboolean in_progress_lockout = FALSE;

    if (!in_progress_lockout)
    {
        in_progress_lockout = TRUE;

        gtk_widget_show(lookup_widget(gscope_main, "rebuild_progressbar"));
        gtk_widget_hide(lookup_widget(gscope_main, "status_label"));

        // This widget will not actually appear until the first progress bar update.

        /* Rebuild the cross-reference */
        settings.noBuild = FALSE;   /* Override the noBuild setting (for this session only - leave preferences file as-is) */
        BUILD_initDatabase(lookup_widget(gscope_main, "rebuild_progressbar"));  /* Rebuild the cross-reference */

        /* Close results of previous searches */
        SEARCH_cleanup_prev();

        /*
         * Reset the record of the last query so that the next query will not
         * be reported as current.
         */
        process_query(FIND_NULL);

        gtk_widget_hide(lookup_widget(gscope_main, "rebuild_progressbar"));
        gtk_widget_show(lookup_widget(gscope_main, "status_label"));

        DISPLAY_status("<span foreground=\"seagreen\" weight=\"bold\">Cross Reference rebuild complete</span>");
        in_progress_lockout = FALSE;
    }
}


#ifndef GTK4_BUILD
void on_save_results_activate(GtkMenuItem *menuitem, gpointer  user_data)
#else
void on_save_results_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    gtk_window_set_transient_for(GTK_WINDOW(save_results_file_chooser_dialog), GTK_WINDOW(gscope_main));

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(save_results_file_chooser_dialog), "gscope_results");

    #ifdef GTK4_BUILD
    printf("Incomplete GTK4 callback migration: %s\n", __func__);
    #else
    (GTK_DIALOG(save_results_file_chooser_dialog));
    #endif
    gtk_widget_hide(save_results_file_chooser_dialog);
}


#ifndef GTK4_BUILD
void on_clear_query_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_clear_query_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    DISPLAY_history_clear();

    /* Reset the record of the last query so that the next query will not be reported as current */
    process_query(FIND_NULL);
}


#ifndef GTK4_BUILD
void on_save_query_activate(GtkMenuItem  *menuitem, gpointer user_data)
#else
void on_save_query_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    DISPLAY_history_save();
}


#ifndef GTK4_BUILD
void on_load_query_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_load_query_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    DISPLAY_history_load();
}


#ifndef GTK4_BUILD
void on_delete_history_file_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_delete_history_file_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    unlink(settings.histFile);
}


#ifndef GTK4_BUILD
void on_ignorecase_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    settings.ignoreCase = !settings.ignoreCase;

    if ( gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) != settings.ignoreCase )
    {
        /* Reset the record of the last query so that the next query will not be reported as current */
        process_query(FIND_NULL);
    }
}
#else
void on_ignorecase_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *state = g_action_get_state(G_ACTION(action));

    settings.ignoreCase = !settings.ignoreCase;

    if ( g_variant_get_boolean(state) != settings.ignoreCase )
    {
        g_simple_action_set_state(action, g_variant_new_boolean(settings.ignoreCase));
        /* Reset the record of the last query so that the next query will not be reported as current */
        process_query(FIND_NULL);
    }
}
#endif


#ifndef GTK4_BUILD
void on_useeditor_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    settings.useEditor = !settings.useEditor;

    if ( gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) != settings.useEditor )
    {
        // Do Nothing
    }
}
#else
void on_useeditor_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *state = g_action_get_state(G_ACTION(action));

    settings.useEditor = !settings.useEditor;

    if ( g_variant_get_boolean(state) != settings.useEditor )
    {
        g_simple_action_set_state(action, g_variant_new_boolean(settings.useEditor));
    }
}
#endif


#ifndef GTK4_BUILD
void on_reusewin_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    if ( gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) != settings.reuseWin )
    {
        settings.reuseWin = !settings.reuseWin;
    }
}
#else
void on_reusewin_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *state = g_action_get_state(G_ACTION(action));

    settings.reuseWin = !settings.reuseWin;

    if ( g_variant_get_boolean(state) != settings.reuseWin )
    {
        g_simple_action_set_state(action, g_variant_new_boolean(settings.reuseWin));
    }
}
#endif


#ifndef GTK4_BUILD
void on_retaininput_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    if ( gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)) != settings.retainInput )
    {
        settings.retainInput = !settings.retainInput;
    }
}
#else
void on_retaininput_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *state = g_action_get_state(G_ACTION(action));

    settings.retainInput = !settings.retainInput;

    if ( g_variant_get_boolean(state) != settings.retainInput )
    {
        g_simple_action_set_state(action, g_variant_new_boolean(settings.retainInput));
    }
}
#endif


#ifndef GTK4_BUILD
void on_smartquery_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    if ( gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem))!= settings.smartQuery )
    {
        settings.smartQuery = !settings.smartQuery;
    }
}
#else
void on_smartquery_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *state = g_action_get_state(G_ACTION(action));

    settings.smartQuery = !settings.smartQuery;

    if ( g_variant_get_boolean(state) != settings.smartQuery )
    {
        g_simple_action_set_state(action, g_variant_new_boolean(settings.smartQuery));
    }
}
#endif

#ifndef GTK4_BUILD
void on_preferences_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_preferences_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    GtkWidget *button1;
    GtkWidget *prefs_dialog;

    static gboolean initialized = FALSE;


    prefs_dialog = gscope_preferences;

    if (!initialized)
    {
        /*** Initialize the preference dialog settings (tab #1 "Search and View") ***/
        /****************************************************************************/
        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_checkbutton"),
                                     sticky_settings.retainInput);

        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_failed_search_checkbutton"),
                                     settings.retainFailed);

        #if (UI_VERSION == 1)
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_failed_search_checkbutton"), !sticky_settings.retainInput);
        #endif

        #if (UI_VERSION > 1)
        if ( !settings.retainFailed && !settings.retainInput )
            my_gtk_check_button_set_active(GTK_WIDGET(lookup_widget(GTK_WIDGET(prefs_dialog), "never_retain_text_checkbutton")), TRUE);
        else
            my_gtk_check_button_set_active(GTK_WIDGET(lookup_widget(GTK_WIDGET(prefs_dialog), "never_retain_text_checkbutton")), FALSE);

        if ( settings.retainInput )     // retainInput has top precedence
        {
            if ( settings.retainFailed )    // Config File Error: only one retain-type is allowed at a time
            {
                settings.retainFailed = FALSE;
                APP_CONFIG_set_boolean("retainFailed", settings.retainFailed);  // Update the preferences file
                my_gtk_check_button_set_active(GTK_WIDGET(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_failed_search_checkbutton")), FALSE);
            }
        }

        // Radio Button Group
        gtk_check_button_set_group(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_checkbutton")),
                                   GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_failed_search_checkbutton")));
        gtk_check_button_set_group(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_checkbutton")),
                                   GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "never_retain_text_checkbutton")));
        // End Radio Button Group                           
        #endif  // UI_VERSION > 1

        
        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(prefs_dialog), "ignore_case_checkbutton"),
                                     sticky_settings.ignoreCase);

        
        
        #ifndef GTK4_BUILD
        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(prefs_dialog), "use_viewer_radiobutton"),
                                     !sticky_settings.useEditor);

        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(prefs_dialog), "use_editor_radiobutton"),
                                     sticky_settings.useEditor);

        #else
        // Radio Button Group
        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(prefs_dialog), "use_viewer_checkbutton"),
                                     !sticky_settings.useEditor);

        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(prefs_dialog), "use_editor_checkbutton"),
                                     sticky_settings.useEditor);

        gtk_check_button_set_group(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "use_viewer_checkbutton")),
                                   GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "use_editor_checkbutton")));
        // End Radio Button Group
        #endif
   
        
        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(prefs_dialog), "reuse_window_checkbutton"),
                                     sticky_settings.reuseWin);

        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "editor_command_entry")), MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "editor_command_entry")), settings.fileEditor);
        if (settings.useEditor)
        {
            gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "editor_command_entry"), TRUE);
            gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "editor_command_label"), TRUE);
            gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "reuse_window_checkbutton"), FALSE);
        }
        else
        {
            gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "editor_command_entry"), FALSE);
            gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "editor_command_label"), FALSE);
            gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "reuse_window_checkbutton"), TRUE);
        }


        /***Initialize the preference dialog settings (tab #2 "Cross Reference") ***/
        /***************************************************************************/
        // Start-up build rules:  If conflicting info in config file, use the following
        // functional precedence:  Force Rebuild, Rebuild if Needed, No Rebuild
        
        #ifndef GTK4_BUILD  // Truncate symbols function removed as of GTK4
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "truncate_symbols_checkbutton")),
                                     settings.truncateSymbols);
        #endif


        #if defined(GTK4_BUILD)
        // Create rebuild (radio) button group
        gtk_check_button_set_group(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "force_rebuild_checkbutton")),
                    GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "no_rebuild_checkbutton")));
        gtk_check_button_set_group(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "force_rebuild_checkbutton")),
                    GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "rebuild_checkbutton")));
        #endif

        if (settings.updateAll)     // force rebuild
            #ifndef GTK4_BUILD
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "force_rebuild_radiobutton")), TRUE);
            #else
            gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "force_rebuild_checkbutton")), TRUE);
            #endif
        else
        {
            if (settings.noBuild)   // no rebuild
                #ifndef GTK4_BUILD
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "no_rebuild_radiobutton")), TRUE);
                #else
                gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "no_rebuild_checkbutton")), TRUE);
                #endif
            else        // rebuild if needed
                #ifndef GTK4_BUILD
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "rebuild_radiobutton")), TRUE);
                #else
                gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "rebuild_checkbutton")), TRUE);
                #endif
        }

        {   // Display the Startup Config File (UI V1=page4 UI v2=page2)
            gchar *markup_buf;
            my_asprintf(&markup_buf, "<span foreground=\"seagreen\" weight=\"bold\">%s</span>", settings.rcFile);
            gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(prefs_dialog), "rc_filename_label")), markup_buf);
            g_free(markup_buf);
        }


        #ifndef GTK4_BUILD
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "recursive_search_mode_checkbutton")),
                                settings.recurseDir);
        #else
        gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "recursive_search_mode_checkbutton")),
                                settings.recurseDir);
        #endif

        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "suffix_entry")),   MAX_GTK_ENTRY_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "suffix_entry")),   settings.suffixList);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "typeless_entry")), MAX_GTK_ENTRY_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "typeless_entry")), settings.typelessList);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "ignored_entry")),  MAX_GTK_ENTRY_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "ignored_entry")),  settings.ignoredList);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "source_entry")),   MAX_GTK_ENTRY_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "source_entry")),   settings.srcDir);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "include_entry")),  MAX_GTK_ENTRY_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "include_entry")),  settings.includeDir);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "name_entry")),       MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "name_entry")),       settings.nameFile);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "cref_entry")),       MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "cref_entry")),       settings.refFile);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "search_log_entry")), MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "search_log_entry")), settings.searchLogFile);

        // Initialize Search log settings
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "search_log_label"), settings.searchLogging);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "search_log_entry"), settings.searchLogging);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "search_log_browse_button"), settings.searchLogging);
        if (settings.searchLogging)
            gtk_button_set_label(GTK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "search_log_button")), "Disable");
        else
            gtk_button_set_label(GTK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "search_log_button")), "Enable");

        // "Show menu icons" supported from GTK 2.15 through GTK3.  Not supported on GTK4 (no more GtkImageMenuItem 
        // Use a GtkMenuItem containing a GtkBox with a GtkAccelLabel and a GtkImage instead).
        if ( (gtk_get_major_version() > 2 && gtk_get_major_version() < 4) || ((gtk_get_major_version() == 2) && (gtk_get_minor_version() > 15)))
        {
            #ifndef GTK4_BUILD
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "showicons_checkbutton")), settings.menuIcons);
            #else
            gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "showicons_checkbutton")), settings.menuIcons);
            #endif
        }
        else
        {
            // if the current gtk library does not provide gtk_image_menu_item_set_always_show_image()
            // We cannot support the "Show Menu Icons" functionality, so disable that preference item.
            gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "showicons_checkbutton"), FALSE);
            #ifndef GTK4_BUILD
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "showicons_checkbutton")), FALSE);
            #else
            gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "showicons_checkbutton")), FALSE);
            #endif
        }
        
        #ifndef GTK4_BUILD
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "single_click_checkbutton")), settings.singleClick);
        #else
        gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "single_click_checkbutton")), settings.singleClick);
        #endif

        #ifndef GTK4_BUILD
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "show_includes_checkbutton")), settings.showIncludes);
        #else
        gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "show_includes_checkbutton")), settings.showIncludes);
        #endif

        #ifndef GTK4_BUILD
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "compress_symbols_checkbutton")), !settings.compressDisable);
        #else
        gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "compress_symbols_checkbutton")), !settings.compressDisable);
        #endif

        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "terminal_app_entry")), MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "terminal_app_entry")), settings.terminalApp);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "file_manager_app_entry")), MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "file_manager_app_entry")), settings.fileManager);

 
        /*** Initialize the preference dialog settings (Tab #3 "General") ***/
        /********************************************************************/
        #ifndef GTK4_BUILD
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog),
                                    "confirmation_checkbutton")), settings.exitConfirm);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(quit_confirm_dialog),
                                    "confirm_exit_checkbutton")), settings.exitConfirm);
        #else
        gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog),
                                    "confirmation_checkbutton")), settings.exitConfirm);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(quit_confirm_dialog),
                                    "confirm_exit_checkbutton")), settings.exitConfirm);
        #endif


        /*** Initialize the AutoGen settings (Tab #4 ) ***/
        /*************************************************/

        #ifndef GTK4_BUILD
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_enable_checkbutton1")), settings.autoGenEnable);
        #else
        gtk_check_button_set_active(GTK_CHECK_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_enable_checkbutton1")), settings.autoGenEnable);
        #endif

        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cache_path_entry")),  MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cache_path_entry")),  settings.autoGenPath);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_suffix_entry1")),     MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_suffix_entry1")),     settings.autoGenSuffix);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cmd_entry1")),        MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cmd_entry1")),        settings.autoGenCmd);
        gtk_entry_set_max_length(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_search_root_entry1")), MAX_STRING_ARG_SIZE - 1);
        my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_search_root_entry1")), settings.autoGenRoot);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cache_threshold_spinbutton")), settings.autoGenThresh);

        // If/when more than one autogen meta-source type is supported, these elements will need a unique "enable" setting (per type)
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_suffix_entry1"),        settings.autoGenEnable);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cmd_entry1"),           settings.autoGenEnable);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_id_entry1"),            settings.autoGenEnable);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_search_root_entry1"),   settings.autoGenEnable);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_suffix_label"),         settings.autoGenEnable);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cmd_label"),            settings.autoGenEnable);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_id_label"),            settings.autoGenEnable);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_root_label"),           settings.autoGenEnable);

        // These items will be "composite enabled across multiple enables" if/when more than one autogen meta-source type is supported.
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cache_path_entry"),     settings.autoGenEnable);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_cache_location_label"), settings.autoGenEnable);

        {   // Conditional Content Labels.
            gchar   active_path[PATHLEN * 2];
            gchar   auto_name[PATHLEN * 2];

            if (settings.autoGenEnable)
            {
                snprintf(active_path, PATHLEN * 2, "My cache path: <span color=\"steelblue\">%s/%s/auto*_*</span>",
                         settings.autoGenPath,
                         getenv("USER"));
                snprintf(auto_name, PATHLEN * 2, "Generated filename format:  <span weight=\"bold\">foo</span>"
                         "<span weight=\"bold\" color=\"darkred\">%s</span> ---> "
                         "<b>foo</b><span weight=\"bold\" color=\"darkblue\">%s.c</span>  AND "
                         "<b>foo</b><span weight=\"bold\" color=\"darkblue\">%s.h</span>",
                         settings.autoGenSuffix,
                         settings.autoGenId,
                         settings.autoGenId
                        );
            }
            else
            {
                snprintf(active_path, PATHLEN * 2, "My cache path:");
                snprintf(auto_name, PATHLEN * 2, "Generated filename format: ");
            }

            gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_active_cache_path_label")), active_path);
            gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(prefs_dialog), "autogen_generated_filename_label")), auto_name);
        }


        #ifndef GTK4_BUILD      // As of GTK4, this dialog is instantiated and set trasient to "main" in main.c
        gtk_window_set_transient_for(GTK_WINDOW(prefs_dialog), GTK_WINDOW(gscope_main));
        #endif

        initialized = TRUE;
    }

    gtk_widget_set_visible(prefs_dialog, TRUE);
}


// Revisit: This callback works with a statistics dialog that has not yet been created for gtk4
#ifndef GTK4_BUILD
void on_session_statistics_activate(GtkMenuItem  *menuitem, gpointer user_data)
#else
void on_session_statistics_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    #define MAX_STAT_STRING     1024

    static SrcFile_stats  *sft_list = NULL;
    static SrcFile_stats  *si_sft_list = NULL;
    static stats_struct_t symbol_stats;

    gchar tmp_str[MAX_STAT_STRING + 1];
    GtkWidget *header_hbox;
    GtkWidget *value_hbox;
    GtkWidget *system_files_stats_label;
    GtkWidget *path_warning_label;

    SrcFile_stats  *entry;
    SrcFile_stats  *tmp_ptr;

    GtkWidget *user_files_stats_label;
    gchar cwd[PATHLEN + 1];
    unsigned int length;
    gchar working[MAX_STATS_PATH + 1];
    gchar *offset_ptr;
    char  *cwd_ptr;


    if (!stats_visible)
    {
        /* Free up any old user source data */
        /************************************/
        if (sft_list != NULL)
        {
            entry = sft_list;

            while (entry != NULL)
            {
                g_free(entry->suffix);  // free the suffix entry
                g_object_unref(entry->col_label);
                g_object_unref(entry->col_value);
                g_object_unref(entry->vseparator_l);
                g_object_unref(entry->vseparator_v);
                tmp_ptr = entry;
                entry = entry->next;
                g_free(tmp_ptr);          // free the list entry
            }

            sft_list = NULL;
        }

        /* Free up any old /usr/include/... data */
        /************************************/
        if (si_sft_list != NULL)
        {
            entry = si_sft_list;

            while (entry != NULL)
            {
                g_free(entry->suffix);  // free the suffix entry
                if (strcmp(settings.includeDir, "") != 0)    // If the include directory list is not "null"
                {
                    g_object_unref(entry->col_label);
                    g_object_unref(entry->col_value);
                    g_object_unref(entry->vseparator_l);
                    g_object_unref(entry->vseparator_v);
                }
                tmp_ptr = entry;
                entry = entry->next;
                g_free(tmp_ptr);          // free the list entry
            }

            si_sft_list = NULL;
        }


        /* Get the source file directory path */
        cwd_ptr = DIR_get_path(DIR_SOURCE);
        if (cwd_ptr)
        {
            strncpy(cwd, cwd_ptr, PATHLEN + 1);
            cwd[PATHLEN] = '\0';   // make sure we are null-terminated
        }
        else
        {
            strcpy(cwd, "<Error: path unknown>");
        }


        // if the CWD path is greater than MAX_STATS_PATH characters, truncate it
        length = strlen(cwd);
        if (length > MAX_STATS_PATH)
        {
            // Trim off excess chars from the beginning of the string [and leave 3 bytes for the ellipsis]
            offset_ptr = (&(cwd[0]) + (length - (MAX_STATS_PATH - 3)));
            // move forward to the first '/' character AFTER the offset
            offset_ptr = strchr(offset_ptr, '/');
            if (offset_ptr == NULL)
                strcpy(working, "<NULL>");
            else
                sprintf(working, "...%s", offset_ptr);
        }
        else
            strcpy(working, cwd);

        // Add an informative table header to the User Application Files statistics.
        sprintf(cwd, "<span size=\"large\" weight=\"bold\">Source File Type Statistics:  User Application Files.\nDirectory:  </span>"
                "<span size=\"large\" color=\"dark red\">(%s/...)</span>", working);

        user_files_stats_label = lookup_widget(GTK_WIDGET(stats_dialog), "user_files_stats_label");
        gtk_label_set_markup(GTK_LABEL(user_files_stats_label), cwd);



        /* Get fresh statistics */
        /************************/
        sft_list  = create_stats_list(&si_sft_list); /* Construct a list of source file suffixes and per-suffix file-counts */
        SEARCH_stats(&symbol_stats);         /* get the overall xref symbol type counts */


        /* Now update the dynamic portion of the Dialog */
        /***************************************************/

        path_warning_label = lookup_widget(GTK_WIDGET(stats_dialog), "path_warning_label");

        if (strcmp(settings.includeDir, "") != 0)    // If the include directory list is not "null"
        {
            header_hbox = lookup_widget(GTK_WIDGET(stats_dialog), "si_header_hbox");
            value_hbox =  lookup_widget(GTK_WIDGET(stats_dialog), "si_value_hbox");
            entry = si_sft_list;

            while (entry != NULL)
            {
                entry->col_label = gtk_label_new(entry->suffix);
                gtk_widget_show(entry->col_label);
                
                my_gtk_box_pack_start(GTK_BOX(header_hbox), entry->col_label, FALSE, TRUE, 0);
                
                gtk_widget_set_size_request(entry->col_label, 60, -1);

                #if defined(GTK4_BUILD) || defined(GTK3_BUILD)
                entry->vseparator_l = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
                entry->vseparator_v = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
                
                #else
                entry->vseparator_l = gtk_vseparator_new();
                entry->vseparator_v = gtk_vseparator_new();
                #endif

                my_gtk_box_pack_start(GTK_BOX(header_hbox), entry->vseparator_l, FALSE, TRUE, 0);
                
                sprintf(tmp_str, "<span color=\"blue\">%d</span>", entry->fcount);
                entry->col_value = gtk_label_new(tmp_str);
                gtk_widget_show(entry->col_value);
                
                my_gtk_box_pack_start(GTK_BOX(value_hbox), entry->col_value, FALSE, FALSE, 0);
                gtk_widget_set_size_request(entry->col_value, 60, -1);
                gtk_label_set_use_markup(GTK_LABEL(entry->col_value), TRUE);

                my_gtk_box_pack_start(GTK_BOX(value_hbox), entry->vseparator_v, FALSE, TRUE, 0);


                entry = entry->next;
            }

            system_files_stats_label = lookup_widget(GTK_WIDGET(stats_dialog), "system_files_stats_label");

            sprintf(tmp_str, "<span size=\"large\" weight=\"bold\">\nSource File Type Statistics: Files found on the "
                    "Include-file-search path.\nPath: </span><span size=\"large\" color=\"dark red\">"
                    "(%s)</span>", settings.includeDir);
            gtk_label_set_text(GTK_LABEL(system_files_stats_label), tmp_str);
            gtk_label_set_use_markup(GTK_LABEL(system_files_stats_label), TRUE);

            /* Set the (dynamic) path warning label text */
            sprintf(tmp_str, "<span size=\"large\" weight=\"bold\" color=\"Red\">Note :</span> If your Include File Search Path overlaps with the User Application "
                    "source tree, files that reside in an\noverlapping directory will only be counted in "
                    "the \"Files found on the Include-file-search path\" statistics.");
            gtk_label_set_text(GTK_LABEL(path_warning_label), tmp_str);
            gtk_label_set_use_markup(GTK_LABEL(path_warning_label), TRUE);
        }
        else
        {
            /* Set the (dynamic) path warning label text */
            sprintf(tmp_str, "<span size=\"large\" weight=\"bold\">Note: <span weight=\"normal\">No <span color =\"dark red\">Include "
                    "File Search Path</span> specified, so no\nadditional include files have been cross-referenced.</span></span>");
            gtk_label_set_text(GTK_LABEL(path_warning_label), tmp_str);
            gtk_label_set_use_markup(GTK_LABEL(path_warning_label), TRUE);
        }



        header_hbox = lookup_widget(GTK_WIDGET(stats_dialog), "header_hbox");
        value_hbox =  lookup_widget(GTK_WIDGET(stats_dialog), "value_hbox");
        entry = sft_list;

        while (entry != NULL)
        {
            entry->col_label = gtk_label_new(entry->suffix);
            gtk_widget_show(entry->col_label);
            my_gtk_box_pack_start(GTK_BOX(header_hbox), entry->col_label, FALSE, TRUE, 0);
            gtk_widget_set_size_request(entry->col_label, 60, -1);

            #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
            entry->vseparator_l = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
            entry->vseparator_v = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
            
            #else
            entry->vseparator_l = gtk_vseparator_new();
            entry->vseparator_v = gtk_vseparator_new();
            #endif

            my_gtk_box_pack_start(GTK_BOX(header_hbox), entry->vseparator_l, FALSE, TRUE, 0);


            sprintf(tmp_str, "<span color=\"blue\">%d</span>", entry->fcount);
            entry->col_value = gtk_label_new(tmp_str);
            gtk_widget_show(entry->col_value);
            my_gtk_box_pack_start(GTK_BOX(value_hbox), entry->col_value, FALSE, FALSE, 0);
            gtk_widget_set_size_request(entry->col_value, 60, -1);
            gtk_label_set_use_markup(GTK_LABEL(entry->col_value), TRUE);

            my_gtk_box_pack_start(GTK_BOX(value_hbox), entry->vseparator_v, FALSE, TRUE, 0);

            entry = entry->next;
        }

        sprintf(tmp_str, "<span size=\"large\" color=\"blue\">%d</span>", symbol_stats.define_cnt);
        gtk_label_set_label(GTK_LABEL(lookup_widget(GTK_WIDGET(stats_dialog), "definition_count_label")), tmp_str);

        sprintf(tmp_str, "<span size=\"large\" color=\"blue\">%d</span>", symbol_stats.identifier_cnt);
        gtk_label_set_label(GTK_LABEL(lookup_widget(GTK_WIDGET(stats_dialog), "identifier_count_label")), tmp_str);

        sprintf(tmp_str, "<span size=\"large\" color=\"blue\">%d</span>", symbol_stats.fn_cnt);
        gtk_label_set_label(GTK_LABEL(lookup_widget(GTK_WIDGET(stats_dialog), "function_count_label")), tmp_str);

        sprintf(tmp_str, "<span size=\"large\" color=\"blue\">%d</span>", symbol_stats.class_cnt);
        gtk_label_set_label(GTK_LABEL(lookup_widget(GTK_WIDGET(stats_dialog), "class_definition_count_label")), tmp_str);

        sprintf(tmp_str, "<span size=\"large\" color=\"blue\">%d</span>", symbol_stats.fn_calls_cnt);
        gtk_label_set_label(GTK_LABEL(lookup_widget(GTK_WIDGET(stats_dialog), "function_calls_count_label")), tmp_str);

        sprintf(tmp_str, "<span size=\"large\" color=\"blue\">%d</span>", symbol_stats.include_cnt);
        gtk_label_set_label(GTK_LABEL(lookup_widget(GTK_WIDGET(stats_dialog), "include_file_count_label")), tmp_str);

        gtk_window_set_transient_for(GTK_WINDOW(stats_dialog), GTK_WINDOW(gscope_main));

        gtk_widget_show(stats_dialog);
        stats_visible = TRUE;
    }
    // else "stats_dialog" already visible, do nothing.
}


#ifndef GTK4_BUILD
void on_list_all_functions1_activate(GtkMenuItem *menuitem, gpointer  user_data)
#else
void on_list_all_functions1_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_ALL_FUNCTIONS);
        search_button_lockout = FALSE;
    }
}


#ifndef GTK4_BUILD
void on_list_autogen_errors_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_list_autogen_errors_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    if ( !search_button_lockout )
    {
        search_button_lockout = TRUE;
        process_query(FIND_AUTOGEN_ERRORS);
        search_button_lockout = FALSE;
    }
}


#ifndef GTK4_BUILD
void on_usage1_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_usage1_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{

    const gchar *message =
       "\n<span foreground=\"blue\" weight=\"bold\" size=\"x-large\">Usage:</span>"
       "\n"
       "\n  <span weight=\"bold\">gscope [OPTION...] [source files]</span>"
       "\n"
       "\n<span weight=\"bold\">Help Options:</span>"
       "\n  -?, --help                      Show help options"
       "\n  --help-all                      Show all help options"
       "\n  --help-gtk                     Show GTK Options"
       "\n"
       "\n<span weight=\"bold\">Application Options:</span>"
       "\n  -b, --refOnly"
       "\n                          Build the cross-reference only"
       "\n                          (don't start GUI)."
       "\n  -c, --compressOff"
       "\n                          Use only ASCII characters in the"
       "\n                          cross-reference file (don't compress)."
       "\n  -d, --noBuild"
       "\n                          Do not update the cross-reference file."
       "\n  -f, --refFile=FILE"
       "\n                          Use the filename specified as the cross"
       "\n                          reference [output] file name instead of"
       "\n                          'cscope_db.out`"
       "\n  -i, --nameFile=FILE"
       "\n                          Use the filename specified as the"
       "\n                          list of source files to cross-reference."
       "\n  -I, --includeDir=PATH"
       "\n                          Use the specified directory search path"
       "\n                          to find #include files. (:dir1:dir2:dirN:)"
       "\n  -r, --rcFile=FILE"
       "\n                          Start Gscope using the preferences info"
       "\n                          from FILE."
       "\n  -R, --recurseDir"
       "\n                          Recursively search all subdirecties"
       "\n                          [Default = search below &lt;current-dir&gt;]"
       "\n                          for source files."
       "\n  -S, --srcDir=DIRECTORY"
       "\n                          Search the specified directory for source"
       "\n                          files.  When used with -R, use DIRECTORY"
       "\n                          as the root directory of the recursive search."
       "\n  -T, --truncSymbols"
       "\n                          Use only the first eight characters to"
       "\n                          match against C symbols."
       "\n  -v, --version"
       "\n                          Show version information"
    ;

    DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_OTHER, message, FALSE);
}


#ifndef GTK4_BUILD
void on_setup1_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_setup1_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    const gchar *message =
       "<span foreground=\"blue\" weight=\"bold\" size=\"x-large\">G-Scope Setup Hints</span>\n\n"
       "G-Scope can be fully configured from within the application.\n\n"
       "However, if you wish to override some start-up settings, \nG-Scope can be configured with:\n"
       "    - Command line arguments\n"
       "    - Shell environment variables\n"
       "    - By editing the APPLICATION configuration file <span weight=\"bold\">~/.gscope/gscoperc</span>\n\n"
       "<span weight=\"bold\" foreground=\"red\">Warning:</span> gscoperc is actively modified "
       "by the G-Scope program. <span weight=\"bold\">DO NOT EDIT THIS FILE WHILE G-SCOPE IS RUNNING.</span>\n\n"
       "If you wish to modify GTK STYLE settings for G-Scope, you can modify the file: "
       #if defined(GTK4_BUILD) || defined(GTK3_BUILD)
       " ~/.gscope/gscope.css\n\n"
       #else    // gtk2
       " ~/.gscope/gtkrc\n\n"
       #endif
       "<span weight=\"bold\" size=\"large\">G-Scope Environment Variables</span>\n\n"
       "    $TMPDIR = <span style=\"italic\">path</span>  [All G-Scope temporary files are placed in this directory]";


    DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_OTHER, (gchar *)message, FALSE);
}


#ifndef GTK4_BUILD
void on_about1_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
void on_about1_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    static gboolean customized = FALSE;

    if (!customized)
    {
        gchar description[] = "Interactive C source code browser";
        gchar based_on[] =    "\n\nDerived from CSCOPE version 15.5";
        #if defined(GTK4_BUILD)
            gchar modified_by[] = "\nPorted to Linux with GTK2 UI (July 2007)\nAdded GTK3 UI (October 2015)\nAdded GTK4 UI (January 2026)";
            #else
                #if defined(GTK3_BUILD)
                    gchar modified_by[] = "\nPorted to Linux with GTK2 UI (July 2007)\nAdded GTK3 UI (October 2015)";
                #else   // GTK2
                    gchar modified_by[] = "\nPorted to Linux with GTK2 UI (July 2007)";
                #endif
        #endif
        gchar gtk_version[] = "\n\nCurrent System GTK Version: ";
        gchar build_date[] =  "\n\nBuild Date: "__DATE__;

        gchar *full_comment;

        #ifndef GTK4_BUILD
        GdkPixbuf *pixbuf = create_pixbuf("gnome-stock-about.png");
        if (pixbuf)
        {
            gtk_window_set_icon(GTK_WINDOW(aboutdialog1), pixbuf);
            g_object_unref(pixbuf);
        }
        #endif

        // The UI maker for GTK4 does not allow direct configuration of the program icon using a custom image (only allows icons)
        #if defined(GTK4_BUILD)
        {
            GFile *logo_file = g_file_new_for_path(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps/gscope_globe3.png");
            GdkTexture *gscope_logo = gdk_texture_new_from_file(logo_file, NULL);
            g_object_unref(logo_file);
            gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(aboutdialog1), GDK_PAINTABLE(gscope_logo));
        }
        #endif

        gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(aboutdialog1), "Version "VERSION);

        my_asprintf(&full_comment, "%s%s%s%s%d.%d.%d%s",
                    description,
                    based_on,
                    modified_by,
                    gtk_version, gtk_get_major_version(), gtk_get_minor_version(), gtk_get_micro_version(),
                    build_date);

        gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(aboutdialog1), full_comment);
        g_free(full_comment);

        gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(aboutdialog1), "https://github.com/tefletch/gscope/wiki");
        gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(aboutdialog1), "Gscope Wiki");

        gtk_window_set_transient_for(GTK_WINDOW(aboutdialog1), GTK_WINDOW(gscope_main));

        customized = TRUE;
    }


    gtk_widget_show(aboutdialog1);

}


//===================================================
//     End GTK multi-variant MENU ITEM handling
//===================================================






//--------------------- Application Exit Callbacks -----------------------





//#ifndef GTK4_BUILD  //  GTK4 shutdown semantics are different
#if 0
void on_quit_close(GtkDialog *dialog, gpointer user_data)
{
    printf("Test hello: %s\n", __func__);
}
void on_quit_close_request(GtkWindow *window, gpointer user_data)
{
    printf("Test hello: %s\n", __func__);
}
void on_quit_destroy(GtkWindow *window)
{
    printf("Test hello: %s\n", __func__);
}
#endif





//#endif

//--------------------- End Application Exit Callbacks -----------------------

#ifndef GTK4_BUILD
void on_aboutdialog1_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    switch (response_id)
    {
        case GTK_RESPONSE_CLOSE:
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_hide(GTK_WIDGET(dialog));
            break;

        default:
        {
            char *error_string;

            my_asprintf(&error_string, "\nG-Scope Warning: Unexpected response: [%d] from: \"about\" dialog.", response_id);
            DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_WARNING, error_string, TRUE);
            g_free(error_string);
            gtk_widget_hide(GTK_WIDGET(dialog));
        }
            break;
    }

    return;
}
#endif



#ifndef GTK4_BUILD
gboolean on_aboutdialog1_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
#else
gboolean on_aboutdialog1_close_request(GtkWidget *widget, GdkEvent *event, gpointer user_data)
#endif
{
    gtk_widget_hide(widget);
    return TRUE;     // Do not destroy the widget
}



void on_find_c_identifier_button_clicked(GtkButton       *button,
                                         gpointer         user_data)
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_SYMBOL);
        search_button_lockout = FALSE;
    }
}


void on_find_definition_of_button_clicked(GtkButton       *button,
                                          gpointer         user_data)
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_DEF);
        search_button_lockout = FALSE;
    }
}


void on_find_functions_called_by_button_clicked(GtkButton       *button,
                                                gpointer         user_data)
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_CALLEDBY);
        search_button_lockout = FALSE;
    }
}


void on_find_functions_calling_button_clicked(GtkButton       *button,
                                              gpointer         user_data)
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_CALLING);
        search_button_lockout = FALSE;
    }
}


void on_find_text_string_button_clicked(GtkButton       *button,
                                        gpointer         user_data)
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_STRING);
        search_button_lockout = FALSE;
    }
}


void on_find_egrep_pattern_button_clicked(GtkButton       *button,
                                          gpointer         user_data)
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_REGEXP);
        search_button_lockout = FALSE;
    }
}


void on_find_files_button_clicked(GtkButton       *button,
                                  gpointer         user_data)
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_FILE);
        search_button_lockout = FALSE;
    }
}


void on_find_files_including_button_clicked(GtkButton       *button,
                                            gpointer         user_data)
{
    if (!search_button_lockout)
    {
        search_button_lockout = TRUE;
        process_query(FIND_INCLUDING);
        search_button_lockout = FALSE;
    }
}




void on_cancel_button_clicked(GtkButton       *button,
                              gpointer         user_data)
{
    SEARCH_cancel();           // Signal the search routine to stop
    cancel_requested = TRUE;   // Set the cancel indicator - UI will report that search results are truncated.
}


void on_open_terminal(GtkWidget *menuitem, gchar *container)
{
    gchar *command;
    gchar *coded_container;

    coded_container = strdup(container);
    my_space_codec(ENCODE, coded_container);     // Encode any 'space' characters found in the container path

    my_asprintf(&command, settings.terminalApp, coded_container);  // Synthesize the full command line
    my_system(command);

    free(command);
    free(coded_container);
}


void on_open_file_browser(GtkWidget *menuitem, gchar *container)
{
    gchar *command;
    gchar *coded_container;

    coded_container = strdup(container);
    my_space_codec(ENCODE, coded_container);     // Encode any 'space' characters found in the container path

    my_asprintf(&command, settings.fileManager, coded_container);
    my_system(command);

    free(command);
    free(coded_container);
}


void on_open_call_browser(GtkWidget *menuitem, gchar *entry)
{
    gchar * fname;
    gchar *linenum;
    gchar *function;
    gchar *esc_markup;
    static gboolean initialized = FALSE;

    if ( !initialized )
    {
       initialized = BROWSER_init();
    }

    fname = entry;

    while (*entry++ != '|');
    *(entry - 1) = '\0'; // null terminate file name
    linenum = entry;

    while (*entry++ != '|');
    *(entry - 1) = '\0'; // null terminate line num
    function = entry;

    esc_markup = g_markup_escape_text(function, -1);    // Needed when function = "<global>".
    browser_window = BROWSER_create(esc_markup, fname, linenum);
    g_free(esc_markup);

    gtk_widget_show(browser_window);

}


void on_open_quick_view(GtkWidget *menuitem, gchar *file_and_line)
{
    gchar *linenum;

    linenum = strchr(file_and_line, '|');

    if (linenum)
    {
        *linenum++ = '\0';  // Null-terminate the filename and advance the linenum pointer to start of line number data

        FILEVIEW_create(file_and_line, atoi(linenum));
    }
    else
        fprintf(stderr, "Warning: Unexpected file_and_line parse error in %s()\nFile-view window creation failed.\n", __func__);
}



#ifndef GTK4_BUILD  // Consider replacing this function with on_history_treeview_row_activated (GTK4 solution - #else below)
gboolean on_history_treeview_button_press_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    static GtkWidget *query_entry;
    static gboolean initialized = FALSE;
    gchar *entry;

    if (!initialized)
    {
        initialized = TRUE;
       query_entry = lookup_widget(GTK_WIDGET(gscope_main), "query_entry");    // Get a pointer to the query entry widget
    }


    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;   // only process single button click events (filter out GDK_[23]BUTTON_PRESS event types)

    if (DISPLAY_get_clicked_entry(event->x, event->y, &entry))
    {
        my_gtk_entry_set_text(GTK_ENTRY(query_entry), entry);
        g_free(entry);
    }

    return FALSE;
}
#else       // GTK4 "new and improved" treeview button handling NOTE: This migt work for all GTK versions in lieu of history_treeview_button_press_event(above)
void on_history_treeview_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    gchar            *entry;
    static GtkWidget *query_entry;
    static gboolean  initialized = FALSE;

    if ( !initialized )
    {
        initialized = TRUE;
        query_entry = lookup_widget(GTK_WIDGET(gscope_main), "query_entry");    // Get a pointer to the query entry widget
    }
    if ( DISPLAY_get_history_row_entry(path, &entry) )
    {
        my_gtk_entry_set_text(GTK_ENTRY(query_entry), entry);
        g_free(entry);
    }
}
#endif




#ifndef GTK4_BUILD  // GTK4 treeview EventButton handling 
void show_context_menu(GtkWidget *treeview, GdkEventButton *event, gchar *filename, gchar *linenum, gchar *symbol)
{
    static GtkWidget   *menu = NULL;
    static GtkWidget   *menu_item;
    gchar              tmp_path[PATHLEN];
    static gchar       container[(2 * PATHLEN) + 1];                   // +1 for '/'
    static gchar       file_and_line[PATHLEN + MAX_LINENUM_SIZE + 2];  // +2 for Field-Delimiter and trailing null
    static gchar       entry[1024];

    /*** Construct the Context Menu ***/
    if (!menu)
    {
        menu = gtk_menu_new();

        menu_item = gtk_menu_item_new_with_label("Open Containing Folder");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
        g_signal_connect(menu_item, "activate", (GCallback)on_open_terminal, container);

        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

        menu_item = gtk_menu_item_new_with_label("Browse Containing Folder");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
        g_signal_connect(menu_item, "activate", (GCallback)on_open_file_browser, container);


        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

        menu_item = gtk_menu_item_new_with_label("Browse Static Calls");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
        g_signal_connect(menu_item, "activate", (GCallback)on_open_call_browser, entry);

        menu_item = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

        menu_item = gtk_menu_item_new_with_label("Quick-View");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
        g_signal_connect(menu_item, "activate", (GCallback)on_open_quick_view, file_and_line);
    }


    /*** Update the contents of "container" ***/
    strcpy(tmp_path, filename);
    my_dirname(tmp_path);

    if (tmp_path[0] != '/')    // Not an absolute path
    {
        if (tmp_path[0] == '\0')
        {
            // Null 'dirname' add CWD
            if ( g_strlcpy(container, DIR_get_path(DIR_CURRENT_WORKING), PATHLEN) >= PATHLEN )
            {
                // Do nothing: Quietly truncate the path
            }
        }
        else
        {
            // Relative 'dirname' make it absolute
            snprintf(container, (2 * PATHLEN), "%s/%s", DIR_get_path(DIR_CURRENT_WORKING), tmp_path);
        }
    }

    /*** Update the contents of "file_and_line ***/
    snprintf(file_and_line, PATHLEN + MAX_LINENUM_SIZE + 1, "%s|%s", filename, linenum);

    snprintf(entry, 1023, "%s|%s|%s", filename, linenum, symbol);

    gtk_widget_show_all(menu);
    /*** Run the context menu (beware of event == NULL) ***/
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   (event != NULL) ? event->button : 0,
                   gdk_event_get_time((GdkEvent *)event));
}
#else
void on_open_folder_activate(GSimpleAction *action, GVariant *parameter, gpointer app) 
{
    g_print("Item1\n");
}

void on_browse_folder_activate(GSimpleAction *action, GVariant *parameter, gpointer app) 
{
    g_print("Item2\n");
}

void show_context_menu(gint x, gint y, gchar *filename, gchar *linenum, gchar *symbol)
{
    GdkRectangle    rect;
    GMenuModel      *context_menu_model;

    rect.x = (gint) x;
    rect.y = (gint) y;
    rect.width = rect.height = 1;

    GtkWidget *label1 = gtk_label_new("Open Containing Folder");

    // Create the menu model
    GMenu *context_menu = g_menu_new();
    g_menu_append(context_menu, "Open Containing Folder", "app.open_containing_folder");
    g_menu_append(context_menu, "Browse Containing Folder", "app.browse_containing_folder");

    // Create Popover from model
    GtkWidget *popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(context_menu));

    
    // Attach Popover to desired widget
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    //g_object_set((gpointer) rect, "popover", popover, NULL);
    
    
         //g_object_unref(menu_item1);
        //g_object_unref(menu_item2);

    //gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);

    g_object_unref(context_menu); // Model is owned by popover

    gtk_popover_popup(GTK_POPOVER(popover));
    
#if 0
    if ( gtk_popover_menu_add_child( GTK_POPOVER_MENU(popover), label1, "test1") )
        printf("popover_menu_add_child: succeeded\n");
    else
        printf("popover_menu_add_child: failed\n");
#endif

}
#endif


#ifndef GTK4_BUILD
/*** Handle shift-F10 (context menu without a mouse) ***/
gboolean on_treeview1_popup_menu(GtkWidget       *widget,
                                 gpointer         user_data)
{
    gchar *filename;
    gchar *linenum;
    gchar *symbol;

    GtkTreeSelection *selection;
    GtkTreeModel     *model;
    GtkTreeIter      iter;

    GtkTreePath     *path;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        path = gtk_tree_model_get_path(model, &iter);

        if (DISPLAY_get_entry_info(path, &filename, &linenum, &symbol)) // Get the selected filename
        {
            // pass filename into show menu call
            show_context_menu(widget, NULL, filename, linenum, symbol);
        }

        gtk_tree_path_free(path);
    }
    // else no row selected.

    return TRUE;
}


#endif  // GTK4 treeview EventButton handling 


#ifndef GTK4_BUILD
gboolean on_treeview1_button_press_event(GtkWidget *widget, GdkEventButton  *event, gpointer user_data)
{
    gchar *filename;
    gchar *linenum;
    gchar *symbol;

    GtkTreePath     *path;

    if (event->type == GDK_BUTTON_PRESS)  // Process single button click (ignore GDK_[23]BUTTON_PRESS event types)
    {
        switch (event->button)
        {
            case 1:     // left-button -- (optional) single click open
                if (settings.singleClick)     // Single Click mode
                {
                    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, NULL, NULL, NULL))
                    {
                        if (DISPLAY_get_filename_and_lineinfo(path, &filename, &linenum))
                        {
                            if (settings.useEditor)
                            {
                                my_start_text_editor(filename, linenum);
                            }
                            else
                            {
                                FILEVIEW_create(filename, atoi(linenum));
                            }
                        }
                        gtk_tree_path_free(path);
                    }
                }
                break;

            case 3:     // right-button -- List item context menu

                // Get the GtkTreePath
                if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, NULL, NULL, NULL))
                {
                    if (DISPLAY_get_entry_info(path, &filename, &linenum, &symbol)) // Get the selected filename
                    {
                        // pass filename into show menu call
                        show_context_menu(widget, event, filename, linenum, symbol);
                    }
                    gtk_tree_path_free(path);
                }

                break;

            default:
                // Do nothing
                break;
        }
    }
    return FALSE;
}

#else
// Left-button press -- OPTIONAL single click open
void on_treeview1_button1_pressed(GtkGestureClick* self, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    printf("Hello from: %s\n", __func__);
}


// Right-button press -- Present item context menu
void on_treeview1_button3_pressed(GtkGestureClick* self, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    GtkTreePath     *path;
    gchar           *filename;
    gchar           *linenum;
    gchar           *symbol;
    gint            bin_x;
    gint            bin_y;

    printf("Hello from: %s\n", __func__);

    gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(my_lookup_widget("treeview1")), (gint) x, (gint) y, &bin_x, &bin_y);
    if ( gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(my_lookup_widget("treeview1")), bin_x, bin_y, &path, NULL, NULL, NULL) )
    {
        if (DISPLAY_get_entry_info(path, &filename, &linenum, &symbol)) // Get the selected filename
        {
            show_context_menu(bin_x, bin_y, filename, linenum, symbol);
            printf("%s: file: %s, line: %s, sym: %s\n", __func__, filename, linenum, symbol);
        }
        gtk_tree_path_free(path);      
    }
    else
        printf("%s: tree view path-get FAILED\n", __func__);
 

}

#endif


void on_treeview1_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    printf("Hello from: %s\n", __func__);
    if (!settings.singleClick)
    {
        gchar *filename;
        gchar *linenum;

        if (DISPLAY_get_filename_and_lineinfo(path, &filename, &linenum))
        {
            if (settings.useEditor)
            {
                my_start_text_editor(filename, linenum);
            }
            else
            {
                FILEVIEW_create(filename, atoi(linenum));
            }
        }
    }
}


//----------------- Callbacks from the Preferences Dialog ----------------------


void on_preferences_dialog_close_button_clicked(GtkButton *button, gpointer user_data)
{
    gtk_widget_set_visible(gscope_preferences, FALSE);
}

//================================
// Callbacks not used for GTK4
//================================
#ifndef GTK4_BUILD
gboolean on_gscope_preferences_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    gtk_widget_hide(widget);  // Just hide the widget
    return TRUE;   // Don't destroy the widget
}

gboolean on_folder_chooser_dialog_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    gtk_widget_hide(widget);
    return TRUE;     // Do not destroy the widget
}



#endif


#ifndef GTK4_BUILD
void on_use_viewer_radiobutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_use_viewer_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    // Don't change the useEditor setting - that is handled in on_use_editor_radiobutton_toggled()

    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "reuse_window_checkbutton"), !settings.useEditor);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "editor_command_entry"), settings.useEditor);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "editor_command_label"), settings.useEditor);
}


// The setting managed by this callback interacts with the "options menu" (temporary overrides)
// on_useeditor_activate
#ifndef GTK4_BUILD
void on_use_editor_radiobutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_use_editor_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.useEditor )
    {
        // Toggle the "sticky" setting
        sticky_settings.useEditor = !(sticky_settings.useEditor);

        #ifndef GTK4_BUILD
        // Align the "options menu".  Warning, this "set-active" call toggles this "settings" value;
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), "useeditor_checkmenuitem")),
                                       sticky_settings.useEditor);
        
        #else
        GAction *action = g_action_map_lookup_action(G_ACTION_MAP(gscope_app), "useeditor");
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(sticky_settings.useEditor));
        #endif

        // Align application behavior with the new "sticky" setting (override any change caused by "set_active" callback)
        settings.useEditor = sticky_settings.useEditor;  // undo the value change caused by the "set_active" callback
        APP_CONFIG_set_boolean("useEditor", settings.useEditor);
    }

    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "reuse_window_checkbutton"), !settings.useEditor);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "editor_command_entry"), settings.useEditor);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "editor_command_label"), settings.useEditor);
}


void on_editor_command_entry_changed(GtkEditable *editable, gpointer user_data)
{
    editor_command_changed = TRUE;
}



//*** Start multi-Gtk version focus handling *******/
/***************************************************/

#ifndef GTK4_BUILD
gboolean on_editor_command_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_editor_command_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *editor_command;

    if (editor_command_changed)
    {
        editor_command =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "editor_command_entry")));

        if (strcmp(settings.fileEditor, editor_command) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("fileEditor", editor_command);

            // Update the application setting
            strcpy(settings.fileEditor, editor_command);
        }
        editor_command_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}



#ifndef GTK4_BUILD
gboolean on_suffix_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_suffix_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *suffix_list;

    if (suffix_entry_changed)
    {
        suffix_list = my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "suffix_entry")));

        if (strcmp(settings.suffixList, suffix_list) != 0)  // Only update if a real change has been made
        {
            if (APP_CONFIG_valid_list("File Suffix List", (char *)suffix_list, &settings.suffixDelim))
            {
                // Update the preferences file
                APP_CONFIG_set_string("suffixList", suffix_list);

                // Update the application setting
                strcpy(settings.suffixList, suffix_list);
            }
            else    // Invalid list syntax, revert to original (valid-by-default) setting.
            {
                my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "suffix_entry")), settings.suffixList);
            }
        }

        suffix_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_typeless_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_typeless_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *typeless_list;

    if (typeless_entry_changed)
    {
        typeless_list = my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "typeless_entry")));

        if (strcmp(settings.typelessList, typeless_list) != 0)  // Only update if a real change has been made
        {
            if (APP_CONFIG_valid_list("Typeless File List", (char *)typeless_list, &settings.typelessDelim))
            {
                // Update the preferences file
                APP_CONFIG_set_string("typelessList", typeless_list);

                // Update the application setting
                strcpy(settings.typelessList, typeless_list);
            }
            else
            {
                my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "typeless_entry")), settings.typelessList);
            }
        }

        typeless_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_ignored_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_ignored_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *ignored_list;

    if (ignored_entry_changed)
    {
        ignored_list = my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "ignored_entry")));

        if (strcmp(settings.ignoredList, ignored_list) != 0)  // Only update if a real change has been made
        {
            if (APP_CONFIG_valid_list("Ignored Directory List", (char *)ignored_list, &settings.ignoredDelim))
            {
                // Update the preferences file
                APP_CONFIG_set_string("ignoredList", ignored_list);

                // Update the application setting
                strcpy(settings.ignoredList, ignored_list);

                // Update the master ignored list
                DIR_list_join(settings.ignoredList, MASTER_IGNORED_LIST);
            }
            else
            {
                DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_WARNING, "Invalid 'Ignored List' Syntax.\nUpdate aborted.", TRUE);
                my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "ignored_entry")), settings.ignoredList);
            }
        }

        ignored_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_source_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_source_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *dirname;

    if (source_entry_changed)
    {
        dirname = my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "source_entry")));

        if (strcmp(settings.srcDir, dirname) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("srcDir", dirname);

            // Update the application setting
            strcpy(settings.srcDir, dirname);
        }

        source_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_include_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_include_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *include_list;

    if (include_entry_changed)
    {
        include_list = my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "include_entry")));

        if (strcmp(settings.includeDir, include_list) != 0)  // Only update if a real change has been made
        {
            if (APP_CONFIG_valid_list("Include File Search Path", (char *)include_list, &settings.includeDirDelim))
            {
                // Update the preferences file
                APP_CONFIG_set_string("includeDir", include_list);

                // Update the application setting
                strcpy(settings.includeDir, include_list);
            }
            else
            {
                my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "include_entry")), settings.includeDir);
            }
        }

        include_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_name_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_name_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *filename;

    if (name_entry_changed)
    {
        filename = my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "name_entry")));

        if (strcmp(settings.nameFile, filename) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("nameFile", filename);

            // Update the application setting
            strcpy(settings.nameFile, filename);
        }

        name_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_cref_entry_focus_out_event(GtkWidget   *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_cref_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *filename;

    if (cref_entry_changed)
    {
        filename = my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "cref_entry")));

        if (strcmp(settings.refFile, filename) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("refFile", filename);

            // Update the application setting
            strcpy(settings.refFile, filename);
        }

        cref_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_search_log_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_search_log_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *filename;

    if (search_log_entry_changed)
    {
        filename = my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "search_log_entry")));

        if (strcmp(settings.searchLogFile, filename) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("searchLogFile", filename);

            // Update the application setting
            strcpy(settings.searchLogFile, filename);
        }

        search_log_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_autogen_cache_path_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_autogen_cache_path_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *autogen_cache_path;

    if (autogen_cache_path_entry_changed)
    {
        autogen_cache_path =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cache_path_entry")));

        if (strlen(autogen_cache_path) == 0  ||
            access(autogen_cache_path, R_OK | W_OK) < 0
           )
        {
            // The user is not allowed to configure a "null" string for the cache path
            // and the directory must exist and have the required permissions

            DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_ERROR, "Invalid local cache path selected, reverting to default path.", TRUE);
            my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cache_path_entry")), autoGenPathDef);
            autogen_cache_path =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cache_path_entry")));
        }

        if (strcmp(settings.autoGenPath, autogen_cache_path) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("autoGenPath", autogen_cache_path);

            // Update the application setting
            strcpy(settings.autoGenPath, autogen_cache_path);
        }

        {
            gchar       active_path[PATHLEN * 2];

            snprintf(active_path, PATHLEN * 2, "My cache path: <span color=\"steelblue\">%s/%s/auto*_*</span>",
                     settings.autoGenPath,
                     getenv("USER"));
            gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_active_cache_path_label")), active_path);
        }

        autogen_cache_path_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_autogen_search_root_entry1_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_autogen_search_root_entry1_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *entry_text;

    if (autogen_search_root_entry1_changed)
    {
        entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_search_root_entry1")));

        if (strcmp(settings.autoGenRoot, entry_text) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("autoGenRoot", entry_text);

            // Update the application setting
            strcpy(settings.autoGenRoot, entry_text);
        }

        autogen_search_root_entry1_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_autogen_suffix_entry1_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_autogen_suffix_entry1_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *entry_text;
    gchar       auto_name[PATHLEN * 2];

    if (autogen_suffix_entry_1_changed)
    {
        entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_suffix_entry1")));

        if (strlen(entry_text) < 2 || entry_text[0] != '.')
        {
            // We always expect .<something>

            DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_ERROR, "Invalid Meta-source suffix, reverting to default.", TRUE);
            my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_suffix_entry1")), autoGenSuffixDef);
            entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_suffix_entry1")));
        }

        if (strcmp(settings.autoGenSuffix, entry_text) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("autoGenSuffix", entry_text);

            // Update the application setting
            strcpy(settings.autoGenSuffix, entry_text);

            // Update conditional content label
            snprintf(auto_name, PATHLEN * 2, "Generated filename format:  <span weight=\"bold\">foo</span>"
                     "<span weight=\"bold\" color=\"darkred\">%s</span> ---> "
                     "<b>foo</b><span weight=\"bold\" color=\"darkblue\">%s.c</span>  AND "
                     "<b>foo</b><span weight=\"bold\" color=\"darkblue\">%s.h</span>",
                     settings.autoGenSuffix,
                     settings.autoGenId,
                     settings.autoGenId
                    );
            gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_generated_filename_label")), auto_name);
        }

        autogen_suffix_entry_1_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_autogen_cmd_entry1_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_autogen_cmd_entry1_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *entry_text;

    if (autogen_cmd_entry1_changed)
    {
        entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cmd_entry1")));

        if (strlen(entry_text) < 1)
        {
            // We always expect <something>

            DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_ERROR, "Invalid Meta-source compile command, reverting to default.", TRUE);
            my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cmd_entry1")),  autoGenCmdDef);
            entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cmd_entry1")));
        }

        if (strcmp(settings.autoGenCmd, entry_text) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("autoGenCmd", entry_text);

            // Update the application setting
            strcpy(settings.autoGenCmd, entry_text);
        }

        autogen_cmd_entry1_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_autogen_id_entry1_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_autogen_id_entry1_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *entry_text;
    gchar       auto_name[PATHLEN * 2];

    if (autogen_id_entry1_changed)
    {
        entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_id_entry1")));

        // ID can be anything, including "" (null string)

        if (strcmp(settings.autoGenId, entry_text) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("autoGenId", entry_text);

            // Update the application setting
            strcpy(settings.autoGenId, entry_text);

            // Update conditional content label
            snprintf(auto_name, PATHLEN * 2, "Generated filename format:  <span weight=\"bold\">foo</span>"
                     "<span weight=\"bold\" color=\"darkred\">%s</span> ---> "
                     "<b>foo</b><span weight=\"bold\" color=\"darkblue\">%s.c</span>  AND "
                     "<b>foo</b><span weight=\"bold\" color=\"darkblue\">%s.h</span>",
                     settings.autoGenSuffix,
                     settings.autoGenId,
                     settings.autoGenId
                    );
            gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_generated_filename_label")), auto_name);
        }

        autogen_id_entry1_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_autogen_cache_threshold_spinbutton_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_autogen_cache_threshold_spinbutton_focus_out_event(GtkEventControllerFocus *widget, gpointer user_data)
#endif
{
    guint new_value;

    if (cache_threshold_changed)
    {
        new_value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

        if (new_value != settings.autoGenThresh) // Only update the config file is the value has actually changed
        {
            // Update the preferences file
            APP_CONFIG_set_integer("autoGenThresh", new_value);

            // Update the application setting
            settings.autoGenThresh = new_value;
        }

        cache_threshold_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_terminal_app_entry_focus_out_event(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_terminal_app_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *entry_text;
    gchar       *tmp_ptr;

    if (terminal_app_entry_changed)
    {
        entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "terminal_app_entry")));

        if ((strlen(entry_text) < 3) || (strstr(entry_text, "%s") == NULL))
        {
            // We always expect something in entry_text and at least one %s format specifier
            DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_ERROR, "Invalid Terminal App command, reverting to default.", TRUE);
            my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "terminal_app_entry")),  terminalAppDef);
            entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "terminal_app_entry")));
        }

        // We always expect a command with one single argument  e.g."terminal <path>"
        tmp_ptr = strchr(entry_text, ' ');
        if (tmp_ptr == NULL || strchr(tmp_ptr + 1, ' ') != NULL)   //  if num_args != 1
        {
            DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_ERROR, "Terminal App command format error, single (1) option required, reverting to default.", TRUE);
            my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "terminal_app_entry")),  terminalAppDef);
            entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "terminal_app_entry")));
        }

        if (strcmp(settings.terminalApp, entry_text) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("terminalApp", entry_text);

            // Update the application setting
            strcpy(settings.terminalApp, entry_text);
        }

        terminal_app_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


#ifndef GTK4_BUILD
gboolean on_file_manager_app_entry_focus_out_event(GtkWidget  *widget, GdkEventFocus *event, gpointer user_data)
#else
void on_file_manager_app_entry_focus_out_event(GtkEventControllerFocus *controller, gpointer user_data)
#endif
{
    const gchar *entry_text;
    gchar       *tmp_ptr;

    if (file_manager_app_entry_changed)
    {
        entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "file_manager_app_entry")));

        if ((strlen(entry_text) < 3) || (strstr(entry_text, "%s") == NULL))
        {
            // We always expect something in entry_text and at least one %s format specifier
            DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_ERROR, "Invalid File Manager App command, reverting to default.", TRUE);
            my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "file_manager_app_entry")),  fileManagerDef);
            entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "file_manager_app_entry")));
        }

        // We always expect a command with one single argument  e.g."terminal <path>"
        tmp_ptr = strchr(entry_text, ' ');
        if (tmp_ptr == NULL || strchr(tmp_ptr + 1, ' ') != NULL)   //  if num_args != 1
        {
            DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_ERROR, "File Manager App command format error, single (1) option required, reverting to default.", TRUE);
            my_gtk_entry_set_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "file_manager_app_entry")),  fileManagerDef);
            entry_text =  my_gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(gscope_preferences), "file_manager_app_entry")));
        }

        if (strcmp(settings.terminalApp, entry_text) != 0)  // Only update if a real change has been made
        {
            // Update the preferences file
            APP_CONFIG_set_string("fileManager", entry_text);

            // Update the application setting
            strcpy(settings.fileManager, entry_text);
        }

        file_manager_app_entry_changed = FALSE;
    }

    #ifndef GTK4_BUILD
    return(FALSE);
    #endif
}


//***** End multi-Gtk version focus handling *******/
/***************************************************/


#ifndef GTK4_BUILD
void on_no_rebuild_radiobutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_no_rebuild_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif

{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) )   // noBuild is being selected
    {
        if ( !settings.noBuild )  // Turn ON noBuild
        {
            settings.noBuild = TRUE;
            APP_CONFIG_set_boolean("noBuild", settings.noBuild);    // Update the preferences file
        }
        if ( settings.updateAll)
        {
            settings.updateAll = FALSE;
            APP_CONFIG_set_boolean("updateAll", settings.updateAll);// Update the preferences file
        }
    }
    else    // noBuild is being de-selected
    {
        if ( settings.noBuild )
        {
            settings.noBuild = FALSE;
            APP_CONFIG_set_boolean("noBuild", settings.noBuild);    // Update the preferences file
        }
    }
    //printf("on_no_rebiuld: force=%s, nobuild=%s\n", settings.updateAll ? "T" : "F", settings.noBuild ? "T" : "F");
}


#ifndef GTK4_BUILD
void on_rebuild_radiobutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_rebuild_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) )   // Build-if-needed is being selected
    {
        if ( settings.updateAll )
        {
            settings.updateAll = FALSE;
            APP_CONFIG_set_boolean("updateAll", settings.updateAll);    // Update the preferences file
        }
        if ( settings.noBuild )
        {
            settings.noBuild = FALSE;
            APP_CONFIG_set_boolean("noBuild", settings.noBuild);    // Update the preferences file
        }
    }
 // else
 //     Nothing to do when rebuild_checkbutton goes off
    //printf("on_rebuild: force=%s, nobuild=%s\n", settings.updateAll ? "T" : "F", settings.noBuild ? "T" : "F");
}


#ifndef GTK4_BUILD
void on_force_rebuild_radiobutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_force_rebuild_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) )   // Force rebuild is being selected
    {
        if ( !settings.updateAll )
        {
            settings.updateAll = TRUE;
            APP_CONFIG_set_boolean("updateAll", settings.updateAll);    // Update the preferences file
        }
        if ( settings.noBuild )
        {
            settings.noBuild = FALSE;
            APP_CONFIG_set_boolean("noBuild", settings.noBuild);    // Update the preferences file
        }
    }
    else    // force-rebuild is being de-selected
    {
        if (settings.updateAll)
        {
            settings.updateAll = FALSE;
            APP_CONFIG_set_boolean("updateAll", settings.updateAll);// Update the preferences file
        }
    }
    //printf("on_force: force=%s, nobuild=%s\n", settings.updateAll ? "T" : "F", settings.noBuild ? "T" : "F");
}




void on_suffix_entry_changed(GtkEditable *editable, gpointer user_data)
{
    suffix_entry_changed = TRUE;
}


void on_typeless_entry_changed(GtkEditable     *editable,
                               gpointer         user_data)
{
    typeless_entry_changed = TRUE;
}


void on_ignored_entry_changed(GtkEditable     *editable,
                              gpointer         user_data)
{
    ignored_entry_changed = TRUE;
}


static void open_directory_browser(void)
{
    gtk_widget_show(folder_chooser_dialog);
}


void on_source_directory_browse_button_clicked(GtkButton  *button, gpointer user_data)
{
    open_directory_browser();
    active_dir_entry = 0;
}


void on_folder_chooser_dialog_response(GtkDialog       *dialog,
                                       gint             response_id,
                                       gpointer         user_data)
{
    static GtkWidget *source_entry[3];
    static gboolean initialized = FALSE;
    gchar *dirname;

    if (!initialized)
    {
        // This is an array of "entry" widgets served by this directory chooser response handler
        source_entry[0] = lookup_widget(GTK_WIDGET(gscope_preferences), "source_entry");
        //source_entry[1] = lookup_widget(GTK_WIDGET (gscope_preferences), "autogen_cache_path_entry");
    }

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            dirname = my_gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            my_gtk_entry_set_text(GTK_ENTRY(source_entry[active_dir_entry]), dirname);

            
            // Update the settings structure and the preferences file
            switch (active_dir_entry)
            {
                case 0:    // Source Directory
                    strcpy(settings.srcDir, dirname);
                    APP_CONFIG_set_string("srcDir", dirname);
                    break;

#if 0
                case 1:    // AutoGen Cache Location
                    strcpy(settings.autoGenPath, dirname);
                    APP_CONFIG_set_string("autoGenPath", dirname);
                    {
                        gchar       active_path[PATHLEN * 2];

                        snprintf(active_path, PATHLEN * 2, "Active cache path: <span color=\"blue\">%s.%d</span>",
                                 settings.autoGenPath,
                                 getpid());
                        gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(gscope_preferences),"autogen_active_cache_path_label")), active_path);
                    }
                    break;

                case 2:    // Include Directory
                    strcpy(settings.includeDir, dirname);
                    APP_CONFIG_set_string("includeDir", dirname);
                    break;
#endif
            }

            g_free(dirname);
            gtk_widget_hide(GTK_WIDGET(dialog));

            break;

        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_hide(GTK_WIDGET(dialog));
            break;

        default:
        {
            char *error_string;

            my_asprintf(&error_string, "\nG-Scope Warning: Unexpected response: [%d] from: \"folder chooser\" dialog.", response_id);
            DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_WARNING, error_string, TRUE);
            g_free(error_string);

            gtk_widget_hide(GTK_WIDGET(dialog));
        }

            break;
    }

    return;
}





void on_source_entry_changed(GtkEditable     *editable,
                             gpointer         user_data)
{
    source_entry_changed = TRUE;
}


void on_include_entry_changed(GtkEditable *editable, gpointer user_data)
{
    include_entry_changed = TRUE;
}


// ==== Input File Selection ====
static void open_file_name_browser(void)
{
    gtk_widget_show(open_file_chooser_dialog);
}


void on_open_file_chooser_dialog_response(GtkDialog       *dialog,
                                          gint             response_id,
                                          gpointer         user_data)
{
    static GtkWidget *input_entry[2];
    static gboolean initialized = FALSE;
    gchar *filename;

    if (!initialized)
    {
        // This is an array of "entry" widgets served by the "open file" chooser response handler
        input_entry[0] = lookup_widget(GTK_WIDGET(gscope_preferences), "name_entry");
        //input_entry[1] = lookup_widget(GTK_WIDGET (gscope_preferences), "context_entry");
    }

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
            filename = my_gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            my_gtk_entry_set_text(GTK_ENTRY(input_entry[active_input_entry]), filename);

            // Update the settings structure and the preferences file
            switch (active_input_entry)
            {
                case 0:    // Source file name list
                    strcpy(settings.nameFile, filename);
                    APP_CONFIG_set_string("nameFile", filename);
                    break;

                case 1:    // No longer used
                    /* do nothing */
                    break;
            }

            g_free(filename);
            gtk_widget_hide(GTK_WIDGET(dialog));

            break;

        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_hide(GTK_WIDGET(dialog));
            break;

        default:
        {
            char *error_string;

            my_asprintf(&error_string, "\nG-Scope Warning: Unexpected response: [%d] from: \"file open\" dialog.", response_id);
            DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_WARNING, error_string, TRUE);
            g_free(error_string);

            gtk_widget_hide(GTK_WIDGET(dialog));
        }

            break;
    }

    return;
}



gboolean on_open_file_chooser_dialog_delete_event(GtkWidget *widget,
                                                  GdkEvent *event,
                                                  gpointer user_data)
{
    gtk_widget_hide(widget);
    return TRUE;     // Do not destroy the widget
}



void on_name_entry_changed(GtkEditable     *editable,
                           gpointer         user_data)
{
    name_entry_changed = TRUE;
}


void on_file_name_browse_button_clicked(GtkButton *button, gpointer user_data)
{
    open_file_name_browser();
    active_input_entry = 0;
}


// ==== Output file selection ====
static void open_output_file_name_browser(void)
{
    gtk_widget_show(output_file_chooser_dialog);
}



void on_output_file_chooser_dialog_response(GtkDialog       *dialog,
                                            gint             response_id,
                                            gpointer         user_data)
{
    static GtkWidget *output_entry[2];
    static gboolean initialized = FALSE;
    gchar *filename;

    if (!initialized)
    {
        // This is an array of "entry" widgets served by the "open file" chooser response handler
        output_entry[0] = lookup_widget(GTK_WIDGET(gscope_preferences), "cref_entry");
        output_entry[1] = lookup_widget(GTK_WIDGET(gscope_preferences), "search_log_entry");
    }

    switch (response_id)
    {
        case GTK_RESPONSE_OK:

            #ifndef GTK4_BUILD
            filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            #else
            GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
            filename = g_file_get_path(file);
            g_free(file);
            #endif

            if (strlen(filename) >= MAX_STRING_ARG_SIZE)
            {
                DISPLAY_message_dialog(GTK_WINDOW(gscope_preferences), GTK_MESSAGE_WARNING, "Selected File path is too long.\nUpdate aborted.", TRUE);
                filename = (gchar *)my_gtk_entry_get_text(GTK_ENTRY(output_entry[active_output_entry]));
            }
            else
            {
                my_gtk_entry_set_text(GTK_ENTRY(output_entry[active_output_entry]), filename);
            }

            // Update the settings structure and the preferences file
            switch (active_output_entry)
            {
                case 0:    // Source file name list
                    if (strcmp(settings.refFile, filename) != 0)  // Only update if a real change has been made
                    {
                        strcpy(settings.refFile, filename);
                        APP_CONFIG_set_string("refFile", filename);
                    }
                    break;

                case 1:    // Initial context file
                    if (strcmp(settings.searchLogFile, filename) != 0)  // Only update if a real change has been made
                    {
                        strcpy(settings.searchLogFile, filename);
                        APP_CONFIG_set_string("searchLogFile", filename);
                    }
                    break;
            }

            g_free(filename);
            gtk_widget_hide(GTK_WIDGET(dialog));

            break;

        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_DELETE_EVENT:
            gtk_widget_hide(GTK_WIDGET(dialog));
            break;

        default:
        {
            char *error_string;

            my_asprintf(&error_string, "\nG-Scope Warning: Unexpected response: [%d] from: \"output file\" dialog.", response_id);
            DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_WARNING, error_string, TRUE);
            g_free(error_string);

            gtk_widget_hide(GTK_WIDGET(dialog));
        }

            break;
    }

    return;
}



gboolean on_output_file_chooser_dialog_delete_event(GtkWidget *widget,
                                                    GdkEvent *event,
                                                    gpointer user_data)
{
    gtk_widget_hide(widget);
    return TRUE;     // Do not destroy the widget
}



void on_cref_entry_changed(GtkEditable     *editable,
                           gpointer         user_data)
{
    cref_entry_changed = TRUE;
}


void on_cross_reference_browse_button_clicked(GtkButton       *button,
                                              gpointer         user_data)
{
    open_output_file_name_browser();
    active_output_entry = 0;
}


void on_search_log_entry_changed(GtkEditable     *editable,
                                 gpointer         user_data)
{
    search_log_entry_changed = TRUE;
}


void on_search_log_browse_button_clicked(GtkButton       *button,
                                         gpointer         user_data)
{
    open_output_file_name_browser();
    active_output_entry = 1;

}


// Enable/Disable search logging
void on_search_log_button_clicked(GtkButton       *button,
                                  gpointer         user_data)
{
    settings.searchLogging = !settings.searchLogging;

    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "search_log_label"), settings.searchLogging);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "search_log_entry"), settings.searchLogging);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "search_log_browse_button"), settings.searchLogging);

    APP_CONFIG_set_boolean("searchLogging", settings.searchLogging);

    if (settings.searchLogging)
        gtk_button_set_label(GTK_BUTTON(lookup_widget(GTK_WIDGET(gscope_preferences), "search_log_button")), "Disable");
    else
        gtk_button_set_label(GTK_BUTTON(lookup_widget(GTK_WIDGET(gscope_preferences), "search_log_button")), "Enable");
}



//----------------- Start Session Statistics Reporting ---------------


#ifndef GTK4_BUILD
gboolean on_stats_dialog_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    gtk_widget_hide(widget);
    stats_visible = FALSE;
    return TRUE;
}
#endif

void on_stats_dialog_closebutton_clicked(GtkButton       *button,
                                         gpointer         user_data)
{
    gtk_widget_hide(GTK_WIDGET(stats_dialog));
    stats_visible = FALSE;
}

//----------------- End Session Statistics Reporting ---------------


// UI_VERSION 1: The setting managed by this callback interacts with the "options menu" (temporary overrides)
// UI_VERSION 2: Top menu Option-->Retain input for next query (deleted menu item)
void on_retain_text_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    if ( my_gtk_check_button_get_active(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_checkbutton")) != sticky_settings.retainInput )
    {
        /*** Manage Settings ***/
        /***********************/
        sticky_settings.retainInput = !(sticky_settings.retainInput);   // Toggle the "sticky" setting
        settings.retainInput = sticky_settings.retainInput;             // Align application behavior with the new "sticky" setting
                                                                        // (override any change caused by "set_active" callback) 
        APP_CONFIG_set_boolean("retainInput", settings.retainInput);    // Update the preferences file

        /*** Manage UI ***/
        /*****************/
        #if (UI_VERSION == 1)  // "Options-->Retain Input for next query" deprecated @ UIv2 (GTK4)
        // Align the "options menu".  Warning, this "set-active" call toggles this "settings" value;
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), "retaininput_checkmenuitem")),
                                       sticky_settings.retainInput);

        // If retainInput = TRUE, disable the "retain_text_failed_search" checkbutton, otherwise enable the checkbutton
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "retain_text_failed_search_checkbutton"), !settings.retainInput);
        gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "retain_text_failed_search_image"), !settings.retainInput);
        // GTK2 ??? gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "retain_text_failed_search_label"), !settings.retainInput);
        #endif

        #if (UI_VERSION > 1)  // UIv2+ Query text retention radiobutton-like semantics Always/Fails/None
        GtkWidget *retain_text = lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_checkbutton");
        GtkWidget *retain_text_failed_search = lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_failed_search_checkbutton");
        GtkWidget *never_retain_text = lookup_widget(GTK_WIDGET(prefs_dialog), "never_retain_text_checkbutton");

        if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(retain_text)) )        // if retain: off-to-on transition
        {
            if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(retain_text_failed_search)) ) // failed: if on, force off
                my_gtk_check_button_set_active(retain_text_failed_search, FALSE);

            if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(never_retain_text)) ) // never: if on, force off
                my_gtk_check_button_set_active(never_retain_text, FALSE);
        }
        else    // else retain: on-to-off transition
        {
            if (  !gtk_check_button_get_active(GTK_CHECK_BUTTON(retain_text_failed_search)) &&
                  !gtk_check_button_get_active(GTK_CHECK_BUTTON(never_retain_text)) )
                my_gtk_check_button_set_active(never_retain_text, TRUE);

        }
       #endif
    }
}


void on_retain_text_failed_search_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    if ( my_gtk_check_button_get_active(lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_failed_search_checkbutton")) != settings.retainFailed )
    {
        // This setting is only implemented as a start-up setting, hence there is no "active" (volatile) setting to match.

        /*** Manage Settings ***/
        /***********************/
        settings.retainFailed = !(settings.retainFailed);
        APP_CONFIG_set_boolean("retainFailed", settings.retainFailed);  // Update the preferences file

        /*** Manage UI ***/
        /*****************/
        #if (UI_VERSION > 1)
        GtkWidget *retain_text = lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_checkbutton");
        GtkWidget *retain_text_failed_search = lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_failed_search_checkbutton");
        GtkWidget *never_retain_text = lookup_widget(GTK_WIDGET(prefs_dialog), "never_retain_text_checkbutton");

        if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(retain_text_failed_search)) )        // if failed: off-to-on transition
        {
            if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(retain_text)) )                   // retain: if on, force off
                my_gtk_check_button_set_active(retain_text, FALSE);

            if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(never_retain_text)) ) // never: if on, force off
                my_gtk_check_button_set_active(never_retain_text, FALSE);
        }
        else  // else failed: on-to-off transition
        {
            if (  !gtk_check_button_get_active(GTK_CHECK_BUTTON(retain_text)) &&
                  !gtk_check_button_get_active(GTK_CHECK_BUTTON(never_retain_text)) )
                my_gtk_check_button_set_active(never_retain_text, TRUE);
        }
        #endif
    }
}


#if (UI_VERSION > 1)    // New checkbutton for UIv2+
void on_never_retain_text_checkbutton_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
    GtkWidget *retain_text = lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_checkbutton");
    GtkWidget *retain_text_failed_search = lookup_widget(GTK_WIDGET(prefs_dialog), "retain_text_failed_search_checkbutton");
    GtkWidget *never_retain_text = lookup_widget(GTK_WIDGET(prefs_dialog), "never_retain_text_checkbutton");

    if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(never_retain_text)) )             // never: off-to-on transition
    {
        /*** Manage UI ***/
        if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(retain_text)) )               // retain: if on, force off
            my_gtk_check_button_set_active(retain_text, FALSE);

        if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(retain_text_failed_search)) ) // failed: if on, force off
                my_gtk_check_button_set_active(retain_text_failed_search, FALSE);
    }
    else                                                                                // never: on-to-off transition
    {
        // do nothing
    }
}
#endif


// The setting managed by this callback interacts with the "options menu" (temporary overrides)
// on_ignorecase_activate()
#ifndef GTK4_BUILD
void on_ignore_case_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_ignore_case_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.ignoreCase )
    {
        // Toggle the "sticky" setting
        sticky_settings.ignoreCase = !(sticky_settings.ignoreCase);

        #ifndef GTK4_BUILD
        // Align the "options menu".  Warning, this "set-active" call toggles this "settings" value;
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), "ignorecase_checkmenuitem")),
                                       sticky_settings.ignoreCase);

        #else
        GAction *action = g_action_map_lookup_action(G_ACTION_MAP(gscope_app), "ignorecase");
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(sticky_settings.ignoreCase));
        #endif

        // Align application behavior with the new "sticky" setting (override any change caused by "set_active" callback)
        settings.ignoreCase = sticky_settings.ignoreCase;
        APP_CONFIG_set_boolean("ignoreCase", settings.ignoreCase);  // Update the preferences file
    }
}




// The setting managed by this callback interacts with the "options menu" (temporary overrides)
#ifndef GTK4_BUILD
void on_reuse_window_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_reuse_window_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.reuseWin )
    {
        // Toggle the "sticky" setting
        sticky_settings.reuseWin = !(sticky_settings.reuseWin);

        #ifndef GTK4_BUILD
        // Align the "options menu".  Warning, this "set-active" call toggles this "settings" value;
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), "reusewin_checkmenuitem")),
                                       sticky_settings.reuseWin);
        #else
        GAction *action = g_action_map_lookup_action(G_ACTION_MAP(gscope_app), "reusewin");
        g_simple_action_set_state(G_SIMPLE_ACTION(action), g_variant_new_boolean(sticky_settings.reuseWin));
        #endif
        
        // Align application behavior with the new "sticky" setting (override any change caused by "set_active" callback)
        settings.reuseWin = sticky_settings.reuseWin;

        // Update the preferences file
        APP_CONFIG_set_boolean("reuseWin", settings.reuseWin);
    }
}


#ifndef GTK4_BUILD
void on_truncate_symbols_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_truncate_symbols_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.truncateSymbols )
    {
        settings.truncateSymbols = !(settings.truncateSymbols);
        APP_CONFIG_set_boolean("truncateSymbols", settings.truncateSymbols);    // Update the preferences file
    }
}


#ifndef GTK4_BUILD
void on_compress_symbols_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_compress_symbols_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.compressDisable )
    {
        settings.compressDisable = !(settings.compressDisable);
        APP_CONFIG_set_boolean("compressDisable", !settings.compressDisable);    // Update the preferences file
    }
}


// Handle Exit-confirm changes from Initialization and the preferences dialog
#ifndef GTK4_BUILD
void on_confirmation_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_confirmation_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.exitConfirm )
    {
        settings.exitConfirm = !(settings.exitConfirm);
        APP_CONFIG_set_boolean("exitConfirm", settings.exitConfirm);    // Update the preferences file
        
        // Synch the checkbutton in the confirm_checkout dialog.
        my_gtk_check_button_set_active(lookup_widget(GTK_WIDGET(quit_confirm_dialog),
                            "confirm_exit_checkbutton"), settings.exitConfirm);
    }
}



#ifndef GTK4_BUILD
void on_show_includes_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_show_includes_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.showIncludes )
    {
        settings.showIncludes = !(settings.showIncludes);
        APP_CONFIG_set_boolean("showIncludes", settings.showIncludes);  // Update the preferences file
    }
}


#ifndef GTK4_BUILD
void on_recursive_search_mode_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_recursive_search_mode_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.recurseDir )
    {
        settings.recurseDir = !(settings.recurseDir);
        APP_CONFIG_set_boolean("recurseDir", settings.recurseDir);  // Update the preferences file
    }
}


#ifndef GTK4_BUILD
void on_showicons_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_showicons_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.menuIcons )
    {
        settings.menuIcons = !(settings.menuIcons);
        APP_CONFIG_set_boolean("menuIcons", settings.menuIcons);    // Update the preferences file
        
        DISPLAY_always_show_image(settings.menuIcons);  // Change the application behavior - Turn on/off menu icons
    }
}


#ifndef GTK4_BUILD
void on_single_click_checkbutton_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_single_click_checkbutton_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.singleClick )
    {
        settings.singleClick = !(settings.singleClick);
        APP_CONFIG_set_boolean("singleClick", settings.singleClick);    // Update the preferences file
    }
}


#ifndef GTK4_BUILD
void on_autogen_enable_checkbutton1_toggled(GtkToggleButton *checkbutton, gpointer user_data)
#else
void on_autogen_enable_checkbutton1_toggled(GtkCheckButton *checkbutton, gpointer user_data)
#endif
{
    if ( my_gtk_check_button_get_active(GTK_WIDGET(checkbutton)) != settings.autoGenEnable )
    {
        settings.autoGenEnable = !(settings.autoGenEnable);
        APP_CONFIG_set_boolean("autoGenEnable", settings.autoGenEnable);    // Update the preferences file
    }

    // If/when more than one autogen meta-source type is supported, these elements will need a unique "enable" setting (per type)
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_suffix_entry1"),        settings.autoGenEnable);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cmd_entry1"),           settings.autoGenEnable);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_id_entry1"),            settings.autoGenEnable);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_search_root_entry1"),   settings.autoGenEnable);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_suffix_label"),         settings.autoGenEnable);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cmd_label"),            settings.autoGenEnable);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_id_label"),            settings.autoGenEnable);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_root_label"),           settings.autoGenEnable);

    // These items will be "composite enabled across multiple enables" if/when more than one autogen meta-source type is supported.
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cache_path_entry"),     settings.autoGenEnable);
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_cache_location_label"), settings.autoGenEnable);
    #ifndef GTK4_BUILD
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_main),        "imagemenuitem20"),              settings.autoGenEnable);
    #endif

    {
        gchar       active_path[PATHLEN * 2];
        gchar       active_file[PATHLEN * 2];

        if (settings.autoGenEnable)
        {
            snprintf(active_path, PATHLEN * 2, "My cache path: <span color=\"steelblue\">%s/%s/auto*_*</span>",
                     settings.autoGenPath,
                     getenv("USER"));
            snprintf(active_file, PATHLEN * 2, "Generated filename format:  <span weight=\"bold\">foo</span>"
                     "<span weight=\"bold\" color=\"darkred\">%s</span> ---> "
                     "<b>foo</b><span weight=\"bold\" color=\"darkblue\">%s.c</span>  AND "
                     "<b>foo</b><span weight=\"bold\" color=\"darkblue\">%s.h</span>",
                     settings.autoGenSuffix,
                     settings.autoGenId,
                     settings.autoGenId
                    );
        }
        else
        {
            snprintf(active_path, PATHLEN * 2, "My cache path:");
            snprintf(active_file, PATHLEN * 2, "Generated file name format:");
        }

        gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_active_cache_path_label")), active_path);
        gtk_label_set_markup(GTK_LABEL(lookup_widget(GTK_WIDGET(gscope_preferences), "autogen_generated_filename_label")), active_file);
    }
}





void on_save_results_file_chooser_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
    gchar *filename;
    gboolean save_success = FALSE;
    gboolean bad_filename = FALSE;

    #ifndef GTK4_BUILD
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    #else
    GFile *file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
    filename = g_file_get_path(file);
    g_free(file);
    #endif

    switch (response_id)
    {
        case GTK_RESPONSE_CANCEL:        /* Cancel button */
        case GTK_RESPONSE_DELETE_EVENT:  /* Close Window */
            save_success = TRUE;         /* Nothing really saved, but we don't want to trigger the "No Results" pop-up */
            break;

        case 1:       /* Save as HTML button */
            if (filename)
                save_success = SEARCH_save_html(filename);
            else
                bad_filename = TRUE;
            break;


        case 2:    /* Save as Text button */
            if (filename)
                save_success = SEARCH_save_text(filename);
            else
                bad_filename = TRUE;
            break;

        case 3:   /* Save as CSV button */
            if (filename)
                save_success = SEARCH_save_csv(filename);
            else
                bad_filename = TRUE;
            break;

        default:
        {
            char error_string[150];

            sprintf(error_string, "\nG-Scope Warning: Unexpected response: [%d] from: \"Save Results As\" dialog.", response_id);
            DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_WARNING, error_string, TRUE);
            save_success = TRUE;      /* Nothing really saved, but we don't want to trigger the "No Results" pop-up */
        }

            break;
    }

    if (bad_filename)
    {
        DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_ERROR, "Error:  You must specify a folder and file name.", TRUE);
    }
    else
    {
        if (!save_success)
            DISPLAY_message_dialog(GTK_WINDOW(gscope_main), GTK_MESSAGE_ERROR, "Error:  No search results available to save.", TRUE);
    }



    if (filename)
        g_free(filename);

    return;
}


#ifndef GTK4_BUILD
gboolean on_save_results_file_chooser_dialog_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    return TRUE;     // Cancel the delete_event (do not destroy the widget)
}
#endif



void on_session_info_button_clicked(GtkButton *button, gpointer user_data)
{
    #ifndef GTK4_BUILD 
    on_session_statistics_activate(NULL, NULL);
    #else
    on_session_statistics_activate(NULL, NULL, NULL);
    #endif
}


void on_prefs_help_search_button_clicked(GtkButton       *button,
                                         gpointer         user_data)
{
    #ifndef GTK4_BUILD
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#Search_and_View_Tab", GDK_CURRENT_TIME, NULL);
    #else
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#Search_and_View_Tab", GDK_CURRENT_TIME);
    #endif
}


void on_prefs_help_cross_button_clicked(GtkButton       *button,
                                        gpointer         user_data)
{
    #ifndef GTK4_BUILD
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#Cross_Reference_Tab", GDK_CURRENT_TIME, NULL);
    #else
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#Cross_Reference_Tab", GDK_CURRENT_TIME);

    #endif
}


void on_prefs_help_source_button_clicked(GtkButton       *button,
                                         gpointer         user_data)
{
    #ifndef GTK4_BUILD
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#Source_File_Search_Tab", GDK_CURRENT_TIME, NULL);
    #else
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#Source_File_Search_Tab", GDK_CURRENT_TIME);
    #endif
}


void on_prefs_help_file_button_clicked(GtkButton       *button,
                                       gpointer         user_data)
{
    #ifndef GTK4_BUILD
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#File_Names_Tab", GDK_CURRENT_TIME, NULL);
    #else
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#File_Names_Tab", GDK_CURRENT_TIME);
    #endif
}


void on_prefs_help_general_button_clicked(GtkButton       *button,
                                          gpointer         user_data)
{
    #ifndef GTK4_BUILD
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#General_Tab", GDK_CURRENT_TIME, NULL);
    #else
    gtk_show_uri(NULL, "https://github.com/tefletch/gscope/wiki/Configuring-Gscope#General_Tab", GDK_CURRENT_TIME);
    #endif
}



void on_clear_query_button_clicked(GtkButton       *button,
                                   gpointer         user_data)
{
    GtkWidget *query_entry;

    query_entry   = lookup_widget(GTK_WIDGET(gscope_main), "query_entry");

    /* Clear the entry text */
    my_gtk_entry_set_text(GTK_ENTRY(query_entry), "");
}


void on_autogen_cache_path_entry_changed(GtkEditable *editable, gpointer user_data)
{
    autogen_cache_path_entry_changed = TRUE;
}


void on_autogen_search_root_entry1_changed(GtkEditable *editable, gpointer user_data)
{
    autogen_search_root_entry1_changed = TRUE;
}


void on_autogen_suffix_entry1_changed(GtkEditable *editable, gpointer user_data)
{
    autogen_suffix_entry_1_changed = TRUE;
}


void on_autogen_cmd_entry1_changed(GtkEditable *editable, gpointer user_data)
{
    autogen_cmd_entry1_changed = TRUE;
}


void on_autogen_id_entry1_changed(GtkEditable *editable, gpointer user_data)
{
    autogen_id_entry1_changed = TRUE;
}


void on_autogen_cache_threshold_spinbutton_changed(GtkEditable *editable, gpointer user_data)
{
    cache_threshold_changed = TRUE;
}


void on_terminal_app_entry_changed(GtkEditable *editable, gpointer user_data)
{
    terminal_app_entry_changed = TRUE;
}


void on_file_manager_app_entry_changed(GtkEditable *editable, gpointer user_data)
{
    file_manager_app_entry_changed = TRUE;
}


