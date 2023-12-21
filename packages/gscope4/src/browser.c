#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

#include "support.h"
#include "utils.h"
#include "app_config.h"
#include "search.h"
#include "display.h"
#include "fileview.h"


// Defines
//=======================

#define NUM_COL_MAX             (40)
#define INITIAL_SCA_COL_WIDTH   (70)
#define MIN_COLUMN_WIDTH        (25)  /* Units = points, should be sufficient for both large and small default font sizes */
#define GDK_BUTTON_ANY          0

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
    NULL_COL,
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
    gchar           *file_name;
    struct entry    *next;
    struct entry    *prev;
    gboolean        last_entry;  // if the entry is the last function in a block
} column_entry_t;


typedef struct
{
    column_entry_t  *member_list_head;      // column member linked list
    column_entry_t  *member_list_tail;
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
static GtkWidget*   create_browser_window(gchar *name, gchar *root_file, gchar *line_num);
static void         expand_table(guint origin_y, guint origin_x, tcb_t *tcb, dir_e direction);
static void         collapse_table(guint origin_row, guint origin_col, tcb_t *tcb, dir_e direction);
static void         move_table_widget(GtkWidget *widget, tcb_t *tcb, guint row, guint col);
static void         create_header_button_with_adjuster(tcb_t *tcb, guint col, gint label);
static void         delete_header_button_with_adjuster(tcb_t *tcb, guint col);
static void         create_dummy_adjuster(tcb_t *tcb, guint col, guint row);

static void         extend_table_right(tcb_t *tcb, guint col);
static void         make_expander_at_position(tcb_t *tcb, guint col, guint row, dir_e direction);
static void         make_collapser_at_position(tcb_t *tcb, guint col, guint row, dir_e direction);
static gboolean     on_browser_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data);

static void         initialize_table(tcb_t *tcb, gchar *root_fname, gchar *root_file, gchar *line_num);
static void         ensure_column_capacity(tcb_t *tcb, guint origin_col, dir_e direction);
static void         ensure_row_capacity(tcb_t *tcb, guint origin_row, guint row_add_count);
static void         make_filler_column(tcb_t *tcb, guint position);
static void         add_functions_to_column(tcb_t *tcb, result_t *function_list, guint num_results,
                                            guint col, guint starting_row, dir_e direction);
static void         make_expander_at_position(tcb_t *tcb, guint col, guint row, dir_e direction);
static void         shift_column_down(tcb_t *tcb, guint col, guint starting_row, guint amount);
static guint        delete_items_from_column(tcb_t *tcb, guint col, guint starting_row, guint end_row);
static void         shift_column_up(tcb_t *tcb, guint col, guint starting_row, guint amount);
static void         delete_children(tcb_t *tcb, guint col, guint upper_bound, guint lower_bound, dir_e direction);
static void         shift_table_up(tcb_t *tcb, guint upper_bound, guint amount, dir_e direction);
static void         remove_unused_columns(tcb_t *tcb, dir_e direction);
static void         shift_table_right(tcb_t *tcb, guint start_col);
static void         move_column(tcb_t *tcb, guint source, guint dest);
static void         shift_table_left(tcb_t *tcb, guint amount);
static void         delete_column(tcb_t *tcb, guint col);
static void         delete_table(tcb_t *tcb);
static void         update_table_height(tcb_t *tcb);
static GtkWidget*   make_straight_connector(void);
static GtkWidget*   make_three_way_connector(dir_e direction);
static void         add_connectors(tcb_t *tcb, guint col, guint row, guint count, dir_e direction);
static GtkWidget*   make_end_connector(dir_e direction);

static void         column_list_insert(col_list_t *list, GtkWidget *widget, guint row);
static void         column_list_insert_file(col_list_t *list, GtkWidget *widget, guint row, gchar *file);
static void         column_list_remove(col_list_t *list, GtkWidget *widget);
static column_entry_t* column_list_get_row(col_list_t *list, guint row);
static int          column_list_get_next(col_list_t *list, guint row);
static gboolean     end_of_block(col_list_t *column, guint row);

static void         get_function(tcb_t *tcb, guint col, guint row, dir_e direction, gchar **fname, gchar **file);
static result_t*    parse_results(search_results_t *results);
//static void         column_list_print_rows(col_list_t *list);
//static guint        results_list_remove_duplicates(result_t *front);
//static void         results_list_sort(result_t *front);
//static void         swap(result_t *a, result_t *b);
//static guint        results_list_filter_file(result_t **front_ptr, gchar *file);
//static gboolean     column_list_is_sorted(col_list_t *list);

//---------------- Private Globals ----------------------------------

static GdkPixbuf    *adjuster_active_image   = NULL;
static GdkPixbuf    *adjuster_inactive_image = NULL;
static gdouble      initial_x_offset = 0;






//=============================================================================================
//                               --- Public Functions ---                         
//=============================================================================================


//**********************************************************************************************
// BROWSER_init
//********************************************************************************************** 
gboolean BROWSER_init(GtkWidget *main)
{
    adjuster_active_image = create_pixbuf("sca_col_resize_active.png");
    if (adjuster_active_image)
    {
        g_object_ref(adjuster_active_image); // Create a temporary reference to prevent auto-destruction of the widget
    }

    adjuster_inactive_image = create_pixbuf("sca_col_resize_inactive.png");
    if (adjuster_inactive_image)
    {
        g_object_ref(adjuster_inactive_image);
    }

    return( adjuster_active_image && adjuster_inactive_image);
}



//**********************************************************************************************
// BROWSER_create
//********************************************************************************************** 
GtkWidget* BROWSER_create(gchar *name, gchar *root_file, gchar *line_num)
{
    return (create_browser_window(name, root_file, line_num));
}


//=============================================================================================
//                               --- Private Functions ---                         
// 
// All the functions defined past this point are private to this component
//.
//=============================================================================================



// Private Widget Callback Functions
//=============================================================================================


//********************************************************************************************** 
// on_browser_delete_event
//********************************************************************************************** 
static gboolean on_browser_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    // Free the TCB before we destroy this widget
    g_free(user_data);

    return FALSE;    // Return FALSE to destroy the widget.
}


//********************************************************************************************** 
// on_left_expander_button_release_event
//********************************************************************************************** 
static void on_left_expander_button_release_event(GtkGestureClick *gesture, int n_press, double x, double y, gpointer *user_data)
{
    tcb_t   *tcb = (tcb_t *) user_data;
    guint   row, col;

    // Get the expander widget's position in the table
    gtk_grid_get_child_at(GTK_GRID(tcb->browser_table), col, row);

    // Remove the selected expander widget from the [col] column list
    column_list_remove(&(tcb->col_list[col]), widget);

    // Destroy the selected expander widget and associated controller
    gtk_widget_destroy(gtk_event_controller_get_widget(gesture));
    g_object_unref(gesture);

    make_collapser_at_position((tcb_t *) user_data, col, row, LEFT);
}


//********************************************************************************************** 
// on_right_expander_button_release_event
//********************************************************************************************** 
static void on_right_expander_button_release_event(GtkGestureClick *gesture, int n_press, double x, double y, gpointer *user_data)
{
    tcb_t   *tcb = user_data;
    guint   row, col;

    // Get the expander widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
                            "top-attach", &row,
                            "left-attach", &col,
                            NULL);

    // Remove the selected expander widget from the [col] column list
    column_list_remove(&(tcb->col_list[col]), widget);

    // Destroy the selected expander widget and associated controller
    gtk_widget_destroy(gtk_event_controller_get_widget(gesture));
    g_object_unref(gesture);

    make_collapser_at_position(tcb, col, row, RIGHT);
}


