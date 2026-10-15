#include <gtk/gtk.h>

static void grid_init(GtkWidget *grid)
{
    GtkWidget *label;
    GtkWidget *button1, *button2;
    char *var_string;
    guint count = 0;
    guint row, col;

    // Create filler columns on both ends of the table
    button1 = gtk_button_new_with_label("Filler1");
    gtk_widget_set_focus_on_click(button1, FALSE);
    gtk_widget_set_hexpand(button1, TRUE);
    gtk_widget_set_halign(button1, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid), button1, 0, 0, 1, 1);

    button2 = gtk_button_new_with_label("Filler2");
    gtk_widget_set_focus_on_click(button2, FALSE);
    gtk_widget_set_hexpand(button2, TRUE);
    gtk_widget_set_halign(button2, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid), button2, 5, 0, 1, 1);
    
    // Populate the grid
    for (row = 0; row < 8; row++)
    {
        for (col = 1; col < 4; col++)
        {
            asprintf(&var_string, "kinda long test string %d", count++);
            label = gtk_label_new(var_string);
            g_free(var_string);

            gtk_label_set_max_width_chars(GTK_LABEL(label), 4);
            gtk_label_set_width_chars(GTK_LABEL(label), 4);
            gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
            gtk_grid_attach(GTK_GRID(grid), label, col, row, 1, 1);
        }
    }

}

// Callback: horizontal scale "value-changed"
static void on_value_changed(GtkRange *range, gpointer user_data)
{
    guint row;
    GtkWidget *child;
    GtkWidget *grid = (GtkWidget *) user_data;

    gdouble value = gtk_range_get_value(range);
    g_print("Value: %.0f\n", value);

    for (row = 0; row < 8; row++)
    {
        child = gtk_grid_get_child_at(GTK_GRID(grid), 2, row);
        gtk_label_set_max_width_chars(GTK_LABEL(child), value);
        gtk_label_set_width_chars(GTK_LABEL(child), value);
    }
}

// 'Activated' Callback
static void activate (GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *scale;
    GtkWidget *scrolledwindow;
    GtkWidget *viewport;
    GtkWidget *grid;

    // Create application window
    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "GTK4 Grid");
    gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);

    // Insert vertical table
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    // Create a scroll window and insert it into the table
    scrolledwindow = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scrolledwindow), FALSE);
    gtk_box_append(GTK_BOX (vbox), scrolledwindow);

    // Create a viewport and insert it into the scrolledwindow
    viewport = gtk_viewport_new(NULL, NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow), viewport);

    // Create a grid and insert it into the scrolledwindow viewport
    grid = gtk_grid_new();
    gtk_widget_set_vexpand(grid, TRUE);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_viewport_set_child(GTK_VIEWPORT(viewport), grid);
    //gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow), grid);

    // Create a horizontal scale and insert it into the table
    scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 4.0, 80.0, 1.0);
    gtk_scale_set_digits(GTK_SCALE(scale), 0);
    gtk_range_set_value(GTK_RANGE(scale), 10.0);
    gtk_widget_set_hexpand(scale, TRUE);
    g_signal_connect(scale, "value-changed", G_CALLBACK(on_value_changed), grid);
    gtk_box_append(GTK_BOX(vbox), scale);

    // Populate the grid
    grid_init(grid);

    // Present the window to the user
    gtk_window_present (GTK_WINDOW (window));
}

int main (int argc, char **argv) {
    GtkApplication *app;
    int status;

    // Create a new GtkApplication instance
    app = gtk_application_new ("grid.demo", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);

    // Run the application
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    return status;
}
