
/* VERSION_ANNOUNCE is usually a NULL string.  It is reserved for really big announcements */
#if 0
#define VERSION_ANNOUNCE \
"<span color=\"red\" weight=\"bold\">Major Feature Introduction:  </span>" \
"G-Scope now provides cross-referencing of Protobuf Compiler output files.\n"\
"The new functionality is automatic as long as you have '.proto' in your Source File Search suffix list.\n\n"

// Version 3.1, 3.2, 3.3 special announcement
#define VERSION_ANNOUNCE \
"Welcome to the <span color=\"blue\" weight=\"bold\">Open Source</span> version of Gscope.\n" \
"This application is provided to the open source community\n" \
"courtesy of Hewlett Packard Inc.\n\n"

// Version 3.12 special announcement
#define VERSION_ANNOUNCE \
"<span color=\"blue\" weight=\"bold\">New Feature Introduction:\n</span>" \
"G-Scope has added built-in support for several popular text editors: \n" \
"       - code                 [Microsoft VS Code]\n" \
"       - subl(lime_text)  [Sublime text editor]\n" \
"       - atom                 [Atom text editor]\n\n"
#endif

// Version 2.14 2.16 special announcement
#define VERSION_ANNOUNCE \
"<span color=\"blue\" weight=\"bold\">New Feature Introduction:\n\n</span>" \
"G-Scope now provides <span weight=\"bold\">Static Function Call Analysis</span>. \n\n" \
"To access static call analysis, right-click on any search result\n" \
"record and select \"Static Call Analysis\".\n\n"

