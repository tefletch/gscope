#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <string.h>

#include "support.h"
#include "callbacks.h"
#include "fileview.h"
#include "app_config.h"
#include "utils.h"

// ---- Defines ----

// ---- Typedefs ----

// ---- local function prototypes ----
#ifndef GTK4_BUILD  // needs gtk4 menu migration
#endif
static void createFileViewer(ViewWindow *windowPtr);
static gboolean is_mf_or_makefile(const char *filename);



// ---- File Globals ----
static ViewWindow *viewListBegin = NULL;
static ViewWindow *viewListEnd   = NULL;


void on_fileview_destroy(GtkWidget *object, gpointer user_data)
{
    ViewWindow *windowPtr;
    ViewWindow *prevPtr;

    // find the window in the list and delete it
    windowPtr = viewListBegin;

    // make sure list is not empty
    if (windowPtr == NULL)
    {
        fprintf(stderr,"Gscope internal error:  Unexpected empty window list\n");
        return;
    }

    prevPtr = NULL;
    while (windowPtr->topWidget != GTK_WIDGET(user_data) )
    {
        prevPtr = windowPtr;
        windowPtr = windowPtr->next;
        if (windowPtr == NULL)
        {
            fprintf(stderr,"Gscope internal error:  Window search failure\n");
            return;
        }
    }

    // Beginning of list case
    if (windowPtr == viewListBegin)
    {
        if (windowPtr == viewListEnd)    // This is the last entry in the list
        {
            viewListEnd = NULL;          // Null-out begin/end pointers (make empty list)
            viewListBegin = NULL;
        }
        else
            viewListBegin = windowPtr->next;  // This is not the last list entry, move the "Begin" pointer forward

        g_free(windowPtr->filename);
        g_free(windowPtr);
    }
    else    // Middle or end of list
    {
        prevPtr->next = windowPtr->next;
        if (windowPtr == viewListEnd)    // If the window is on the end of the list, push back "End" pointer.
            viewListEnd = prevPtr;
        g_free(windowPtr->filename);
        g_free(windowPtr);
    }
    return;
}

#if defined(GTK3_BUILD) || defined(GTK4_BUILD)
/* Not needed for GTK2, since the scrolling can happen correctly before the window is first shown.  
   
   In GTK3 (and beyond), the scroll will be ignored or executed incorrectly because the true source
   window height is not yet known (or up-to-date).
   
   According to GTK "experts":
   To avoid a race condition where the scroll operation is executed while line-height calculations
   are ongoing in an idle handler, "schedule" the "scroll" in an idle callback.

   The scrolling depends on computing the height of the text inside the buffer, since the text view widget needs
   to know where to scroll to as a function of the pixel height of the text. In some cases, that height might not
   be computed before the "scroll" operation is executed

   Implementation example:
        g_idle_add_full(G_PRIORITY_IDLE_LOW, scroll_view_cb, GTK_TEXT_VIEW(windowPtr->srcViewWidget), NULL);
*/

static gboolean scroll_view_cb(gpointer data)
{
    GtkTextView *view;
    GtkTextBuffer *s_buffer;
    GtkTextMark *mark;

    view = GTK_TEXT_VIEW (data);
    s_buffer = gtk_text_view_get_buffer (view);

    // Scroll the cursor line into view when the widget is first shown
    mark = gtk_text_buffer_get_insert (GTK_TEXT_BUFFER (s_buffer));
    gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view), mark, 0, TRUE, 0, 0.5);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(view), TRUE);

    // This event is handled -- don't fire it again
    return FALSE;
}
#endif


