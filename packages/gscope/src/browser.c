#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <string.h>

#include "support.h"
#include "utils.h"
#include "app_config.h"
#include "search.h"


// Defines
//=======================

#define NUM_COL_MAX     (40)



// Typedefs
//=======================

typedef enum
{
    LEFT,
    RIGHT,
} dir_e;

// linked list node
// for a list of search results
typedef struct result
{
    gchar *function_name;
    gchar *file_name;
    gchar *line_num;
    gchar *buf;      // buffer to hold result information for parsing
    struct result *next;
    void *tcb;       // a pointer back to tcb so that it can be used to reset the table
} result_t;

typedef enum
{
    FILLER,
    EXPANDER,
    NAME,
    NUM_TYPES,
} column_type_e;

// node in a column member doubly linked list
// an ordered list of widgets
// in a column
typedef struct entry
{
    GtkWidget       *widget;
    guint           row;
    gchar           file[500];  // to differentiate between multi definition fuctions
    struct entry    *next;
    struct entry    *prev;
    gboolean        last_entry;  // if the entry is the last function in a block
} column_entry_t;


typedef struct
{
    GtkWidget       *header_column_label;
    GtkWidget       *slider;
    guint           header_column_num;      // label number not actual column number in table
    column_entry_t  *column_member_list;    // column member linked list
    column_entry_t  *back;
    column_type_e   type;
} col_list_t;


typedef struct
{
    GtkWidget   *browser_table;
    result_t    root_entry;     // used to store root file name and function name for callback
    guint       root_col;       // Table widget column currently containing the root/origin [0] column
    guint       num_cols;       // Total number of columns in the table
    guint       num_rows;       // Total number of rows in the table.
    guint       left_height;    // Current size of "tallest" left column
    guint       right_height;   // Current size of "tallest" right column
    col_list_t  col_list[NUM_COL_MAX];
}   tcb_t;

// Widget Table row/column tracking structure definition
//
//                   -------------------------
//      column list  | 0 |  1 |  2 | ... | n |
//                   -------------------------
//                    |  |  |      |       |
//                    V  V  V      V       V
//
//                    Linked Lists of column
//                    specific [sparse data]
//                    entries


// Private Function Prototypes
//============================
static GtkWidget   *create_browser_window(gchar *name, gchar *root_file, gchar *line_num);
static GtkWidget   *make_collapser(guint child_count);
static GtkWidget   *make_expander (dir_e direction, gboolean new_expander);
static void        expand_table(guint origin_y, guint origin_x, tcb_t *tcb, dir_e direction);
static void        collapse_table(guint origin_row, guint origin_col, tcb_t *tcb, dir_e direction);
static void        move_table_widget(GtkWidget *widget, tcb_t *tcb, guint row, guint col);

static gboolean    on_browser_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static gboolean    on_column_hscale_change_value(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data);
static gboolean    on_root_button_clicked(GtkWidget *widget, gpointer user_data);
static gboolean    on_left_expander_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_right_expander_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_left_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_right_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_function_button_press(GtkWidget *widget, GdkEventButton *event, result_t *user_data);
static void        right_click_menu(GtkWidget *widget, GdkEventButton *event, result_t *function_box); 
static void        on_reroot(GtkWidget *menuitem, result_t *function_box);

static void         initialize_table(tcb_t *tcb, gchar *root_fname, gchar *root_file, gchar *line_num);
static void         ensure_column_capacity(tcb_t *tcb, guint origin_col, dir_e direction); 
static void         ensure_row_capacity(tcb_t *tcb, guint origin_row, guint row_add_count); 
static void         make_name_column(tcb_t *tcb, guint col, dir_e direction); 
static void         make_name_column_labeled(tcb_t *tcb, guint col, dir_e direction, guint label); 
static void         make_expander_column(tcb_t *tcb, guint position, dir_e direction); 
static void         make_filler_column(tcb_t *tcb, guint position); 
static void         add_functions_to_column(tcb_t *tcb, result_t *function_list, guint num_results,
        guint col, guint starting_row, dir_e direction); 
static void         make_expander_at_position(tcb_t *tcb, guint col, guint row, dir_e direction);
static void         shift_column_down(tcb_t *tcb, guint col, guint starting_row, guint amount);
static int          delete_functions_from_column(tcb_t *tcb, guint col, guint starting_row, guint end_row, dir_e direction); 
static void         shift_column_up(tcb_t *tcb, guint col, guint starting_row, guint amount); 
static void         delete_children(tcb_t *tcb, guint col, guint upper_bound, guint lower_bound, dir_e direction); 
static void         shift_table_up(tcb_t *tcb, guint upper_bound, guint amount, dir_e direction); 
static void         remove_unused_columns(tcb_t *tcb, dir_e direction);
static void         shift_table_right(tcb_t *tcb, guint start_col); 
static void         move_column(tcb_t *tcb, guint source, guint dest);
static void         shift_table_left(tcb_t *tcb, guint amount); 
static void         delete_column(tcb_t *tcb, guint col); 
static void         delete_table(tcb_t *tcb); 
static void         update_rows(tcb_t *tcb); 
static void         make_sliders(tcb_t *tcb); 
static void         clear_sliders(tcb_t *tcb); 
static GtkWidget    *make_straight_connector();
static GtkWidget    *make_three_way_connector(dir_e direction);
static void         add_connectors(tcb_t *tcb, guint col, guint row, guint count, dir_e direction);
static GtkWidget    *make_end_connector(dir_e direction);

static void         column_list_insert(col_list_t *list, GtkWidget *widget, guint row);
static void         column_list_insert_file(col_list_t *list, GtkWidget *widget, guint row, gchar *file);
static void         column_list_remove(col_list_t *list, GtkWidget *widget);
static gboolean     column_list_is_sorted(col_list_t *list);
static column_entry_t  *column_list_get_row(col_list_t *list, guint row); 
static void         column_list_print_rows(col_list_t *list); 
static guint        column_list_get_next(col_list_t *list, guint row); 
static gboolean     end_of_block(col_list_t *column, guint row);

static void         get_function(tcb_t *tcb, guint col, guint row, dir_e direction, gchar **fname, gchar **file);
static result_t     *parse_results(search_results_t *results); 
static guint        results_list_remove_duplicates(result_t *front); 
static void         results_list_sort(result_t *front); 
static void         swap(result_t *a, result_t *b); 
static guint        results_list_filter_file(result_t **front_ptr, gchar *file);
static guint        get_children(gchar *function, gchar *file);
// Local Global Variables
//=======================

static GtkWidget    *test_widget;

