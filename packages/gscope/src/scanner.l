%{
// 8/5/2016 TEF : To build scanner.c use the following command:  flex -oscanner.c scanner.l
//                Scanner changes so rarely that I didn't bother adding it to the Makefile

/*===========================================================================
 Copyright (c) 1998-2000, The Santa Cruz Operation
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 *Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 *Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 *Neither name of The Santa Cruz Operation nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 DAMAGE.
 =========================================================================*/

/*  cscope - interactive C symbol cross-reference
 *
 *  C symbol scanner
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <assert.h>
#include <string.h>
#include <glib.h>
#include <stdint.h>

#include "scanner.h"
#include "lookup.h"
#include "crossref.h"
#include "dir.h"
#include "utils.h"
#include "app_config.h"

#define     PATLEN                  PATHLEN         /* symbol pattern length */
#define     STMTMAX                 50000           /* maximum source statement length */

/* the line counting has been moved from character reading for speed */
/* comments are discarded */

#ifndef FLEX_SCANNER
# error Sorry, this scanner needs flex. It is not usable with AT&T Lex.
#endif

#define IFLEVELINC  5   /* #if nesting level size increment */
#define YY_NO_TOP_STATE 1


int first;  /* buffer index for first char of symbol */
int last;   /* buffer index for last char of symbol */
int lineno; /* symbol line number */
int myylineno = 1;

/* HBB 20001007: new variables, emulating yytext in a way that allows
 * the yymore() simulation, my_yymore(), to be used even in the presence of
 * yyless(). */
size_t my_yyleng = 0;
char *my_yytext = NULL;

static  gboolean    arraydimension; /* inside array dimension declaration */
static  gboolean    bplisting;      /* breakpoint listing */
static  int         braces;         /* unmatched left brace count */
static  gboolean    classdef;       /* c++ class definition */
static  gboolean    elseelif;       /* #else or #elif found */
static  gboolean    esudef;         /* enum/struct/union global definition */
static  gboolean    external;       /* external definition */
static  int         externalbraces; /* external definition outer brace count */
static  gboolean    fcndef;         /* function definition */
static  gboolean    global;         /* file global scope (outside functions) */
static  size_t      iflevel;        /* #if nesting level */
static  gboolean    initializer;    /* data initializer */
static  int         initializerbraces;    /* data initializer outer brace count */
static  gboolean    lex;            /* lex file */
static  size_t      miflevel = IFLEVELINC;  /* maximum #if nesting level */
static  int         *maxifbraces;   /* maximum brace count within #if */
static  int         *preifbraces;   /* brace count before #if */
static  int         parens;         /* unmatched left parenthesis count */
static  gboolean    ppdefine;       /* preprocessor define statement */
static  gboolean    pseudoelif;     /* pseudo-#elif */
static  gboolean    oldtype;        /* next identifier is an old type */
static  gboolean    rules;          /* lex/yacc rules */
static  gboolean    sdl;            /* sdl file */
static  gboolean    structfield;    /* structure field declaration */
static  int         tagdef;         /* class/enum/struct/union tag definition */
static  gboolean    template;       /* function template */
static  int         templateparens; /* function template outer parentheses count */
static  int         typedefbraces = -1; /* initial typedef brace count */
static  int         token;          /* token found */
static  int     ident_start;        /* begin of preceding identifier */

static  void    my_yymore(void);

%}
identifier  [a-zA-Z_$][a-zA-Z_0-9$]*
number      \.?[0-9][.0-9a-fA-FlLuUxX]*
comment     \/\*([^*]*(\*+[^*/])?)*\*+\/|\/\/[^\n]*\n
ws      [ \t\r\v\f]
wsnl        [ \t\r\v\f\n]|{comment}

/* flex options: stack of start conditions, and don't use yywrap() */
%option stack
%option noyywrap

%start SDL
%a 4000
%o 7000