//**********************************************************************************************
// on_left_collapser_button_release_event
//********************************************************************************************** 
static void on_left_collapser_button_release_event(GtkGestureClick *gesture, int n_press, double x, double y, gpointer *user_data)
{
    tcb_t   *tcb = user_data;
    guint   row, col;

    // Get the collapser widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
                            "top-attach", &row,
                            "left-attach", &col,
                            NULL);

    // Remove the selected collapser widget pointer from the column [col] member list
    column_list_remove(&(tcb->col_list[col]), widget);

    // Destroy the selected expander widget and associated controller
    gtk_widget_destroy(gtk_event_controller_get_widget(gesture));
    g_object_unref(gesture);

    make_expander_at_position(tcb, col, row, LEFT);
    collapse_table(row, col, tcb, LEFT);
}


//**********************************************************************************************
// on_right_collapser_button_release_event
//********************************************************************************************** 
static void on_right_collapser_button_release_event(GtkGestureClick *gesture, int n_press, double x, double y, gpointer *user_data)
{
    tcb_t   *tcb = user_data;
    guint   row, col;

    // Get the collapser widget's position in the table
    gtk_container_child_get(GTK_CONTAINER(tcb->browser_table), widget,
                            "top-attach", &row,
                            "left-attach", &col,
                            NULL);

    // Remove the selected collapser widget pointer from the column [col] member list
    column_list_remove(&(tcb->col_list[col]), widget);

    // Destroy the selected expander widget and associated controller
    gtk_widget_destroy(gtk_event_controller_get_widget(gesture));
    g_object_unref(gesture);

    make_expander_at_position((tcb_t *) user_data, col, row, RIGHT);
    collapse_table(row, col, tcb, RIGHT);
}


//********************************************************************************************** 
// on_function_button_press
//********************************************************************************************** 
static void on_function_button_press(GtkGestureClick *gesture, int n_press, double x, double y, result_t  *box)
{
    if ( (n_press == 2) && gtk_gesture_single_get_current_button(gesture) == GDK_BUTTON_PRIMARY ) // Double-click, left button: View/Edit entry
    {
        if (settings.useEditor)
        {
            my_start_text_editor(box->file_name, box->line_num);
        }
        else
        {
            FILEVIEW_create(box->file_name, atoi(box->line_num));
        }
    }
    else if ( (n_press == 1) && gtk_gesture_single_get_current_button(gesture) == GDK_BUTTON_SECONDARY )  // Single-click, right button: Open Menu
    {
        right_click_menu(gtk_event_controller_get_widget(gesture), box);
    }
}


//********************************************************************************************** 
// on_reroot
//********************************************************************************************** 
static void on_reroot(GtkWidget *menuitem, result_t *function_box)
{
    search_results_t *matches;
    // search for the function and see how many results come up
    matches = SEARCH_lookup(FIND_DEF, function_box->function_name);

    if (matches->match_count == 1)
    {
        // only 1 match we can just reset the browser with the appropriate function
        delete_table((tcb_t *)function_box->tcb);
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


//********************************************************************************************** 
// on_adjuster_eventbox_enter_notify_event
//********************************************************************************************** 
static void on_adjuster_eventbox_enter_notify_event(GtkEventControllerMotion* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkImage *adjuster_image = (GtkImage *) user_data;

    gtk_image_set_from_pixbuf(adjuster_image, adjuster_active_image);
}


//********************************************************************************************** 
// on_adjuster_eventbox_leave_notify_event
//********************************************************************************************** 
static void on_adjuster_eventbox_leave_notify_event(GtkEventControllerMotion* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkImage *adjuster_image = (GtkImage *) user_data;

    gtk_image_set_from_pixbuf(adjuster_image, adjuster_inactive_image);
}


//********************************************************************************************** 
// on_adjuster_eventbox_button_press_event
//********************************************************************************************** 
static void on_adjuster_eventbox_button_press_event(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    // Set the initial offset in a private-global [to be used motion_notify callback]
    // We can share this value via a global because only one adjuster_eventbox can
    // be active at a time.
    initial_x_offset = x;
}


//********************************************************************************************** 
// on_adjuster_eventbox_motion_notify_event
//********************************************************************************************** 
static void on_adjuster_eventbox_motion_notify_event(GtkEventControllerMotion* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkAllocation allocation;
    GtkRequisition req;
    gdouble offset;
    GtkWidget *header_button = (GtkWidget *) user_data;

    gtk_widget_size_request(header_button, &req);
    gtk_widget_get_allocation(header_button, &allocation);

    // We can share initial_x_offset via a global because only one adjuster_eventbox can
    // be active at a time.
    offset = x - initial_x_offset;

    allocation.width += offset;
    if (allocation.width < MIN_COLUMN_WIDTH)
        allocation.width = MIN_COLUMN_WIDTH;

    gtk_widget_set_size_request(header_button, allocation.width, req.height);
    gtk_widget_size_allocate(header_button, &allocation);
}



// Private Functions (General)
//=============================================================================================


//********************************************************************************************** 
// create_browser_window
//********************************************************************************************** 
static GtkWidget* create_browser_window(gchar *name, gchar *root_file, gchar *line_num)
{
    GtkWidget *browser_window;
    GdkPixbuf *browser_window_icon_pixbuf;
    GtkWidget *browser_vbox;
    GtkWidget *browser_scrolledwindow;
    GtkWidget *browser_viewport;
    GtkWidget *browser_table;
    gchar *var_string;
    tcb_t     *tcb;

    // Malloc a Table Control Block (TCB) for this window.  This keeps the table data with the
    // widget and makes this function re-entrant-safe.

    tcb = g_malloc(sizeof(tcb_t));

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

    /* Add the scrollable call-tree table */
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

    g_signal_connect((gpointer)browser_window, "delete_event",
                     G_CALLBACK(on_browser_delete_event),
                     tcb);

    /* Add the non-scrollable table header */


    return browser_window;
}


//********************************************************************************************** 
// make_collapser_at_position
//********************************************************************************************** 
static void make_collapser_at_position(tcb_t *tcb, guint col, guint row, dir_e direction)
{
    GtkWidget   *collapser;
    GtkWidget   *image;
    GtkGesture  *gesture;

    // Create a new collapser widget
    collapser = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_name(collapser, "expander_eventbox");
    gtk_widget_show(collapser);
    image = create_pixmap(collapser, "sca_collapser_multi.png");
    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(collapser), image);

    // Attach new collapser widget to the table at the location previously occupied by the expander widget [row, col]
    gtk_table_attach(GTK_TABLE(tcb->browser_table), collapser, col, col + 1, row, row + 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);
    column_list_insert(&(tcb->col_list[col]), collapser, row);

    tcb->col_list[col].type = EXPANDER;     // Yes, this is correct, a collapser is an EXPANDER-class item

    gesture = gtk_gesture_click_new();  // Caller must free gesture
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(collapser), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(collapser, GTK_EVENT_CONTROLLER(gesture));

    if ( direction == RIGHT )
    {
        g_signal_connect(gesture, "release",
                         G_CALLBACK(on_right_collapser_button_release_event),
                         tcb);

        // Expand the table and add the relevant functions
        expand_table(row, col, tcb, RIGHT);
    }
    else    // direction == LEFT
    {
        g_signal_connect(gesture, "release_event",
                         G_CALLBACK(on_left_collapser_button_release_event),
                         tcb);

        // Expand the table and add the relevant functions
        expand_table(row, col, tcb, LEFT);
    }
}


//**********************************************************************************************
// make_expander_at_position
//********************************************************************************************** 
static void make_expander_at_position(tcb_t *tcb, guint col, guint row, dir_e direction)
{
    GtkWidget   *expander;
    GtkWidget   *image;
    GtkGesture  *gesture;

    //expander = gtk_event_box_new();
    expander = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_name(expander, "expander_eventbox");
    gtk_widget_show(expander);

    if (direction == LEFT)
        image = create_pixmap(expander, "sca_expander_left.png");
    else
        image = create_pixmap(expander, "sca_expander_right.png");
    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(expander), image);
   
    // Add the new expander widget to the [col] column list [at row]
    gtk_table_attach(GTK_TABLE(tcb->browser_table), expander, col, col + 1, row, row + 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);
    column_list_insert(&(tcb->col_list[col]), expander, row);

    tcb->col_list[col].type = EXPANDER;

    gesture = gtk_gesture_click_new();  // Caller must free gesture
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(expander), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(expander, GTK_EVENT_CONTROLLER(gesture));

    if (direction == RIGHT)
    {
        g_signal_connect(gesture, "released",
                         G_CALLBACK(on_right_expander_button_release_event),
                         tcb);
    }
    else
    {
        g_signal_connect(gesture, "released",
                         G_CALLBACK(on_left_expander_button_release_event),
                         tcb);
    }
}


