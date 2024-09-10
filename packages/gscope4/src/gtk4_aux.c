
#include <gtk/gtk.h>


// Temporary GTK4 stub functions for DISPLAY component
//====================================================

void DISPLAY_msg(GtkMessageType type, const gchar *message)
{
    printf("Stub call: %s\n", __func__);
    fprintf(stderr,"%s\n", message);
}


void DISPLAY_update_build_progress(guint count, guint max)
{
    gdouble fraction;
    gchar *message;

    fraction = (gdouble)count/(gdouble)max;
    message = g_strdup_printf("Building Cross Reference:  %.0f%% Complete", fraction * 100.0);

    //gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(active_progress_bar), fraction);
    //gtk_progress_bar_set_text(GTK_PROGRESS_BAR(active_progress_bar), message);
    printf("Progress bar update: %s\n", message);

    // Revisit Not "proper" for GTK4
    //while (gtk_events_pending() )
    //    gtk_main_iteration();

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

void GTK4_message_dialog_stub(char *message)
{
    fprintf(stderr, "%s", message);
}


void on_find_c_identifier_button_clicked(GtkButton       *button,
                                         gpointer         user_data)
{
    printf("Hello from CALLBACK: %s\n", __func__);
}

