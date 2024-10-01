//===========================================================
//
//  Routines to display things in the main Tree View Widget
//
//===========================================================

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>     /* stat */
#include <glib.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "support.h"

#include "search.h"
#include "display.h"
#include "dir.h"
#include "utils.h"
#include "app_config.h"

// ==== defines ====
#define MAX_FUNCTION_SIZE       100     /* Max size of function name */
#define MAX_DISPLAY_SOURCE      511     /* Max number of character to display for source text */
#define MAX_CRAZY_BIG_SIZE      15000   /* Max nubmer of source text chars before warnings are generated */
#define MAX_EXCERPT_SIZE        100

// ==== typedefs ====

enum
{
    FILENAME = 0,
    FUNCTION,
    LINE,
    TEXT,
    COLUMNS
};

enum
{
    HISTORY = 0,
    H_COLUMNS
};

typedef struct
{
    char command[PATHLEN + 1];
} HistoryItem;

// ==== globals ====

GtkWidget *treeview;
GtkListStore *store;
GtkTreeIter iter;

GtkWidget *h_treeview;
GtkListStore *h_store;

// Display columns
GtkTreeViewColumn *display_col[4];
GtkTreeViewColumn *h_col;

gboolean line_number_info_avail = FALSE;   // Indicates if the most recent query
                                           // produced line number information.

//  ==== Private Global Variables ====
static GtkWidget   *gscope_main = NULL;

/*** Local Function Prototypes ***/

static void on_filename_col_clicked(GtkTreeViewColumn *column, gpointer user_data);
static void on_function_col_clicked(GtkTreeViewColumn *column, gpointer user_data);
static void on_line_col_clicked(GtkTreeViewColumn *column, gpointer user_data);
static void on_text_col_clicked(GtkTreeViewColumn *column, gpointer user_data);
static void configure_columns(gchar new_mask);
static void update_list_store(search_results_t *results);
static gboolean search_equal_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer search_data);