//
// Private Functions
// ========================================================
//
static GtkWidget   *create_browser_window(gchar *name, gchar *root_file, gchar *line_num)
{
    GtkWidget *browser_window;
    GdkPixbuf *browser_window_icon_pixbuf;
    GtkWidget *browser_vbox;
    GtkWidget *browser_scrolledwindow;
    GtkWidget *browser_viewport;
    GtkWidget *browser_table;
    gchar *var_string;
    tcb_t     *tcb;

    //
    // Malloc a Table Control Block (TCB) for this window.  This keeps the table data with the
    // widget and makes this function re-entrant-safe.
    //========================================================================================
    tcb = g_malloc( sizeof(tcb_t) );

    browser_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(browser_window, "browser_window");
    gtk_widget_set_size_request(browser_window, 250, 130);
    my_asprintf(&var_string, "Static Call Browser (%s)", name);
    gtk_window_set_title(GTK_WINDOW(browser_window), var_string);
    g_free(var_string);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(browser_window), TRUE);
    browser_window_icon_pixbuf = create_pixbuf("icon-search.png");
    if (browser_window_icon_pixbuf)
    {
        gtk_window_set_icon(GTK_WINDOW(browser_window), browser_window_icon_pixbuf);
        gdk_pixbuf_unref(browser_window_icon_pixbuf);
    }

    browser_vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_set_name(browser_vbox, "browser_vbox");
    gtk_widget_show(browser_vbox);
    gtk_container_add(GTK_CONTAINER(browser_window), browser_vbox);

    browser_scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(browser_scrolledwindow, "browser_scrolledwindow");
    gtk_widget_show(browser_scrolledwindow);
    gtk_box_pack_start(GTK_BOX(browser_vbox), browser_scrolledwindow, TRUE, TRUE, 0);

    browser_viewport = gtk_viewport_new(NULL, NULL);
    gtk_widget_set_name(browser_viewport, "browser_viewport");
    gtk_widget_show(browser_viewport);
    gtk_container_add(GTK_CONTAINER(browser_scrolledwindow), browser_viewport);

    browser_table = gtk_table_new(3, 5, FALSE);
    gtk_widget_set_name(browser_table, "browser_table");
    gtk_widget_show(browser_table);
    gtk_container_add(GTK_CONTAINER(browser_viewport), browser_table);

    // Initialize the TCB
    tcb->browser_table = browser_table;
    initialize_table(tcb, name, root_file, line_num);

    g_signal_connect((gpointer) browser_window, "delete_event",
            G_CALLBACK (on_browser_delete_event),
            tcb);

    return browser_window;
}




//
// Public Functions
// ========================================================
//

GtkWidget* BROWSER_init(gchar *name, gchar *root_file, gchar *line_num)
{
    return( create_browser_window(name, root_file, line_num) );
}


//
// Private Callbacks for "Static Call Browser" functionality
// =========================================================
//

static gboolean on_browser_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    // Free the TCB before we destroy this widget
    g_free(user_data);

    return FALSE;    // Return FALSE to destroy the widget.
}


static gboolean on_column_hscale_change_value(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data)
{
    gtk_label_set_width_chars(GTK_LABEL(user_data), (gint)value);

    return FALSE;
}




static gboolean on_left_expander_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    guint       child_count;
    GtkWidget   *collapser;
    guint       row, col;
    tcb_t       *tcb = user_data;
    col_list_t *column_list;


    // Get the widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
            "top-attach", &row,
            "left-attach", &col,
            NULL);

    // Destroy the selected expander
    column_list = &(tcb->col_list[col]);
    column_list_remove(column_list, widget);
    gtk_widget_destroy(widget);


    // replace it with a collapser 
    collapser = make_collapser(child_count);
    column_list_insert(column_list, collapser, row);

    // Revisit: need to figure out table position from "widget" before destroying it.  Cheating for now...
    gtk_table_attach(GTK_TABLE(tcb->browser_table), collapser, col, col + 1, row, row + 1,
            (GtkAttachOptions)(GTK_FILL),
            (GtkAttachOptions)(GTK_FILL), 0, 0);

    g_signal_connect((gpointer)collapser, "button_release_event",
            G_CALLBACK(on_left_collapser_button_release_event),
            user_data);

    // add the relevant functions to the table and grow
    // if necessary
    expand_table(row, col, tcb, LEFT);

    return FALSE;
}

static gboolean on_right_expander_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    guint       child_count;
    GtkWidget   *collapser;
    guint       row, col;
    tcb_t       *tcb = user_data;
    col_list_t *list;

    // Get the widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
            "top-attach", &row,
            "left-attach", &col,
            NULL);

    // Destroy the selected expander
    list = &(tcb->col_list[col]);
    column_list_remove(list, widget);
    gtk_widget_destroy(widget);


    collapser = make_collapser(child_count);
    column_list_insert(list, collapser, row);

    // Revisit: need to figure out table position from "widget" before destroying it.  Cheating for now...
    gtk_table_attach(GTK_TABLE(tcb->browser_table), collapser, col, col + 1, row, row + 1,
            (GtkAttachOptions)(GTK_FILL),
            (GtkAttachOptions)(GTK_FILL), 0, 0);


    g_signal_connect((gpointer)collapser, "button_release_event",
            G_CALLBACK(on_right_collapser_button_release_event),
            user_data);

    expand_table(row, col, tcb, RIGHT);

    return FALSE;
}


static gboolean on_left_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkWidget   *expander;
    tcb_t       *tcb = user_data;
    guint       row, col, child_count;
    col_list_t *list;

    // Get the widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
            "top-attach", &row,
            "left-attach", &col,
            NULL);

    // Destroy the selected expander
    list = &(tcb->col_list[col]);
    column_list_remove(list, widget);
    gtk_widget_destroy(widget);

    make_expander_at_position(tcb, col, row, LEFT);

    collapse_table(row, col, tcb, LEFT);
    return FALSE;
}



static gboolean on_right_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkWidget   *expander;
    tcb_t       *tcb = user_data;
    guint       row, col, child_count;
    col_list_t *list;

    // Get the widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
            "top-attach", &row,
            "left-attach", &col,
            NULL);

    // Destroy the selected expander
    list = &(tcb->col_list[col]);
    column_list_remove(list, widget);
    gtk_widget_destroy(widget);

    make_expander_at_position(tcb, col, row, RIGHT);

    collapse_table(row, col, tcb, RIGHT);
    return FALSE;
}

static gboolean on_function_button_press(GtkWidget *widget, GdkEventButton *event, result_t  *box)
{
    // open text editor at location on left double click, open menu on right click
    if (event->type == GDK_2BUTTON_PRESS && event->button == 1) // left double click 
    {
        if (settings.useEditor)
        {
            start_text_editor(box->file_name, box->line_num);
        }
        else
        {
            FILEVIEW_create(box->file_name, atoi(box->line_num));
        }
    }
    else if (event->type == GDK_BUTTON_PRESS && event->button == 3) // right click
    {
        right_click_menu(widget, event, box);
    }
}