static gboolean open_file (GtkSourceBuffer *sBuf, const gchar *filename)
{
    GtkSourceLanguageManager *lm;
    GtkSourceLanguage *language = NULL;
    GError *err = NULL;
    gboolean reading;
    GtkTextIter iter;
    GIOChannel *io;
    gchar *buffer;

    g_return_val_if_fail (sBuf != NULL, FALSE);
    g_return_val_if_fail (filename != NULL, FALSE);
    
    #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
    g_return_val_if_fail (GTK_SOURCE_IS_BUFFER (sBuf), FALSE);
    #else
    g_return_val_if_fail (GTK_IS_SOURCE_BUFFER (sBuf), FALSE);
    #endif

    lm = gtk_source_language_manager_get_default();
    if( is_mf_or_makefile(filename) )
        language = gtk_source_language_manager_get_language (lm, "makefile");
    else
        language = gtk_source_language_manager_guess_language(lm, filename, NULL);

    if (language != NULL)
    {
        gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (sBuf), TRUE);
        gtk_source_buffer_set_language (GTK_SOURCE_BUFFER(sBuf), language);
    }

    /* Now load the file from Disk */
    io = g_io_channel_new_file (filename, "r", &err);
    if (!io)
    {
        g_print("error: %s %s\n", (err)->message, filename);
        return FALSE;
    }

    if (g_io_channel_set_encoding (io, "utf-8", &err) != G_IO_STATUS_NORMAL)
    {
        g_print("err: Failed to set encoding:\n%s\n%s", filename, (err)->message);
        return FALSE;
    }

    #ifndef GTK4_BUILD
    gtk_source_buffer_begin_not_undoable_action (sBuf);
    #else
    gtk_text_buffer_begin_irreversible_action(GTK_TEXT_BUFFER(sBuf));
    #endif

    // Revisit: is this needed?  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (sBuf), "", 0);
    buffer = g_malloc (4096);
    reading = TRUE;
    while (reading)
    {
        gsize bytes_read;
        GIOStatus status;

        status = g_io_channel_read_chars (io, buffer, 4096, &bytes_read, &err);
        switch (status)
        {
            case G_IO_STATUS_EOF: reading = FALSE;

            case G_IO_STATUS_NORMAL:
                if (bytes_read == 0) continue;
                gtk_text_buffer_get_end_iter ( GTK_TEXT_BUFFER (sBuf), &iter);
                gtk_text_buffer_insert (GTK_TEXT_BUFFER(sBuf),&iter,buffer,bytes_read);
                break;

            case G_IO_STATUS_AGAIN: continue;

            case G_IO_STATUS_ERROR:

            default:
                g_print("err (%s): %s", filename, (err)->message);
                /* because of error in input we clear already loaded text */
                gtk_text_buffer_set_text (GTK_TEXT_BUFFER (sBuf), "", 0);

                reading = FALSE;
                break;
        }
    }
    g_free (buffer);

    #ifndef GTK4_BUILD
    gtk_source_buffer_end_not_undoable_action (sBuf);
    #else
    gtk_text_buffer_end_irreversible_action(GTK_TEXT_BUFFER(sBuf));
    #endif

    g_io_channel_unref (io);

    if (err)
    {
        g_error_free (err);
        return FALSE;
    }

    gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (sBuf), FALSE);

    g_object_set_data_full (G_OBJECT (sBuf),"filename", g_strdup (filename),
                            (GDestroyNotify) g_free);

    return TRUE;
}