void DISPLAY_init(GtkWidget *main)
{

    GtkCellRenderer *renderer;
    GtkWidget *image1;
    GtkWidget *sms_button;

    gscope_main = main;     // Save a convenience pointer to the main window

    #ifndef GTK4_BUILD  // gtk4: Bypass treeview initialization for now (does this logic belong here, or callbacks?)
    // ======== Set up the search RESULTS treeview ========
    // Add four columms to the GtkTreeView.
    // All four columns will be displayed as text.

    treeview   = lookup_widget(GTK_WIDGET (gscope_main), "treeview1");

    /* Create a new GtkCellRendererText, add it to the tree view column and
     * append the column to the tree view. */

    renderer = gtk_cell_renderer_text_new();
    display_col[FILENAME] = gtk_tree_view_column_new_with_attributes("File", renderer, "text", FILENAME, NULL);
    gtk_tree_view_column_set_sizing   (display_col[FILENAME], GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), display_col[FILENAME]);
    gtk_tree_view_column_set_clickable(display_col[FILENAME], TRUE);
    g_signal_connect((GtkTreeViewColumn *)display_col[FILENAME], "clicked", G_CALLBACK (on_filename_col_clicked), NULL);

    renderer = gtk_cell_renderer_text_new();
    display_col[FUNCTION] = gtk_tree_view_column_new_with_attributes("Function", renderer, "text", FUNCTION, NULL);
    gtk_tree_view_column_set_sizing   (display_col[FUNCTION], GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), display_col[FUNCTION]);
    gtk_tree_view_column_set_clickable(display_col[FUNCTION], TRUE);
    g_signal_connect((GtkTreeViewColumn *)display_col[FUNCTION], "clicked", G_CALLBACK (on_function_col_clicked), NULL);

    renderer = gtk_cell_renderer_text_new();
    display_col[LINE] = gtk_tree_view_column_new_with_attributes("Line", renderer, "text", LINE, NULL);
    gtk_tree_view_column_set_sizing   (display_col[LINE],     GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), display_col[LINE]);
    gtk_tree_view_column_set_clickable(display_col[LINE], TRUE);
    g_signal_connect((GtkTreeViewColumn *)display_col[LINE], "clicked", G_CALLBACK (on_line_col_clicked), NULL);

    renderer = gtk_cell_renderer_text_new();
    display_col[TEXT] = gtk_tree_view_column_new_with_attributes("Source Text", renderer, "text", TEXT, NULL);
    gtk_tree_view_column_set_sizing   (display_col[TEXT],     GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), display_col[TEXT]);
    gtk_tree_view_column_set_clickable(display_col[TEXT], TRUE);
    g_signal_connect((GtkTreeViewColumn *)display_col[TEXT], "clicked", G_CALLBACK (on_text_col_clicked), NULL);

    /* Create a new tree model with 4 columns [string, string, string, string] */
    store = gtk_list_store_new(COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    /* Add the tree model to the tree view and unreference it so that the model will be
     * destroyed along with the tree view. */
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));
    g_object_unref(store);

    /* Configure a custom function for interactive searches */
    gtk_tree_view_set_search_equal_func (GTK_TREE_VIEW(treeview), search_equal_func, NULL, NULL);


    // ======== Set up the search HISTORY treeview ========
    // Add one columm to the GtkTreeView.
    // The column will be displayed as text.
    h_treeview = lookup_widget(GTK_WIDGET (gscope_main), "history_treeview");

    renderer = gtk_cell_renderer_text_new();
    h_col = gtk_tree_view_column_new_with_attributes("History", renderer, "text", HISTORY, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(h_treeview), h_col);

    /* Create a new tree model with 1 column [string] */
    h_store = gtk_list_store_new(H_COLUMNS, G_TYPE_STRING);

    /* Add the tree model to the tree view and unreference it so that the model will be
     * destroyed along with the tree view. */
    gtk_tree_view_set_model(GTK_TREE_VIEW(h_treeview), GTK_TREE_MODEL(h_store));
    g_object_unref(h_store);
    #else
    printf("GTK4: Bypassing treeview setup for now\n");
    #endif

    // ======== Initialize the static status info ========

    sms_button = lookup_widget(GTK_WIDGET(gscope_main), "src_mode_status_button");

    if (settings.recurseDir)
    {
        image1 = create_pixmap (gscope_main, "recursive.png");
        gtk_widget_show (image1);
        #ifndef GTK4_BUILD
        gtk_container_add (GTK_CONTAINER (sms_button), image1);
        #else
        gtk_button_set_child (GTK_BUTTON (sms_button), image1);
        #endif

        gtk_widget_set_tooltip_text (sms_button, "Source Search Mode: [Recursive]");
    }
    else
    {
        image1 = create_pixmap (gscope_main, "non-recursive.png");
        gtk_widget_show (image1);
        #ifndef GTK4_BUILD
        gtk_container_add (GTK_CONTAINER (sms_button), image1);
        #else
        gtk_button_set_child (GTK_BUTTON (sms_button), image1);
        #endif

        gtk_widget_set_tooltip_text (sms_button, "Source Search Mode: [Non-Recursive]");
    }

    // ===== Initialize optional sensitivity Menu Items =====
    #ifndef GTK4_BUILD
    gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(gscope_main), "imagemenuitem20"), settings.autoGenEnable);

    #ifndef GTK3_BUILD
    // ======== Initialize ImageMenuItem Icon Display Behavior ========
    DISPLAY_always_show_image(settings.menuIcons);
    #endif

    #else   // GTK4_BUILD
    printf("GTK4: bypassing menu configuration (for now)\n");
    #endif
}


void DISPLAY_update_stats_tooltip(gchar *msg)
{
    GtkWidget   *info_button;

    info_button = lookup_widget(GTK_WIDGET(gscope_main), "session_info_button");
    //info_button = g_object_get_data(G_OBJECT(gscope_main), "session_info_button");

    gtk_widget_set_tooltip_text (info_button, msg);
}


void DISPLAY_update_path_label(gchar *path)
{
    #define MAX_DISPLAY_PATH    70
    #define PANGO_OVERHEAD      70

    GtkWidget *path_label;
    gchar *cwd_buf;
    gchar *working;

    gchar *offset_ptr;
    unsigned int length;

    // if the path is longer than MAX_DISPLAY_PATH characters, truncate it
    length = strlen(path);
    if ( length > MAX_DISPLAY_PATH)
    {
        // Trim off excess chars from the beginning of the string [and leave 3 bytes for the ellipsis]
        offset_ptr = (path + (length - (MAX_DISPLAY_PATH - 3)));
        // move forward to the first '/' character AFTER the offset
        offset_ptr = strchr(offset_ptr, '/');
        if (offset_ptr == NULL)
            my_asprintf(&working, "<NULL>");
        else
            my_asprintf(&working, "...%s", offset_ptr);
    }
    else
        my_asprintf(&working, "%s", path);

    // add pango markup to path string.  Caution: pango text length must not exceed PANGO_OVERHEAD
    my_asprintf(&cwd_buf, "Source Directory: <span foreground=\"seagreen\">%s</span>", working);
    // update the label
    path_label = lookup_widget(GTK_WIDGET (gscope_main), "path_label");
    gtk_label_set_markup(GTK_LABEL(path_label), cwd_buf);
    g_free(cwd_buf);

    // Place the path information in the main_window title bar
    my_asprintf(&cwd_buf, "G-Scope: %s", working);
    gtk_window_set_title(GTK_WINDOW(gscope_main), cwd_buf);
    g_free(cwd_buf);

    g_free(working);
    #undef PANGO_OVERHEAD
    #undef MAX_DISPLAY_PATH
}