static void on_reroot(GtkWidget *menuitem, result_t *function_box)
{
    GtkWidget *browser;
    search_results_t *matches;
    // search for the function and see how many results come up
    matches = SEARCH_lookup(FIND_DEF, function_box->function_name);

    if (matches->match_count == 1)
    {
        // only 1 match we can just reset the browser with the appropriate function
        delete_table((tcb_t *) function_box->tcb);
        initialize_table((tcb_t *)function_box->tcb,
                function_box->function_name,
                function_box->file_name,
                function_box->line_num);
    }
    else if (matches->match_count > 1)
    {
        // serach and let the user chose which function to pick
        DISPLAY_search_results(FIND_DEF, matches);
    }

    SEARCH_free_results(matches);
}

// clear every widget from the table
static void delete_table(tcb_t *tcb) 
{
    int i;
    clear_sliders(tcb);
    for (i = 0; i < tcb->num_cols; i++) {
        delete_column(tcb, i);
    }
}

static void right_click_menu(GtkWidget *widget, GdkEventButton *event, result_t *function_box) 
{
    GtkWidget *menu, *menu_item;

    menu = gtk_menu_new();

    menu_item = gtk_menu_item_new_with_label("reroot");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect(menu_item, "activate", (GCallback) on_reroot, function_box);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
            (event != NULL) ? event->button : 0,
            gdk_event_get_time((GdkEvent*)event));
}

static void initialize_table(tcb_t *tcb, gchar *root_fname, gchar *root_file, gchar *line_num)
{
    GtkWidget *header_button;
    GtkWidget *root_function_blue_eventbox;
    GtkWidget *root_function_label;
    GtkWidget *root_hscale;
    gchar *var_string;
    search_results_t *children;

    gtk_table_resize(GTK_TABLE(tcb->browser_table), 2, 5);

    tcb->root_col      = 2;     // Center column of a zero-based 5 column initial table
    tcb->num_cols      = 5;
    tcb->num_rows      = 2;
    tcb->left_height   = 1;
    tcb->right_height  = 1;
    memset(tcb->col_list, 0, sizeof(tcb->col_list));

    // Configure the initial columns
    {
        tcb->col_list[0].type = FILLER;         // First and last 'even' columns are FILLER columns
        tcb->col_list[1].type = EXPANDER;       // All 'odd' columns are EXPANDER colums
        tcb->col_list[2].type = NAME;           // All non-first, non-last 'even' columns are NAME columns
        tcb->col_list[3].type = EXPANDER;
        tcb->col_list[4].type = FILLER;
    }

    // put file name and function name into a result_t to pass to the on-click callback
    tcb->root_entry.function_name = root_fname;
    tcb->root_entry.file_name = root_file;
    tcb->root_entry.line_num = line_num;
    tcb->root_entry.tcb = tcb;

    /* Left Expander */
    make_expander_at_position(tcb, 1, 1, LEFT);

    /* RIGHT Expander */
    make_expander_at_position(tcb, 3, 1, RIGHT);

    header_button = gtk_button_new_with_mnemonic("0");
    gtk_widget_set_name(header_button, "header_button");
    gtk_widget_show(header_button);
    gtk_table_attach(GTK_TABLE(tcb->browser_table), header_button, 1, 4, 0, 1,
            (GtkAttachOptions)(GTK_FILL),
            (GtkAttachOptions)(0), 0, 0);
    gtk_widget_set_can_focus(header_button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(header_button), FALSE);

    root_function_blue_eventbox = gtk_event_box_new();
    gtk_widget_set_name(root_function_blue_eventbox, "blue_eventbox");
    gtk_widget_show(root_function_blue_eventbox);
    gtk_table_attach(GTK_TABLE(tcb->browser_table), root_function_blue_eventbox, 2, 3, 1, 2,
            (GtkAttachOptions)(GTK_FILL),
            (GtkAttachOptions)(GTK_FILL), 0, 0);

    // Initialize the left filler column
    make_filler_column(tcb, 0);
    {   // Initialize the root column entries
        col_list_t *root_col = &(tcb->col_list[2]);
        root_col->header_column_label = header_button;
        column_list_insert_file(root_col, root_function_blue_eventbox, 1, root_file);
    }

    // Initialize the right filler column
    make_filler_column(tcb, 4);

    my_asprintf(&var_string, "<span color=\"darkred\">%s</span>", root_fname);
    root_function_label = gtk_label_new(var_string);
    g_free(var_string);
    gtk_widget_set_name(root_function_label, "root_function_label");
    gtk_widget_show(root_function_label);
    gtk_container_add(GTK_CONTAINER(root_function_blue_eventbox), root_function_label);
    gtk_label_set_use_markup(GTK_LABEL(root_function_label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(root_function_label), 0, 0.5);
    gtk_misc_set_padding(GTK_MISC(root_function_label), 1, 0);
    gtk_label_set_ellipsize (GTK_LABEL (root_function_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(GTK_LABEL(root_function_label), 10);

    root_hscale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(10, 4, 40, 1, 0, 0)));
    gtk_widget_set_name(root_hscale, "root_hscale");
    gtk_widget_show(root_hscale);
    gtk_table_attach(GTK_TABLE(tcb->browser_table), root_hscale, 2, 3, 2, 3,
            (GtkAttachOptions)(GTK_FILL),
            (GtkAttachOptions)(GTK_FILL), 0, 0);
    gtk_scale_set_draw_value(GTK_SCALE(root_hscale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(root_hscale), 0);

    tcb->col_list[2].slider = root_hscale;

    g_signal_connect ((gpointer) root_function_blue_eventbox, "button_press_event",
            G_CALLBACK (on_function_button_press), &(tcb->root_entry)); 

    g_signal_connect((gpointer)root_hscale, "change_value",
            G_CALLBACK(on_column_hscale_change_value),
            root_function_label);

}

static void move_table_widget(GtkWidget *widget, tcb_t *tcb, guint row, guint col)
{
    g_object_ref(widget);  // Create a temporary reference to prevent the widget from being destroyed.

    gtk_container_remove(GTK_CONTAINER(tcb->browser_table), widget);

    gtk_table_attach(GTK_TABLE(tcb->browser_table), widget, col, col + 1, row, row + 1,
            (GtkAttachOptions)(GTK_FILL),
            (GtkAttachOptions)(GTK_FILL), 0, 0);

    g_object_unref((gpointer)widget);  // Create a temporary reference to prevent the widget from being destroyed.


}

/*
   This function implements a table expansion associated with an "expander"-clicked event.
   Overall functionality includes:
   1) Add new rows and/or new columns to the table, as required [resize table widget]
   2) Relocate existing widgets that need to be moved in order to accommodate the expansion.
   3) Add any new widgets: Icons and function names "exposed" by the expansion operation.
   */
static void expand_table(guint origin_row, guint origin_col, tcb_t *tcb, dir_e direction)
{
    int i, add_pos, adjusted_col;
    guint row_add_count;
    col_list_t *list;
    gchar *fname, *file;
    result_t *results;
    search_results_t *children;
    search_t operation;

    // set the search type depending on which way we are expanding
    operation = direction == RIGHT ? FIND_CALLEDBY : FIND_CALLING;

    // get the function associated with the expander that was clicked
    // search for the appropriate children for that function and
    // parse them into a usable list
    get_function(tcb, origin_col, origin_row, direction, &fname, &file); 
    children = SEARCH_lookup(operation, fname);
    if (children->match_count == 0)
    {
        return; 
    }
    clear_sliders(tcb);
    
    results = parse_results(children);
    //children->match_count -= results_list_filter_file(&results, file);
    row_add_count = children->match_count - 1; // the first child is on the same line so it is not an added ro
    SEARCH_free_results(children);  //children is unusable after this

    // make sure the table has enough room to add functions
    ensure_row_capacity(tcb, origin_row, row_add_count);
    ensure_column_capacity(tcb, origin_col,  direction);

    

    // shift down columns left or right of center depening on direction
    // then add functions
    if (direction == RIGHT) {
        for (i = tcb->root_col; i < tcb->num_cols; i++) {
            list = &(tcb->col_list[i]);
            if (!column_list_is_sorted(list)) {
                printf("ERROR COLUMN %d OUT OF ORDER:\n\n", i);
            }
            shift_column_down(tcb, i, origin_row, row_add_count);
        }
        add_functions_to_column(tcb, results, row_add_count + 1, origin_col + 1, origin_row, RIGHT);
        add_connectors(tcb, origin_col, origin_row, row_add_count + 1, RIGHT);
    } else {
        // if this is the furthest left column then origin_col - 1 is not the correct
        // place to add due to columns being shifted right
        add_pos = origin_col == 1 ? 2: origin_col - 1;
        adjusted_col = origin_col == 1 ? 3 : origin_col;

        for (i = tcb->root_col; i > 0; i--) {
            list = &(tcb->col_list[i]);
            if (!column_list_is_sorted(list)) {
                printf("ERROR COLUMN %d OUT OF ORDER:\n\n", i);
            }
            shift_column_down(tcb, i, origin_row, row_add_count);
        }
        add_functions_to_column(tcb, results, row_add_count + 1, add_pos, origin_row, LEFT);
        add_connectors(tcb, adjusted_col, origin_row, row_add_count + 1, LEFT);
    }


    // update table height 
    if(direction == RIGHT) {
        tcb->right_height += row_add_count;
    } else {
        tcb->left_height += row_add_count;
    }
    update_rows(tcb);
    
    make_sliders(tcb);
}

static void ensure_column_capacity(tcb_t *tcb, guint origin_col, dir_e direction) {
    if (direction == RIGHT) {
        if (origin_col == tcb->num_cols - 2) {  // -2 because of filler column and zero based indexing
            // name column and another expander column
            gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols + 2);

            // now we need to update the position of the filler column

            gtk_widget_destroy(tcb->col_list[origin_col + 1].header_column_label);
            make_filler_column(tcb, origin_col + 3);

            // and update the column type of the newly added columns
            // NAME and EXPANDER
            tcb->col_list[origin_col + 1].type = NAME;
            tcb->col_list[origin_col + 2].type = EXPANDER;
            tcb->num_cols += 2;

            make_name_column(tcb, origin_col + 1, RIGHT);
        }
    } else {
        // direction == LEFT
        if (origin_col == 1) {
            // need to expand the table to the left
            // which means that every column from 1-num_cols
            // needs to be shifted 2 to the right
            shift_table_right(tcb, origin_col);
        }
    }

}

// moves every column right of start_col (inclusive) 2
// columns to the right leaving 2 empty columns
// in position start_col and start_col + 1
static void shift_table_right(tcb_t *tcb, guint start_col) {
    int i;
    gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols + 2);
    for (i = tcb->num_cols - 1; i >= start_col; i--) {
        move_column(tcb, i, i + 2);
    }
    //leaves rows 1 and two empty, 1 is an expander column, 2 is a name column
    make_name_column(tcb, 2, LEFT);
    tcb->num_cols += 2;
    tcb->root_col += 2;
}