//**********************************************************************************************
// delete_table
//  
// Clear every widget from the table
//********************************************************************************************** 
static void delete_table(tcb_t *tcb)
{
    int i;

    for (i = 0; i < tcb->num_cols; i++)
    {
        delete_column(tcb, i);
    }
}


//********************************************************************************************** 
// right_click_menu
//********************************************************************************************** 
static void right_click_menu(GtkWidget *widget, result_t *function_box)
{
    GtkWidget * menu,*menu_item;

    menu = gtk_menu_new();

    menu_item = gtk_menu_item_new_with_label("reroot");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    g_signal_connect(menu_item, "activate", (GCallback)on_reroot, function_box);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}


// Revisit:  Might be able to "calculate" the column label number.  If so, we can eliminate
// the 'gint label' paramater
//**********************************************************************************************
// create_header_button_with_adjuster
// 
// Create a header button and its associated width [right-side] adjuster.  Place both widgets 
// in the appropriate GTK table columns.
//********************************************************************************************** 
static void create_header_button_with_adjuster(tcb_t *tcb, guint col, gint label)
{
    GtkWidget   *header_button;
    GtkWidget   *adjuster_eventbox;
    GtkWidget   *adjuster_image;
    col_list_t  *column;
    gchar       *num_str;
    GtkEventController *pointer_motion;
    GtkGesture  *gesture;

    int left_bound  = col;
    int right_bound = col + 1;

    // Create and insert the header button
    //====================================

    my_asprintf(&num_str, "%d", label);
    header_button  = gtk_button_new_with_mnemonic(num_str);
    g_free(num_str);

    gtk_widget_set_name(header_button, "header_button");
    gtk_widget_set_can_focus(header_button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(header_button), FALSE);
    gtk_widget_set_size_request (header_button, INITIAL_SCA_COL_WIDTH, -1);
    gtk_widget_show(header_button);

    gtk_table_attach(GTK_TABLE(tcb->browser_table), header_button, left_bound, right_bound, 0, 1,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    column_list_insert(&(tcb->col_list[col]), header_button, 0);

    column = &(tcb->col_list[col]);
    column->type = NAME;

    // Create and insert the column width adjuster
    // (to the immediate right of the new header button)
    //==================================================

    adjuster_eventbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_name (adjuster_eventbox, "adjuster_eventbox");
    gtk_widget_set_can_focus(adjuster_eventbox, FALSE);
    gtk_widget_set_size_request (adjuster_eventbox, 8, -1);
    gtk_widget_show (adjuster_eventbox);

    adjuster_image = gtk_image_new_from_pixbuf(adjuster_inactive_image);
    gtk_widget_show(adjuster_image);
    gtk_container_add (GTK_CONTAINER (adjuster_eventbox), adjuster_image);

    gtk_table_attach (GTK_TABLE (tcb->browser_table), adjuster_eventbox,
                      left_bound  + 1,   // adjuster left_bound
                      right_bound + 1,   // adjuster right_bound
                      0, 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 0, 0);
    column_list_insert(&(tcb->col_list[col + 1]), adjuster_eventbox, 0);

    column = &(tcb->col_list[col + 1]);
    column->type                = EXPANDER;

    pointer_motion = gtk_event_controller_motion_new();
    gtk_widget_add_controller(adjuster_eventbox, GTK_EVENT_CONTROLLER(pointer_motion));

    gesture = gtk_gesture_click_new();  // Caller must free gesture
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(adjuster_eventbox), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(adjuster_eventbox, GTK_EVENT_CONTROLLER(gesture));


    g_signal_connect (pointer_motion, "enter",
                      G_CALLBACK(on_adjuster_eventbox_enter_notify_event),
                      adjuster_image);

    g_signal_connect (pointer_motion, "leave",
                      G_CALLBACK(on_adjuster_eventbox_leave_notify_event),
                      adjuster_image);

    g_signal_connect (gesture, "pressed",
                      G_CALLBACK(on_adjuster_eventbox_button_press_event),
                      NULL);

    g_signal_connect (pointer_motion, "motion",
                      G_CALLBACK(on_adjuster_eventbox_motion_notify_event),
                      header_button);
}


//**********************************************************************************************
// create_dummy_adjuster
//********************************************************************************************** 
static void create_dummy_adjuster(tcb_t *tcb, guint col, guint row)
{
    GtkWidget   *adjuster_eventbox;
    GtkWidget   *adjuster_image;

    adjuster_eventbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name (adjuster_eventbox, "adjuster_eventbox");
    gtk_widget_set_can_focus(adjuster_eventbox, FALSE);
    gtk_widget_set_size_request (adjuster_eventbox, 8, -1);
    gtk_widget_show (adjuster_eventbox);

    adjuster_image = gtk_image_new_from_pixbuf(adjuster_inactive_image);
    gtk_widget_show(adjuster_image);
    gtk_container_add (GTK_CONTAINER (adjuster_eventbox), adjuster_image);

    gtk_table_attach (GTK_TABLE (tcb->browser_table), adjuster_eventbox,
                      col,        // adjuster left_bound
                      col + 1,    // adjuster right_bound
                      row, row + 1,
                      (GtkAttachOptions) (GTK_FILL),
                      (GtkAttachOptions) (GTK_FILL), 0, 0);
    column_list_insert(&(tcb->col_list[col]), adjuster_eventbox, 0);

    (tcb->col_list[col]).type = EXPANDER;
}


//**********************************************************************************************
// initialize_table
//********************************************************************************************** 
static void initialize_table(tcb_t *tcb, gchar *root_fname, gchar *root_file, gchar *line_num)
{
    GtkWidget   *root_function_blue_eventbox;
    GtkWidget   *root_function_label;
    gchar       *var_string;
    GtkGesture  *gesture;

    gtk_table_resize(GTK_TABLE(tcb->browser_table), 2, 5);

    tcb->root_col      = 2;     // Center column of a zero-based 5 column initial table
    tcb->num_cols      = 5;
    tcb->num_rows      = 2;
    tcb->left_height   = 1;
    tcb->right_height  = 1;
    memset(tcb->col_list, 0, sizeof(tcb->col_list));

    // put file name and function name into a result_t to pass to the on-click callback
    tcb->root_entry.function_name = root_fname;
    tcb->root_entry.file_name = root_file;
    tcb->root_entry.line_num = line_num;
    tcb->root_entry.tcb = tcb;

    make_expander_at_position(tcb, 1, 1, LEFT);     // Create Left Expander
    make_expander_at_position(tcb, 3, 1, RIGHT);    // Create Right Expander

    // Create the column header [and right-side adjuster]
    create_header_button_with_adjuster(tcb, tcb->root_col, 0);

    // Create and Insert a dummy left-side [header] adjuster for the root column
    create_dummy_adjuster(tcb, tcb->root_col - 1, 0);

    make_filler_column(tcb, 0);     // Create the left filler column
    make_filler_column(tcb, 4);     // Create the right filler column

    // Create the root function table entry label -- Revisit:  Lots of common code with add_functions_to_column()
    my_asprintf(&var_string, "<span color=\"darkred\">%s</span>", root_fname);
    root_function_label = gtk_label_new(var_string);
    g_free(var_string);
    gtk_widget_set_name(root_function_label, "root_function_label");
    gtk_widget_show(root_function_label);
    gtk_label_set_use_markup(GTK_LABEL(root_function_label), TRUE);
    gtk_misc_set_alignment(GTK_MISC(root_function_label), 0, 0.5);
    gtk_misc_set_padding(GTK_MISC(root_function_label), 1, 0);
    gtk_label_set_ellipsize(GTK_LABEL(root_function_label), PANGO_ELLIPSIZE_END);

    // Place File and Line information into the tooltip for the label.
    my_asprintf(&var_string, "%s : %s", tcb->root_entry.file_name, tcb->root_entry.line_num);
    gtk_widget_set_tooltip_text(root_function_label, var_string); 
    g_free(var_string);

    // Create the root function table entry eventbox [container for label]
    root_function_blue_eventbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_name(root_function_blue_eventbox, "blue_eventbox");
    gtk_widget_show(root_function_blue_eventbox);
    gtk_table_attach(GTK_TABLE(tcb->browser_table), root_function_blue_eventbox, tcb->root_col, tcb->root_col + 1, 1, 2,
                     (GtkAttachOptions)(GTK_FILL),
                     (GtkAttachOptions)(GTK_FILL), 0, 0);
    column_list_insert_file(&(tcb->col_list[tcb->root_col]), root_function_blue_eventbox, 1, root_file);
    tcb->col_list[tcb->root_col].type = NAME;

    // Place label in eventbox and hook up button press event.
    gtk_container_add(GTK_CONTAINER(root_function_blue_eventbox), root_function_label);

    gesture = gtk_gesture_click_new();  // Caller must free gesture
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(root_function_blue_eventbox), GDK_BUTTON_PRIMARY);
    gtk_widget_add_controller(root_function_blue_eventbox, GTK_EVENT_CONTROLLER(gesture));
   
    g_signal_connect(gesture, "pressed",
                     G_CALLBACK(on_function_button_press), &(tcb->root_entry);

}


//**********************************************************************************************
// move_table_widget
//********************************************************************************************** 
static void move_table_widget(GtkWidget *widget, tcb_t *tcb, guint row, guint col)
{
    GtkAttachOptions    xoptions;

    g_object_ref(widget);  // Create a temporary reference to prevent the widget from being destroyed.

    gtk_container_remove(GTK_CONTAINER(tcb->browser_table), widget);

    if ( tcb->col_list[col].type == FILLER )
        xoptions = (GtkAttachOptions) (GTK_EXPAND | GTK_FILL);
    else
        xoptions = (GtkAttachOptions) (GTK_FILL);

    gtk_table_attach(GTK_TABLE(tcb->browser_table), widget, col, col + 1, row, row + 1,
                     (GtkAttachOptions) xoptions,
                     (GtkAttachOptions)(GTK_FILL), 0, 0);

    g_object_unref((gpointer)widget);  // Release the temporary reference to prevent the widget from being destroyed.


}


//********************************************************************************************** 
// expand_table
//  
// This function implements a table expansion associated with an "expander"-clicked event.
// Overall functionality includes:
//   1) Add new rows and/or new columns to the table, as required [resize table widget]
//   2) Relocate existing widgets that need to be moved in order to accommodate the expansion.
//   3) Add any new widgets: Icons and function names "exposed" by the expansion operation.
//********************************************************************************************** 
static void expand_table(guint origin_row, guint origin_col, tcb_t *tcb, dir_e direction)
{
    int i, add_pos, adjusted_col;
    guint row_add_count;
    gchar * fname,*file;
    result_t *results;
    search_results_t *children;
    search_t operation;

    // set the search type depending on which way we are expanding
    operation = (direction == RIGHT ? FIND_CALLEDBY : FIND_CALLING);

    // get the function associated with the expander that was clicked
    // search for the appropriate children for that function and
    // parse them into a usable list
    get_function(tcb, origin_col, origin_row, direction, &fname, &file);
    children = SEARCH_lookup(operation, fname);
    if (children->match_count == 0)
    {
        return;
    }

    results = parse_results(children);
    //children->match_count -= results_list_filter_file(&results, file);
    row_add_count = children->match_count - 1; // the first child is on the same line so it is not an added row
    SEARCH_free_results(children);  //children is unusable after this

    // make sure the table has enough room to add functions
    ensure_row_capacity(tcb, origin_row, row_add_count);
    ensure_column_capacity(tcb, origin_col,  direction);


    // shift down columns left or right of center depening on direction, then add functions
    if (direction == RIGHT)
    {
        for (i = tcb->root_col; i < tcb->num_cols; i++)
        {
            shift_column_down(tcb, i, origin_row, row_add_count);
        }
        add_functions_to_column(tcb, results, row_add_count + 1, origin_col + 1, origin_row, RIGHT);
        add_connectors(tcb, origin_col, origin_row, row_add_count + 1, RIGHT);
    }
    else    // direction == LEFT
    {
        if ( origin_col > 1 )   // Perform normal Left-Expansion
        {
            add_pos = origin_col - 1;
            adjusted_col = origin_col;
        }
        else    // origin_col == 1, perform special Left-Expansion for leftmost (non-filler) column
        {
            add_pos = 2;
            adjusted_col = 3;
        }

        // if this is the furthest left column then origin_col - 1 is not the correct
        // place to add due to columns being shifted right
        //add_pos = origin_col == 1 ? 2 : origin_col - 1;
        // adjusted_col = origin_col == 1 ? 3 : origin_col;

        for (i = tcb->root_col; i > 1; i--)
        {
            shift_column_down(tcb, i, origin_row, row_add_count);
        }
        add_functions_to_column(tcb, results, row_add_count + 1, add_pos, origin_row, LEFT);
        add_connectors(tcb, adjusted_col, origin_row, row_add_count + 1, LEFT);
    }


    // update table height
    if (direction == RIGHT)
    {
        tcb->right_height += row_add_count;
    }
    else
    {
        tcb->left_height += row_add_count;
    }
    update_table_height(tcb);
}


//**********************************************************************************************
// ensure_column_capacity
//********************************************************************************************** 
static void ensure_column_capacity(tcb_t *tcb, guint origin_col, dir_e direction)
{
    if (direction == RIGHT)
    {
        // If we are expanding at the extreme right column, we need to expand the
        // table to the left by growing the GTK table by two columns.  No existing
        // colunm data needs to be shifted.
        if (origin_col == tcb->num_cols - 2)  // -2 because of filler column and zero based indexing
        {
            extend_table_right(tcb, origin_col);
        }
    }
    else    // direction == LEFT
    {
        // If we are expanding at the extreme left column, we need to expand the
        // table to the left by growing the GTK table by two columns and shifting every 
        // existing column within the GTK table two positions to the right.
        if (origin_col == 1)  // if we are expanding at the extreme left column, we need to
        {
                shift_table_right(tcb, origin_col);
        }
    }
}



//**********************************************************************************************
// extend_table_right
//********************************************************************************************** 
static void extend_table_right(tcb_t *tcb, guint col)
{
    guint   i;
    gint header_num;

    gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols + 2);

    for (i = tcb->num_cols - 1; i > col; i--)
    {
        move_column(tcb, i, i + 2);
    }

    header_num = (gint) ((col + 1) - tcb->root_col) / 2;
    create_header_button_with_adjuster(tcb, col + 1, header_num);

    tcb->num_cols += 2;
}