/* Place a message in the query status label widget */
void DISPLAY_status(gchar *msg)
{
    static GtkWidget *status_label = NULL;

    if (status_label == NULL)
        status_label = lookup_widget(GTK_WIDGET (gscope_main),"status_label");

    gtk_label_set_markup ( GTK_LABEL(status_label), msg );
}



/* Display the results of the query */
void DISPLAY_search_results(search_t button, search_results_t *results)
{
    #define FILE_FN_LN_TXT_COL_MASK  0x0f
    #define FILE_LN_TXT_COL_MASK     0x0d
    #define FILE_COL_MASK            0x01

    if (results->match_count < 1) return;   /* This should never happen, but just in case... */

    gtk_list_store_clear(store);

    #if 0 // failed attempt to have combination manual resize and auto resize columns
    gtk_tree_view_column_set_resizable(display_col[FILENAME], FALSE);
    gtk_tree_view_column_set_sizing   (display_col[FILENAME], GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    //gtk_tree_view_column_set_resizable(display_col[FUNCTION], FALSE);
    gtk_tree_view_column_set_sizing   (display_col[FUNCTION], GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    //gtk_tree_view_column_set_resizable(display_col[LINE],     FALSE);
    gtk_tree_view_column_set_sizing   (display_col[LINE],     GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    //gtk_tree_view_column_set_resizable(display_col[TEXT],     FALSE);
    gtk_tree_view_column_set_sizing   (display_col[TEXT],     GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    #endif

    switch (button)
    {
        case FIND_SYMBOL:
        case FIND_CALLEDBY:
        case FIND_CALLING:
            configure_columns(FILE_FN_LN_TXT_COL_MASK);
            update_list_store(results);
            line_number_info_avail = TRUE;
        break;

        case FIND_DEF:
        case FIND_STRING:
        case FIND_REGEXP:
        case FIND_INCLUDING:
        case FIND_ALL_FUNCTIONS:
        case FIND_AUTOGEN_ERRORS:
            configure_columns(FILE_LN_TXT_COL_MASK);
            update_list_store(results);
            line_number_info_avail = TRUE;
        break;

        case FIND_FILE:
            configure_columns(FILE_COL_MASK);
            update_list_store(results);
            line_number_info_avail = FALSE;
        break;
    }

    #if 0  // This sort of works, but the column stays "fixed-width" after user manually resizes (not good)
    gtk_tree_view_columns_autosize((GtkTreeView *)treeview);
    gtk_tree_view_column_queue_resize(display_col[FILENAME]);
    while ( gtk_events_pending() )
    {
        gtk_main_iteration();
    }

    printf("width = %d\n", gtk_tree_view_column_get_width(display_col[FILENAME]) );
    gtk_tree_view_column_set_resizable(display_col[FILENAME], TRUE);
    gtk_tree_view_column_set_resizable(display_col[FUNCTION], TRUE);
    gtk_tree_view_column_set_resizable(display_col[LINE],     TRUE);
    gtk_tree_view_column_set_resizable(display_col[TEXT],     TRUE);
    #endif
}



/* Add the input string to the input history list. */
void DISPLAY_history_update(const gchar *newEntry)
{
    gchar *entry;
    GtkTreePath *path;

    /*
     * If newEntry already exists somewhere in the list, remove the old
     * instance before adding it again at the bottom of the list.
     */
    if ( gtk_tree_model_get_iter_first(GTK_TREE_MODEL(h_store), &iter) )
    {
        do
        {
            gtk_tree_model_get(GTK_TREE_MODEL(h_store), &iter, HISTORY, &entry, -1);

            if ( strcmp(newEntry, entry) == 0 )
            {
                gtk_list_store_remove( h_store, &iter );
                g_free(entry);
                break;
            }

            g_free(entry);

        }  while ( gtk_tree_model_iter_next(GTK_TREE_MODEL(h_store), &iter) );
    }

    /* Add the new entry to the bottom of the list */
    gtk_list_store_append(h_store, &iter);
    gtk_list_store_set(h_store, &iter, HISTORY, newEntry, -1);

    /* Make the new entry visible in the history scroll window */
    gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW(h_treeview),
                                  path = gtk_tree_model_get_path(GTK_TREE_MODEL(h_store), &iter),
                                  NULL,
                                  TRUE,     // Use alignment
                                  1,        // Row Align - Bottom
                                  0);       // Col Align - Left

    gtk_tree_path_free (path);
}



void DISPLAY_history_save()
{
    gchar *entry;
    GtkTreeIter iter;
    FILE *hist_file;

    if ( gtk_tree_model_get_iter_first(GTK_TREE_MODEL(h_store), &iter) )
    {
        /* (Re)create the history file */
        hist_file = fopen(settings.histFile,"w");

        if (hist_file != NULL)
        {
            do
            {
                gtk_tree_model_get(GTK_TREE_MODEL(h_store), &iter, HISTORY, &entry, -1);

                fprintf(hist_file, "%s\n", entry);

                g_free(entry);

            }  while ( gtk_tree_model_iter_next(GTK_TREE_MODEL(h_store), &iter) );

            fclose(hist_file);
        }
    }
    else
        fprintf(stderr, "Error: Failed to open 'History' file.\n");
}



/* Append the 'saved' history snapshot to the query history pane */
void DISPLAY_history_load()
{
    struct      stat statstruct;          /* file status */
    FILE        *hist_file;
    char        *hist_file_buf = NULL;
    char        *new_entry;
    char        *work_ptr;
    char        *end_of_file;
    GtkTreeIter iter;
    GtkTreePath *path;

    if ( (stat(settings.histFile, &statstruct) == 0 ) && ((hist_file = fopen(settings.histFile, "r")) != NULL) )
    {
        // Clear the "current" history pane -- avoids duplication -- avoids list merging
        DISPLAY_history_clear();

        hist_file_buf = g_malloc(statstruct.st_size);    /* malloc a buffer to hold the entire history file */

        if ( hist_file_buf )
        {
            /* Read in the entire file */
            if ( fread(hist_file_buf, 1, statstruct.st_size, hist_file) == statstruct.st_size )
            {
                // Walk the newline-separated list and create a new entry in the history list for
                // each line contained in the file

                end_of_file = hist_file_buf + statstruct.st_size;  // Set the EOF marker
                new_entry = hist_file_buf;
                work_ptr = new_entry;

                while (work_ptr < end_of_file)
                {
                    // Find the next newline
                    while (*work_ptr != '\n')
                        work_ptr++;

                    *work_ptr++ = '\0';

                    // Add the new entry to the bottom of the list
                    gtk_list_store_append(h_store, &iter);
                    gtk_list_store_set(h_store, &iter, HISTORY, new_entry, -1);


                    // Set the pointer to the beginning of the "next" string.
                    new_entry = work_ptr;
                }

                /* Make the new entry visible in the history scroll window */
                gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW(h_treeview),
                                              path = gtk_tree_model_get_path(GTK_TREE_MODEL(h_store), &iter),
                                              NULL,
                                              TRUE,     // Use alignment
                                              1,        // Row Align - Bottom
                                              0);       // Col Align - Left

                gtk_tree_path_free (path);
            }
            else
            {
                fprintf(stderr, "Error: Unable to to read history file.  Load aborted.\n");
            }

            g_free(hist_file_buf);
        }
        else
        {
            fprintf(stderr, "Error: Unable to allocate memory for history file.  Load aborted.\n");
        }

        fclose(hist_file);
    }
    // else: Quietly move on.  No need to post a message.
}



void DISPLAY_history_clear()
{
    GtkTreeIter iter;

    if ( gtk_tree_model_get_iter_first(GTK_TREE_MODEL(h_store), &iter) )
    {
       while ( gtk_list_store_remove( h_store, &iter ) );
    }
}



gboolean DISPLAY_get_clicked_entry(gint widget_x, gint widget_y, gchar **entry_p)
{
    GtkTreePath *path;
    //gint bin_x, bin_y;  // bin_window (x,y) coordinates

    // In the future we may want to use gtk_tree_view_convert_widget_to_bin_window_coords ().
    // However this call is not available until Gtk2 version 2.12.  Breaking this application
    // on old distros that don't support GTK 2.12 (like RHEL5) for this one call seems a bit harsh.
    //
    // gtk_tree_view_convert_widget_to_bin_window_coords( GTK_TREE_VIEW(h_treeview), widget_x, widget_y, &bin_x, &bin_y);

    if ( gtk_tree_view_get_path_at_pos( GTK_TREE_VIEW(h_treeview), widget_x, widget_y, &path, NULL, NULL, NULL ) )
    {
       // if a row exists at the clicked coordinate - we now have a valid path - get the corresponding iterator
       if ( gtk_tree_model_get_iter( GTK_TREE_MODEL(h_store), &iter, path) )
       {
           gtk_tree_model_get(GTK_TREE_MODEL(h_store), &iter, HISTORY, entry_p, -1);
           gtk_tree_path_free(path);
           return(TRUE);
       }
    }
    return(FALSE);
}



gboolean DISPLAY_get_filename_and_lineinfo(GtkTreePath *path, gchar **filename, gchar **line_num)
{
    static gchar zero_line[] = "0";

    if ( gtk_tree_model_get_iter( GTK_TREE_MODEL(store), &iter, path ) )
    {
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, FILENAME, filename, -1);
        if (line_number_info_avail)
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, LINE, line_num, -1);
        else
          *line_num = zero_line;

        return(TRUE);
    }
    else
        return(FALSE);
}