// copys all column elements from source to dest
// all column elements in dest will be deleted
// and elements in source will be deleted after moving
// leaveing source empty
static void move_column(tcb_t *tcb, guint source, guint dest) {
    col_list_t *column, *dest_col;
    column_entry_t *curr, *next;
    dir_e direction;
    GtkWidget *header_button, *widget;
    guint row;

    // get the lists that are being modified
    dest_col = &(tcb->col_list[dest]);
    column = &(tcb->col_list[source]);

    // delete the destination column, we don't need to keep anything
    delete_column(tcb, dest);
    if (column->type == NAME) {
        if (source == tcb->root_col) {
            // the root column is a speical case since it has
            // a wider header
            header_button = gtk_button_new_with_mnemonic("0");
            gtk_widget_set_name(header_button, "header_button");
            gtk_widget_show(header_button);
            gtk_table_attach(GTK_TABLE(tcb->browser_table), header_button, dest - 1, dest + 2, 0, 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
            gtk_widget_set_can_focus(header_button, FALSE);
            gtk_button_set_focus_on_click(GTK_BUTTON(header_button), FALSE);

            tcb->col_list[dest].header_column_label = header_button;
            tcb->col_list[dest].type = NAME;
        } else {
            direction = source > tcb->root_col ? RIGHT : LEFT; 
            // we always want to keep the header_number so it doesn't get recauculated and change
            make_name_column_labeled(tcb, dest, direction, column->header_column_num);
        }
    }

    if (column->type == FILLER) {
        make_filler_column(tcb, dest);
    }
    if (column->type == EXPANDER) {
        tcb->col_list[dest].type = EXPANDER;
    }

    // copy each widget one by one
    curr = column->column_member_list;
    while (curr != NULL) {
        widget = curr->widget;
        next = curr->next;
        row = curr->row;
        column_list_remove(column, widget);  // this frees the curr node
        column_list_insert_file(dest_col, widget, row, curr->file); 
        column_list_get_row(dest_col, row)->last_entry = curr->last_entry;
        move_table_widget(curr->widget, tcb, curr->row, dest);
        curr = next;
    }

    // everything is moved, we can safely delete the source column
    delete_column(tcb, source);

    // TODO not sure if this is necessary, revist
    if (column->type == NAME && source != tcb->root_col) {
        direction = source > tcb->root_col ? RIGHT : LEFT; 
        make_name_column(tcb, dest, direction);
    }

}


static void ensure_row_capacity(tcb_t *tcb, guint origin_row, guint row_add_count) {
    guint delta = origin_row + row_add_count - tcb->num_rows; 
    // make sure we have two extra rows at the bottom for filler and hscale
    if (delta > 0) {
        // add enough rows
        gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows + delta, tcb->num_cols);
    }
}


/*
   This function implements a table reduction associated with a "collapser"-clicked event.
   Overall functionality includes:
   2) Relocate existing widgets that need to be moved in order to accommodate the reduction.
   3) Remove any eclipsed widgets: Icons and function names "hidden" by the reduction operation.
   1) Remove existing rows and/or existing columns from the table, as required  [resize table widget]
   */