void FILEVIEW_create(gchar *file_name, gint line)
{
    GtkTextMark *mark;
    GtkTextIter iter;
    GtkTextBuffer *s_buffer = NULL;
    ViewWindow *windowPtr;
    gboolean   sameName;

    // Search the view window list looking for file_name
    sameName = FALSE;
    for (windowPtr = viewListBegin; windowPtr != NULL; windowPtr = windowPtr->next)
    {
        if (strcmp(file_name, windowPtr->filename) == 0)
        {
            sameName = TRUE;
            break;
        }
    }

    if (sameName)    // --- A view window is already open for this file ---
    {
        s_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(windowPtr->srcViewWidget));
    }
    else
    {   // --- No view already open for this file ---
        if ( settings.reuseWin && (viewListEnd != NULL) )
        {
            // --- Reuse the last window on the list for viewing this file ---
            windowPtr = viewListEnd;
            g_free(windowPtr->filename);
            windowPtr->filename = strdup(file_name);
            gtk_window_set_title (GTK_WINDOW (windowPtr->topWidget), windowPtr->filename);
            gtk_widget_set_name(GTK_WIDGET(windowPtr->topWidget), windowPtr->filename);
            s_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(windowPtr->srcViewWidget));

            // Clear the current buffer text
            #ifndef GTK4_BUILD  // needs gtk4 menu migration
            gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (s_buffer) );
            gtk_text_buffer_set_text (GTK_TEXT_BUFFER (s_buffer), "", 0);
            gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (s_buffer) );
            gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (s_buffer), FALSE);
            #endif
            
            #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
            g_idle_add (&scroll_view_cb, windowPtr->srcViewWidget);
            #endif
        }
        else
        {
            // We could not find a window displaying this file.
            // And we are not to re-use an old window
            // Create a new window and add it to the list.

            windowPtr = (ViewWindow *) g_malloc(sizeof(ViewWindow));
            windowPtr->filename = strdup(file_name);

            // create a new GtkSourceView window
            createFileViewer(windowPtr);
            s_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(windowPtr->srcViewWidget));

            // Create a marker to indicate the matched line
            gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (s_buffer), &iter, line - 1);
            gtk_source_buffer_create_source_mark (GTK_SOURCE_BUFFER (s_buffer), "LineMarker", "LMtype", &iter);

            if (viewListBegin == NULL)
            {
                // This is the first window in the list
                 windowPtr->next = NULL;
                 viewListBegin = viewListEnd = windowPtr;
            }
            else
            {
                // Add the window to the end of the list
                windowPtr->next = NULL;
                if (viewListEnd) viewListEnd->next = windowPtr;
                viewListEnd = windowPtr;
            }
        }

        // Now load the file
        open_file ( GTK_SOURCE_BUFFER (s_buffer), file_name);
    }
    // Store the current line number (for future reference)
    windowPtr->line = line;

    // Raise the window to the top of the pile
    gtk_window_present(GTK_WINDOW(windowPtr->topWidget));

    gtk_widget_show (windowPtr->topWidget);

    /* move cursor to the specified line */
    gtk_text_buffer_get_iter_at_line (GTK_TEXT_BUFFER (s_buffer), &iter, line - 1);
    gtk_text_buffer_place_cursor (GTK_TEXT_BUFFER (s_buffer), &iter);

    // Get the mark that represents the curent cursor position
    mark = gtk_text_buffer_get_insert(GTK_TEXT_BUFFER (s_buffer));
    // Scroll to the specified line number
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(windowPtr->srcViewWidget), mark, 0, TRUE, 0, 0.5);

    /* Move the search marker to the specified line */
    gtk_text_buffer_move_mark_by_name (GTK_TEXT_BUFFER (s_buffer), "LineMarker", &iter);

    return;
}

// Fileview context menu action callbacks
#ifndef GTK4_BUILD
static void on_fileview_start_editor_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
static void on_fileview_start_editor_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    gchar *line_str;
    ViewWindow *windowPtr = user_data;

    // Get the current line number from the ViewWindow structure
    my_asprintf(&line_str, "%d", windowPtr->line);
    my_start_text_editor(windowPtr->filename, line_str);
    g_free(line_str);

    #ifndef GTK4_BUILD
    gtk_widget_destroy(GTK_WIDGET(windowPtr->topWidget));
    #else
    gtk_window_destroy(GTK_WINDOW(windowPtr->topWidget));
    #endif
}


#ifndef GTK4_BUILD
static void on_fileview_close_activate(GtkMenuItem *menuitem, gpointer user_data)
#else
static void on_fileview_close_activate(GSimpleAction *action, GVariant *parameter, gpointer user_data)
#endif
{
    ViewWindow *windowPtr = user_data;

    #ifndef GTK4_BUILD
    gtk_widget_destroy(GTK_WIDGET(windowPtr->topWidget));
    #else
    gtk_window_destroy(GTK_WINDOW(windowPtr->topWidget));
    #endif
}


#ifndef GTK4_BUILD  // Pre-GTK4 modify text popup (function + callbacks)