gboolean DISPLAY_get_entry_info(GtkTreePath *path, gchar **filename, gchar **line_num, gchar **symbol)
{
    static gchar zero_line[] = "0";

    if ( gtk_tree_model_get_iter( GTK_TREE_MODEL(store), &iter, path ) )
    {
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, FUNCTION, symbol, -1);
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, FILENAME, filename, -1);
        if (line_number_info_avail)
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, LINE, line_num, -1);
        else
          *line_num = zero_line;

        return(TRUE);
    }
    else
        return(FALSE);

}


// Update the specified progress bar using optional progress message (Pass NULL for no message)
void DISPLAY_progress(GtkWidget *bar, char *progress_msg, guint count, guint max)
{
    gdouble fraction;

    fraction = (gdouble)count/(gdouble)max;

    if ( progress_msg )
    {
        gchar *message;
        message = g_strdup_printf("%s %.0f%% Complete", progress_msg, fraction * 100.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(bar), message);
        g_free(message);
    }

    //printf("fraction = %.8f\n", fraction);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), fraction);
    

    // Process pending GTK events
    while ( g_main_context_pending( NULL/*default context*/  ) )
        g_main_context_iteration( NULL/*default context*/, TRUE/*may block*/);
}



#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)


void DISPLAY_always_show_image(gboolean always_show)
{
    #ifndef GTK4_BUILD
    char item[40];
    int i;

    if ( gtk_major_version > 2 || ((gtk_major_version == 2) && (gtk_minor_version > 15)) )
    {
        for (i = 1; i <= 18; i++ )
        {
            sprintf(item, "imagemenuitem%d", i);
            /* The call below is only available in GTK2 Version 2.16 (or later) */
            gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(lookup_widget(GTK_WIDGET(gscope_main), item)), always_show);
        }
    }
    #endif
}



