
#include <gtk/gtk.h>

#include "utils.h"
#include "search.h"
#include "display.h"


// Temporary GTK4 stub functions for CALLBACKS component
//====================================================
void CALLBACKS_init(GtkWidget *main)
{
    DISPLAY_init(main);
}

GtkWidget *CALLBACKS_get_widget(gchar *widget_name)
{
    if ( strcmp(widget_name, "gscope_main") == 0 )
        my_lookup_widget("gscope_main");
    if ( strcmp(widget_name, "progressbar1") == 0 )
        return(my_lookup_widget(widget_name));

    return(NULL);
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


void on_find_c_identifier_button_clicked(GtkButton       *button,
                                         gpointer         user_data)
{
    printf("Hello from CALLBACK: %s\n", __func__);
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