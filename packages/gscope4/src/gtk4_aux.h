

void DISPLAY_msg(GtkMessageType type, const gchar *message);
void DISPLAY_update_build_progress(guint count, guint max);
void DISPLAY_update_stats_tooltip(gchar *msg);
void DISPLAY_set_cref_current(gboolean up_to_date);
void DISPLAY_update_path_label(gchar *path);

void SEARCH_init();

void GTK4_message_dialog_stub(char *message);
