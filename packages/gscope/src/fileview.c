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

// ---- Defines ----

// ---- Typedefs ----

// ---- local function prototypes ----
static gboolean open_file (GtkSourceBuffer *sBuf, const gchar *filename);
#ifndef GTK4_BUILD  // needs gtk4 menu migration
void ModifyTextPopUp(GtkTextView *textview, GtkMenu *menu, gpointer user_data);
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

#ifdef GTK3_BUILD
/* Not needed for GTK2, since the scrolling can happen correctly before
   the window is first shown.  In GTK3, the scroll will be ignored or
   calculated incorrectly unless the window geometry is known / visible */
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

    // This event is handled -- don't fire it again
    return FALSE;
}
#endif

void FILEVIEW_create(gchar *file_name, gint line)
{
    GtkTextMark *mark;
    GtkTextIter iter;
    GtkTextBuffer *s_buffer;
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
        s_buffer =gtk_text_view_get_buffer(GTK_TEXT_VIEW(windowPtr->srcViewWidget));
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

            // Clear the current buffer text
            #ifndef GTK4_BUILD  // needs gtk4 menu migration
            s_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(windowPtr->srcViewWidget));
            gtk_source_buffer_begin_not_undoable_action (GTK_SOURCE_BUFFER (s_buffer) );
            gtk_text_buffer_set_text (GTK_TEXT_BUFFER (s_buffer), "", 0);
            gtk_source_buffer_end_not_undoable_action (GTK_SOURCE_BUFFER (s_buffer) );
            gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (s_buffer), FALSE);
            #endif
            
            #ifdef GTK3_BUILD
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


static void createFileViewer(ViewWindow *windowPtr)
{
    GtkWidget *window;
    GtkWidget *pScrollWin;
    GtkWidget *sView;
    GtkSourceBuffer *sBuf;
    GtkSourceLanguageManager *lm;
    
    PangoFontDescription *font_desc;
    const gchar * const *lang_dirs;
    #ifndef GTK4_BUILD  // needs gtk4 menu migration
    static guint x = 400;
    static guint y = 400;
    #endif

    // Data required to support non-standard gtksourceview install directory
    lang_dirs = gtk_source_language_manager_get_search_path (gtk_source_language_manager_get_default ());

    // Create a Window
    #ifndef GTK4_BUILD  // needs gtk4 menu migration
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    #else
    window = gtk_window_new();
    #endif
    gtk_window_set_title (GTK_WINDOW (window), windowPtr->filename);
    gtk_widget_set_name(GTK_WIDGET(window), windowPtr->filename);
    gtk_window_set_default_size (GTK_WINDOW(window), 660, 500);
    
    #ifndef GTK4_BUILD  // needs gtk4 menu migration
    gtk_window_move(GTK_WINDOW (window), x, y);
    x+=30;
    y+=30;
    #endif

    windowPtr->topWidget = window;
    g_signal_connect (GTK_WINDOW(window),"destroy",G_CALLBACK(on_fileview_destroy), window);

    GdkPixbuf *fileview_icon_pixbuf = create_pixbuf ("icon-search.png");

    #ifndef GTK4_BUILD  // needs gtk4 menu migration
    if (fileview_icon_pixbuf)
        gtk_window_set_icon (GTK_WINDOW (window), fileview_icon_pixbuf);
    #endif

    /* Create a Scrolled Window that will contain the GtkSourceView */
    #ifndef GTK4_BUILD  // needs gtk4 menu migration
    pScrollWin = gtk_scrolled_window_new (NULL, NULL);
    #else
    pScrollWin = gtk_scrolled_window_new ();
    #endif

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (pScrollWin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* Now create a GtkSourceLanguagesManager */

    // "lang_files_dir" HACK: instead of simply calling gtk_source_languages_manager_new(), we
    // create the language manager with the "lang_files_dir" property set to $PACKAGE_DATA_DIR/gtksourceview/language-specs.
    // This enables gtksourceview to find the language spec files when it is not installed at its "normal" path.

    lm = gtk_source_language_manager_new();
    #ifndef GTK4_BUILD  // needs gtk4 menu migration
    gtk_source_language_manager_set_search_path(lm, (gchar **) lang_dirs);
    #else
    gtk_source_language_manager_set_search_path(lm, (const gchar * const*) lang_dirs);
    #endif

    /* and a GtkSourceBuffer to hold text (similar to GtkTextBuffer) */
    sBuf = GTK_SOURCE_BUFFER (gtk_source_buffer_new (NULL));

    g_object_ref (lm);
    g_object_set_data_full ( G_OBJECT (sBuf), "languages-manager",
                             lm, (GDestroyNotify) g_object_unref);

    /* Create the GtkSourceView and associate it with the buffer */
    sView = gtk_source_view_new_with_buffer(sBuf);
    windowPtr->srcViewWidget = GTK_SOURCE_VIEW(sView);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(sView), FALSE);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(sView), FALSE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(sView), TRUE);
    gtk_source_view_set_show_line_marks(GTK_SOURCE_VIEW(sView), TRUE);
    #ifndef GTK4_BUILD  // needs gtk4 menu migration
    #ifdef GTK3_BUILD
    {
        GtkSourceMarkAttributes *attrs;

        attrs = gtk_source_mark_attributes_new ();
        gtk_source_mark_attributes_set_pixbuf(attrs, fileview_icon_pixbuf);
        gtk_source_view_set_mark_attributes(GTK_SOURCE_VIEW(sView), "LMtype", attrs, 1);
    }
    #else
    gtk_source_view_set_mark_category_icon_from_pixbuf(GTK_SOURCE_VIEW(sView), "LMtype", fileview_icon_pixbuf);
    #endif
    #else
    {
        GtkSourceMarkAttributes *attrs;

        attrs = gtk_source_mark_attributes_new();
        gtk_source_mark_attributes_set_pixbuf(attrs, fileview_icon_pixbuf);
        gtk_source_mark_attributes_render_icon(attrs, GTK_WIDGET(sView), 20);
        gtk_source_view_set_mark_attributes(GTK_SOURCE_VIEW(sView), "LMtype", attrs, 1);
    }
    #endif

    g_object_unref (fileview_icon_pixbuf);  // Free the pixbuf
    #ifndef GTK4_BUILD  // needs gtk4 menu migration
    g_signal_connect (GTK_SOURCE_VIEW(sView),"populate-popup",G_CALLBACK(ModifyTextPopUp),windowPtr);
    #endif

    #ifdef GTK3_BUILD
    g_idle_add (&scroll_view_cb, sView);
    #endif

    /* Set default Font name,size */
    #ifndef GTK4_BUILD
    font_desc = pango_font_description_from_string ("Monospace");
    #ifdef GTK3_BUILD
    gtk_widget_override_font (sView, font_desc);
    #else
    gtk_widget_modify_font (sView, font_desc);
    #endif
    pango_font_description_free (font_desc);
    #else
    {
        GtkCssProvider *provider = gtk_css_provider_new();
        
        gtk_css_provider_load_from_string (provider, "textview { font-family: Monospace; font-size: 8pt; }");
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
    gtk_window_set_child(GTK_WINDOW(pScrollWin), GTK_WIDGET(sView));
    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(pScrollWin));
    #endif

}



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