//********************************************************************************************** 
// shift_table_right
// 
// Move every column right of start_col (inclusive) two columns to the right 
// leaving two empty columns in position start_col and start_col + 1.
// 
//********************************************************************************************** 
static void shift_table_right(tcb_t *tcb, guint start_col)
{
    guint i;
    gint header_num;
    guint adjusted_col = start_col;

    gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols + 2);

    for (i = tcb->num_cols - 1; i >= start_col; i--)
    {
        move_column(tcb, i, i + 2);
    }

    if ( start_col == 1 )
    {
        // Relocate the dummy adjuster moved by the move_column() calls (above).
        move_table_widget(tcb->col_list[3].member_list_head->widget, tcb, 0, 1);
        column_list_insert(&(tcb->col_list[1]), tcb->col_list[3].member_list_head->widget, 0);
        column_list_remove(&(tcb->col_list[3]), tcb->col_list[1].member_list_head->widget);

        tcb->col_list[1].type = EXPANDER;

        adjusted_col = 2;   // Shift new header header over by 1 colulmn;
    }
    
    if ( start_col <= tcb->root_col )    // If we shifted the root column
        tcb->root_col += 2;

    header_num = (gint) ((start_col + 1) - tcb->root_col) / 2;
    create_header_button_with_adjuster(tcb, adjusted_col, header_num);

    tcb->num_cols += 2;
}