/* exclusive start conditions. not available in AT&T lex -> use flex! */
%x IN_PREPROC WAS_ENDIF WAS_IDENTIFIER WAS_ESU IN_DQUOTE IN_SQUOTE COMMENT

%%

%\{     {   /* lex/yacc C declarations/definitions */
            global = TRUE;
            goto more;
            /* NOTREACHED */
        }
%\}     {
            global = FALSE;
            goto more;
            /* NOTREACHED */
        }
^%%     {   /* lex/yacc rules delimiter */
            braces = 0;
            if (rules == FALSE) {
                /* this %% starts the section containing the rules */
                rules = TRUE;

                /* Copy yytext to private buffer, to be able to add further
                 * content following it: */
                my_yymore();

                /* simulate a yylex() or yyparse() definition */
                (void) strcat(my_yytext, " /* ");
                first = strlen(my_yytext);
                if (lex == TRUE) {
                    (void) strcat(my_yytext, "yylex");
                } else {
                    /* yacc: yyparse implicitly calls yylex */
                    char *s = " yylex()";
                    char *cp = s + strlen(s);
                    while (--cp >= s) {
                        unput(*cp);
                    }
                    (void) strcat(my_yytext, "yyparse");
                }
                last = strlen(my_yytext);
                (void) strcat(my_yytext, " */");
                my_yyleng = strlen(my_yytext);
                return(FCNDEF);
            } else {
                /* were in the rules section, now comes the closing one */
                rules = FALSE;
                global = TRUE;
                last = first;
                my_yymore();
                return(FCNEND);
                /* NOTREACHED */
            }
        }

<SDL>STATE[ \t]+({identifier}|\*)   { /* sdl state, treat as function def */
            braces = 1;
            fcndef = TRUE;
            token = FCNDEF;
            goto findident;
            /* NOTREACHED */
        }
<SDL>ENDSTATE[ \t]  { /* end of an sdl state, treat as end of a function */
            goto endstate;
            /* NOTREACHED */
        }

\{      {   /* count unmatched left braces for fcn def detection */
            ++braces;

            /* mark an untagged enum/struct/union so its beginning
               can be found */
            if (tagdef) {
                if (braces == 1) {
                    esudef = TRUE;
                }
                token = tagdef;
                tagdef = '\0';
                last = first;
                my_yymore();
                return(token);
            }
            goto more;
            /* NOTREACHED */
        }

\#{ws}* { /* start a preprocessor line */
            if (rules == FALSE)     /* don't consider CPP for lex/yacc rules */
                BEGIN(IN_PREPROC);
            yyleng = 1; /* get rid of the blanks, if any */
            goto more;
            /* NOTREACHED */
        }
<IN_PREPROC>endif([^a-zA-Z0-9_$\n].*)?  {   /* #endif */
            /* delay treatment of #endif depending on whether an
             * #if comes right after it, or not */
            /* HBB 20010619: new pattern allows trailing garbage
             * after the #endif */
            BEGIN(WAS_ENDIF);
            goto more;
            /* NOTREACHED */
        }
<WAS_ENDIF>\n{wsnl}*#{ws}*if(ndef|def)?{ws}+        {
            /* attempt to correct erroneous brace count caused by:
             *
             * #if ...
             *  ... {
             * #endif
             * #if ...
             *  ... {
             * #endif
             */
            /* the current #if must not have an #else or #elif */
            if (elseelif == TRUE) {
                goto endif;
                /* NOTREACHED */
            }
            pseudoelif = TRUE;
            BEGIN(INITIAL);
            yyless(1);  /* rescan all but the line ending */
            yy_set_bol(1);
            goto eol;
            /* NOTREACHED */
        }
<WAS_ENDIF>\n{wsnl}*         {  /* an #endif with no #if right after it */
        endif:
            if (iflevel > 0) {
                /* get the maximum brace count for this #if */
                if (braces < maxifbraces[--iflevel]) {
                    braces = maxifbraces[iflevel];
                }
            }
            BEGIN(INITIAL);
            yyless(1);
            yy_set_bol(1);
            goto eol;
            /* NOTREACHED */
        }

