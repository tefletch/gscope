/* return a file's base name from its path name */

#define     ENCODE  TRUE
#define     DECODE  FALSE

char       *my_basename(const char *path);
char       *my_dirname(char *path);
void        my_cannotopen(char *file);
GtkWidget  *my_lookup_widget(gchar *name);
void        my_add_widget(gpointer data, gpointer user_data);
pid_t       my_system(gchar *application);
void        my_space_codec(gboolean encode, gchar *my_string);

