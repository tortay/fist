/*
 * Portions are Copyright (c) Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Copyright (c) 2006-2024 IN2P3 Computing Centre
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Written by Loic Tortay <tortay@cc.in2p3.fr>.
 *
 * For questions, comments or bug reports please contact the author.
 *
 */

/*
 * fist: "Fast fIlesystem Stat Tool".
 * "fist" produces a detailed list of the meta-data of objects in a given
 * directory or filesystem.
 * It's similar in intent to what "find $dir -xdev -ls" does, except:
 * . it supposedly faster since it's much simpler;
 * . the output is not meant to be human-readable but "parseable" with Perl,
 *   AWK, Python, whatever (i.e. the ouput is not similar to "ls -lids");
 * . almost all fields of the "struct stat" for an object are printed (the
 *   field set is limited to what is defined in SUSv3;
 *   the "st_dev" and "st_ino" fields are not printed since there are of
 *   limited interest for statistics or comparisons);
 * . UIDs and GIDs are not resolved and printed as numbers;
 * . dates are printed as epoch based numbers.
 * . names are percent-encoded, RFC3986 like (except '/')
 *
 * "fist" can be used to gather informations on objects in a filesystem for
 * statistical purposes, it can also be used to compare filesystems
 * meta-data.
 *
 * The output is colon (':') separated, the fields are in "find -ls" order
 * (with "atime" and "ctime" inserted before "name"):
 *  "blocks perms nlinks uid gid size mtime atime ctime name"
 * "name" is "name -> lname" when the object is a link.
 *
 * Version: 1.99
 *
 */


#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef NEED_STAT64
# define FIST_SSTAT	struct stat64
# define FIST_LSTAT	lstat64
#else
# define FIST_SSTAT	struct stat
# define FIST_LSTAT	lstat
#endif /* NEED_STAT64 */

#ifndef HAS_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif

#ifndef HAS_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif

void error(const int, const int, const char *, ...);
void warning(const int, const char *, ...);
static void verror(const int, const char *, va_list);

void print_metadata(FILE *, const char *, const char *,
	const FIST_SSTAT *);
int dir_lookup(const dev_t, const char *, const char *);

int print_percent_encoded_char(const char, FILE*);

int
main(int argc, char *argv[])
{
	FIST_SSTAT st;

	if (argc != 2) {
		fprintf(stderr, "Absolute directory name or \".\" argument required\n");
		exit(1);
	}

	if (chdir(argv[1]) == -1)
		error(1, errno, "Unable to change directory to '%s'", argv[1]);

	if (FIST_LSTAT(argv[1], &st) == -1)
		error(1, errno, "Unable to lstat(2) '%s'", argv[1]);

	print_metadata(stdout, argv[1], NULL, &st);

	if (dir_lookup(st.st_dev, argv[1], argv[1]))
		warning(-1, "A problem occurred while traversing '%s'",
		    argv[1]);

	return (0);
}


/*
 * Simple recursive depth-first directory traversal.
 */
int
dir_lookup(const dev_t dev, const char *thisdir, const char *parent)
{
	char		 pwd[PATH_MAX];
	FIST_SSTAT	 st;
	DIR		*dirp = NULL;
	struct dirent	*dp = NULL;
	int		 r = 0;

	if ((dirp = opendir(thisdir)) == NULL) {
		warning(errno, "Unable to open directory '%s'", parent);
		return (-1);
	}

	if (chdir(thisdir) == -1) {
		warning(errno, "Unable to change directory to '%s'", thisdir);
		return (-1);
	}

	while ((dp = readdir(dirp)) != NULL) {
		if (FIST_LSTAT(dp->d_name, &st) == -1) {
			warning(errno, "Unable to lstat('%s%s%s')",
			    parent != NULL ? parent : "",
			    parent != NULL ? "/" : "",
			    dp->d_name);
			continue;
		}
		print_metadata(stdout, dp->d_name, parent, &st);
		/*
		 * If the current object is:
		 *  - a directory,
		 *  - not a mount point,
		 *  - neither '.' nor '..'
		 * then we'll try to look inside it.
		 */
		if (S_ISDIR(st.st_mode) && (st.st_dev == dev)
		    && !(dp->d_name[0] == '.' && ((dp->d_name[1] == '\0')
		        || (dp->d_name[1] == '.' && dp->d_name[2] == '\0')))) {
			if (strlcpy(pwd, parent, PATH_MAX) >= PATH_MAX) {
				warning(-1, "parent name too long: '%s'",
				    parent);
				break;
			}
			if (strlcat(pwd, "/", PATH_MAX) >= PATH_MAX) {
				warning(-1, "pwd name too long: '%s'", pwd);
				break;
			}
			if (strlcat(pwd, dp->d_name, PATH_MAX) >= PATH_MAX) {
				warning(-1, "dp->d_name name too long: '%s'",
				    dp->d_name);
				break;
			}
			r = dir_lookup(dev, dp->d_name, pwd);
		}
	}

	if (closedir(dirp) == -1)
		warning(errno, "Error while closing directory '%s'", thisdir);

	/*
	 * We can safely chdir("..") since we do not cross mount points and
	 * follow symlinks.  Furthermore, we already successfully changed
	 * directory to "thisdir", so it's safe to go back to the previous
	 * level.
	 */
	if (chdir("..") == -1) {
		warning(errno, "Unable to change directory to '%s'", parent);
		return (-1);
	}

	return (r);
}