void DISPLAY_set_cref_current(gboolean up_to_date)
{
    static gboolean initialized = FALSE;
    static GtkWidget *cref_update_button;
    static GtkWidget *image_blue, *image_red, *image_visible;

    static const char blue_string[] = "Cross Reference Status: [Up-to-Date]\nClick to check status.";
    static const char red_string[]  = "Cross Reference Status: [Out-of-Date]\nClick to rebuild.";
    GtkWidget  *update_image;
    const char *update_string;

    if ( !initialized )
    {
        cref_update_button = lookup_widget(GTK_WIDGET(gscope_main),"cref_update_button");
        image_blue         = create_pixmap (gscope_main, "refresh_blue.png");
        image_red          = create_pixmap (gscope_main, "refresh_red.png");
        image_visible      = image_blue;
        update_string      = blue_string;

        /* Increment the reference count for these objects so that they are not destroyed when removed from the button container */
        g_object_ref(image_blue);
        g_object_ref(image_red);

        #ifndef GTK4_BUILD
        gtk_container_add (GTK_CONTAINER (cref_update_button), image_visible);
        #else
        gtk_button_set_child(GTK_BUTTON(cref_update_button), image_visible);
        #endif
        gtk_widget_set_tooltip_text(cref_update_button, update_string);
        gtk_widget_show(image_visible);

        initialized = TRUE;
    }


    if (up_to_date)
    {
        update_image = image_blue;
        update_string = blue_string;
        SEARCH_set_cref_status(TRUE);
    }
    else
    {
        update_image = image_red;
        update_string = red_string;
        SEARCH_set_cref_status(TRUE);
    }

    if ( update_image != image_visible)
    {
        #ifndef GTK4_BUILD
        gtk_container_remove(GTK_CONTAINER(cref_update_button), image_visible);
        image_visible = update_image;
        gtk_container_add(GTK_CONTAINER (cref_update_button), image_visible);
        #else
        gtk_widget_unparent(image_visible);
        image_visible = update_image;
        gtk_button_set_child(GTK_BUTTON(cref_update_button), image_visible);
        #endif

        gtk_widget_set_tooltip_text(cref_update_button, update_string);
        gtk_widget_show(image_visible);
    }
}