//********************************************************************************************** 
// move_column
// 
// Copy all column elements from src_col to dest_col.  All column elements in dest will be 
// deleted.   Elements in src_col will be deleted after moving (leaving src_col empty).
// 
//********************************************************************************************** 
static void move_column(tcb_t *tcb, guint source, guint dest)
{
    col_list_t      *src_col;
    col_list_t      *dest_col;
    column_entry_t  *curr;

    // get the lists that are being modified
    dest_col = &(tcb->col_list[dest]);
    src_col  = &(tcb->col_list[source]);

    // Destroy all widgets (if any) currently residing in the destination column.
    //===========================================================================
    delete_column(tcb, dest);

    // Copy the src_col tracking data to the dest_col
    //===============================================
    *dest_col = *src_col;     // Structure copy: src_col->dest_col

    // Relocate all widgets from src_col to dest_col
    //==============================================
    curr = src_col->member_list_head;
    while (curr != NULL)
    {
        move_table_widget(curr->widget, tcb, curr->row, dest);
        curr = curr->next;
    }

    // Everything is moved, delete the source column
    //==============================================
    src_col->type = NULL_COL;
    src_col->member_list_head = NULL;
    src_col->member_list_tail = NULL;
}


//**********************************************************************************************
// ensure_row_capacity 
//********************************************************************************************** 
static void ensure_row_capacity(tcb_t *tcb, guint origin_row, guint row_add_count)
{
    guint delta = origin_row + row_add_count - tcb->num_rows + 1;

    if (delta > 0)
    {
        // Add sufficient rows
        gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows + delta, tcb->num_cols);
        tcb->num_rows += delta;
    }
}


//**********************************************************************************************
// collapse_table
//
// This function implements a table reduction associated with a "collapser"-clicked event.
// Overall functionality includes:
//   2) Relocate existing widgets that need to be moved in order to accommodate the reduction.
//   3) Remove any eclipsed widgets: Icons and function names "hidden" by the reduction operation.
//   1) Remove existing rows and/or existing columns from the table, as required  [resize table widget]
//********************************************************************************************** 
static void collapse_table(guint origin_row, guint origin_col, tcb_t *tcb, dir_e direction)
{
    gint    next_row;
    guint   i;
    guint   lower_bound;

    // Recursivly collapse all children.  To do this we need to know the position
    // of the next element in this column in order to put a lower bound on what we 
    // collpase (don't want to collapse children of other funcitons)

    if (direction == RIGHT)
    {
        lower_bound = tcb->right_height + 1;

        for (i = origin_col - 1; i != tcb->root_col; i -= 2)
        {
            next_row = column_list_get_next(&(tcb->col_list[i]), origin_row);
            if (next_row != -1)
            {
                lower_bound = next_row;
                break;
            }
        }
    }
    else    // direction == LEFT
    {
        lower_bound = tcb->left_height + 1;

        for (i = origin_col + 1; i != tcb->root_col; i += 2)
        {
            next_row = column_list_get_next(&(tcb->col_list[i]), origin_row);
            if (next_row != -1)
            {
                lower_bound = next_row;
                break;
            }
        }
    }

    //delete all children of the collapser and shift up the table
    //the appropriate amount
    delete_children(tcb, origin_col, origin_row, lower_bound, direction);

    //check if the table needs to be downsized, if it does it will always
    //be a multiple of 2 rows for name and collapser columns
    remove_unused_columns(tcb, direction);
}


//**********************************************************************************************
// delete_header_button_with_adjuster
// 
// Delete and destroy the header_button and adjuster accociated with the specified column.
// 
// Note: This function should never be called on a non-empty [other than the header entry]
//       column.
//**********************************************************************************************
static void delete_header_button_with_adjuster(tcb_t *tcb, guint col)
{
    col_list_t      *col_list;
    guint           col_iter;

    for (col_iter = col; col_iter < col + 2; col_iter += 2)
    {
        col_list = &(tcb->col_list[col_iter]);

        // Delete the obsolete header widget
        gtk_widget_destroy(col_list->member_list_head->widget);
        column_list_remove(col_list, col_list->member_list_head->widget);

        // Delete the obsolete adjuster widget [on RHS of header]
        col_list = &(tcb->col_list[col_iter + 1]);
        gtk_widget_destroy(col_list->member_list_head->widget);
        column_list_remove(col_list, col_list->member_list_head->widget);
    }
}


