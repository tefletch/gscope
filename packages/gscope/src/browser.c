#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <string.h>

#include "support.h"
#include "utils.h"


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


typedef enum
{
    FILLER,
    EXPANDER,
    NAME,
    NUM_TYPES,
} column_type_e;


typedef struct entry
{
    GtkWidget       *widget;
    guint           row;
    struct entry    *next;
    struct entry    *prev;
} column_entry_t;


typedef struct
{
    GtkWidget       *header_column_label;
    guint           header_column_num;
    column_entry_t  *column_member_list;
    column_entry_t  *back;
    column_type_e   type;
    guint           size;
} col_list_t;


typedef struct
{
    GtkWidget   *browser_table;
    guint       root_col;       // Table widget column currently containing the root/origin [0] column
    guint       num_cols;       // Total number of columns in the table
    guint       num_rows;       // Total number of rows in the table.
    guint       left_height;    // Current size of "tallest/first" left column (1st left column is labelled -1)
    guint       right_height;   // Current size of "tallest/first" right column (1st right column is labelled 1)
    col_list_t  col_list[NUM_COL_MAX];
}   tcb_t;

static int FUNCTION_NUM = 1;


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
static GtkWidget   *create_browser_window(gchar *name);
static GtkWidget   *make_collapser(guint child_count);
static GtkWidget   *make_expander (dir_e direction, gboolean new_expander);
static void        expand_table(guint origin_y, guint origin_x, tcb_t *tcb, dir_e direction, guint row_add_count);
static void        collapse_table(guint origin_row, guint origin_col, guint rows_removed, tcb_t *tcb, dir_e direction);
static void        move_table_widget(GtkWidget *widget, tcb_t *tcb, guint row, guint col);

static gboolean    on_browser_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static gboolean    on_column_hscale_change_value(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data);
static gboolean    on_left_expander_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_right_expander_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_left_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_right_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

