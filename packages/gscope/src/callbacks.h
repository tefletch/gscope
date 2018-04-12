#include <gtk/gtk.h>

void CALLBACKS_init(GtkWidget *main);


void
on_window1_destroy                     (GtkWidget        *widget,
                                        gpointer         user_data);

gboolean
on_window1_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_rebuild_database1_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_list_all_functions1_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_quit1_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_usage1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_setup1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_find_c_identifier_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_find_definition_of_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_find_functions_called_by_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_find_functions_calling_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_find_text_string_button_press_event (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_find_egrep_pattern_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_find_files_button_press_event       (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_find_files_including_button_press_event
                                        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_aboutdialog1_response               (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);

void
on_preferences_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_ignorecase_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_useeditor_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_reusewin_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_retaininput_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_cancel_button_button_press_event    (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_find_c_identifier_button1_clicked   (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_definition_of_button_clicked   (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_functions_called_by_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_functions_calling_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_text_string_button_clicked     (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_egrep_pattern_button_clicked   (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_files_button_clicked           (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_files_including_button_enter   (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_files_including_button_clicked (GtkButton       *button,
                                        gpointer         user_data);

void
on_cancel_button_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_find_c_identifier_button_clicked    (GtkButton       *button,
                                        gpointer         user_data);

void
on_smartquery_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_history_treeview_button_press_event (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_treeview1_row_activated             (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);

void 
on_fileview_start_editor_activate      (GtkMenuItem *menuitem, 
                                        gpointer user_data);

void 
on_fileview_close_activate             (GtkMenuItem *menuitem, 
                                        gpointer user_data);


void
on_retain_text_checkbutton_toggled     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_ignore_case_checkbutton_toggled     (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_use_viewer_radiobutton_toggled      (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_reuse_window_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_use_editor_radiobutton_toggled      (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_preferences_dialog_close_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_gscope_preferences_destroy_event    (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_gscope_preferences_delete_event     (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_editor_command_entry_changed        (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_editor_command_entry_focus_out_event
                                        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_no_rebuild_radiobutton_activate     (GtkButton       *button,
                                        gpointer         user_data);

void
on_rebuild_radiobutton_activate        (GtkButton       *button,
                                        gpointer         user_data);

void
on_force_rebuild_radiobutton_activate  (GtkButton       *button,
                                        gpointer         user_data);

void
on_no_rebuild_radiobutton_toggled      (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_rebuild_radiobutton_toggled         (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_force_rebuild_radiobutton_toggled   (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_truncate_symbols_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_compress_symbols_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_suffix_entry_changed                (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_suffix_entry_focus_out_event        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_typeless_entry_changed              (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_typeless_entry_focus_out_event      (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_ignored_entry_changed               (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_ignored_entry_focus_out_event       (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_source_directory_browse_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_folder_chooser_dialog_response      (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);

void
on_source_entry_changed                (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_source_entry_focus_out_event        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_include_entry_changed               (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_include_entry_focus_out_event       (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_name_entry_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_name_entry_focus_out_event          (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_file_name_browse_button_clicked     (GtkButton       *button,
                                        gpointer         user_data);

void
on_initial_context_browse_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_cref_entry_changed                  (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_cref_entry_focus_out_event          (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_cross_reference_browse_button_clicked
                                        (GtkButton       *button,
                                        gpointer         user_data);

void
on_search_log_entry_changed            (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_search_log_entry_focus_out_event    (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_search_log_button_clicked           (GtkButton       *button,
                                        gpointer         user_data);

void
on_open_file_chooser_dialog_response   (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);

void
on_search_log_browse_button_clicked    (GtkButton       *button,
                                        gpointer         user_data);

void
on_output_file_chooser_dialog_response (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);


void
on_session_statistics_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_quit_confirm_dialog_response        (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);

void
on_confirmation_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_confirm_exit_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

gboolean
on_stats_dialog_delete_event           (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_stats_dialog_closebutton_clicked    (GtkButton       *button,
                                        gpointer         user_data);

void
on_include_dirlist_entry_changed       (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_include_dirlist_entry_focus_out_event
                                        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_search_root_entry_changed           (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_search_root_entry_focus_out_event   (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_show_includes_checkbutton_toggled   (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_show_includes_checkbutton_toggled   (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_recursive_search_mode_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_save_query_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_load_query_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_reset_query_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_delete_history_file_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_clear_query_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_showicons_checkbutton_toggled       (GtkToggleButton *togglebutton,
                                        gpointer         user_data);


void
on_session_info_button_clicked         (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_treeview1_button_press_event        (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_single_click_checkbutton_toggled    (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_retain_text_failed_search_checkbutton_toggled
                                        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_save_results_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);


void
on_save_results_file_chooser_dialog_response
                                        (GtkDialog       *dialog,
                                        gint             response_id,
                                        gpointer         user_data);

void
on_overview_wiki_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_usage_wiki_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_configure_wiki_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_prefs_help_cross_button_clicked     (GtkButton       *button,
                                        gpointer         user_data);

void
on_prefs_help_source_button_clicked    (GtkButton       *button,
                                        gpointer         user_data);

void
on_prefs_help_file_button_clicked      (GtkButton       *button,
                                        gpointer         user_data);

void
on_prefs_help_general_button_clicked   (GtkButton       *button,
                                        gpointer         user_data);

void
on_prefs_help_search_button_clicked    (GtkButton       *button,
                                        gpointer         user_data);

void
on_release_wiki_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_cref_update_button_clicked          (GtkButton       *button,
                                        gpointer         user_data);


void
on_clear_query_button_clicked          (GtkButton       *button,
                                        gpointer         user_data);

void
on_autogen_cache_path_entry_changed    (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_autogen_cache_path_entry_focus_out_event
                                        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_autogen_enable_checkbutton1_toggled (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
on_autogen_search_root_entry1_changed  (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_autogen_search_root_entry1_focus_out_event
                                        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_autogen_suffix_entry1_changed       (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_autogen_suffix_entry1_focus_out_event
                                        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_autogen_cmd_entry1_changed          (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_autogen_cmd_entry1_focus_out_event  (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

gboolean
on_autogen_id_entry1_focus_out_event   (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_autogen_id_entry1_changed           (GtkEditable     *editable,
                                        gpointer         user_data);

void
on_autogen_cache_threshold_spinbutton_changed
                                        (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_autogen_cache_threshold_spinbutton_focus_out_event
                                        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

gboolean
on_save_results_file_chooser_dialog_delete_event
                                        (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_aboutdialog1_delete_event           (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_folder_chooser_dialog_delete_event  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_open_file_chooser_dialog_delete_event
                                        (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_output_file_chooser_dialog_delete_event
                                        (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_quit_confirm_dialog_delete_event    (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_treeview1_popup_menu                (GtkWidget       *widget,
                                        gpointer         user_data);

void
on_terminal_app_entry_changed          (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_terminal_app_entry_focus_out_event  (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_file_manager_app_entry_changed      (GtkEditable     *editable,
                                        gpointer         user_data);

gboolean
on_file_manager_app_entry_focus_out_event
                                        (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data);

void
on_list_autogen_errors_activate        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
