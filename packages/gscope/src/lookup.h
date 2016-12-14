/* $Id: lookup.h,v 1.1 2001/06/01 12:43:24 broeker Exp $ */


//#ifndef CSCOPE_LOOKUP_H
//#define CSCOPE_LOOKUP_H

/*** Defines ***/

/*** Public Globals ***/
/* keyword text for fast testing of keywords in the scanner */
extern	char	enumtext[];
extern	char	externtext[];
extern	char	structtext[];
extern	char	typedeftext[];
extern	char	uniontext[];

extern	struct	keystruct {
	char	*text;
	char	delim;
	struct	keystruct *next;
} keyword[];


/*** Public Function Interfaces ***/
void   initsymtab(void);
char * lookup(char *ident);

#//endif /* CSCOPE_LOOKUP_H */