static void collapse_table(guint origin_row, guint origin_col, tcb_t *tcb, dir_e direction)
{

    col_list_t *column;
    guint next_row;
    guint i;

    guint offset = direction == RIGHT ? -1 : 1;
    guint update = 2 * offset;
    guint lower_bound = direction == RIGHT ?
                                            tcb->right_height + 1 :
                                            tcb->left_height + 1;
    //first thing that needs to be done is to recursivly collapse all children
    //to do this we need to know the position of the next element in this column
    //to put a lower bound on what we collpase (don't want to collapse children
    //of other funcitons)

    //TODO get the lower bound
    for (i = origin_col + offset; i != tcb->root_col; i += update)
    {
        column = &(tcb->col_list[i]); 
        next_row = column_list_get_next(column, origin_row);
        if (next_row != -1)
        {
            lower_bound = next_row;
            break;
        }
    }

    clear_sliders(tcb);

    //delete all children of the collapser and shift up the table
    //the appropriate amount
    delete_children(tcb, origin_col, origin_row, lower_bound, direction);

    //check if the table needs to be downsized, if it does it will always
    //be a multiple of 2 rows for name and collapser columns
    remove_unused_columns(tcb, direction);

    // recreate the sliders to update their position
    make_sliders(tcb);
}

// removes every non filler column that contains no elements
// that is direction (left or right) of the root column
// and appropriatly shifts columns and resizes the table as necessary
static void remove_unused_columns(tcb_t *tcb, dir_e direction) {
    int i;
    int count = 0; // number of columns that are removed
    col_list_t *column;

    if (direction == RIGHT) {
        delete_column(tcb, tcb->num_cols - 1);  // always delete the right filler column
        for (i = tcb->num_cols - 3; i > tcb->root_col; i -= 2) {      // num_cols - 3 is furthest right name column 
            column = &(tcb->col_list[i]);
            if (column->column_member_list == NULL) {
                // destory the name column and its expander column 
                delete_column(tcb, i);
                delete_column(tcb, i + 1);
                count += 2;
            }
        }
        gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols - count);
        tcb->num_cols -= count;
        make_filler_column(tcb, tcb->num_cols - 1);  // remake the right filler column
    } else {
        // direction is left

        // count the number of columns to be removed
        // only count name columns, expander columns may be correctly
        // empty
        for (i = 2; i < tcb->root_col; i +=2) {    // 2 is furthest left name column
            col_list_t *column = &(tcb->col_list[i]);
            if (column->column_member_list == NULL) {
                count += 2;
            }
        }
        if (count > 0) {
            shift_table_left(tcb, count);
            for (i = tcb->num_cols - count; i < tcb->num_cols; i++) {
                delete_column(tcb, i);
            }
            gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols - count);
            tcb->num_cols -= count; 
            tcb->root_col -= count;
        }
    }
}

// moves every column to the right of amount, amount spots to the left in the tcb->col_list
static void shift_table_left(tcb_t *tcb, guint amount) {
    int i;
    for (i = amount + 1; i < tcb->num_cols; i++) {
        move_column(tcb, i, i - amount);
    }
}

// removes every widget in a column
static void delete_column(tcb_t *tcb, guint col) {
    column_entry_t *curr, *next;
    col_list_t *column = &(tcb->col_list[col]);

    if (column->header_column_label != NULL) {
        gtk_widget_destroy(column->header_column_label);
        column->slider = NULL;
    }
    if (column->slider != NULL) {
        gtk_widget_destroy(column->slider);
        column->slider = NULL;
    }
    curr = column->column_member_list;

    // clear the column list
    while (curr != NULL) {
        gtk_widget_destroy(curr->widget);
        next = curr->next;
        g_free(curr);
        curr = next;
    }

    // probably not necessary but it can't hurt
    memset(column, 0, sizeof(col_list_t));
}

// shifts every widget belowe uper_bound on the direction side of the root column up 
// by specified amount
static void shift_table_up(tcb_t *tcb, guint upper_bound, guint amount, dir_e direction) {
    int i;
    col_list_t *list;
    guint left, right;

    // set bounds depending on direction
    left = direction == RIGHT ? tcb->root_col : 1;
    right = direction == RIGHT ? tcb->num_cols : tcb->root_col;

    for (i = left; i < right; i++) {
        list = &(tcb->col_list[i]);
        shift_column_up(tcb, i, upper_bound, amount); 
        if (!column_list_is_sorted(list)) {
            printf("ERROR COLUMN %d OUT OF ORDER:\n\n", i);
        }
    }
    if (direction == RIGHT) {
        tcb->right_height -= amount;
    } else {
        tcb->left_height -= amount;
    }
    update_rows(tcb);
}

static void delete_children(tcb_t *tcb, guint col, guint upper_bound, guint lower_bound, dir_e direction) {
    int i, lowest, height, shift;
    lowest = upper_bound; // "lowest" row removed, is actually the highest number

    if (direction == RIGHT) {
        for (i = tcb->num_cols - 2; i > col; i--) {
            height = delete_functions_from_column(tcb, i, upper_bound, lower_bound, RIGHT);        
            lowest = height > lowest ? height : lowest;
        }
        // delete the connectors from the clicked expander column
        for (i = tcb->root_col + 1; i <= col; i += 2) {
            delete_functions_from_column(tcb, i, upper_bound + 1, lower_bound, RIGHT);
        }
    } else {
        // direction == left
        for (i = 1; i < col; i++) {
            height = delete_functions_from_column(tcb, i, upper_bound, lower_bound, LEFT);
            lowest = height > lowest ? height : lowest;
        }
        // delete the connectors from the clicked expander column
        for (i = tcb->root_col - 1; i >= col; i--) {
            delete_functions_from_column(tcb, i, upper_bound + 1, lower_bound, RIGHT);
        }
    }
    shift = lowest - upper_bound; 
    shift_table_up(tcb, upper_bound, shift, direction);
}

static int delete_functions_from_column(tcb_t *tcb, guint col, guint starting_row, guint end_row, dir_e direction) {
    int offset = direction == RIGHT ? 1 : -1;

    col_list_t *function_list = &(tcb->col_list[col]);

    guint lowest = starting_row;  // the "lowest" column removed, so highest col number
    guint i;
    column_entry_t *function, *expander;
    for (i = starting_row; i < end_row; i++) {
        function = column_list_get_row(function_list, i);
        if (function != NULL) {
            lowest = function->row > lowest ? function->row : lowest;
            gtk_widget_destroy(function->widget);
            column_list_remove(function_list, function->widget);
        }
    }
    return lowest;
}

static void shift_column_up(tcb_t *tcb, guint col, guint starting_row, guint amount) {
    col_list_t *elements = &(tcb->col_list[col]);

    column_entry_t *node = elements->column_member_list;
    while (node != NULL) {
        if (node->row > starting_row + amount) {
            move_table_widget(node->widget, tcb, node->row - amount, col);
            node->row -= amount;
        }
        node = node->next;
    }
}