//---------------------------------------------------------------------------
//
// Display a message dialog of the specified type (severity) over the 
// specified parent window.
// Parameters:
//     parent:  The parent window (or NULL for the default)
//
//   severity:  Mesage type, one of [GTK_MESSAGE_INFO | GTK_MESSAGE_WARNING |
//                               GTK_MESSAGE_QUESTION | GTK_MESSAGE_ERROR |
//                               GTK_MESSAGE_OTHER ]
//
//   message:  The text to be displayed in the message dialog.  Note,
//             The string parameter "message" can be a Pango text
//             markup string.
//
//     modal:  Set TRUE if the message dialog window is modal
//
//---------------------------------------------------------------------------
void DISPLAY_message_dialog(GtkWindow *parent, GtkMessageType severity, const gchar *message, gboolean modal)
{
    GtkWidget *MsgDialog;
    const char *title[5] = {"Info", "Warning", "Question", "Error", "Misc"}; // Message type names indexed by GtkMessageType: severity

    if ( !parent )  // Use the 'default' parent
        parent = GTK_WINDOW(lookup_widget(gscope_main, "main"));

    MsgDialog = gtk_message_dialog_new_with_markup(parent,
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   severity,
                                                   GTK_BUTTONS_OK,
                                                   NULL);


 // Pango markup example:
 //    _("<span foreground=\"blue\" size=\"x-large\" weight=\"bold\">Build Monitor 0.1</span>\nA panel application for monitoring print build status\n2007 Tom Fletcher")

    #ifndef GTK4_BUILD
    gtk_window_set_title(GTK_WINDOW(MsgDialog), title[severity]);

    if (modal)
    {
        gtk_dialog_run(GTK_DIALOG (MsgDialog));
        gtk_widget_destroy(GTK_WIDGET (MsgDialog));
    }
    else  // display a non-modal message dialog
    {
        gtk_widget_show(MsgDialog);

        // Destroy the dialog when the user responds to it (e.g. clicks a button)
        g_signal_connect_swapped (MsgDialog, "response",
                               G_CALLBACK (gtk_widget_destroy),
                               MsgDialog);
    }
    #else   // GTK4 build

    GtkWindow   *message_window;
    GtkLabel    *message_label;
    GtkImage    *message_image;
    const char *icon_name[5] = {"dialog-information", "dialog-warning", "dialog-question", "dialog-error", "emblem-important"};


    message_window = GTK_WINDOW(my_lookup_widget("message_window"));
    message_label  = GTK_LABEL(my_lookup_widget("message_label"));
    message_image  = GTK_IMAGE(my_lookup_widget("message_image"));

    gtk_window_set_modal(message_window, modal);
    gtk_window_set_transient_for(message_window, parent);
    gtk_window_set_title(message_window, title[severity]);
    gtk_image_set_from_icon_name(message_image, icon_name[severity]);
    gtk_label_set_markup(message_label, message);
    gtk_widget_show(GTK_WIDGET(message_window));
    #endif
}





// ======================= Display Module Private Functions ===============================


static void on_filename_col_clicked(GtkTreeViewColumn *column, gpointer user_data)
{
    gtk_tree_view_set_search_column( GTK_TREE_VIEW(treeview), FILENAME);
}



static void on_function_col_clicked(GtkTreeViewColumn *column, gpointer user_data)
{
    gtk_tree_view_set_search_column( GTK_TREE_VIEW(treeview), FUNCTION);
}



static void on_line_col_clicked(GtkTreeViewColumn *column, gpointer user_data)
{
    gtk_tree_view_set_search_column( GTK_TREE_VIEW(treeview), LINE);
}