static void show_custom_menu(GdkEvent *event, gpointer user_data)
{
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *menu_item1 = gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT, NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item1);
        g_signal_connect(menu_item1, "activate", G_CALLBACK(on_fileview_start_editor_activate), user_data);        

        GtkWidget *menu_item2 = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item2);
        g_signal_connect(menu_item2, "activate", G_CALLBACK(on_fileview_close_activate), user_data);        
                
        gtk_widget_show_all(menu);

        #ifndef GTK3_BUILD  // GTK2
        // 2. Pop up the custom menu
        gtk_menu_popup(GTK_MENU(menu), 
            NULL,          /* parent_menu_shell */
            NULL,          /* parent_menu_item */
            NULL,          /* GtkMenuPositionFunc (NULL defaults to pointer context) */
            NULL,          /* user_data */
            0,             /* button that triggered the event (e.g., event->button or 0) */
            gtk_get_current_event_time());  /* time of the event (e.g., event->time or gtk_get_current_event_time()) */
        
        #else   // GTK3
         // 2. Pop up the custom menu
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
        #endif
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
 {
    // Check if it's a right-click (button 3) and a single click
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) 
    {
        show_custom_menu((GdkEvent *)event, user_data);
        // Return TRUE to stop further propagation and block the default menu
        return TRUE; 
    }

    // Return FALSE for all other clicks (left/middle) to keep default behavior
    return FALSE; 
}

static gboolean on_popup_menu(GtkWidget *widget, gpointer user_data)
{
    show_custom_menu(NULL, user_data);
    return TRUE; // Blocks default keyboard menu
}

void modify_text_popup(ViewWindow *window_ptr)
{
    // Intercept right click (and keyboard shortcut <shift>F10)
    //=========================================================
    g_signal_connect(window_ptr->srcViewWidget, "button-press-event", G_CALLBACK(on_button_press), window_ptr);
    g_signal_connect(window_ptr->srcViewWidget, "popup-menu", G_CALLBACK(on_popup_menu), window_ptr);
}

#else   // GTK4 modify text popup (function + callbacks)

// GTK4 does not make this sort of functionality easy or straightforward. After struggling for days, the 
// Google AI overview to the following query produced sample solutions that actually worked: 
//   "gtk4 override gtktextview action menu.popup"

static void on_right_click_pressed(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    GtkWidget *popover = GTK_WIDGET(user_data);

    // Only intercept right-clicks (Button 3)
    if (gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture)) == 3)
    {
        
        // Point the popover exactly where the user clicked
        GdkRectangle rect = { (int)x, (int)y, 1, 1 };
        gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
        
        // Show the custom menu
        gtk_popover_popup(GTK_POPOVER(popover));

        // Stop event propagation so the native menu.popup doesn't fire
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static gboolean on_shortcut_activated (GtkWidget *widget, GVariant *args, gpointer user_data) 
{
    GtkWidget *popover = GTK_WIDGET(user_data);
    
    // Point the popover to the center of the window
    gint width = gtk_widget_get_width(widget);
    gint height = gtk_widget_get_height(widget);
    gint x = width / 2;
    gint y = height / 2;

    GdkRectangle rect = { x, y, 1, 1 };
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
        
    // Show the custom menu
    gtk_popover_popup(GTK_POPOVER(popover));

    return TRUE; // Handled
}

static void modify_text_popup(ViewWindow *window_ptr)
{
    // Intercept right click (and Keyboard shortcut <shift>F10)
    //=========================================================
    
    GSimpleActionGroup *action_group = g_simple_action_group_new ();    // Create an action group

    // Create context menu actions
    GSimpleAction *edit_action = g_simple_action_new("edit", NULL);
    g_signal_connect(edit_action, "activate", G_CALLBACK(on_fileview_start_editor_activate), window_ptr);
    GSimpleAction *close_action = g_simple_action_new("close", NULL);
    g_signal_connect(close_action, "activate", G_CALLBACK(on_fileview_close_activate), window_ptr);
    
    // Map the new actions to the action map
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(edit_action));
    g_action_map_add_action(G_ACTION_MAP(action_group), G_ACTION(close_action));
    g_object_unref(edit_action);
    g_object_unref(close_action);
    
    // Add new actions to the grouop
    gtk_widget_insert_action_group(GTK_WIDGET(window_ptr->srcViewWidget), "view", G_ACTION_GROUP(action_group));
    g_object_unref(action_group);
    
    // Define a custom popover menu
    GMenu *menu = g_menu_new();
    g_menu_append(menu, "Edit", "view.edit");
    g_menu_append(menu, "Close", "view.close");

    GtkWidget *custom_popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    gtk_widget_set_parent(custom_popover, GTK_WIDGET(window_ptr->srcViewWidget));
    gtk_popover_set_has_arrow(GTK_POPOVER(custom_popover), FALSE);

    // Suppress the default handler using a gesture
    GtkGesture *gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 0); // Listen to all buttons

    // CAPTURE phase intercepts the event before the textview handles it natively
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(gesture), GTK_PHASE_CAPTURE);

    g_signal_connect(gesture, "pressed", G_CALLBACK(on_right_click_pressed), custom_popover);
    gtk_widget_add_controller(GTK_WIDGET(window_ptr->srcViewWidget), GTK_EVENT_CONTROLLER(gesture));

    // Intercept the keyboard shortcut
    GtkShortcutTrigger *trigger = gtk_shortcut_trigger_parse_string("<Shift>F10");
    GtkShortcutAction  *action = gtk_callback_action_new (on_shortcut_activated, custom_popover, NULL);
    GtkShortcut        *shortcut = gtk_shortcut_new (trigger, action);
    GtkEventController *controller = gtk_shortcut_controller_new ();
    
    gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);
    gtk_widget_add_controller (GTK_WIDGET(window_ptr->srcViewWidget), controller);
}
#endif  // GTK4 modify text popup