<IN_PREPROC>ifndef{ws}+ |
<IN_PREPROC>ifdef{ws}+      |
<IN_PREPROC>if{ws}+     { /* #if directive */
            elseelif = FALSE;
            if (pseudoelif == TRUE) {
                pseudoelif = FALSE;
                goto elif;
                /* NOTREACHED */
            }
            /* make sure there is room for the current brace count */
            if (iflevel == miflevel) {
                miflevel += IFLEVELINC;
                maxifbraces = g_realloc(maxifbraces, miflevel * sizeof(*maxifbraces));
                preifbraces = g_realloc(preifbraces, miflevel * sizeof(*preifbraces));
            }
            /* push the current brace count */
            preifbraces[iflevel] = braces;
            maxifbraces[iflevel++] = 0;
            BEGIN(INITIAL);
            goto more;
            /* NOTREACHED */
        }
<IN_PREPROC>else({ws}.*)?   { /* #else --- eat up whole line */
            elseelif = TRUE;
            if (iflevel > 0) {

                /* save the maximum brace count for this #if */
                if (braces > maxifbraces[iflevel - 1]) {
                    maxifbraces[iflevel - 1] = braces;
                }
                /* restore the brace count to before the #if */
                braces = preifbraces[iflevel - 1];
            }
            BEGIN(INITIAL);
            goto more;
            /* NOTREACHED */
        }
<IN_PREPROC>elif{ws}+   { /* #elif */
            /* elseelif = TRUE; --- HBB I doubt this is correct */
        elif:
            if (iflevel > 0) {

                /* save the maximum brace count for this #if */
                if (braces > maxifbraces[iflevel - 1]) {
                    maxifbraces[iflevel - 1] = braces;
                }
                /* restore the brace count to before the #if */
                braces = preifbraces[iflevel - 1];
            }
            BEGIN(INITIAL);
            goto more;
            /* NOTREACHED */
        }

<IN_PREPROC>include{ws}*\"[^"\n]+\" |
<IN_PREPROC>include{ws}*<[^>\n]+>   { /* #include file */
            char    *s;
            char remember = yytext[yyleng-1];

            my_yymore();
            s = strpbrk(my_yytext, "\"<");
            if (!s)
                return(LEXERR);
            my_yytext[my_yyleng-1] = '\0';
            DIR_incfile(s + 1);
            my_yytext[my_yyleng-1] = remember;
            first = s - my_yytext;
            last = my_yyleng - 1;
            if (settings.compressDisable == FALSE) {
                my_yytext[0] = '\2';    /* compress the keyword */
            }
            BEGIN(INITIAL);
            return(INCLUDE);
            /* NOTREACHED */
        }

\}      {
            /* could be the last enum member initializer */
            if (braces == initializerbraces) {
                initializerbraces = -1;
                initializer = FALSE;
            }
            if (--braces <= 0) {
        endstate:
                braces = 0;
                classdef = FALSE;
            }
            if (braces == 0 || (braces == 1 && classdef == TRUE)) {

                /* if the end of an enum/struct/union definition */
                if (esudef == TRUE) {
                    esudef = FALSE;
                }
                /* if the end of the function */
                else if (fcndef == TRUE) {
                    fcndef = FALSE;
                    last = first;
                    my_yymore();
                    return(FCNEND);
                }
            }
            goto more;
            /* NOTREACHED */
        }

\(      {   /* count unmatched left parentheses for function templates */
            ++parens;
            goto more;
            /* NOTREACHED */
        }
\)      {
            if (--parens <= 0) {
                parens = 0;
            }
            /* if the end of a function template */
            if (parens == templateparens) {
                templateparens = -1;
                template = FALSE;
            }
            goto more;
            /* NOTREACHED */
        }
=       {   /* if a global definition initializer */
            if (!my_yytext)
                return(LEXERR);
            if (global == TRUE && ppdefine == FALSE && my_yytext[0] != '#') {
                initializerbraces = braces;
                initializer = TRUE;
            }
            goto more;
            /* NOTREACHED */
        }
