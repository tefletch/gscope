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
} column_entry_t;


typedef struct
{
    GtkWidget       *header_column_label;
    guint           header_column_num;
    column_entry_t  *column_member_list;
    column_type_e   type;
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


// Private Function Prototypes
//============================
static GtkWidget   *create_browser_window(gchar *name);
static GtkWidget   *make_collapser(guint child_count);
static GtkWidget   *make_expander (dir_e direction, gboolean new_expander);
static void        expand_table(guint origin_y, guint origin_x, tcb_t *tcb, dir_e direction, guint row_add_count);
static void        move_table_widget(GtkWidget *widget, tcb_t *tcb, guint row, guint col);

static gboolean    on_browser_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static gboolean    on_column_hscale_change_value(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data);
static gboolean    on_left_expander_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_right_expander_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_left_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean    on_right_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data);


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

    // Malloc a Table Control Block (TCB) for this window.  This keeps the table data with the widget and
    // makes this function re-entrant-safe.
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

    browser_table = gtk_table_new(4, 5, FALSE);
    gtk_widget_set_name(browser_table, "browser_table");
    gtk_widget_show(browser_table);
    gtk_container_add(GTK_CONTAINER(browser_viewport), browser_table);

    // Initialize the TCB
    tcb->browser_table = browser_table;
    tcb->root_col      = 2;     // Center column of a zero-based 5 column initial table
    tcb->num_cols      = 5;
    tcb->num_rows      = 4;
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

    left_horizontal_filler_header_button = gtk_button_new_with_mnemonic("");
    gtk_widget_set_name(left_horizontal_filler_header_button, "left_horizontal_filler_header_button");
    gtk_widget_show(left_horizontal_filler_header_button);
    gtk_table_attach(GTK_TABLE(browser_table), left_horizontal_filler_header_button, 0, 1, 0, 1,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_widget_set_can_focus(left_horizontal_filler_header_button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(left_horizontal_filler_header_button), FALSE);

    right_horizontal_filler_header_button = gtk_button_new_with_mnemonic("");
    gtk_widget_set_name(right_horizontal_filler_header_button, "right_horizontal_filler_header_button");
    gtk_widget_show(right_horizontal_filler_header_button);
    gtk_table_attach(GTK_TABLE(browser_table), right_horizontal_filler_header_button, 4, 5, 0, 1,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_widget_set_can_focus(right_horizontal_filler_header_button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(right_horizontal_filler_header_button), FALSE);

    /* Left Expander */
    left_expander_eventbox = make_expander(LEFT, TRUE);
    gtk_widget_set_name(left_expander_eventbox, "expander_eventbox");
    gtk_widget_show(left_expander_eventbox);
    gtk_table_attach(GTK_TABLE(browser_table), left_expander_eventbox, 1, 2, 1, 2,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    /* Right Expander */
    right_expander_eventbox = make_expander(RIGHT, TRUE);
    gtk_widget_set_name(right_expander_eventbox, "expander_eventbox");
    gtk_widget_show(right_expander_eventbox);
    gtk_table_attach(GTK_TABLE(browser_table), right_expander_eventbox, 3, 4, 1, 2,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    header_button = gtk_button_new_with_mnemonic("0");
    gtk_widget_set_name(header_button, "header_button");
    gtk_widget_show(header_button);
    gtk_table_attach(GTK_TABLE(browser_table), header_button, 1, 4, 0, 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    gtk_widget_set_can_focus(header_button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(header_button), FALSE);

    vertical_filler_label = gtk_label_new("");
    gtk_widget_set_name(vertical_filler_label, "vertical_filler_label");
    gtk_widget_show(vertical_filler_label);
    gtk_table_attach(GTK_TABLE(browser_table), vertical_filler_label, 2, 3, 2, 3,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_EXPAND), 0, 0);

    root_function_blue_eventbox = gtk_event_box_new();
    gtk_widget_set_name(root_function_blue_eventbox, "blue_eventbox");
    gtk_widget_show(root_function_blue_eventbox);
    gtk_table_attach(GTK_TABLE(browser_table), root_function_blue_eventbox, 2, 3, 1, 2,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    root_hscale = gtk_hscale_new(GTK_ADJUSTMENT(gtk_adjustment_new(10, 4, 40, 1, 0, 0)));
    gtk_widget_set_name(root_hscale, "root_hscale");
    gtk_widget_show(root_hscale);
    gtk_table_attach(GTK_TABLE(browser_table), root_hscale, 2, 3, 3, 4,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);
    gtk_scale_set_draw_value(GTK_SCALE(root_hscale), FALSE);
    gtk_scale_set_digits(GTK_SCALE(root_hscale), 0);


    {   // Initialize the table tracking structure

        // Initialize the left filler column
        tcb->col_list[0].header_column_label = left_horizontal_filler_header_button;
        tcb->col_list[0].header_column_num = 0;     // This "northwest" filler column never moves, but others will.
        tcb->col_list[0].column_member_list = NULL; // FILLER columns have no children (ever).

        // Initialize the root column [three physical columns wide)
        tcb->col_list[1].header_column_label = header_button;   // The root header label spans 3 columns (1,2 & 3)
        tcb->col_list[0].header_column_label = left_horizontal_filler_header_button;
        tcb->col_list[0].header_column_num = 0;     // This "northwest" filler column never moves, but others will.
        tcb->col_list[0].column_member_list = NULL; // FILLER columns have no children (ever).
        tcb->col_list[1].header_column_num = 1;
        {   // Initialize the root column entries
            tcb->col_list[1].column_member_list = g_malloc( sizeof(column_entry_t) );
            tcb->col_list[1].column_member_list->widget = left_expander_eventbox;
            tcb->col_list[1].column_member_list->row = 1;
            tcb->col_list[1].column_member_list->next = NULL;

            tcb->col_list[2].column_member_list = g_malloc( sizeof(column_entry_t) );
            tcb->col_list[2].column_member_list->widget = root_function_blue_eventbox;
            tcb->col_list[2].column_member_list->row = 1;
            tcb->col_list[2].column_member_list->next = g_malloc( sizeof(column_entry_t) );

            tcb->col_list[2].column_member_list->next->widget = vertical_filler_label;
            tcb->col_list[2].column_member_list->next->row = 2;
            tcb->col_list[2].column_member_list->next->next = g_malloc( sizeof(column_entry_t) );

            tcb->col_list[2].column_member_list->next->next->widget = root_hscale;
            tcb->col_list[2].column_member_list->next->next->row = 3;
            tcb->col_list[2].column_member_list->next->next->next = NULL;

            tcb->col_list[3].column_member_list = g_malloc( sizeof(column_entry_t) );
            tcb->col_list[3].column_member_list->widget = right_expander_eventbox;
            tcb->col_list[3].column_member_list->row = 1;
            tcb->col_list[3].column_member_list->next = NULL;
        }

        // Initialize the right filler column
        tcb->col_list[4].header_column_label = right_horizontal_filler_header_button;
        tcb->col_list[4].header_column_num = 4;
        tcb->col_list[4].column_member_list = NULL; // FILLER columns have no children (ever).
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
    g_signal_connect((gpointer)left_expander_eventbox, "button_release_event",
                     G_CALLBACK(on_left_expander_button_release_event),
                     tcb);  // Revisit: Pass "table" (and possibly table position)
    g_signal_connect((gpointer)right_expander_eventbox, "button_release_event",
                     G_CALLBACK(on_right_expander_button_release_event),
                     tcb);
    g_signal_connect((gpointer)root_hscale, "change_value",
                     G_CALLBACK(on_column_hscale_change_value),
                     root_function_label);

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

    // Get the widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
                            "top-attach", &row,
                            "left-attach", &col,
                            NULL);

    // Destroy the selected expander
    gtk_widget_destroy(widget);

    // child_count = search_children_count()
    child_count = 1;
    // ^^^^^^^^^^^^^ test value

    collapser = make_collapser(child_count);

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

    // Get the widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
                            "top-attach", &row,
                            "left-attach", &col,
                            NULL);

    // Destroy the selected expander
    gtk_widget_destroy(widget);

    // child_count = search_children_count()
    child_count = 3;
    // ^^^^^^^^^^^^^ test value

    collapser = make_collapser(child_count);

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

    // Revisit: Get the widget's position in the table

    // Destroy the selected expander
    gtk_widget_destroy(widget);

    expander = make_expander(LEFT, FALSE);

    // Revisit: need to figure out table position from "widget" before destroying it.  Cheating for now...
    gtk_table_attach(GTK_TABLE(tcb->browser_table), expander, 1, 2, 1, 2,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    g_signal_connect((gpointer)expander, "button_release_event",
                     G_CALLBACK(on_left_expander_button_release_event),
                     user_data);  // Revisit: Pass "table" (and possibly table position)

    return FALSE;
}



static gboolean on_right_collapser_button_release_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    GtkWidget   *expander;
    tcb_t       *tcb = user_data;

    // Revisit: Get the widget's position in the table

    // Destroy the selected expander
    gtk_widget_destroy(widget);

    expander = make_expander(RIGHT, FALSE);

    // Revisit: need to figure out table position from "widget" before destroying it.  Cheating for now...
    gtk_table_attach(GTK_TABLE(tcb->browser_table), expander, 3, 4, 1, 2,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    g_signal_connect((gpointer)expander, "button_release_event",
                     G_CALLBACK(on_right_expander_button_release_event),
                     user_data);  // Revisit: Pass "table" (and possibly table position)

    return FALSE;
}

#if 0
void shift_cells_down(guint x, guint y, tcb_t tcb, dir_e direction, guint add_count)
{
    guint col_span;
    guint col, row;
    gboolean expander_col = TRUE;

    if ( direction == LEFT )
    {

    }
    else    // direction == RIGHT
    {
        col_span = (x - tcb->root_col);

        for (col = x; col > tcb->root_col; col--)
        {
            for (row = y + tcb->right_height + 2; row < y; row--)   // initializer is +2 to cover the vertical expander label row and slider/scale row.

                GTK_WIDGET(iter->data)
            {
                if (row < y + add_count)    // We're in the new-content region (add new stuff)
                {
                    if ( expander_col )
                    {
                        // Create new Descenders/Expander
                    }
                    else
                    {
                        // Create new event_box(label)
                    }
                }
                else    // We're in the existing-content region (move existing stuff)
                {
                    if ( expander_col )
                    {
                        // Relocate existing Expanders (move widget from [x - add_count] to [x]
                    }
                    else
                    {
                        // Relocate existing event_box(label)s
                    }
                }
            }

            expander_col = !expander_col;
        }

    }
}


void add_rows(guint x, guint y, tcb_t tcb, dir_e direction, guint row_add_count);
{
    guint free_row_count;

    if ( direction == LEFT )
    {
        tcb->left_height += row_add_count;
        if ( tcb->left_height > tcb->right_height )
        {
            // grow the table widget by (row_add_count) rows.
            grow_table_widget(tcb, LEFT, row_add_count);
        }
        else
        {
            // If the number of rows to be added exceeds the height of the "other side",
            // grow the table widget by (n, n < row_add_count) rows.
            free_row_count = tcb->right_height - tcb->left_height;
            if ( free_row_count < row_add_count )
            {
                // grow the table by (row_add_count - free_row_count)
                grow_table_widget(tcb, LEFT, row_add_count - free_row_count);
            }
        }
    }
    else    // direction == RIGHT
    {
        tcb-right_height += row_add_count;
        if ( tcb->right_height > tcb->left_height )
        {
            // grow the table widget by (row_add_count) rows.
            grow_table_widget(tcb, RIGHT, row_add_count);
        }
        else
        {
            // If the number of rows to be added exceeds the height of the "other side",
            // grow the table widget by (n, n < row_add_count) rows.
            free_row_count = tcb->left_height - tcb->right_height;
            if ( free_row_count < row_add_count )
            {
                // grow the table by (row_add_count - free_row_count)
                grow_table_widget(tcb, RIGHT, row_add_count - free_row_count);
            }
        }
    }

    // Now that we have enough room in the widget, shift the affected table entries.
    shift_cells_down(x, y, tcb, direction, row_add_count);
}
#endif


static void move_table_widget(GtkWidget *widget, tcb_t *tcb, guint row, guint col)
{
    gboolean resize = FALSE;

    if ( row > (tcb->num_rows - 1) )
    {
        resize = TRUE;
        tcb->num_rows = row;
    }

    if ( col > (tcb->num_cols - 1) )
    {
        resize = TRUE;
        tcb->num_cols = col;
    }

    if ( resize )
    {
        gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols);      // Needed for GTK < 3.4
        printf("Table resized to %d rows by %d cols\n", tcb->num_rows, tcb->num_cols);
    }

    g_object_ref(widget);  // Create a temporary reference to prevent the widget from being destroyed.

    gtk_container_remove(GTK_CONTAINER(tcb->browser_table), widget);

    gtk_table_attach(GTK_TABLE(tcb->browser_table), test_widget, col, col + 1, row, row + 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    g_object_ref(test_widget);  // Drop the temporary reference.

}



col_list_t **load_widget_grid(GtkWidget *table, guint rows, guint cols)
{
    GList       *children, *iter;
    guint       row, col;

    children = gtk_container_get_children(GTK_CONTAINER(table));

    for (iter = children; iter != NULL; iter = g_list_next(iter))
    {
        gtk_container_child_get(GTK_CONTAINER(table), GTK_WIDGET(iter->data),
                                "top-attach", &row,
                                "left-attach", &col,
                                NULL);

        printf("row=%d col=%d\n", row, col);
        if (row == 0 && col == 4)
        {
            test_widget = iter->data;
        }
        //grid_ptr[row][col] = iter->data;
    }

    g_list_free(children);

    return( (col_list_t **) NULL);
}

/* Add and populate new rows and columns as required */
void expand_table(guint origin_y, guint origin_x, tcb_t *tcb, dir_e direction, guint row_add_count)
{
    //tcb->col_list = load_widget_grid(tcb->browser_table, tcb->num_cols, tcb->num_rows);

    #if (1)  // test logic for relocating a widget in a table (copy-to-new and remove old)
    {
        move_table_widget(test_widget, tcb, 1, 5);
    }
    #endif


    #if 0
    /* Conditionally expand the number of [direction] rows */
    if ( row_add_count > 0)
        add_rows(origin_y, origin_x, tcb, direction, row_add_count);

    add_descender(direction, row_add_count);

    /* Conditionally expand the number of [direction] columns*/
    add_column(origin_x, tcb, direction);

    #endif

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

        if ( !new_expander )
        {
            // count the number of whitespace blocks between the expander "parent" and the next column entry (or bottom of table).
            //if (compression_count > 0)
            {
                // Delete compression_count LEFT rows
            }

            // Conditionally delete LEFT column (if no more children in "child" columm)
            // Conditionally delete LEFT expander column (if no more children in "child" columm)
        }
    }
    else
    {
        image = create_pixmap(eventbox, "sca_expander_right.png");

        if ( !new_expander )
        {
            // count the number of whitespace blocks between the expander "parent" and the next column entry (or bottom of table).
            //if (compression_count > 0)
            {
                // Delete compression_count RIGHT rows
            }

            // Conditionally delete RIGHT column (if no more children in "child" columm)
            // Conditionally delete RIGHT expander column (if no more children in "child" columm)
        }
    }

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return(eventbox);
}


