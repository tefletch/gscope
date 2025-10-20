
#include <gtk/gtk.h>

#include "utils.h"
#include "search.h"
#include "display.h"


// Temporary GTK4 stub functions for SEARCH component
//===================================================

void SEARCH_init()
{
    printf("hello from Gtk4aux: %s\n", __func__);
}


void SEARCH_cleanup_prev()
{
    printf("hello from Gtk4aux: %s\n", __func__);
}



search_results_t *SEARCH_lookup(search_t search_operation, gchar *pattern)
{
    printf("hello from Gtk4aux: %s\n", __func__);
}

void SEARCH_free_results(search_results_t *results)
{
    printf("hello from Gtk4aux: %s\n", __func__);
}

void SEARCH_cancel()
{
    printf("hello from Gtk4aux: %s\n", __func__);
}

void SEARCH_set_cref_status(gboolean status)
{
    printf("hello from Gtk4aux: %s\n", __func__);
}

gboolean SEARCH_save_text(gchar *filename)
{
    printf("hello from Gtk4aux: %s\n", __func__);
}

gboolean SEARCH_save_html(gchar *filename)
{
    printf("hello from Gtk4aux: %s\n", __func__);
}

gboolean SEARCH_save_csv(gchar *filename)
{
    printf("hello from Gtk4aux: %s\n", __func__);
}



// Temporary GTK4 stub functions for BROWSER component
//====================================================
gboolean BROWSER_init(GtkWidget *main)
{    
    printf("hello from Gtk4aux: %s\n", __func__);
}

GtkWidget* BROWSER_create(gchar *name, gchar *root_file, gchar *line_num)
{    
    printf("hello from Gtk4aux: %s\n", __func__);
}



// Temporary GTK4 stub functions for FILEVIEW component
//=====================================================
void FILEVIEW_create(gchar *file_name, gint line)
{
    printf("hello from Gtk4aux: %s\n", __func__);
}



// Temporary GTK4 stub functions for message dialogs
//===================================================

void GTK4_message_dialog(GtkMessageType msg_type, char *message)
{
    GtkWindow   *message_window;
    GtkLabel    *message_label;
    GtkImage    *message_image;

    message_window = GTK_WINDOW(my_lookup_widget("message_window"));
    message_label  = GTK_LABEL(my_lookup_widget("message_label"));
    message_image  = GTK_IMAGE(my_lookup_widget("message_image"));

    switch (msg_type)
    {
        case GTK_MESSAGE_INFO:
            gtk_image_set_from_icon_name(message_image, "dialog-information");
            gtk_window_set_title(message_window, "Info");
        break;

        case GTK_MESSAGE_WARNING:
            gtk_image_set_from_icon_name(message_image, "dialog-warning");
            gtk_window_set_title(message_window, "Warning");
        break;

        case GTK_MESSAGE_QUESTION:
            gtk_image_set_from_icon_name(message_image, "dialog-question");
            gtk_window_set_title(message_window, "Question");
        break;

        case GTK_MESSAGE_ERROR:
            gtk_image_set_from_icon_name(message_image, "dialog-error");
            gtk_window_set_title(message_window, "Error");
        break;

        default:  //other
            gtk_image_set_from_icon_name(message_image, "emblem-important");
            gtk_window_set_title(message_window, "Other");
        break;
    }

    gtk_label_set_markup(message_label, message);

    gtk_widget_show(GTK_WIDGET(message_window));
}


void on_message_button_clicked(GtkButton *button, gpointer user_data)
{
    gtk_widget_hide(my_lookup_widget("message_window"));
}

gboolean on_message_window_close_request(GtkWindow *window, gpointer user_data)
{
    gtk_widget_hide(my_lookup_widget("message_window"));
    return(TRUE);       // Stop other handlers from being invoked for this signal
}