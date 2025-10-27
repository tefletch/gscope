
void        CALLBACKS_init(GtkWidget *main);
#if defined (GTK4_BUILD)
void        CALLBACKS_register_app(GtkApplication *app);
#endif
GtkWidget   *CALLBACKS_get_widget(gchar *widget_name);