void
print_metadata(FILE *fp, const char *name, const char *parent,
    const FIST_SSTAT *st)
{
	static char	 lnvalue[PATH_MAX];
	unsigned char	*c = NULL;
	int		 lnlen = -1;

	/*
	 * Don't print '.' and '..' for the non-root directories.
	 */
	if (S_ISDIR(st->st_mode) && parent != NULL && (name[0] == '.'
		&& ((name[1] == '\0') || (name[1] == '.' && name[2] == '\0'))))
			return;

	fprintf(fp, "%u:%o:%u:%u:%u:%" PRIu64 ":%u:%u:%u:",
	    (unsigned int) ((st->st_blocks + 1) >> 1),
	    (unsigned int) st->st_mode, (unsigned int) st->st_nlink,
	    (unsigned int) st->st_uid, (unsigned int) st->st_gid,
	    (uint64_t) st->st_size, (unsigned int) st->st_mtime,
	    (unsigned int) st->st_atime, (unsigned int) st->st_ctime);

	if (parent != NULL) {
		for (c = (unsigned char *) parent; c != NULL && *c != '\0'; c++)
			print_percent_encoded_char(*c, fp);
		fputc('/', fp);
	}

	for (c = (unsigned char *) name; c != NULL && *c != '\0'; c++)
		print_percent_encoded_char(*c, fp);

	if (S_ISLNK(st->st_mode)) {
		if ((lnlen = readlink(name, lnvalue,
		    sizeof(lnvalue) - 1)) == -1) {
			warning(errno, "Unable to readlink(2) '%s'", name);
		}
		if (lnlen < 0)
			lnlen = 0;
		lnvalue[lnlen] = '\0';

		fputs(" -> ", fp);
		for (c = (unsigned char *) lnvalue; c != NULL && *c != '\0'; c++)
			print_percent_encoded_char(*c, fp);
	}

	fputc('\n', fp);
}


int
print_percent_encoded_char(const char c, FILE* fp)
{
	int rc;

	switch (c) {
		case '\b':
		case '\n':
		case '\r':
		case '\t':
		case ' ':
		case '!':
		case '"':
		case '#':
		case '$':
		case '%':
		case '&':
		case '\'':
		case '(':
		case ')':
		case '*':
		case '+':
		case ',':
		case ':':
		case ';':
		case '<':
		case '=':
		case '>':
		case '?':
		case '@':
		case '[':
		case '\\':
		case ']':
		case '`':
		case '{':
		case '|':
		case '}':
		case '~':
		case 27: /* ESC */
		case 127: /* DEL */
			rc = fprintf(fp, "%%%02X", (int) c);
			break;
		default:
			if (isprint(c)) {
				rc = fputc(c, fp);
			} else {
				rc = fprintf(fp, "%%%02hhX", (int) c);
			}
			break;
	}

	return (rc);
}


void
verror(const int errnum, const char *fmt, va_list ap)
{
	char *errmsg = NULL;

	if (errnum != -1)
		errmsg = strerror(errnum);

	fprintf(stderr, "fist: ");
	if (fmt != NULL)
		vfprintf(stderr, fmt, ap);

	if (errnum != -1) {
		fprintf(stderr, ": %.100s (%d)\n", errmsg, errnum);
	} else {
		fputc('\n', stderr);
	}
}


void
error(const int excode, const int errnum, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror(errnum, fmt, ap);
	va_end(ap);

	exit(excode);
}


void
warning(const int errnum, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verror(errnum, fmt, ap);
	va_end(ap);
}

#ifndef HAS_STRLCPY
/*      $OpenBSD: strlcpy.c,v 1.8 2003/06/17 21:56:24 millert Exp $        */

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t
strlcpy(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}
#endif /* HAS_STRLCPY */

#ifndef HAS_STRLCAT
/*      $OpenBSD: strlcat.c,v 1.11 2003/06/17 21:56:24 millert Exp $    */

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <string.h>

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t
strlcat(char *dst, const char *src, size_t siz)
{
	register char *d = dst;
	register const char *s = src;
	register size_t n = siz;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end */
	while (n-- != 0 && *d != '\0')
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return(dlen + (s - src));       /* count does not include NUL */
}
#endif /* HAS_STRLCAT */

