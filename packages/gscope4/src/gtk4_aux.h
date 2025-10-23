
void SEARCH_init();
void SEARCH_cleanup_prev();
void SEARCH_set_cref_status(gboolean status);

void GTK4_message_dialog(GtkMessageType msg_type, char *message);

void on_message_button_clicked(GtkButton *button, gpointer user_data);
gboolean on_message_window_close_request(GtkWindow *window, gpointer user_data);
void SEARCH_stats(stats_struct_t *sptr);
gboolean SEARCH_get_cref_status();
void SEARCH_check_cref(void);
void SEARCH_cleanup(void);