static void         ensure_column_capacity(guint origin_col, tcb_t *tcb, dir_e direction); 
static void         ensure_row_capacity(guint origin_row, tcb_t *t, guint row_add_count); 
static void         make_name_column(tcb_t *tcb, guint col, dir_e direction); 
static void         make_expander_column(tcb_t *tcb, guint position, dir_e direction); 
static void         make_filler_column(tcb_t *tcb, guint position); 
static void         add_functions_to_column(tcb_t *tcb, column_entry_t function_list,
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

static void         column_list_insert(col_list_t *list, GtkWidget *widget, guint row);
static void         column_list_remove(col_list_t *list, GtkWidget *widget);
static gboolean     column_list_is_sorted(col_list_t *list);
static column_entry_t  *column_list_get_row(col_list_t *list, guint row); 
static void         column_list_print_rows(col_list_t *list); 
static guint        column_list_get_next(col_list_t *list, guint row); 


// Local Global Variables
//=======================

static GtkWidget    *test_widget;

//
// Private Functions
// ========================================================
//
static GtkWidget   *create_browser_window(gchar *name)
{
    GtkWidget *browser_window;
    GdkPixbuf *browser_window_icon_pixbuf;
    GtkWidget *browser_vbox;
    GtkWidget *browser_scrolledwindow;
    GtkWidget *browser_viewport;
    GtkWidget *browser_table;
    GtkWidget *left_horizontal_filler_header_button;
    GtkWidget *right_horizontal_filler_header_button;
    GtkWidget *left_expander_eventbox;
    GtkWidget *right_expander_eventbox;
    GtkWidget *header_button;
    GtkWidget *vertical_filler_label;
    GtkWidget *root_function_blue_eventbox;
    GtkWidget *root_hscale;
    GtkWidget *browser_bottom_hbox;
    GtkWidget *root_function_entry_label;
    GtkWidget *root_function_entry;
    GtkWidget *root_function_label;
    gchar     *var_string;
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

    browser_table = gtk_table_new(2, 5, FALSE);
    gtk_widget_set_name(browser_table, "browser_table");
    gtk_widget_show(browser_table);
    gtk_container_add(GTK_CONTAINER(browser_viewport), browser_table);

    // Initialize the TCB
    tcb->browser_table = browser_table;
    tcb->root_col      = 2;     // Center column of a zero-based 5 column initial table
    tcb->num_cols      = 5;
    tcb->num_rows      = 2;
    tcb->left_height   = 1;
    tcb->right_height  = 1;

    //memset(tcb->col_list, 0, sizeof(tcb->col_list));

    // Configure the initial columns
    {
        tcb->col_list[0].type = FILLER;         // First and last 'even' columns are FILLER columns
        tcb->col_list[1].type = EXPANDER;       // All 'odd' columns are EXPANDER colums
        tcb->col_list[2].type = NAME;           // All non-first, non-last 'even' columns are NAME columns
        tcb->col_list[3].type = EXPANDER;
        tcb->col_list[4].type = FILLER;
    }

    /* Left Expander */
    make_expander_at_position(tcb, 1, 1, LEFT);

    /* Right Expander */
    make_expander_at_position(tcb, 3, 1, RIGHT);

    header_button = gtk_button_new_with_mnemonic("0");
    gtk_widget_set_name(header_button, "header_button");
    gtk_widget_show(header_button);
    gtk_table_attach(GTK_TABLE(browser_table), header_button, 1, 4, 0, 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_widget_set_can_focus(header_button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(header_button), FALSE);

    root_function_blue_eventbox = gtk_event_box_new();
    gtk_widget_set_name(root_function_blue_eventbox, "blue_eventbox");
    gtk_widget_show(root_function_blue_eventbox);
    gtk_table_attach(GTK_TABLE(browser_table), root_function_blue_eventbox, 2, 3, 1, 2,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);


    /*
    root_hscale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(10, 4, 40, 1, 0, 0)));
    gtk_widget_set_name(root_hscale, "root_hscale");
    gtk_widget_show(root_hscale);
    gtk_table_attach(GTK_TABLE(browser_table), root_hscale, 2, 3, 3, 4,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);
    gtk_scale_set_draw_value(GTK_SCALE(root_hscale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(root_hscale), 0);
    */

    {   // Initialize the table tracking structure

        // Initialize the left filler column
        make_filler_column(tcb, 0);
        {   // Initialize the root column entries
            col_list_t *list = &(tcb->col_list[2]);
            list->header_column_label = header_button;
            column_list_insert(list, root_function_blue_eventbox, 1);
            //column_list_insert(list, vertical_filler_label, 2);
            //column_list_insert(list, root_hscale, 3);
        }

        // Initialize the right filler column
        make_filler_column(tcb, 4);
    }

    my_asprintf(&var_string, "<span color=\"darkred\">%s</span>", name);
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

    browser_bottom_hbox = gtk_hbox_new(FALSE, 0);
    gtk_widget_set_name(browser_bottom_hbox, "browser_bottom_hbox");
    gtk_widget_show(browser_bottom_hbox);
    gtk_box_pack_start(GTK_BOX(browser_vbox), browser_bottom_hbox, FALSE, TRUE, 0);

    root_function_entry_label = gtk_label_new("<span color=\"steelblue\" weight=\"bold\">Root Function</span>");
    gtk_widget_set_name(root_function_entry_label, "root_function_entry_label");
    gtk_widget_show(root_function_entry_label);
    gtk_box_pack_start(GTK_BOX(browser_bottom_hbox), root_function_entry_label, FALSE, FALSE, 0);
    gtk_label_set_use_markup(GTK_LABEL(root_function_entry_label), TRUE);
    gtk_misc_set_padding(GTK_MISC(root_function_entry_label), 4, 8);

    root_function_entry = gtk_entry_new();
    gtk_widget_set_name(root_function_entry, "root_function_entry");
    gtk_widget_show(root_function_entry);
    gtk_box_pack_start(GTK_BOX(browser_bottom_hbox), root_function_entry, TRUE, TRUE, 0);
    gtk_entry_set_invisible_char(GTK_ENTRY(root_function_entry), 8226);
    
    g_signal_connect((gpointer) browser_window, "delete_event",
                    G_CALLBACK (on_browser_delete_event),
                    tcb);
 
    /*
    g_signal_connect((gpointer)root_hscale, "change_value",
                    G_CALLBACK(on_column_hscale_change_value),
                    root_function_label);
    */
    return browser_window;
}




//
// Public Functions
// ========================================================
//

GtkWidget* BROWSER_init(gchar *name)
{
    return( create_browser_window(name) );
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

    // child_count = search_children_count()
    child_count = 3;
    // ^^^^^^^^^^^^^ test value

    collapser = make_collapser(child_count);
    column_list_insert(list, collapser, row);

    // Revisit: need to figure out table position from "widget" before destroying it.  Cheating for now...
    gtk_table_attach(GTK_TABLE(tcb->browser_table), collapser, col, col + 1, row, row + 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    g_signal_connect((gpointer)collapser, "button_release_event",
                     G_CALLBACK(on_left_collapser_button_release_event),
                     user_data);

    expand_table(row, col, tcb, LEFT, child_count - 1);

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

    // child_count = search_children_count()
    child_count = 3;
    // ^^^^^^^^^^^^^ test value

    collapser = make_collapser(child_count);
    column_list_insert(list, collapser, row);

    // Revisit: need to figure out table position from "widget" before destroying it.  Cheating for now...
    gtk_table_attach(GTK_TABLE(tcb->browser_table), collapser, col, col + 1, row, row + 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);


    g_signal_connect((gpointer)collapser, "button_release_event",
                     G_CALLBACK(on_right_collapser_button_release_event),
                     user_data);

    expand_table(row, col, tcb, RIGHT, child_count - 1);

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

    child_count = 3; // TEST VALUE WILL HAVE TO GET ACTUAL CHILD COUNT LATER

    make_expander_at_position(tcb, col, row, LEFT);

    collapse_table(row, col, child_count - 1, tcb, LEFT);
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

    child_count = 3;

    make_expander_at_position(tcb, col, row, RIGHT);

    collapse_table(row, col, child_count - 1, tcb, RIGHT);
    return FALSE;
}

static void move_table_widget(GtkWidget *widget, tcb_t *tcb, guint row, guint col)
{
    g_object_ref(widget);  // Create a temporary reference to prevent the widget from being destroyed.
    //printf("row: %d  col: %d\n", row, col);


    gtk_container_remove(GTK_CONTAINER(tcb->browser_table), widget);

    gtk_table_attach(GTK_TABLE(tcb->browser_table), widget, col, col + 1, row, row + 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    g_object_unref((gpointer)widget);  // Create a temporary reference to prevent the widget from being destroyed.
    //g_object_ref(test_widget);  // Drop the temporary reference.

}

/*
    This function implements a table expansion associated with an "expander"-clicked event.
    Overall functionality includes:
       1) Add new rows and/or new columns to the table, as required [resize table widget]
       2) Relocate existing widgets that need to be moved in order to accommodate the expansion.
       3) Add any new widgets: Icons and function names "exposed" by the expansion operation.
*/
static void expand_table(guint origin_row, guint origin_col, tcb_t *tcb, dir_e direction, guint row_add_count)
{
    int i, add_pos;
    column_entry_t hi;
    col_list_t *list;

    // first check if we need to grow the table
    ensure_row_capacity(origin_row, tcb, row_add_count);
    ensure_column_capacity(origin_col, tcb, direction);
    tcb->num_rows += row_add_count;
    
    if (direction == RIGHT) {
        for (i = tcb->root_col; i < tcb->num_cols; i++) {
            list = &(tcb->col_list[i]);
            if (!column_list_is_sorted(list)) {
                printf("ERROR COLUMN %d OUT OF ORDER:\n\n", i);
            }
            shift_column_down(tcb, i, origin_row, row_add_count);
        }
        add_functions_to_column(tcb, hi, origin_col + 1, origin_row, RIGHT);
    } else {
        add_pos = origin_col == 1 ? 2: origin_col - 1;
        for (i = tcb->root_col; i > 0; i--) {
            list = &(tcb->col_list[i]);
            if (!column_list_is_sorted(list)) {
                printf("ERROR COLUMN %d OUT OF ORDER:\n\n", i);
            }
            shift_column_down(tcb, i, origin_row, row_add_count);
        }
        add_functions_to_column(tcb, hi, add_pos, origin_row, LEFT);
    }
}

static void ensure_column_capacity(guint origin_col, tcb_t *tcb, dir_e direction) {
    if (direction == RIGHT) {
        if (origin_col == tcb->num_cols - 2) {  // -2 because of filler column and zero based indexing
           // name column and another expander column
           // TODO we conditionally may not need to add the second column for expanders
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

static void shift_table_right(tcb_t *tcb, guint start_col) {
    int i;
    gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols + 2);
    for (i = tcb->num_cols - 1; i >= start_col; i--) {
        move_column(tcb, i, i + 2);
    }
    make_name_column(tcb, 2, LEFT);
    tcb->num_cols += 2;
    tcb->root_col += 2;
}

static void move_column(tcb_t *tcb, guint source, guint dest) {
    col_list_t *column, *dest_col;
    column_entry_t *curr, *next;
    dir_e direction;
    GtkWidget *header_button, *widget;
    guint row;
    
    dest_col = &(tcb->col_list[dest]);
    column = &(tcb->col_list[source]);

    delete_column(tcb, dest);

    if (column->type == NAME) {
        if (source == tcb->root_col) {
            // the root column is a speical case since it has
            // a wider header
            //gtk_widget_destroy(tcb->col_list[source].header_column_label);
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
            make_name_column(tcb, dest, direction);
        }
    }
    if (column->type == FILLER) {
        //gtk_widget_destroy(column->header_column_label);
        make_filler_column(tcb, dest);
    }
    if (column->type == EXPANDER) {
        tcb->col_list[dest].type = EXPANDER;
    }
    curr = column->column_member_list;
    while (curr != NULL) {
        widget = curr->widget;
        next = curr->next;
        row = curr->row;
        column_list_remove(column, widget);  // this frees the curr node
        column_list_insert(dest_col, widget, row); 
        move_table_widget(curr->widget, tcb, curr->row, dest);
        curr = next;
    }

    delete_column(tcb, source);
}


static void ensure_row_capacity(guint origin_row, tcb_t *tcb, guint row_add_count) {
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
static void collapse_table(guint origin_row, guint origin_col, guint rows_removed, tcb_t *tcb, dir_e direction)
{
    //first thing that needs to be done is to recursivly collapse all children
    //to do this we need to know the position of the next element in this column
    //to put a lower bound on what we collpase (don't want to collapse children
    //of other funcitons)

    col_list_t *list = &(tcb->col_list[origin_col]);
    guint lower_bound = column_list_get_next(list, origin_row);
    // -1 means there is no lower bound
    if (lower_bound == -1) {
        lower_bound = tcb->num_rows;
    }
    
    //delete all children of the collapser and shift up the table
    //the appropriate amount
    delete_children(tcb, origin_col, origin_row, lower_bound, direction);

    //check if the table needs to be downsized, if it does it will always
    //be a multiple of 2 rows for name and collapser columns
    //
    remove_unused_columns(tcb, direction);
}

static void remove_unused_columns(tcb_t *tcb, dir_e direction) {
   int i;
   int count = 0;
   col_list_t *column;

   if (direction == RIGHT) {
       for (i = tcb->num_cols - 3; i > tcb->root_col; i -= 2) {    // num_cols - 3 is the furthest right name_column 
           column = &(tcb->col_list[i]);
           if (column->column_member_list == NULL) {
               // destory the column header
               gtk_widget_destroy(tcb->col_list[i].header_column_label);
               // move the filler column
               gtk_widget_destroy(tcb->col_list[i + 2].header_column_label);
               make_filler_column(tcb, i);

               // update the table size
               gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols - 2);
               tcb->num_cols -= 2;
           }
       }
    } else {
        // direction is left
       for (i = 1; i < tcb->root_col; i ++) {
           col_list_t *column = &(tcb->col_list[i]);
           if (column->column_member_list == NULL) {
               count++;
           }
       }
       if (count > 0) {
           shift_table_left(tcb, count);
           for (i = tcb->num_cols - count; i < tcb->num_cols; i++) {
               // delete the column
               delete_column(tcb, i);
           }
           gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols - count);
           tcb->num_cols -= count; 
           tcb->root_col -= count;
       }
    }
}

static void shift_table_left(tcb_t *tcb, guint amount) {
    int i;

    for (i = amount + 1; i < tcb->num_cols; i++) {
        move_column(tcb, i, i - amount);
    }
}

static void delete_column(tcb_t *tcb, guint col) {
    column_entry_t *curr, *next;
    col_list_t *column = &(tcb->col_list[col]);

    if (column->header_column_label != NULL) {
        gtk_widget_destroy(column->header_column_label);
    }
    curr = column->column_member_list;
    while (curr != NULL) {
        if (curr->widget == NULL) {
            printf("hmmmm\n");
        }
        gtk_widget_destroy(curr->widget);
        next = curr->next;
        g_free(curr);
        curr = next;
    }
    memset(column, 0, sizeof(col_list_t));

}

static void shift_table_up(tcb_t *tcb, guint upper_bound, guint amount, dir_e direction) {
    int i;
    col_list_t *list;
    guint left, right;

    left = direction == RIGHT ? tcb->root_col : 1;
    right = direction == RIGHT ? tcb->num_cols : tcb->root_col;

    for (i = left; i < right; i++) {
        list = &(tcb->col_list[i]);
        shift_column_up(tcb, i, upper_bound, amount); 
        if (!column_list_is_sorted(list)) {
            printf("ERROR COLUMN %d OUT OF ORDER:\n\n", i);
        }
    }
    tcb->num_rows -= amount;
}

static void delete_children(tcb_t *tcb, guint col, guint upper_bound, guint lower_bound, dir_e direction) {
    int i, num_removed, cols_removed;
    cols_removed = num_removed = 0;

    if (direction == RIGHT) {
        for (i = tcb->num_cols - 2; i > col; i--) {
            num_removed += delete_functions_from_column(tcb, i, upper_bound, lower_bound, RIGHT);        
            if (num_removed > 0) {
                cols_removed++;
            }
        }
    } else {
        // direction == left
        for (i = 2; i < col; i++) {
            num_removed += delete_functions_from_column(tcb, i, upper_bound, lower_bound, LEFT);
            if (num_removed > 0) {
                cols_removed++;
            }
        }
    }
    shift_table_up(tcb, upper_bound, lower_bound - upper_bound - 1, direction);
}

static int delete_functions_from_column(tcb_t *tcb, guint col, guint starting_row, guint end_row, dir_e direction) {
    int offset = direction == RIGHT ? 1 : -1;

    col_list_t *function_list = &(tcb->col_list[col]);
    col_list_t *expander_list = &(tcb->col_list[col + offset]);
    
    guint num_removed = 0;
    guint i;
    column_entry_t *function, *expander;
    for (i = starting_row; i < end_row; i++) {
        function = column_list_get_row(function_list, i);
        expander = column_list_get_row(expander_list, i);

        if (function != NULL) {
            num_removed++;
            gtk_widget_destroy(function->widget);
            column_list_remove(function_list, function->widget);
        }
        if (expander != NULL) {
            gtk_widget_destroy(expander->widget);
            column_list_remove(expander_list, expander->widget);
        }
    }
    return num_removed;
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


static void add_functions_to_column(tcb_t *tcb, column_entry_t function_list,
                                    guint col, guint starting_row, dir_e direction) {
    int row, offset;
    col_list_t *list;
    char *var_string, function_name[16];
    GtkWidget *function_label;
    GtkWidget *function_event_box;

    offset = direction == RIGHT ? 1 : -1;
    
    for ( row = starting_row; row < starting_row + 3; row++) {
        function_event_box = gtk_event_box_new();
        gtk_widget_set_name(function_event_box, "event_box");
        gtk_widget_show(function_event_box);
        gtk_table_attach(GTK_TABLE(tcb->browser_table), function_event_box, col, col + 1, row, row + 1,
                         (GtkAttachOptions)(GTK_FILL),
                         (GtkAttachOptions)(GTK_FILL), 0, 0);
        sprintf(function_name, "function %d", FUNCTION_NUM++);
        my_asprintf(&var_string, "<span color=\"darkred\">%s</span>", function_name);
        function_label = gtk_label_new(var_string);
        g_free(var_string);
        gtk_widget_set_name(function_label, "root_function_label");
        gtk_widget_show(function_label);
        gtk_container_add(GTK_CONTAINER(function_event_box), function_label);
        gtk_label_set_use_markup(GTK_LABEL(function_label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(function_label), 0, 0.5);
        gtk_misc_set_padding(GTK_MISC(function_label), 1, 0);
        gtk_label_set_ellipsize (GTK_LABEL (function_label), PANGO_ELLIPSIZE_END);
        gtk_label_set_width_chars(GTK_LABEL(function_label), strlen(function_name) );

        list = &(tcb->col_list[col]);
        column_list_insert(list, function_event_box, row);

        make_expander_at_position(tcb, col + offset, row, direction);
    }
        
}

// TODO make this funciton work
// currently just fills in a single dummy function as a Test value
// needs to get results for either functions calling or called by
// probably will accept list of functions as argument 
static void make_name_column(tcb_t *tcb, guint col, dir_e direction) {
        GtkWidget *vertical_filler_label;
        GtkWidget *header_button;
        col_list_t *column = &(tcb->col_list[col]);
        int left_bound = direction == RIGHT ? col : col - 1;
        int right_bound = direction == RIGHT ? col + 2 : col + 1;

        /*
        vertical_filler_label = gtk_label_new("");
        gtk_widget_set_name(vertical_filler_label, "vertical_filler_label");
        gtk_widget_show(vertical_filler_label);
        gtk_table_attach(GTK_TABLE(tcb->browser_table), vertical_filler_label, col, col + 1, 2, 3,
                         (GtkAttachOptions)(GTK_FILL),
                         (GtkAttachOptions)(GTK_FILL), 0, 0);
                         */
        
        header_button  = gtk_button_new_with_mnemonic("7");
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

// sorted insert into column list, column list should also be in sorted order
static void column_list_insert(col_list_t *list, GtkWidget *widget, guint row) {
    // create the new node to add
    column_entry_t *new_node = g_malloc( sizeof(column_entry_t) );
    new_node->widget = widget;
    new_node->row = row;
    new_node->next = NULL;
    new_node->prev = NULL;

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
    list->size++;
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
        }
        list->size--;
    } else {
        // find the widget to remove
        column_entry_t *curr = list->column_member_list;
        while (curr->next != NULL && curr->next->widget != widget) {
            curr = curr->next;
        }
        if (curr->next != NULL) {
            column_entry_t *temp = curr->next;
            curr->next = curr->next->next;
            if (curr->next == NULL) {
                list->back = curr;
            } else {
                curr->next->prev = curr;
            }
            list->size--;
            g_free(temp);
        }
    }
    list->size--;
}

static column_entry_t  *column_list_get_row(col_list_t *list, guint row) {
    column_entry_t *curr = list->column_member_list;
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
        if (curr->row == row) {
            return curr->next->row;
        }
        curr = curr->next;
    }
    return -1;
}
