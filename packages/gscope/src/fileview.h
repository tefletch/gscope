
#if defined(GTK3_BUILD) || defined(GTK4_BUILD)
#include <gtksourceview/gtksource.h>
#include <gtksourceview/gtksourcemarkattributes.h>
#else   // GTK2
#include <gtksourceview/gtksourceview.h>
#endif

#include <gtksourceview/gtksourcebuffer.h>
#include <gtksourceview/gtksourcelanguage.h>
#include <gtksourceview/gtksourcelanguagemanager.h>


// ---- Typedefs ----
typedef struct _viewWindow
{
    GtkWidget     *topWidget;
    GtkSourceView *srcViewWidget;
    gchar         *filename;
    gint          line;
    struct        _viewWindow *next;
} ViewWindow;


/* Create a new file-view window */
void FILEVIEW_create(gchar *file_name, gint line);

