#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <sys/wait.h>

#include "utils.h"
#include "app_config.h"


//----------------------- File-Private Globals ------------------------
static GHashTable   *hash_table = NULL;

//------------------- Private Function Prototypes ---------------------
static void _handle_sigchld(int sig);
static void _register_child_handler();



//----------------- Private (non-API) Functions ----------------------

static void _handle_sigchld(int sig)
{
    int saved_errno = errno;  // Preserve errno -- it might be changed by waitpid
    while (waitpid(-1, 0, WNOHANG) > 0) {}    // Process terminated child
    errno = saved_errno;      // Pevent interference with code outside the handler.
}


static void _register_child_handler()
{
    struct sigaction sa;

    sa.sa_handler = &_handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    
    if (sigaction(SIGCHLD, &sa, 0) == -1)
    {
        perror(0);
        exit(1);
    }
}



//----------------------- Public Functions ----------------------------

// Return a file's base name from its path name
// Boundary Condition behavior:
//    No '/'        - return pointer to path
//                    (e.g. "basic_file_name")
//    No basename   - return pointer to null termination of path [null string]
//                    (e.g. ".../dir1/dir2/" or "/")

char *my_basename(const char *path)
{
    char    *base = strrchr(path, '/');
    return( base ? base + 1 : (char *) path);
}



// Return a file's dirname
//
// Warning this function modifies the path string passed to it.
// Callers should make a copy of their "path" string and use that
// to make a call to this function.
// 
// Typical Caller Usage:
//
//     pcopy = strdup(pathname);  // aternatively, caller can pass a statically allocated array.
//     dir_name = my_dirname(pcopy);
// 
//     ...use dir_name however needed...
// 
//     free(pcopy)   // pcopy and dir_name no longer valid.  // only needed if pcopy is dynamic
//
char *my_dirname(char *path)
{
    char *base = strrchr(path, '/');
    if (base)
        *base = '\0';   // Trim off the basename
    else
        *path = '\0';   // No path, just a basename, return a null string.

    return (path);
}



void my_chdir(gchar *path)
{
    if ( chdir(path) < 0 )      // Report chdir() failure and continue.
        fprintf(stderr, "Fatal Error: chdir() failed: path=%s\n%s\n", path, strerror(errno));
}



void my_cannotopen(char *file)
{
    fprintf(stderr, "Cannot open file %s\n", file);
}


// Widget lookup support for GTK3 apps.  Overrides GTK2's auto generated function
// "lookup_widget()" function.
//
GtkWidget   *my_lookup_widget(gchar *name)
{
    return( g_hash_table_lookup(hash_table, name) );
}


void my_add_widget(gpointer widget, gpointer user_data)
{
    GType       my_type;

    static GType nb1, nb2, nb3, nb4;

    if ( !hash_table )   // Need to initialize
    {
        hash_table = g_hash_table_new(g_str_hash,g_str_equal);

        // This is a brute-force method for determining if an object is a "buildable"
        // Additional values may need to be added if the glade file for a specific
        // application defines other "non-buildable" objects.
        nb1 = g_type_from_name("GtkAccelGroup");            // Glade-Gtk3
        nb2 = g_type_from_name("GtkTreeSelection");         // Glade-Gtk3
        nb3 = g_type_from_name("GMenu");                    // Cambalache-Gtk4
        nb4 = g_type_from_name("GtkEventControllerFocus");  // Cambalache-Gtk4
    }

    my_type = G_OBJECT_TYPE(widget);

    if ( my_type == nb1 || my_type == nb2 || my_type == nb3 || my_type == nb4)     // Brute force Non-Buildable check
    {
        /* my_type is a Non-Buildable type : Do nothing */
    }
    else
    {
        /* Add the widget to to the reference database */
        #ifndef GTK4_BUILD
        g_hash_table_insert(hash_table, strdup(gtk_buildable_get_name(GTK_BUILDABLE(widget))), widget); 
        #else
        g_hash_table_insert(hash_table, strdup(gtk_buildable_get_buildable_id(GTK_BUILDABLE(widget))), widget); 
        #endif

        #if 0   // for debugging
        {
            static count = 0;

            printf("ID-NAME[%d] = %x %s\n", count++, widget, gtk_buildable_get_name(GTK_BUILDABLE(widget)) ); 
        }
        #endif
    }

}

#if 0   // Might be useful in the future for creating transient instances of toplevel widgets.
//------------------- GTK3-only support functions -----------------------------------

GtkWidget *create_widget(gchar *widget_name)
{
    GtkWidget   *widget;
    GtkBuilder  *builder;
    gchar       *toplevel[2];

    toplevel[0] = widget_name;
    toplevel[1] = NULL;

    builder = gtk_builder_new();
    gtk_builder_add_objects_from_file(builder, "../gscope3.glade", toplevel, NULL);
    widget = GTK_WIDGET(gtk_builder_get_object(builder, widget_name));
    #ifndef GTK4_BUILD      //GTK4: gtk_builder_connect_signals() no longer exists. Instead, signals are always connected automatically.
    gtk_builder_connect_signals(builder, NULL);
    #endif
    g_object_unref(G_OBJECT(builder));

    return(widget);
}