static void on_text_col_clicked(GtkTreeViewColumn *column, gpointer user_data)
{
    gtk_tree_view_set_search_column( GTK_TREE_VIEW(treeview), TEXT);
}



/*------------------------------------------------------------------
 Add or Remove displayed columns, as appropriate for the query type,
 as specified by the "new" column mask:

          ---------------------------------
Mask:     | 7 | 6 | 5 | 4 | 3 | 2 | 2 | 0 |
          ---------------------------------
                            ^   ^   ^   ^
               Text_________|   |   |   |
               Line_____________|   |   |
               Function_____________|   |
               File_____________________|

 Each bit in the mask represents a column.  If the bit is set,
 the corresponding column is displayed in the query results.
 If the bit is zero, the corresponding column is not displayed.
--------------------------------------------------------------------*/
static void configure_columns(gchar new_mask)
{
    static gchar column_mask = 0x0f;
    int i;

    if (new_mask != column_mask)
    {
        // Add back only the columns specified by the mask
        for (i = 0; i < COLUMNS; i++)
        {
            if ( (new_mask & 0x01) )
                gtk_tree_view_column_set_visible( display_col[i], TRUE );
            else
                gtk_tree_view_column_set_visible( display_col[i], FALSE );

            new_mask = new_mask >> 1;
        }

        column_mask = new_mask;
    }
    return;
}