//**********************************************************************************************
// remove_unused_columns
// 
// Remove every non-filler column that contains no elements that is direction (left or right)
// of the root column. Shift columns (as appropriate) and resizes the table as necessary
//********************************************************************************************** 
static void remove_unused_columns(tcb_t *tcb, dir_e direction)
{
    int i;
    guint name_col;
    col_list_t *column;
    int shrink_count = 0; // number of columns removed

    if (direction == RIGHT)
    {
        // Starting with the right-most NAME column (num-cols - 3), delete any NAME column
        // and associated EXPANDER column (only if the NAME column is empty).
        // 
        // Once a non-empty NAME column, or the root-column is encountered
        // stop deleting columns.  Keep a count of deleted columns for the
        // subsequent table shift.

        for (name_col = tcb->num_cols - 3; name_col > tcb->root_col; name_col -= 2)
        {
            column = &(tcb->col_list[name_col]);
            if (column->member_list_head != NULL)   // There should always be at least on entry in the column list
            {
                if ( column->member_list_head == column->member_list_tail )
                {
                    // There is a single entry in the column list (The column header).
                    // In this scenario, there are no functions listed in this column.
                    delete_header_button_with_adjuster(tcb, name_col);
                    shrink_count += 2;
                }
                else
                {
                    break;  // Non-empty NAME column encountered.
                }
            }
        }

        if ( shrink_count > 0 )
        {
            move_column(tcb, tcb->num_cols - 1, tcb->num_cols - 1 - shrink_count);  // Move the Right Filler column
            gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols - shrink_count);
            tcb->num_cols -= shrink_count;
        }
    }
    else    // direction == LEFT
    {
        // Starting with the left-most NAME column (2), delete any NAME column, and
        // associated EXPANDER column (only if the NAME column is empty).
        // 
        // Once a non-empty NAME column, or the root-column is encountered
        // stop deleting columns.  Keep a count of deleted columns for the
        // subsequent table shift.

        for (name_col = 2; name_col < tcb->root_col; name_col += 2)
        {
            column = &(tcb->col_list[name_col]);
            if ( column->member_list_head != NULL )     // There should always be at least one entry in the column list
            {
                if ( column->member_list_head == column->member_list_tail )
                {
                    // There is a single entry in the column list (The column header).
                    // In this scenario, there are no functions listed in this column.
                    delete_header_button_with_adjuster(tcb, name_col);
                    shrink_count += 2;
                }
                else
                {
                    break;  // Non-empty NAME column encountered.
                }
            }
        }
        
        if ( shrink_count > 0 )
        {
            // Move the dummy adjuster from row[0], col[1] to row[0] col[shrink_count +1]
            move_table_widget (  tcb->col_list[1].member_list_head->widget, tcb, 0, shrink_count + 1);
            column_list_insert(&(tcb->col_list[shrink_count + 1]), tcb->col_list[1].member_list_head->widget, 0);
            column_list_remove(&(tcb->col_list[1]), tcb->col_list[shrink_count + 1].member_list_head->widget);

            shift_table_left(tcb, shrink_count);
            for (i = tcb->num_cols - shrink_count; i < tcb->num_cols; i++)
            {
                delete_column(tcb, i);
            }
            gtk_table_resize(GTK_TABLE(tcb->browser_table), tcb->num_rows, tcb->num_cols - shrink_count);
            tcb->num_cols -= shrink_count;
            tcb->root_col -= shrink_count;
        }
    }
}


//**********************************************************************************************
// shift_table_left
// 
// Move every column to the right of amount, amount spots to the left in the tcb->col_list
//********************************************************************************************** 
static void shift_table_left(tcb_t *tcb, guint amount)
{
    int i;
    for (i = amount + 1; i < tcb->num_cols; i++)
    {
        move_column(tcb, i, i - amount);
    }
}


//**********************************************************************************************
// delete_column 
// 
// Remove every widget in a column
//********************************************************************************************** 
static void delete_column(tcb_t *tcb, guint col)
{
    column_entry_t *curr;
    column_entry_t *next;
    col_list_t *column;

    column = &(tcb->col_list[col]);
    curr   = column->member_list_head;

    // clear the column list
    while (curr != NULL)
    {
        gtk_widget_destroy(curr->widget);
        next = curr->next;
        g_free(curr);
        curr = next;
    }

    column->member_list_head = NULL;
    column->member_list_tail = NULL;
    column->type = NULL_COL;
}


//********************************************************************************************** 
// shift_column_up
//********************************************************************************************** 
static void shift_column_up(tcb_t *tcb, guint col, guint starting_row, guint amount)
{
    column_entry_t  *node;
    col_list_t      *column;

    column = &(tcb->col_list[col]);
    node = column->member_list_head;

    while (node != NULL)
    {
        if (node->row > starting_row + amount)
        {
            move_table_widget(node->widget, tcb, node->row - amount, col);
            node->row -= amount;
        }
        node = node->next;
    }
}


//**********************************************************************************************
// shift_table_up
//  
// Shift every widget below upper_bound on the direction side of the root column up
// by the specified amount.
//********************************************************************************************** 
static void shift_table_up(tcb_t *tcb, guint upper_bound, guint amount, dir_e direction)
{
    int i;
    guint left, right;

    // set bounds depending on direction
    left  = (direction == RIGHT ? tcb->root_col : 1);
    right = (direction == RIGHT ? tcb->num_cols : tcb->root_col);

    for (i = left; i < right; i++)
    {
        shift_column_up(tcb, i, upper_bound, amount);
    }
    if (direction == RIGHT)
    {
        tcb->right_height -= amount;
    }
    else
    {
        tcb->left_height -= amount;
    }
    update_table_height(tcb);
}


//**********************************************************************************************
//  delete_children
//********************************************************************************************** 
static void delete_children(tcb_t *tcb, guint col, guint upper_row_bound, guint lower_row_bound, dir_e direction)
{
    guint col_iter;
    guint height;     
    gint lowest_row;
    gint shift;

    lowest_row = upper_row_bound; // "lowest" row removed, is actually the highest number

    if (direction == RIGHT)
    {
        for (col_iter = tcb->num_cols - 2; col_iter > col; col_iter--)
        {
            height = delete_items_from_column(tcb, col_iter, upper_row_bound, lower_row_bound);
            lowest_row = height > lowest_row ? height : lowest_row;
        }
        // delete the connectors from the clicked expander column
        for (col_iter = tcb->root_col + 1; col_iter <= col; col_iter += 2)
        {
            delete_items_from_column(tcb, col_iter, upper_row_bound + 1, lower_row_bound);
        }
    }
    else    // direction == LEFT
    {
        for (col_iter = 1; col_iter < col; col_iter++)
        {
            height = delete_items_from_column(tcb, col_iter, upper_row_bound, lower_row_bound);
            lowest_row = height > lowest_row ? height : lowest_row;
        }
        // delete the connectors from the clicked expander column
        for (col_iter = tcb->root_col - 1; col_iter >= col; col_iter--)
        {
            delete_items_from_column(tcb, col_iter, upper_row_bound + 1, lower_row_bound);
        }
    }

    shift = lowest_row - upper_row_bound;
    shift_table_up(tcb, upper_row_bound, shift, direction);
}


//**********************************************************************************************
// delete_items_from_column
// 
// Remove all items from col between starting_row (inclusive) and end_row (Exclusive) and 
// return the position of the "lowest" item deleted (which is the highest number)
//********************************************************************************************** 
static guint delete_items_from_column(tcb_t *tcb, guint col, guint starting_row, guint end_row)
{
    guint row_iter;
    column_entry_t *item;
    guint lowest = starting_row;  // the "lowest" row removed, so highest col number

    for (row_iter = starting_row; row_iter < end_row; row_iter++)
    {
        // check each row to see if a widget exists and if so remove it
        item = column_list_get_row(&(tcb->col_list[col]), row_iter);
        if (item != NULL)
        {
            lowest = item->row > lowest ? item->row : lowest;
            gtk_widget_destroy(item->widget);
            column_list_remove(&(tcb->col_list[col]), item->widget);
        }
    }
    return lowest;
}


//********************************************************************************************** 
// make_filler_column
//********************************************************************************************** 
static void make_filler_column(tcb_t *tcb, guint position)
{
    GtkWidget *button;

    button= gtk_button_new_with_mnemonic("");
    gtk_widget_set_name(button, "horizontal_filler_header_button");
    gtk_widget_show(button);
    gtk_widget_set_can_focus(button, FALSE);
    gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);
    gtk_table_attach(GTK_TABLE(tcb->browser_table), button, position, position + 1, 0, 1,
                     (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                     (GtkAttachOptions)(0), 0, 0);
    column_list_insert(&(tcb->col_list[position]), button, 0);

    tcb->col_list[position].type = FILLER;
}