#endif


void my_space_codec(gboolean encode, gchar *my_string)
{
    gchar *work_ptr;
    gchar from_char;
    gchar to_char;

    work_ptr = my_string;

    if ( encode )
    {
        from_char = ' ';
        to_char   = '\a';  // Convert all ' ' (space) characters to ASCII Bell chr(7)  
    }
    else
    {
        from_char = '\a';
        to_char   = ' ';   // Convert all '\a' characters to ' ' (space) characters.

    }

    while (*work_ptr != '\0')
    {
        if (*work_ptr == from_char) 
        {
            *work_ptr = to_char;
        }
        work_ptr++;
    }
}



#define MY_MAX_COMMAND_ARGS     8

/* A NOWAIT "system()" call */
pid_t my_system(gchar *application)
{
    pid_t pid;
    static gboolean child_handler_init = FALSE;

    // The first time somebody uses this utility, set up the child signal handler
    if ( !child_handler_init )
    {
        _register_child_handler();
        child_handler_init = TRUE;
    }

    pid = fork();
    if (pid < 0)  return(pid);

    if (pid == 0)  // This is the child process
    {
        char *cwd;
        int   exec_result;
        int   i;
        int   arg_count = 0;
        char *arg_ptr;
        char *base_ptr;

        char *execArgs[MY_MAX_COMMAND_ARGS + 2];  // +1 for "command" Args[0] and terminating NULL ptr Args[n+1]
        

        memset(execArgs, 0, sizeof(execArgs));

        base_ptr = application;

        //printf("Launching: %s\n", application);
        if (strlen(application) == 0)
        {
            fprintf(stderr, "my_system() call failed: No command specified\n");
            exit(1);
        }

        /*** spawn the application ***/

        // Decompose the application string into a command plus [0 to n] arguments.  Each argument
        // is seperated by a space character.  Add a copy of each decomposed string segment to execArgs[n].
        // execArgs[0] is the command to exec and is always provided.  Optional arguments are loaded
        // into execArgs[1]..execArgs[max].

        while ( (arg_ptr = strchr(base_ptr, ' ')) != NULL )
        {
            if ( arg_count > MY_MAX_COMMAND_ARGS)
            {
                fprintf(stderr, "my_system() call failed: Too many command arguments.  Max = %d\n", MY_MAX_COMMAND_ARGS);
                exit(1);
            }
            *arg_ptr++ = '\0';
            execArgs[arg_count] = strdup(base_ptr);
            my_space_codec(DECODE, execArgs[arg_count]);
            arg_count++;
            base_ptr = arg_ptr;  // advance the base pointer for next argument ' ' scan.
        }

        // Pick up the final (null-terminated) portion of the command/args string
        if (*base_ptr != '\0')
        {
            if (arg_count > MY_MAX_COMMAND_ARGS)
            {
                fprintf(stderr, "my_system() call failed: Too many command arguments.  Max = %d\n", MY_MAX_COMMAND_ARGS);
                exit(1);
            }
            execArgs[arg_count] = strdup(base_ptr);
            my_space_codec(DECODE, execArgs[arg_count]);
            arg_count++; 
        }

        //for (i = 0; i < arg_count + 1; i ++)
        //    printf("arg-%d = %s\n", i, execArgs[i]);


        execArgs[arg_count] = NULL;     // Should already be NULL ( from memset() )

        /* Start the application */
        exec_result = execvp(execArgs[0], execArgs);

        if (exec_result < 0) 
        {
            fprintf(stderr, "exec Failed: %s \n", strerror(errno));
            cwd = getcwd(NULL, 1024);
            fprintf(stderr, "CWD         = %s\n", cwd);
            free(cwd);
            fprintf(stderr, "Applicaton = /bin/sh -c %s\n", application);

            exit(1);
        }

        for (i = 0; i < MY_MAX_COMMAND_ARGS + 1; i++)
        {
            if (execArgs[i]) free(execArgs[i]);
        }
        
    }

    // Only the parent process gets here
    return(pid); 
}


