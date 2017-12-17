#ifndef _MATCH_H_
#define _MATCH_H_

/* Simple shell-style filename pattern matcher.  Only does ? * and **, and
** multiple patterns separated by |.  Returns 1 or 0.
*/
int match( const char* pattern, const char* string );

#endif /* _MATCH_H_ */