:       {   /* a if global structure field */
            if (!my_yytext)
                return(LEXERR);
            if (global == TRUE && ppdefine == FALSE && my_yytext[0] != '#') {
                structfield = TRUE;
            }
            goto more;
            /* NOTREACHED */
        }
\,      {
            if (braces == initializerbraces) {
                initializerbraces = -1;
                initializer = FALSE;
            }
            structfield = FALSE;
            goto more;
            /* NOTREACHED */
        }
;       {   /* if the enum/struct/union was not a definition */
            if (braces == 0) {
                esudef = FALSE;
            }
            /* if the end of a typedef */
            if (braces == typedefbraces) {
                typedefbraces = -1;
            }
            /* if the end of a external definition */
            if (braces == externalbraces) {
                externalbraces = -1;
                external = FALSE;
            }
            structfield = FALSE;
            initializer = FALSE;
            goto more;
            /* NOTREACHED */
        }
<IN_PREPROC>define{ws}+{identifier} {

            /* preprocessor macro or constant definition */
            ppdefine = TRUE;
            token = DEFINE;
            if (settings.compressDisable == FALSE) {
                my_yytext[0] = '\1';    /* compress the keyword */
            }
        findident:
            /* search backwards through yytext[] to find the identifier */
            /* NOTE: this had better be left to flex, by use of
             * yet another starting condition */
            my_yymore();
            first = my_yyleng - 1;
            while (my_yytext[first] != ' ' && my_yytext[first] != '\t') {
                --first;
            }
            ++first;
            last = my_yyleng;
            BEGIN(INITIAL);
            goto definition;
            /* NOTREACHED */
        }
<IN_PREPROC>\n  {   /* unknown preprocessor line */
            BEGIN(INITIAL);
                        ++myylineno;
            goto more;
            /* NOTREACHED */
        }
<IN_PREPROC>.        |
<IN_PREPROC>{identifier}    {   /* unknown preprocessor line */
            BEGIN(INITIAL);
            goto more;
            /* NOTREACHED */
        }

