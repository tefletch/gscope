
#include <gtk/gtk.h>

#include "utils.h"


// Temporary GTK4 stub functions for DISPLAY component
//====================================================

void DISPLAY_msg(GtkMessageType type, const gchar *message)
{
    printf("Stub call: %s\n", __func__);
    fprintf(stderr,"%s\n", message);
}

// Keep this stub until the equivalent function in display.c is refactored  RENAME to DISPLAY_progress() once all gscope versions refactor complete
void DISPLAY_update_build_progress(GtkWidget *bar, char *progress_msg, guint count, guint max)
{
    gdouble fraction;
    gchar *message;

    fraction = (gdouble)count/(gdouble)max;

    if ( progress_msg )
    {
        message = g_strdup_printf("%s %.0f%% Complete", progress_msg, fraction * 100.0);
        //printf("fraction = %.0f\n", fraction);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(bar), message);
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(bar), fraction);
    

    // Revisit Not "proper" for GTK4
    while ( g_main_context_pending( NULL/*default context*/  ) )
        g_main_context_iteration( NULL/*default context*/, TRUE/*may block*/);

    g_free(message);
}


void DISPLAY_update_stats_tooltip(gchar *msg)
{
    printf("Stub call: %s\n", __func__);
}


void DISPLAY_set_cref_current(gboolean up_to_date)
{
    printf("Stub call: %s\n", __func__);
}

void DISPLAY_update_path_label(gchar *path)
{
    printf("Stub call: %s\n", __func__);
}


// Temporary GTK4 stub functions for SEARCH component
//===================================================

void SEARCH_init()
{
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
            gtk_window_set_title(message_window, "Gscope Info");
        break;

        case GTK_MESSAGE_WARNING:
            gtk_image_set_from_icon_name(message_image, "dialog-warning");
            gtk_window_set_title(message_window, "Gscope Warning");
        break;

        case GTK_MESSAGE_QUESTION:
            gtk_image_set_from_icon_name(message_image, "dialog-question");
            gtk_window_set_title(message_window, "Gscope Question");
        break;

        case GTK_MESSAGE_ERROR:
            gtk_image_set_from_icon_name(message_image, "dialog-error");
            gtk_window_set_title(message_window, "Gscope Error");
        break;

        default:  //other
            gtk_image_set_from_icon_name(message_image, "emblem-important");
            gtk_window_set_title(message_window, "Gscope Other");
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