#if 0   /* Gtksourceview 1.x logic */
    /* get the Language for C source mimetype */
    lm = g_object_get_data (G_OBJECT (sBuf), "languages-manager");

    language = gtk_source_language_manager_get_language_from_mime_type (lm, "text/x-csrc");
    //g_print("Language: [%s]\n", gtk_source_language_get_name(language));
#else   /* Gtksourceview 2.x logic */
    lm = gtk_source_language_manager_get_default();
    if( is_mf_or_makefile(filename) )
        language = gtk_source_language_manager_get_language (lm, "makefile");
    else
        language = gtk_source_language_manager_guess_language(lm, filename, NULL);
#endif

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

    //gtk_text_buffer_set_text (GTK_TEXT_BUFFER (sBuf), "", 0);
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


#ifndef GTK4_BUILD  // needs gtk4 menu migration
void ModifyTextPopUp(GtkTextView *textview, GtkMenu *menu, gpointer user_data)
{
    GList *list;
    GtkWidget *menuitem;
    GtkWidget *ImageMenuItem1, *ImageMenuItem2;
    int i = 0;

    list = gtk_container_get_children (GTK_CONTAINER(menu));

    while( (menuitem = (GtkWidget *) g_list_nth_data( list, i)) != NULL )
    {
        gtk_widget_destroy(menuitem);
        i++;
    }

    g_list_free(list);

    ImageMenuItem1 = gtk_image_menu_item_new_from_stock (GTK_STOCK_EDIT, NULL);
    gtk_container_add (GTK_CONTAINER (menu), ImageMenuItem1);

    ImageMenuItem2 = gtk_image_menu_item_new_from_stock (GTK_STOCK_CLOSE, NULL);
    gtk_container_add (GTK_CONTAINER (menu), ImageMenuItem2);

    g_signal_connect(G_OBJECT(ImageMenuItem1), "activate", G_CALLBACK(on_fileview_start_editor_activate), user_data);
    g_signal_connect(G_OBJECT(ImageMenuItem2), "activate", G_CALLBACK(on_fileview_close_activate), user_data);

    gtk_widget_show_all(GTK_WIDGET(menu));

}
#endif

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