class{wsnl}+{identifier}({wsnl}|{identifier}|[():])*\{  {   /* class definition */
            classdef = TRUE;
            tagdef =  'c';
            yyless(5);      /* eat up 'class', and re-scan */
            yy_set_bol(0);
            goto more;
            /* NOTREACHED */
        }

("enum"|"struct"|"union")   {
            ident_start = first;
            BEGIN(WAS_ESU);
            goto more;
        }
<WAS_ESU>{
({wsnl}+{identifier}){wsnl}*\{      { /* e/s/u definition */
            tagdef = my_yytext[ident_start];
            BEGIN(WAS_IDENTIFIER);
            goto ident;
        }
{wsnl}*\{       { /* e/s/u definition without a tag */
            tagdef = my_yytext[ident_start];
            BEGIN(INITIAL);
            if (braces == 0) {
                esudef = TRUE;
            }
            last = first;
            yyless(0);  /* re-scan all this as normal text */
            tagdef = '\0';
            goto more;
        }
({wsnl}+{identifier})?{wsnl}*   |
.|\n                                        {   /* e/s/u usage */
            BEGIN(WAS_IDENTIFIER);
            goto ident;
        }
}

if{wsnl}*\( {   /* ignore 'if' */
            yyless(2);
            yy_set_bol(0);
            goto more;
}

{identifier}    {   /* identifier found: do nothing, yet. (!) */
            BEGIN(WAS_IDENTIFIER);
            ident_start = first;
            goto more;
            /* NOTREACHED */
        }

<WAS_IDENTIFIER>{
{ws}*\(({wsnl}|{identifier}|{number}|[*&[\]=,.:])*\)([()]|{wsnl})*[:a-zA-Z_#{]  {
            /* a function definition */
            /* note: "#define a (b) {" and "#if defined(a)\n#"
             * are not fcn definitions! */
            /* warning: "if (...)" must not overflow yytext,
             * so the content of function argument definitions
             * is restricted, in particular parentheses are
             * not allowed */
            /* FIXME HBB 20001003: the above 'not allowed' may well be the
             * reason for the parsing bug concerning function pointer usage,
             * I suspect. --- I think my new special-case rule for 'if'
             * could be helpful in removing that limitation */
            if ((braces == 0 && ppdefine == FALSE && my_yytext[0] != '#' && rules == FALSE) ||
                (braces == 1 && classdef == TRUE)) {
                fcndef = TRUE;
                token = FCNDEF;
                goto fcn;
                /* NOTREACHED */
            }
            goto fcncal;
            /* NOTREACHED */
        }
{ws}*\(([*&[\]=,.]|{identifier}|{number}|{wsnl})*       {   /* function call */
        fcncal: if (fcndef == TRUE || ppdefine == TRUE || rules == TRUE) {
                token = FCNCALL;
                goto fcn;
                /* NOTREACHED */
            }
            if (template == FALSE) {
                templateparens = parens;
                template = TRUE;
            }
            goto ident;
            /* NOTREACHED */
        }
("*"|{wsnl})+{identifier}       {   /* typedef name or modifier use */
            goto ident;
            /* NOTREACHED */
        }
.|\n    {       /* general identifer usage */
            char    *s;

            if (global == TRUE && ppdefine == FALSE && my_yytext[0] != '#' &&
                external == FALSE && initializer == FALSE &&
                arraydimension == FALSE && structfield == FALSE &&
                template == FALSE && fcndef == FALSE) {
                if (esudef == TRUE) {
                    /* if enum/struct/union */
                    token = MEMBERDEF;
                } else {
                    token = GLOBALDEF;
                }
            } else {
        ident:
                token = IDENT;
            }
        fcn:
            if (YYSTATE == WAS_IDENTIFIER) {
                /* Position back to the actual identifier: */
                last = first;
                first = ident_start;
                yyless(0);
                /* HBB 20001008: if the anti-backup-pattern above matched,
                 * and the matched context ended with a \n, then the scanner
                 * believes it's at the start of a new line. But the yyless()
                 * should feeds that \n back into the input, so that's
                 * wrong. --> force 'beginning-of-line' status off. */
                yy_set_bol(0);
                BEGIN(INITIAL);
            } else {
                my_yymore();
                last = my_yyleng;
            }
        definition:

            /* if a long line */
            if (yyleng > STMTMAX) {
                int c;

                /* skip to the end of the line */
                warning("line too long");
                while ((c = input()) > LEXEOF) {
                    if (c == '\n') {
                        unput(c);
                        break;
                    }
                }
            }
            /* truncate a long symbol */
            if (yyleng > PATLEN) {
                warning("symbol too long");
                my_yyleng = first + PATLEN;
                my_yytext[my_yyleng] = '\0';
            }

            /* if found word was a keyword: */
            if ((s = lookup(my_yytext + first)) != NULL) {
                first = my_yyleng;

                /* if the start of a typedef */
                if (s == typedeftext) {
                    typedefbraces = braces;
                    oldtype = TRUE;
                }
                /* if an enum/struct/union */
                /* (needed for "typedef struct tag name;" so
                   tag isn't marked as the typedef name) */
                else if (s == enumtext || s == structtext || s == uniontext) {
                    /* do nothing */
                } else if (s == externtext) {
                    /* if an external definition */
                    externalbraces = braces;
                    external = TRUE;
                } else if (templateparens == parens && template == TRUE) {
                    /* keyword doesn't start a function
                     * template */
                    templateparens = -1;
                    template = FALSE;
                } else {
                    /* identifier after typedef was a
                     * keyword */
                    oldtype = FALSE;
                }
            } else {
                /* not a keyword --> found an identifier */
                /* last = yyleng; */

                /* if a class/enum/struct/union tag definition */
                /* FIXME HBB 20001001: why reject "class"? */
                if (tagdef && STRNOTEQUAL(my_yytext + first, "class")) {
                    token = tagdef;
                    tagdef = '\0';
                    if (braces == 0) {
                        esudef = TRUE;
                    }
                } else if (braces == typedefbraces && oldtype == FALSE &&
                           arraydimension == FALSE) {
                    /* if a typedef name */
                    token = TYPEDEF;
                } else {
                    oldtype = FALSE;
                }
                /* my_yymore(); */
                return(token);
                /* NOTREACHED */
            }
        }
}

\[      {   /* array dimension (don't worry or about subscripts) */
            arraydimension = TRUE;
            goto more;
            /* NOTREACHED */
        }
\]      {
            arraydimension = FALSE;
            goto more;
            /* NOTREACHED */
        }
\\\n        {   /* preprocessor statement is continued on next line */
            /* save the '\\' to the output file, but not the '\n': */
            yyleng = 1;
            my_yymore();
            goto eol;
            /* NOTREACHED */
        }
\n      {   /* end of the line */
            if (ppdefine == TRUE) { /* end of a #define */
                ppdefine = FALSE;
                yyless(yyleng - 1);
                last = first;
                my_yymore();
                return(DEFINEEND);
            }
            /* skip the first 8 columns of a breakpoint listing line */
            /* and skip the file path in the page header */
            if (bplisting == TRUE) {
                int c, i;

                /* FIXME HBB 20001007: should call input() instead */
                switch (input()) {  /* tab and EOF just fall through */
                case ' ':   /* breakpoint number line */
                case '[':
                    for (i = 1; i < 8 && input() > LEXEOF; ++i)
                        ;
                    break;
                case '.':   /* header line */
                case '/':
                    /* skip to the end of the line */
                    while ((c = input()) > LEXEOF) {
                        if (c == '\n') {
                            unput(c);
                            break;
                        }
                    }
                    break;
                case '\n':  /* empty line */
                    unput('\n');
                    break;
                }
            }
        eol:
            ++myylineno;
            first = 0;
            last = 0;
            if (symbols > 0) {
                /* no my_yymore(): \n doesn't need to be in my_yytext */
                return(NEWLINE);
            }
            /* line ended --> flush my_yytext */
            if (my_yytext)
                *my_yytext = '\0';
            my_yyleng = 0;
            lineno = myylineno;
        }

\'      {   /* character constant */
            if (sdl == FALSE)
                BEGIN(IN_SQUOTE);
            goto more;
            /* NOTREACHED */
        }
<IN_SQUOTE>\'   {
            BEGIN(INITIAL);
            goto more;
            /* NOTREACHED */
        }
\"      {   /* string constant */
            BEGIN(IN_DQUOTE);
            goto more;
            /* NOTREACHED */
        }
<IN_DQUOTE>\"   {
            BEGIN(INITIAL);
            goto more;
            /* NOTREACHED */
        }
<IN_DQUOTE,IN_SQUOTE>{
\n      {   /* syntax error: unexpected EOL */
            BEGIN(INITIAL);
            goto eol;
            /* NOTREACHED */
        }
\\. |
.       {
            goto more;
            /* NOTREACHED */
        }
\\\n    {       /* line continuation inside a string! */
            myylineno++;
            goto more;
            /* NOTREACHED */
        }
}

^{ws}+      {       /* don't save leading white space */
        }

{ws}+\n     {       /* eat whitespace at end of line */
            unput('\n');
        }

[\t\r\v\f]+ {   /* eat non-blank whitespace sequences, replace
             * by single blank */
            unput(' ');
        }

{ws}{2,}    {   /* compress sequential whitespace here, not in putcrossref() */
            unput(' ');
        }

"/*"                    yy_push_state(COMMENT);
<COMMENT>{
[^*\n]*         |
"*"+[^*/\n]*    ; /* do nothing */
[^*\n]*\n       |
"*"+[^*/\n]*\n  {
            if (ppdefine == FALSE) {
                goto eol;
            } else {
                ++myylineno;
            }
            /* NOTREACHED */
        }
"*"+"/"     {
            /* replace the comment by a single blank */
            unput(' ');
            yy_pop_state();
        }
}

"//".*\n?       {
            /* C++-style one-line comment */
            goto eol;
            /* NOTREACHED */
        }

{number}                |   /* number */
<SDL>STATE[ \t]+        |   /* ... and other syntax error catchers... */
.                       {   /* punctuation and operators */
                        more:
                            my_yymore();
                            first = my_yyleng;
                        }

%%

void
initscanner(char *srcfile)
{
    char    *s;

    if (maxifbraces == NULL) {
        maxifbraces = g_malloc(miflevel * sizeof(*maxifbraces));
        preifbraces = g_malloc(miflevel * sizeof(*preifbraces));
    }
    first = 0;      /* buffer index for first char of symbol */
    last = 0;       /* buffer index for last char of symbol */
    lineno = 1;     /* symbol line number */
    myylineno = 1;      /* input line number */
    arraydimension = FALSE; /* inside array dimension declaration */
    bplisting = FALSE;      /* breakpoint listing */
    braces = 0;     /* unmatched left brace count */
    classdef = FALSE;       /* c++ class definition */
    elseelif = FALSE;       /* #else or #elif found */
    esudef = FALSE;     /* enum/struct/union global definition */
    external = FALSE;       /* external definition */
    externalbraces = -1;    /* external definition outer brace count */
    fcndef = FALSE;     /* function definition */
    global = TRUE;      /* file global scope (outside functions) */
    iflevel = 0;        /* #if nesting level */
    initializer = FALSE;    /* data initializer */
    initializerbraces = -1; /* data initializer outer brace count */
    lex = FALSE;        /* lex file */
    parens = 0;     /* unmatched left parenthesis count */
    ppdefine = FALSE;       /* preprocessor define statement */
    pseudoelif = FALSE; /* pseudo-#elif */
    oldtype = FALSE;        /* next identifier is an old type */
    rules = FALSE;      /* lex/yacc rules */
    sdl = FALSE;        /* sdl file */
    structfield = FALSE;    /* structure field declaration */
    tagdef = '\0';      /* class/enum/struct/union tag definition */
    template = FALSE;       /* function template */
    templateparens = -1;    /* function template outer parentheses count */
    typedefbraces = -1; /* initial typedef braces count */
    ident_start = 0;    /* start of previously found identifier */

    if (my_yytext)
        *my_yytext = '\0';
    my_yyleng = 0;

    BEGIN(INITIAL);

    /* if this is not a C file */
    if ((s = strrchr(srcfile, '.')) != NULL) {
        switch (*++s) { /* this switch saves time on C files */
        case 'b':
            if (strcmp(s, "bp") == 0) { /* breakpoint listing */
                bplisting = TRUE;
            }
            break;
        case 'l':
            if (strcmp(s, "l") == 0) {  /* lex */
                lex = TRUE;
                global = FALSE;
            }
            break;
        case 's':
            if (strcmp(s, "sd") == 0) { /* sdl */
                sdl = TRUE;
                BEGIN(SDL);
            }
            break;
        case 'y':
            if (strcmp(s, "y") == 0) {  /* yacc */
                global = FALSE;
            }
            break;
        }
    }
}

#define MY_YY_ALLOCSTEP 1000
static void
my_yymore(void)
{
    static size_t yytext_size = 0;

    /* my_yytext is an ever-growing buffer. It will not ever
     * shrink, nor will it be freed at end of program, for now */
    while (my_yyleng + yyleng + 1 >= yytext_size) {
        my_yytext = g_realloc(my_yytext, yytext_size += MY_YY_ALLOCSTEP);
    }

    strncpy (my_yytext + my_yyleng, yytext, yyleng+1);
    my_yyleng += yyleng;
}