void my_start_text_editor(gchar *filename, gchar *linenum)
{
    gchar command[1024];

    if ( strcmp(my_basename(settings.fileEditor), "vs") == 0 )          // Visual Slick Edit
    {
        sprintf(command, "%s \"%s\" -#\"goto-line %s\" -#\"goto-col 1\" &", settings.fileEditor, filename, linenum);
    }
    else if ( strcmp(my_basename(settings.fileEditor), "code")         == 0 ||  // Microsoft VS Code
              strncmp(my_basename(settings.fileEditor), "code-", 5)    == 0 ||  // Microsoft VS Code (OSS or Insiders builds)
              strcmp(my_basename(settings.fileEditor), "sublime_text") == 0 ||  // Sublime (fullname)
              strcmp(my_basename(settings.fileEditor), "subl")         == 0 )   // Sublime (shortname)
    {
        sprintf(command, "%s -g %s:%s &", settings.fileEditor, filename, linenum);
    }
    else if ( strcmp(my_basename(settings.fileEditor), "atom") == 0 )   // Atom text editor
    {
        sprintf(command, "%s %s:%s &", settings.fileEditor, filename, linenum);
    }
    else if ( strcmp(my_basename(settings.fileEditor), "kwrite") == 0 ||    // KDE Writer
              strcmp(my_basename(settings.fileEditor), "kate")   == 0 )     // KDE Advanced Text Editor
    {
        sprintf(command, "%s %s --line %s &", settings.fileEditor, filename, linenum);
    }
    else            // Classic UNIX text editors (that use <editor> +<line-number> <file-name>)
    {
        sprintf(command, "%s +%s \"%s\" &", settings.fileEditor, linenum, filename);
    }
    if ( system(command) < 0 )
        fprintf(stderr, "Failed to spawn text editor\n");   // Note the failure and just carry on.
}



// Handle 'bad' return values from asprint() as a fatal error.
//===========================================================================
void my_asprintf(gchar **str_ptr, const char *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    if ( vasprintf(str_ptr, fmt, args) < 0 )
    {
        fprintf(stderr, "Fatal asprintf() malloc error...Closing Gscope\n");
        exit(EXIT_FAILURE);
    }
    va_end(args);
}


#ifndef HAVE_ASPRINTF   // A glimmer of hope for those without asprintf() and friends.

//=====================================================
// WARNING:  This code has not been exhaustively tested
//=====================================================

#include <stdarg.h>
int vasprintf(char **ret, const char *format, va_list ap)
{
    va_list ap2;
    int len = 100;        /* First guess at the size */
    if ((*ret = (char *)malloc(len)) == NULL)
        return -1;
    while (1)
    {
        int nchar;
        va_copy(ap2, ap);
        nchar = vsnprintf(*ret, len, format, ap2);
        if (nchar > -1 && nchar < len)
            return nchar;
        if (nchar > len)
            len = nchar + 1;
        else
            len *= 2;
        if ((*ret = (char *)realloc(*ret, len)) == NULL)
        {
            free(*ret);
            return -1;
        }
    }
}

#if 0   // my_asprintf() is the preferred calling method for Gscope
int asprintf(char **ret, const char *format, ...)
{
    va_list ap;
    int nc;
    va_start(ap, format);
    nc = vasprintf(ret, format, ap);
    va_end(ap);
    return nc;
}
#endif

#endif


// Gtk version-variant abstractions
//=================================

void my_gtk_entry_set_text(GtkEntry *entry, const gchar *text)
{
    #ifndef GTK4_BUILD
    gtk_entry_set_text(entry, text);
    #else
    GtkEntryBuffer *buffer = gtk_entry_get_buffer(entry);
    gtk_entry_buffer_set_text(buffer, text, -1);           
    #endif
}


const gchar *my_gtk_entry_get_text(GtkEntry *entry)
{
    const gchar *text;

    #ifndef GTK4_BUILD
    text = gtk_entry_get_text(GTK_ENTRY(entry));
    #else
    GtkEntryBuffer *buffer = gtk_entry_get_buffer(entry);
    text = gtk_entry_buffer_get_text(buffer);           
    #endif

    return(text);
}



gchar *my_gtk_file_chooser_get_filename(GtkFileChooser *chooser)
{
    gchar *filename;

    #ifndef GTK4_BUILD
    filename =gtk_file_chooser_get_filename(chooser);
    #else
    GFile *file = gtk_file_chooser_get_file(chooser);
    filename = g_file_get_path(file);
    g_free(file);
    #endif

    return(filename);
}



void my_gtk_check_button_set_active(GtkWidget *button, gboolean is_active)
{

    #ifndef GTK4_BUILD
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), is_active);
    #else
        gtk_check_button_set_active(GTK_CHECK_BUTTON(button), is_active);
    #endif
}


gboolean    my_gtk_check_button_get_active(GtkWidget *button)
{
    #ifndef GTK4_BUILD
    return( gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)) );
    #else
    return( gtk_check_button_get_active(GTK_CHECK_BUTTON(button)) );
#endif
}



void my_gtk_box_pack_start (GtkBox* box,   GtkWidget* child,   gboolean expand,   gboolean fill,  guint padding)
{
    #if defined(GTK4_BUILD)
    gtk_box_append(box, child);
    #else
    gtk_box_pack_start(box, child, expand, fill, padding);
    #endif 


}