//********************************************************************************************** 
// add_functions_to_column
//********************************************************************************************** 
static void add_functions_to_column(tcb_t *tcb, result_t *function_list, guint num_results,
                                    guint col, guint starting_row, dir_e direction)
{
    int row, offset;
    col_list_t *list;
    char *var_string;
    GtkWidget *function_label;
    GtkWidget *function_event_box;
    result_t *node;
    search_results_t *children;
    search_t operation;
    GtkGesture  *gesture;

    offset = (direction == RIGHT ? 1 : -1);
    node = function_list;

    for (row = starting_row; row < starting_row + num_results; row++)
    {
        // Revisit: This event_box pattern is almost exactly duplicated elsewhere - add a create_function_box function
        function_event_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
        gtk_widget_set_name(function_event_box, "blue_eventbox");
        gtk_widget_show(function_event_box);
        gtk_table_attach(GTK_TABLE(tcb->browser_table), function_event_box, col, col + 1, row, row + 1,
                         (GtkAttachOptions)(GTK_FILL),
                         (GtkAttachOptions)(GTK_FILL), 0, 0);

        my_asprintf(&var_string, "%s", node->function_name);
        function_label = gtk_label_new(var_string);
        g_free(var_string);

        // Place File and Line information into the tooltip for the label.
        my_asprintf(&var_string, "%s : %s", node->file_name, node->line_num);
        gtk_widget_set_tooltip_text(function_label, var_string); 
        g_free(var_string);

        gtk_widget_set_name(function_label, "function_label");
        gtk_widget_show(function_label);
        gtk_container_add(GTK_CONTAINER(function_event_box), function_label);
        gtk_label_set_use_markup(GTK_LABEL(function_label), TRUE);
        gtk_misc_set_alignment(GTK_MISC(function_label), 0, 0.5);
        gtk_misc_set_padding(GTK_MISC(function_label), 1, 0);
        gtk_label_set_ellipsize(GTK_LABEL(function_label), PANGO_ELLIPSIZE_END);

        list = &(tcb->col_list[col]);
        column_list_insert_file(list, function_event_box, row, node->file_name);

        // set the last_row field
        if (row == starting_row + num_results - 1)
        {
            column_list_get_row(list, row)->last_entry = TRUE;
        }
        else
        {
            column_list_get_row(list, row)->last_entry = FALSE;
        }

        node->tcb = tcb;

        gesture = gtk_gesture_click_new();  // Caller must free gesture
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(function_event_box), GDK_BUTTON_ANY);
        gtk_widget_add_controller(function_event_box, GTK_EVENT_CONTROLLER(gesture));

        g_signal_connect(gesture, "pressed",
                         G_CALLBACK(on_function_button_press), node);

        // check to see if we actually need to add an expander
        // only need one if this function calls [or is called by] other functions
        operation = (direction == RIGHT ? FIND_CALLEDBY : FIND_CALLING);
        children = SEARCH_lookup(operation, node->function_name);
        if (children->match_count > 0)
        {
            make_expander_at_position(tcb, col + offset, row, direction);
        }
        SEARCH_free_results(children);

        node = node->next;
    }
}


//********************************************************************************************** 
// shift_column_down
//********************************************************************************************** 
static void shift_column_down(tcb_t *tcb, guint col, guint starting_row, guint amount)
{
    col_list_t elements = tcb->col_list[col];

    // traverse linked list from the back shifting every element down by amount
    column_entry_t *node = elements.member_list_tail;
    while (node != NULL && node->row > starting_row)
    {
        move_table_widget(node->widget, tcb, node->row + amount, col);
        node->row += amount;
        node = node->prev;
    }
}


//********************************************************************************************** 
// column_list_insert
//********************************************************************************************** 
static void column_list_insert(col_list_t *list, GtkWidget *widget, guint row)
{
    column_list_insert_file(list, widget, row, NULL);
}


//********************************************************************************************** 
// column_list_insert_file
// 
// Sorted insert into column list. Column list should also be in row-sorted order
//********************************************************************************************** 
static void column_list_insert_file(col_list_t *list, GtkWidget *widget, guint row, gchar *file)
{
    column_entry_t *work;

    // Create a new column_member list node
    column_entry_t *new_node = g_malloc(sizeof(column_entry_t));
    new_node->widget = widget;
    new_node->row = row;
    new_node->next = NULL;
    new_node->prev = NULL;
    if ( file )     // If a non-NULL filename is provided
        my_asprintf(&(new_node->file_name), "%s", file);
    else
        new_node->file_name = NULL;

    // Insert the newly created node into the list [in row-sorted order]
    if ( list->member_list_head == NULL )
    {
        // List is empty, create a single entry list.
        list->member_list_head = new_node;
        list->member_list_tail = new_node;
    }
    else 
    {
        if ( row < list->member_list_head->row )
        {
            // Insert the new node at the head of the list
            new_node->next = list->member_list_head;
            list->member_list_head->prev = new_node;
            list->member_list_head = new_node;
        }
        else
        {
            // Figure out where in the list to add the new node
            work = list->member_list_head;
            while ( (work->next != NULL) && (row > work->next->row) )
            {
                work = work->next;
            }

            // Advance to the next node in the list
            new_node->next = work->next;
            if (work->next != NULL)
            {
                work->next->prev = new_node;
            }

            work->next = new_node;
            new_node->prev = work;
            if (new_node->next == NULL)
            {
                list->member_list_tail = new_node;
            }
        }
    }
}


//********************************************************************************************** 
// column_list_remove
// 
// Remove node containing widget from the list.  Does not delete the widget
//********************************************************************************************** 
static void column_list_remove(col_list_t *list, GtkWidget *widget)
{
    column_entry_t *work;
    column_entry_t *temp;

    if (list->member_list_head == NULL)
    {
        return;
    }

    // Find the widget to be removed
    // =============================

    // Check the first entry in the list
    if (list->member_list_head->widget == widget)
    {
        // Don't leak memory, free "file_name"
        g_free(list->member_list_head->file_name);
        list->member_list_head->file_name = NULL;

        list->member_list_head = list->member_list_head->next;
        if (list->member_list_head == NULL)
        {
            list->member_list_tail = NULL;
        }
        else
        {
            list->member_list_head->prev = NULL;
        }
    }
    else
    {
        // No match yet, search the remainder of the list
        work = list->member_list_head;
        while (work->next != NULL && work->next->widget != widget)
        {
            work = work->next;
        }
        if (work->next != NULL)
        {
            // work->next is the node to be removed

            // Don't leak memory, free "file_name"
            if (work->next->file_name)
            {
                g_free(work->next->file_name);
                work->next->file_name = NULL;
            }

            temp = work->next;
            work->next = work->next->next;
            if (work->next == NULL)
            {
                list->member_list_tail = work;
            }
            else
            {
                work->next->prev = work;
            }
            g_free(temp);
        }
    }
}