static GtkWidget *make_collapser(guint child_count)
{
    GtkWidget   *eventbox;
    GtkWidget   *image;

    eventbox = gtk_event_box_new();
    gtk_widget_set_name(eventbox, "expander_eventbox");
    gtk_widget_show(eventbox);

    if ( child_count > 1 )
        image = create_pixmap(eventbox, "sca_collapser_multi.png");
    else
        image = create_pixmap(eventbox, "sca_collapser_single.png");

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return(eventbox);
}


static GtkWidget *make_expander (dir_e direction, gboolean new_expander)
{
    GtkWidget   *eventbox;
    GtkWidget   *image;

    eventbox = gtk_event_box_new();
    gtk_widget_set_name(eventbox, "expander_eventbox");
    gtk_widget_show(eventbox);

    if ( direction == LEFT )
    {
        image = create_pixmap(eventbox, "sca_expander_left.png");
    } else
    {
        image = create_pixmap(eventbox, "sca_expander_right.png");
    }

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return(eventbox);
}

static void make_filler_column(tcb_t *tcb, guint position) {
    GtkWidget *horizontal_filler_header_button = gtk_button_new_with_mnemonic("");
    gtk_widget_set_name(horizontal_filler_header_button, "horizontal_filler_header_button");
    gtk_widget_show(horizontal_filler_header_button);
    gtk_table_attach(GTK_TABLE(tcb->browser_table), horizontal_filler_header_button, position, position + 1, 0, 1,
            (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
            (GtkAttachOptions)(0), 0, 0);
    gtk_widget_set_can_focus(horizontal_filler_header_button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(horizontal_filler_header_button), FALSE);

    tcb->col_list[position].type = FILLER;
    tcb->col_list[position].header_column_label = horizontal_filler_header_button;
    tcb->col_list[position].header_column_num = position; 
    tcb->col_list[position].column_member_list = NULL; 
}

static void make_expander_at_position(tcb_t *tcb, guint col, guint row, dir_e direction) {
    col_list_t *list;

    GtkWidget *expander_eventbox = make_expander(direction, TRUE);
    gtk_widget_set_name(expander_eventbox, "expander_eventbox");
    gtk_widget_show(expander_eventbox);
    gtk_table_attach(GTK_TABLE(tcb->browser_table), expander_eventbox, col, col + 1, row, row + 1,
            (GtkAttachOptions)(GTK_FILL),
            (GtkAttachOptions)(GTK_FILL), 0, 0);

    list = &(tcb->col_list[col]);
    column_list_insert(list, expander_eventbox, row);

    tcb->col_list[col].type = EXPANDER;

    if (direction == RIGHT) {
        g_signal_connect((gpointer)expander_eventbox, "button_release_event",
                G_CALLBACK(on_right_expander_button_release_event),
                tcb);
    } else {
        g_signal_connect((gpointer)expander_eventbox, "button_release_event",
                G_CALLBACK(on_left_expander_button_release_event),
                tcb);  // Revisit: Pass "table" (and possibly table position)
    }

}

static void add_functions_to_column(tcb_t *tcb, result_t *function_list, guint num_results,
        guint col, guint starting_row, dir_e direction) {
    int row, offset;
    col_list_t *list;
    char *var_string;
    GtkWidget *function_label;
    GtkWidget *function_event_box;
    result_t *node, *all_matches;
    search_results_t *children;
    search_t operation;

    offset = direction == RIGHT ? 1 : -1;
    node = function_list;

    for ( row = starting_row; row < starting_row + num_results; row++) {
        function_event_box = gtk_event_box_new();
        gtk_widget_set_name(function_event_box, "event_box");
        gtk_widget_show(function_event_box);
        gtk_table_attach(GTK_TABLE(tcb->browser_table), function_event_box, col, col + 1, row, row + 1,
                (GtkAttachOptions)(GTK_FILL),
                (GtkAttachOptions)(GTK_FILL), 0, 0);
        my_asprintf(&var_string, "<span color=\"darkred\">%s</span>", node->function_name);
        function_label = gtk_label_new(var_string);
        g_free(var_string);
        gtk_widget_set_name(function_label, "function_label");
        gtk_widget_show(function_label);
        gtk_container_add(GTK_CONTAINER(function_event_box), function_label);
        gtk_label_set_use_markup(GTK_LABEL(function_label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(function_label), 0, 0.5);
        gtk_misc_set_padding(GTK_MISC(function_label), 1, 0);
        gtk_label_set_ellipsize (GTK_LABEL (function_label), PANGO_ELLIPSIZE_END);
        gtk_label_set_width_chars(GTK_LABEL(function_label), 10);

        list = &(tcb->col_list[col]);
        column_list_insert_file(list, function_event_box, row, node->file_name);
        
        if (row == starting_row + num_results - 1) {
            column_list_get_row(list, row)->last_entry = TRUE;
        }
        else
        {
            column_list_get_row(list, row)->last_entry = FALSE;
        }
        operation = direction == RIGHT ? FIND_CALLEDBY : FIND_CALLING;

        node->tcb = tcb;
        g_signal_connect ((gpointer) function_event_box, "button_press_event",
                G_CALLBACK (on_function_button_press), node);

        // check to see if we actually need to add an expander
        // only need one if this function calls other functions
        children = SEARCH_lookup(operation, node->function_name);
        if (children->match_count > 0) {
            make_expander_at_position(tcb, col + offset, row, direction);
        }
        SEARCH_free_results(children);

        node = node->next;
    }
}

static void make_name_column(tcb_t *tcb, guint col, dir_e direction) {
    guint prev_col = direction == RIGHT ? -1 : 1; 
    guint col_num = tcb->col_list[col + 2 * prev_col].header_column_num - prev_col;
    make_name_column_labeled(tcb, col, direction, col_num); 
}

static void make_name_column_labeled(tcb_t *tcb, guint col, dir_e direction, guint label) {
    GtkWidget *vertical_filler_label;
    GtkWidget *header_button;
    char num_str[3];
    col_list_t *column = &(tcb->col_list[col]);
    int left_bound = direction == RIGHT ? col : col - 1;
    int right_bound = direction == RIGHT ? col + 2 : col + 1;
    column->header_column_num = label;

    sprintf(num_str, "%d", label);
    header_button  = gtk_button_new_with_mnemonic(num_str);
    gtk_widget_set_name(header_button, "header_button");
    gtk_widget_show(header_button);
    gtk_table_attach(GTK_TABLE(tcb->browser_table), header_button, left_bound, right_bound, 0, 1,
            (GtkAttachOptions)(GTK_FILL),
            (GtkAttachOptions)(0), 0, 0);
    gtk_widget_set_can_focus(header_button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(header_button), FALSE);

    column->type = NAME;
    column->header_column_label = header_button;
}

static void shift_column_down(tcb_t *tcb, guint col, guint starting_row, guint amount) {
    col_list_t elements = tcb->col_list[col];

    // traverse linked list from the back shifting every element down by amount 
    column_entry_t *node = elements.back;
    while (node != NULL && node->row > starting_row) {
        move_table_widget(node->widget, tcb, node->row + amount, col);
        node->row += amount;
        node = node->prev;
    }
}

static void column_list_insert(col_list_t *list, GtkWidget *widget, guint row) {
    column_list_insert_file(list, widget, row, NULL);
}

// sorted insert into column list, column list should also be in sorted order
static void column_list_insert_file(col_list_t *list, GtkWidget *widget, guint row, gchar *file) {
    // create the new node to add
    column_entry_t *new_node = g_malloc( sizeof(column_entry_t) );
    new_node->widget = widget;
    new_node->row = row;
    new_node->next = NULL;
    new_node->prev = NULL;
    if (file != NULL) {
        memcpy(new_node->file, file, strlen(file) + 1);
    }

    // check the front
    if (list->column_member_list == NULL) {
        list->column_member_list = new_node;
        list->back = new_node;
    } else if (list->column_member_list->row > row) {
        // insert at the front

        new_node->next = list->column_member_list;
        list->column_member_list->prev = new_node;
        list->column_member_list = new_node;
    } else {
        // we need to find where to add;

        column_entry_t *curr = list->column_member_list;
        while (curr->next != NULL && curr->next->row < row) {
            curr = curr->next;
        }
        new_node->next = curr->next;
        if (curr->next != NULL) {
            curr->next->prev = new_node;
        }
        curr->next = new_node;
        new_node->prev = curr;
        if (new_node->next == NULL) {
            list->back = new_node;
        }
    }
}

static void column_list_remove(col_list_t *list, GtkWidget *widget) {

    if (list->column_member_list == NULL) {
        return;
    }

    // check the front
    if (list->column_member_list->widget == widget) {
        list->column_member_list = list->column_member_list->next;
        if (list->column_member_list == NULL) {
            list->back = NULL;
        } else {
            list->column_member_list->prev = NULL;
        }
    } else {
        // find the widget to remove
        column_entry_t *curr = list->column_member_list;
        while (curr->next != NULL && curr->next->widget != widget) {
            curr = curr->next;
        }
        if (curr->next != NULL) {
            // curr->next is the node to be removed
            column_entry_t *temp = curr->next;
            curr->next = curr->next->next;
            if (curr->next == NULL) {
                list->back = curr;
            } else {
                curr->next->prev = curr;
            }
            g_free(temp);
        }
    }
}

static column_entry_t  *column_list_get_row(col_list_t *list, guint row) {
    column_entry_t *curr;
    if (list == NULL) return NULL;
    curr = list->column_member_list;
    while (curr != NULL) {
        if (curr->row == row) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static gboolean     column_list_is_sorted(col_list_t *list) {
    column_entry_t *curr = list->column_member_list;
    while (curr != NULL && curr->next != NULL) {
        if (curr->row >= curr->next->row) {
            return FALSE;
        }
        curr = curr->next;
    }
    return TRUE;
}

static void column_list_print_rows(col_list_t *list) {
    column_entry_t *curr = list->column_member_list;
    while (curr != NULL) {
        printf("row: %d\n", curr->row);
        curr = curr->next;
    }
}

static guint column_list_get_next(col_list_t *list, guint row) {
    column_entry_t *curr = list->column_member_list;
    while (curr != NULL && curr->next != NULL) {
        if (curr->next->row > row) {
            return curr->next->row;
        }
        curr = curr->next;
    }
    return -1;
}

static void update_rows(tcb_t *tcb) {
    if (tcb->left_height >= tcb->right_height) {
        tcb->num_rows = tcb->left_height + 1;
    } else {
        tcb->num_rows = tcb->right_height + 1;
    }
}

static void get_function(tcb_t *tcb, guint col, guint row, dir_e direction, gchar **fname, gchar **file) {
    guint offset;
    col_list_t *column;
    column_entry_t *function_node;
    GList *children;
    GtkWidget *label;

    offset = direction == RIGHT ? -1 : 1;

    column = &(tcb->col_list[col + offset]);
    function_node = column_list_get_row(column, row);
    // children is a GList containing only the function label
    children = gtk_container_get_children((GtkContainer *)function_node->widget);
    label = (GtkWidget *) children->data; 
    *fname = (gchar *)gtk_label_get_text((GtkLabel *) label);
    *file = function_node->file;
}

static result_t *parse_results(search_results_t *results) {
    result_t *node, *next_node, *front; 
    guint i, num_removed;
    gchar *curr;
    gboolean first_space;
    gchar *result_ptr = results->start_ptr;

    front = next_node = (result_t *) g_malloc( sizeof(result_t) );
    while (result_ptr != results->end_ptr) {
        node = next_node;
        node->buf = (gchar *) malloc(1024);
        node->file_name = node->buf;
        curr = node->buf;
        first_space = TRUE;
        while(*result_ptr != '\n') {
            if (*result_ptr == '|') {
                *curr = '\0';        
                node->function_name = curr + 1;
            } else if (*result_ptr == ' ') {
                *curr = '\0';
                if (first_space) {
                    node->line_num = curr + 1;
                    first_space = FALSE;
                }
            } else {
                *curr = *result_ptr;
            }
            result_ptr++;
            curr++;
        }
        result_ptr++;
        if (result_ptr != results->end_ptr) {
            next_node = (result_t *) g_malloc( sizeof(result_t) );
            node->next = next_node;
        }
    }
    node->next = NULL;

    //num_removed = results_list_remove_duplicates(front);
    //results->match_count -= num_removed; 
    //results_list_sort(front);
    return front;
}

static guint results_list_remove_duplicates(result_t *front) {
    result_t *outer_curr, *inner_curr;
    guint count = 0;

    outer_curr = front;

    while(outer_curr != NULL && outer_curr->next != NULL) {
        inner_curr = outer_curr;
        while(inner_curr != NULL && inner_curr->next != NULL) {
            while (strcmp(inner_curr->next->function_name, outer_curr->function_name) == 0) {
                inner_curr->next = inner_curr->next->next;
                count++;
            }
            inner_curr = inner_curr->next;
        }
        outer_curr = outer_curr->next;
    }
    return count;
}

static guint results_list_filter_file(result_t **front_ptr, gchar *file) {
    guint count = 0;
    result_t *curr;
    if (*front_ptr == NULL) return;

    while (*front_ptr != NULL && strcmp((*front_ptr)->file_name, file) != 0) {
        *front_ptr = (*front_ptr)->next;
        count++;
    }
    curr = *front_ptr;
    while (curr != NULL && curr->next != NULL) {
        while (curr->next != NULL && strcmp(curr->next->file_name, file) != 0) {
            curr->next = curr->next->next;
            count++;
        }
        curr = curr->next;
    }
    return count;
}

static void results_list_sort(result_t *front) {
    result_t *smallest, *curr, *sorted;
    curr = smallest = front;
    sorted = front;
    while (sorted != NULL && sorted->next != NULL) {
        curr = smallest = sorted;
        while (curr->next != NULL) {
            if (strcmp(curr->function_name, smallest->function_name) < 0) {
                smallest = curr;
            }
            curr = curr->next;
        }
        swap(sorted, smallest);
        sorted = sorted->next;
    }
}

static void swap(result_t *a, result_t *b) {
    result_t swap;
    result_t *a_next, *b_next;
    a_next = a->next;
    b_next = b->next;
    swap = *a;
    *a = *b;
    a->next = a_next;
    *b = swap;
    b->next = b_next;
}

static void free_results(result_t *front) {
    result_t *curr, *next;
    curr = front;
    while (curr != NULL) {
        g_free(curr->buf);
        next = curr->next;
        g_free(curr);
        curr = next;
    }
}

static void make_sliders(tcb_t *tcb) {
    GtkWidget *hscale;            
    guint i;
    col_list_t *column;
    GtkWidget *function_box, *label;
    GList *children;

    for (i = 0; i < tcb->num_cols; i++) {
       column = &(tcb->col_list[i]); 
       if (column->type == NAME) {
           // delete the old slider if it exists
           if (column->slider != NULL)
           {
               gtk_widget_destroy(column->slider);
           }
           // create the new one
           hscale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(10, 4, 40, 1, 0, 0)));
           gtk_widget_set_name(hscale, "hscale");
           gtk_widget_show(hscale);
           gtk_table_attach(GTK_TABLE(tcb->browser_table), hscale, i, i + 1, tcb->num_rows, tcb->num_rows + 1,
                   (GtkAttachOptions)(GTK_FILL),
                   (GtkAttachOptions)(GTK_FILL), 0, 0);
           gtk_scale_set_draw_value(GTK_SCALE(hscale), FALSE);
           gtk_scale_set_digits(GTK_SCALE(hscale), 0);

           column->slider = hscale;

           // to set the width, need to grab the first label in the column
           function_box = column->column_member_list->widget;
           children = gtk_container_get_children((GtkContainer *) function_box);
           label = (GtkWidget *) children->data;
           g_signal_connect((gpointer)hscale, "change_value",
                            G_CALLBACK(on_column_hscale_change_value),
                            label);
       }
    }
}

static void clear_sliders(tcb_t *tcb) {
    guint i;
    col_list_t *column;

    for (i = 0; i < tcb->num_cols; i++)
    {
        column = &(tcb->col_list[i]); 
        if (column->slider != NULL)
        {
            gtk_widget_destroy(column->slider);
            column->slider = NULL;
        }
    }
}

static void add_connectors(tcb_t *tcb, guint col, guint row, guint count, dir_e direction)
{
    guint i, j;
    col_list_t *column;
    GtkWidget *connector;
    column_entry_t *next;
    const gchar *prev_name;
    gboolean is_last_function = FALSE;
    guint bound = col;
    guint offset = direction == RIGHT ? -2 : 2;


    if (direction == RIGHT)
    {
        // add straight connectors to every 
        // expander column before col
        for (i = tcb->root_col; i < bound; i++)
        {
            column = &(tcb->col_list[i]); 
            if (column->type == EXPANDER)
            {
                //check the next column (name) to see if adding connectors
                //is necessary (might be the last function in a block)
                if (!end_of_block(&(tcb->col_list[i + 1]), row))
                {
                    
                    for (j = row + 1; j  < row + count; j++) 
                    {
                        connector = make_straight_connector();
                        gtk_table_attach(GTK_TABLE(tcb->browser_table), connector, i, i + 1, j, j + 1,
                                (GtkAttachOptions)(GTK_FILL),
                                (GtkAttachOptions)(GTK_FILL), 0, 0);
                        column_list_insert(column, connector, j);
                    }
                }
            }
        }
    }        

    else 
    {
        // direction == left

        // add vertical connectors to every expander column before col
        for (i = tcb->root_col; i > bound; i--)
        {
            column = &(tcb->col_list[i]);
            if (column->type == EXPANDER)
            {
                if (!end_of_block(&(tcb->col_list[i - 1]), row))
                {
                    for (j = row + 1; j < row + count; j++)
                    {
                        connector = make_straight_connector();
                        gtk_table_attach(GTK_TABLE(tcb->browser_table), connector, i, i + 1, j, j + 1,
                                (GtkAttachOptions)(GTK_FILL),
                                (GtkAttachOptions)(GTK_FILL), 0, 0);
                        column_list_insert(column, connector, j);
                    }
                }
            }
        }

    }
    column = &(tcb->col_list[col]);
    for (i = row + 1; i < row + count - 1; i++) 
    {
        connector = make_three_way_connector(direction);
        gtk_table_attach(GTK_TABLE(tcb->browser_table), connector, col, col + 1, i, i + 1,
                (GtkAttachOptions)(GTK_FILL),
                (GtkAttachOptions)(GTK_FILL), 0, 0);
        column_list_insert(column, connector, i);
    }
    // only need to add an end connector if we are adding more than one function
    if (count > 1)
    {
        connector = make_end_connector(direction);
        gtk_table_attach(GTK_TABLE(tcb->browser_table), connector, col, col + 1, row + count - 1, row + count,
                (GtkAttachOptions)(GTK_FILL),
                (GtkAttachOptions)(GTK_FILL), 0, 0);
        column_list_insert(column, connector, row + count - 1);
    }

}

static GtkWidget *make_end_connector(dir_e direction)
{
    GtkWidget *eventbox;
    GtkWidget *image;

    eventbox = gtk_event_box_new();
    gtk_widget_set_name(eventbox, "end_connector");
    gtk_widget_show(eventbox);

    if (direction == LEFT)
    {
        image = create_pixmap(eventbox, "sca_end_branch_left.png");
    } else 
    {
        image = create_pixmap(eventbox, "sca_end_branch_right.png");
    }

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return eventbox;
}

static GtkWidget *make_three_way_connector(dir_e direction)
{
    GtkWidget *eventbox;
    GtkWidget *image;

    eventbox = gtk_event_box_new();
    gtk_widget_set_name(eventbox, "three_connector");
    gtk_widget_show(eventbox);

    if (direction == LEFT)
    {
        image = create_pixmap(eventbox, "sca_mid_branch_left.png");
    } else 
    {
        image = create_pixmap(eventbox, "sca_mid_branch_right.png");
    }

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return eventbox;
}

static GtkWidget *make_straight_connector()
{
    GtkWidget *eventbox;
    GtkWidget *image;

    eventbox = gtk_event_box_new();
    gtk_widget_set_name(eventbox, "stragiht_connector");
    gtk_widget_show(eventbox);
    image = create_pixmap(eventbox, "sca_mid_branch_center.png");

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return eventbox;
}

static gboolean end_of_block(col_list_t *column, guint row)
{
    column_entry_t *curr;
    
    if (column->column_member_list->row > row) 
    {
        return TRUE;
    }
    curr = column->column_member_list;
    while (curr != NULL)
    {
        if (curr->next == NULL || curr->next->row > row)
        {
            if (curr->last_entry)
            {
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
        curr = curr->next;
    }
    return FALSE;
}