static void update_list_store(search_results_t *results)
{
    gchar  file       [PATHLEN + 1],
           function   [MAX_FUNCTION_SIZE + 1],
           linenum    [MAX_LINENUM_SIZE + 1],
           source_text[MAX_DISPLAY_SOURCE + 1];

    gchar    field_terminator;
    uint32_t count;
    uint32_t over_count;
    gchar    *work_ptr = results->start_ptr;
    gchar    *start_ptr;
    uint32_t lines = results->match_count;
    int      width;


    while (lines-- > 0)
    {
        // Each line of the search results has the following fields [although some fields may have dummy values]:
        //   File Name
        //   Function Name
        //   Line Number
        //   Source Line Text
        //
        // The line format uses '|' and space ' ' characters for field delimiters as follows:
        //   <file name>|<function name> <line number> <source text>\n
        //
        //   Note: <source text can contain any number of space ' ' characters.


        /*** extract File Name ***/
        count = 0;
        start_ptr = work_ptr;
        field_terminator = '|';
        while ( (*work_ptr != field_terminator) && (count < sizeof(file)) )
        {
            file[count++] = *work_ptr;
            work_ptr++;
        }

        if ( *work_ptr != field_terminator)  // We have an overflow condition
        {
            over_count = count;
            count--;                // Bring the index back to the last char in the array.
            file[count - 1] = '.';  // Ellipsize the overflow string
            file[count - 2] = '.';
            file[count - 3] = '.';

            while (*work_ptr++ != field_terminator) // Skip over the overflow data
                over_count++;
            work_ptr--;     // Move pointer back to field terminator

            fprintf(stderr, "Warning: Field width overflow: [File Path Name] will be truncated.\n");
            fprintf(stderr, "    Max File Path Name length allowed: %d, Actual Path Name length: %d\n", PATHLEN, over_count);
            width = PATHLEN / 4;
            if (width > MAX_EXCERPT_SIZE) width = MAX_EXCERPT_SIZE;
            fprintf(stderr, "    Excerpt [Head]: %.*s\n", width, start_ptr);
            fprintf(stderr, "    Excerpt [Tail]: %.*s\n\n", width, start_ptr + over_count - width);
        }
        file[count] = '\0';         // Null terminate the string
        work_ptr++;                 // skip the field terminator


        /*** extract Function Name ***/
        count = 0;
        start_ptr = work_ptr;
        field_terminator = ' ';
        while ( (*work_ptr != field_terminator) && (count < sizeof(function)) )
        {
            function[count++] = *work_ptr;
            work_ptr++;
        }

        if ( *work_ptr != field_terminator)  // We have an overflow condition
        {
            over_count = count;
            count--;                    // Bring the index back to the last char in the array.
            function[count - 1] = '.';  // Ellipsize the overflow string
            function[count - 2] = '.';
            function[count - 3] = '.';

            while (*work_ptr++ != field_terminator) // Skip over the overflow data
                over_count++;
            work_ptr--;     // Move pointer back to field terminator

            fprintf(stderr, "Warning: Field width overflow: [Function] will be truncated.\n");
            fprintf(stderr, "    Max File Function length allowed: %d, Actual Function length: %d\n", MAX_FUNCTION_SIZE, over_count);
            fprintf(stderr, "    File: %s\n", file);
            width = MAX_FUNCTION_SIZE / 4;

            /* coverity[dead_error_line] this check becomes valid if MAX_FUNCTION_SIZE is changed to a larger value. */
            if (width > MAX_EXCERPT_SIZE) width = MAX_EXCERPT_SIZE;
            fprintf(stderr, "    Excerpt [Head]: %.*s\n", width, start_ptr);
            fprintf(stderr, "    Excerpt [Tail]: %.*s\n\n", width, start_ptr + over_count - width);
        }
        function[count] = '\0';     // Null terminate the string
        work_ptr++;                 // skip the field terminator


        /*** extract Line Number ***/
        count = 0;
        start_ptr = work_ptr;
        field_terminator = ' ';
        while ( (*work_ptr != field_terminator) && (count < sizeof(linenum)) )
        {
            linenum[count++] = *work_ptr;
            work_ptr++;
        }

        if ( *work_ptr != field_terminator)  // We have an overflow condition
        {
            over_count = count;
            count--;                   // Bring the index back to the last char in the array.
            linenum[count - 1] = 'E';  // Ellipsize the overflow string

            while (*work_ptr++ != field_terminator) // Skip over the overflow data
                over_count++;
            work_ptr--;     // Move pointer back to field terminator

            fprintf(stderr, "Warning: Field width overflow: [Line Number] will be truncated.\n");
            fprintf(stderr, "    Max Line Number length allowed: %d, Actual Line Number length: %d\n", MAX_LINENUM_SIZE, over_count);
            fprintf(stderr, "    Full Line Number: %.*s\n\n", over_count, start_ptr);
            fprintf(stderr, "    File: %s\n", file);
        }
        linenum[count] = '\0';     // Null terminate the string
        work_ptr++;                 // skip the field terminator



        /*** extract Source Line Text ***/
        count = 0;
        start_ptr = work_ptr;
        field_terminator = '\n';
        while ( (*work_ptr != field_terminator) && (count < sizeof(source_text)) )
        {
            if ((unsigned char) *work_ptr > 0x7f)
                *work_ptr = '?';    // Eliminate non-ASCII chars from source text.

            source_text[count++] = *work_ptr;
            work_ptr++;
        }

        if ( *work_ptr != field_terminator)  // We have an overflow condition
        {
            over_count = count;
            count--;                    // Bring the index back to the last char in the array.
            source_text[count - 1] = '.';  // Ellipsize the overflow string
            source_text[count - 2] = '.';
            source_text[count - 3] = '.';

            while (*work_ptr++ != field_terminator) // Skip over the overflow data
                over_count++;
            work_ptr--;     // Move pointer back to field terminator

            if ( strcmp(linenum, "1") == 0)
            {
                fprintf(stderr, "*** Warning: File %s is not using Linux newline format\n", file);
            }

            if (over_count > MAX_CRAZY_BIG_SIZE)  // We have so many "big" source lines, we need to filter it down to the "worst".
            {
                fprintf(stderr, "Warning: Field width overflow: [Source Text] will be truncated.\n");
                fprintf(stderr, "    Max Source Text length allowed: %d, Actual Source Text length: %d\n", MAX_DISPLAY_SOURCE, over_count);
                fprintf(stderr, "    File: %s\n    Line: %s\n", file, linenum);
                width = MAX_DISPLAY_SOURCE / 4;
                if (width > MAX_EXCERPT_SIZE) width = MAX_EXCERPT_SIZE;
                fprintf(stderr, "    Excerpt [Head]: %.*s\n", width, start_ptr);
                fprintf(stderr, "    Excerpt [Tail]: %.*s\n\n", width, start_ptr + over_count - width);
            }
        }
        source_text[count] = '\0';     // Null terminate the string
        work_ptr++;                 // skip the field terminator



        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, FILENAME, file, FUNCTION, function,
                           LINE, linenum, TEXT, source_text, -1);
    }
    return;
}



static gboolean search_equal_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer search_data)
{
    gchar *cell_data = NULL;
    gboolean found;

    gtk_tree_model_get(model, iter, column, &cell_data, -1);

    if (!cell_data)
        return !FALSE;  /* Return inverted logic result */

    found = ( strstr(cell_data, key) != NULL );

    return( !found );   /* GtkTreeView search logic is inverted */
}