//********************************************************************************************** 
// column_list_get_row
// 
// Return the node at "row".  Returns NULL if there is not a node at "row"
//********************************************************************************************** 
static column_entry_t* column_list_get_row(col_list_t *list, guint row)
{
    column_entry_t *curr;
    if (list == NULL)
        return NULL;
    curr = list->member_list_head;
    while (curr != NULL)
    {
        if (curr->row == row)
        {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

#if 0
static gboolean     column_list_is_sorted(col_list_t *list)
{
    column_entry_t *curr = list->member_list_head;
    while (curr != NULL && curr->next != NULL)
    {
        if (curr->row >= curr->next->row)
        {
            return FALSE;
        }
        curr = curr->next;
    }
    return TRUE;
}

// for debugging purposes, prints the rows in order
// if order is not sorted then something is broken
static void column_list_print_rows(col_list_t *list)
{
    column_entry_t *curr = list->column_member_list;
    while (curr != NULL)
    {
        printf("row: %d\n", curr->row);
        curr = curr->next;
    }
}
#endif


//********************************************************************************************** 
// column_list_get_next
// 
// Return the row number for the next widget after the given row in this column
//********************************************************************************************** 
static int column_list_get_next(col_list_t *list, guint row)
{
    column_entry_t *curr = list->member_list_head;
    while (curr != NULL && curr->next != NULL)
    {
        if (curr->next->row > row)
        {
            return curr->next->row;
        }
        curr = curr->next;
    }
    return -1;
}


//********************************************************************************************** 
// update_table_height
// 
// Update the height of the tree based on left and right height
//********************************************************************************************** 
static void update_table_height(tcb_t *tcb)
{
    if (tcb->left_height >= tcb->right_height)
    {
        tcb->num_rows = tcb->left_height + 1;
    }
    else
    {
        tcb->num_rows = tcb->right_height + 1;
    }
}


//********************************************************************************************** 
// get_function
// 
// Given the position of a clicked expander or collapser, fills fname and file
// with the function name and file name for the corresponding function
//********************************************************************************************** 
static void get_function(tcb_t *tcb, guint col, guint row, dir_e direction, gchar **fname, gchar **file)
{
    guint offset;
    col_list_t *column;
    column_entry_t *function_node;
    GList *children;
    GtkWidget *label;

    offset = (direction == RIGHT ? -1 : 1);

    column = &(tcb->col_list[col + offset]);
    function_node = column_list_get_row(column, row);
    // children is a GList containing only the function label
    children = gtk_container_get_children((GtkContainer *)function_node->widget);
    label = (GtkWidget *)children->data;
    *fname = (gchar *)gtk_label_get_text((GtkLabel *)label);
    *file = function_node->file_name;
}


//**********************************************************************************************
// parse_results
//  
// Parse results into a result_t which can be easily iterated over
//********************************************************************************************** 
static result_t* parse_results(search_results_t *results)
{
    result_t * node,*next_node,*front;
    gchar *curr;
    gboolean first_space;
    gchar *result_ptr = results->start_ptr;

    front = next_node = (result_t *)g_malloc(sizeof(result_t));
    while (result_ptr != results->end_ptr)
    {
        node = next_node;
        node->buf = (gchar *)g_malloc(1024);
        node->file_name = node->buf;
        curr = node->buf;
        first_space = TRUE;
        // newlines seperate each entry
        while (*result_ptr != '\n')
        {
            if (*result_ptr == '|')
            {
                *curr = '\0';
                node->function_name = curr + 1;
            }
            else if (*result_ptr == ' ')
            {
                *curr = '\0';
                if (first_space)
                {
                    node->line_num = curr + 1;
                    first_space = FALSE;
                }
            }
            else
            {
                *curr = *result_ptr;
            }
            result_ptr++;
            curr++;
        }
        result_ptr++;
        if (result_ptr != results->end_ptr)
        {
            next_node = (result_t *)g_malloc(sizeof(result_t));
            node->next = next_node;
        }
    }
    node->next = NULL;

    return front;
}


// OPTIONAL code to remove duplicate entries from a result list
#if 0
static guint results_list_remove_duplicates(result_t *front)
{
    result_t *outer_curr, *inner_curr;
    guint count = 0;

    outer_curr = front;

    while(outer_curr != NULL && outer_curr->next != NULL)
    {
        inner_curr = outer_curr;
        while(inner_curr != NULL && inner_curr->next != NULL)
        {
            while (strcmp(inner_curr->next->function_name, outer_curr->function_name) == 0)
            {
                inner_curr->next = inner_curr->next->next;
                count++;
            }
            inner_curr = inner_curr->next;
        }
        outer_curr = outer_curr->next;
    }
    return count;
}
#endif

// OPTIONAL code to remover entries from a results list whos files do not match
// given file
//
// works for sorting on the first expansion from root but after that is not really
// useful given how Gscope treats file_names on FIND_CALLING searches
#if 0
static guint results_list_filter_file(result_t **front_ptr, gchar *file)
{
    guint count = 0;
    result_t *curr;
    if (*front_ptr == NULL) return 0;

    while (*front_ptr != NULL && strcmp((*front_ptr)->file_name, file) != 0)
    {
        *front_ptr = (*front_ptr)->next;
        count++;
    }
    curr = *front_ptr;
    while (curr != NULL && curr->next != NULL)
    {
        while (curr->next != NULL && strcmp(curr->next->file_name, file) != 0)
        {
            curr->next = curr->next->next;
            count++;
        }
        curr = curr->next;
    }
    return count;
}
#endif

// OPTIONAL code to alphabetically sort a results list
#if 0
static void results_list_sort(result_t *front)
{
    result_t *smallest, *curr, *sorted;
    curr = smallest = front;
    sorted = front;
    while (sorted != NULL && sorted->next != NULL)
    {
        curr = smallest = sorted;
        while (curr->next != NULL)
        {
            if (strcmp(curr->function_name, smallest->function_name) < 0)
            {
                smallest = curr;
            }
            curr = curr->next;
        }
        swap(sorted, smallest);
        sorted = sorted->next;
    }
}
#endif

#if 0
// swap two nodes in a result list
static void swap(result_t *a, result_t *b)
{
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
#endif


//**********************************************************************************************
// add_connectors
// 
// Assumes that all functions have been added and the tree has already been expanded 
// for a given expansion.
//**********************************************************************************************
static void add_connectors(tcb_t *tcb, guint col, guint row, guint count, dir_e direction)
{
    guint i, j;
    col_list_t *column;
    GtkWidget *connector;
    guint bound = col;


    if (direction == RIGHT)
    {
        // add straight connectors to every
        // expander column before col
        for (i = tcb->root_col; i < bound; i++)
        {
            column = &(tcb->col_list[i]);
            if (column->type == EXPANDER)
            {
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

    else    // direction == left
    {
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
    // the newly expanded column needs 3-way connectors for every added function
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


//**********************************************************************************************
// make_end_connector
//**********************************************************************************************
static GtkWidget* make_end_connector(dir_e direction)
{
    GtkWidget *eventbox;
    GtkWidget *image;

    eventbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_name(eventbox, "end_connector");
    gtk_widget_show(eventbox);

    if (direction == LEFT)
    {
        image = create_pixmap(eventbox, "sca_end_branch_left.png");
    }
    else
    {
        image = create_pixmap(eventbox, "sca_end_branch_right.png");
    }

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return eventbox;
}


//**********************************************************************************************
// make_three_way_connector
//**********************************************************************************************
static GtkWidget* make_three_way_connector(dir_e direction)
{
    GtkWidget *eventbox;
    GtkWidget *image;

    eventbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_name(eventbox, "three_connector");
    gtk_widget_show(eventbox);

    if (direction == LEFT)
    {
        image = create_pixmap(eventbox, "sca_mid_branch_left.png");
    }
    else
    {
        image = create_pixmap(eventbox, "sca_mid_branch_right.png");
    }

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return eventbox;
}


//**********************************************************************************************
// make_straight_connector
//**********************************************************************************************
static GtkWidget* make_straight_connector()
{
    GtkWidget *eventbox;
    GtkWidget *image;

    eventbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_name(eventbox, "straight_connector");
    gtk_widget_show(eventbox);
    image = create_pixmap(eventbox, "sca_mid_branch_center.png");

    gtk_widget_show(image);
    gtk_container_add(GTK_CONTAINER(eventbox), image);

    return eventbox;
}


//**********************************************************************************************
// end_of_block
// 
// Determine if the given row contains a function which is the last child in a block 
// of functions
//**********************************************************************************************
static gboolean end_of_block(col_list_t *column, guint row)
{
    column_entry_t *curr;

    if (column->member_list_head->row > row)
    {
        return TRUE;
    }
    curr = column->member_list_head;
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
