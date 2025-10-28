/* return a file's base name from its path name */

//===============================================================
// Defines
//===============================================================

/* fast string equality tests (avoids most strcmp() calls) - test second char to avoid common "leading-/" strings */
#define     STREQUAL(s1, s2)    (*(s1+1) == *(s2+1) && strcmp(s1, s2) == 0)
#define     STRNOTEQUAL(s1, s2) (*(s1) != *(s2) || strcmp(s1, s2) != 0)

/* database output macros that update it's offset */
#define     dbputc(c)           (++dboffset, (void) putc(c, newrefs))

#define     ENCODE  TRUE
#define     DECODE  FALSE

#if defined(GTK3_BUILD) || defined(GTK4_BUILD)
#define lookup_widget(wiget, name) my_lookup_widget(name)
#endif


//===============================================================
// Public Functions
//===============================================================

char       *my_basename(const char *path);
char       *my_dirname(char *path);
void        my_cannotopen(char *file);
GtkWidget  *my_lookup_widget(gchar *name);
void        my_add_widget(gpointer data, gpointer user_data);
pid_t       my_system(gchar *application);
void        my_space_codec(gboolean encode, gchar *my_string);
void        my_chdir(gchar *path);
void        my_asprintf(gchar **str_ptr, const char *fmt, ...);
void        my_start_text_editor(gchar *filename, gchar *linenum);

// GTK Version-variant abstractions
void        my_gtk_entry_set_text(GtkEntry *entry, const gchar *text);
const gchar *my_gtk_entry_get_text(GtkEntry *entry);
gchar       *my_gtk_file_chooser_get_filename(GtkFileChooser *chooser);
void        my_gtk_check_button_set_active(GtkWidget *button, gboolean is_active);
gboolean    my_gtk_check_button_get_active(GtkWidget *button);
void        my_gtk_box_pack_start (GtkBox* box,   GtkWidget* child,   gboolean expand,   gboolean fill,  guint padding);