static void createFileViewer(ViewWindow *windowPtr)
{
    GtkWidget *window;
    GtkWidget *pScrollWin;
    GtkWidget *sView;
    GtkSourceBuffer *sBuf;
    GtkSourceLanguageManager *lm;
    
    PangoFontDescription *font_desc;
    const gchar * const *lang_dirs;
    #ifndef GTK4_BUILD
    static guint x = 400;
    static guint y = 400;
    #endif

    // Data required to support non-standard gtksourceview install directory
    lang_dirs = gtk_source_language_manager_get_search_path (gtk_source_language_manager_get_default ());

    // Create a Window
    #ifndef GTK4_BUILD
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    #else
    window = gtk_window_new();
    #endif
    
    gtk_window_set_title (GTK_WINDOW (window), windowPtr->filename);
    gtk_widget_set_name(GTK_WIDGET(window), windowPtr->filename);
    gtk_window_set_default_size (GTK_WINDOW(window), 660, 500);
    
    #ifndef GTK4_BUILD
    gtk_window_move(GTK_WINDOW (window), x, y);
    x+=30;
    y+=30;
    #else
    // There is a way to simulate windows_move using X11, but not for Wayland.
    // Just let the window manager place the new windows (at least for now)
    #endif

    windowPtr->topWidget = window;
    g_signal_connect (GTK_WINDOW(window),"destroy",G_CALLBACK(on_fileview_destroy), window);

    // According to documentation, all GTK variants support gtk_window_set_icon_name()
    // gtk_window_set_icon (GTK_WINDOW (window), fileview_icon_pixbuf);  // direct-set call obsoleted 10/17/26
    gtk_window_set_icon_name(GTK_WINDOW (window), "system-search");

    // Create a Scrolled Window that will contain the GtkSourceView
    #ifndef GTK4_BUILD
    pScrollWin = gtk_scrolled_window_new (NULL, NULL);
    #else
    pScrollWin = gtk_scrolled_window_new ();
    #endif

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pScrollWin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    #if defined(GTK3_BUILD) || defined (GTK4_BUILD)
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(pScrollWin), FALSE);
    #endif

    
    // Create a GtkSourceLanguagesManager
        // "lang_files_dir" HACK: instead of simply calling gtk_source_languages_manager_new(), we
        // create the language manager with the "lang_files_dir" property set to $PACKAGE_DATA_DIR/gtksourceview/language-specs.
        // This enables gtksourceview to find the language spec files when it is not installed at its "normal" path.

    lm = gtk_source_language_manager_new();
    #ifndef GTK4_BUILD // Actually gtksourceview-2 and gtksourceview-3
    gtk_source_language_manager_set_search_path(lm, (gchar **) lang_dirs);
    #else   // Actually gtksourceview-5
    gtk_source_language_manager_set_search_path(lm, (const gchar * const*) lang_dirs);
    #endif

    // Add a GtkSourceBuffer to hold text (similar to GtkTextBuffer) */
    sBuf = GTK_SOURCE_BUFFER (gtk_source_buffer_new (NULL));

    g_object_ref (lm);
    g_object_set_data_full ( G_OBJECT (sBuf), "languages-manager", lm, (GDestroyNotify) g_object_unref);

    /* Create the GtkSourceView and associate it with the buffer */
    sView = gtk_source_view_new_with_buffer(sBuf);
    windowPtr->srcViewWidget = GTK_SOURCE_VIEW(sView);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(sView), FALSE);
    // Revisit:  May want to add context menu (or other method) to enable/disable "show line numbers"
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(sView), TRUE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(sView), TRUE);  // only effective for GTK2, harmless for GTK3 and later
    gtk_source_view_set_show_line_marks(GTK_SOURCE_VIEW(sView), TRUE);

    // Configure the source-mark icon
    GdkPixbuf *mark_pixbuf = create_pixbuf ("source-mark.png");
    #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
    {
        GtkSourceMarkAttributes *attrs;

        attrs = gtk_source_mark_attributes_new();
        gtk_source_mark_attributes_set_pixbuf(attrs, mark_pixbuf);
        gtk_source_mark_attributes_render_icon(attrs, GTK_WIDGET(sView), 20);
        gtk_source_view_set_mark_attributes(GTK_SOURCE_VIEW(sView), "LMtype", attrs, 1);
    }
    #else   // GTK2
    gtk_source_view_set_mark_category_icon_from_pixbuf(GTK_SOURCE_VIEW(sView), "LMtype", mark_pixbuf);
    #endif
    g_object_unref (mark_pixbuf);  // Free the pixbuf
    

    // Schedule a deferred callback to avoid race condition between scrolling and 'final/true' souce window height calculations
    #if defined(GTK3_BUILD) || defined(GTK4_BUILD)
    g_idle_add (&scroll_view_cb, sView);
    #endif


    /* Set default Font name,size */
    #ifndef GTK4_BUILD
    font_desc = pango_font_description_from_string ("Monospace");   // GTK2 and GTK3
    #ifdef GTK3_BUILD
    gtk_widget_override_font (sView, font_desc);    // GTK3
    #else
    gtk_widget_modify_font (sView, font_desc);  // GTK2
    #endif
    pango_font_description_free (font_desc);    // GTK2 and GTK3
    #else   // GTK4
    {
        GtkCssProvider *provider = gtk_css_provider_new();
        
        // Revisit: may want to make the font-size user selectable (larger/smaller)
        gtk_css_provider_load_from_string (provider, "textview { font-family: Monospace; }");
        //gtk_css_provider_load_from_string (provider, "textview { font-family: Monospace; font-size: 8pt; }");
        gtk_style_context_add_provider (gtk_widget_get_style_context(sView), GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);
    }
    #endif

    #ifndef GTK4_BUILD
    /* Attach the GtkSourceView to the scrolled Window */
    gtk_container_add (GTK_CONTAINER (pScrollWin), GTK_WIDGET (sView));
    /* And the Scrolled Window to the main Window */
    gtk_container_add (GTK_CONTAINER (window), pScrollWin);
    gtk_widget_show_all (pScrollWin);
    #else
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pScrollWin), GTK_WIDGET(sView));
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(pScrollWin));
    #endif

    modify_text_popup(windowPtr);
}


static gboolean is_mf_or_makefile(const char *filename)
{
    char *ptr;

    // Check if file is a Makefile
    if( (ptr = g_strrstr( filename, "Makefile")) != NULL  && ( *(ptr + 8) == '\0') )
    {
        return (TRUE);
    }

    //Check if file is a ".mf" file
    ptr = strrchr(filename, (int) '.');

    if(ptr)
    {
        if( strcmp(++ptr, "mf") == 0)
        {
            return (TRUE);
        }
    }

    return (FALSE);
}
