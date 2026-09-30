/******************************************************************************
 *                                                                            *
 * namelist.c                                                                 *
 *                                                                            *
 * Read fortran style namelists.                                              *
 *                                                                            *
 * Developed by :                                                             *
 *     AquaticEcoDynamics (AED) Group                                         *
 *     School of Agriculture and Environment                                  *
 *     The University of Western Australia                                    *
 *                                                                            *
 * Copyright 2013-2026 : The University of Western Australia                  *
 *                                                                            *
 *  This file is part of GLM (General Lake Model)                             *
 *                                                                            *
 *  libutil is free software: you can redistribute it and/or modify           *
 *  it under the terms of the GNU General Public License as published by      *
 *  the Free Software Foundation, either version 3 of the License, or         *
 *  (at your option) any later version.                                       *
 *                                                                            *
 *  libutil is distributed in the hope that it will be useful,                *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 *  GNU General Public License for more details.                              *
 *                                                                            *
 *  You should have received a copy of the GNU General Public License         *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                            *
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>     /* D3: isfinite */
#include <limits.h>   /* D3: INT_MIN, INT_MAX */

#include "namelist.h"

//#define DEBUG_NML 1

#define TRUE 1
#define FALSE 0

#ifdef _WIN32
  #define strcasecmp stricmp
  #define strncasecmp _strnicmp
#endif

typedef union _nml_value {
    char  *s;
    double r;
    long long int    i;
    int    b;
} NML_Value;

typedef struct _nml_entry {
    char      *name;
    int        type;
    int        count;
    NML_Value *data;
    int        used;        /* D5: found by get_namelist / get_nml_listlen */
} NML_Entry;

typedef struct _nml_sect {
    char *name;
    int   count;
    NML_Entry *entry;
    int   requested;        /* D5: asked for by get_namelist */
} NML_Section;

typedef struct _nml {
    char *fname;
    FILE *file;
    int   count;
    NML_Section *section;
} NML;

#define BUFCHUNK      262144

/******************************************************************************/
static int  list_count = 0;
static NML *file_list = NULL;
static char buf[BUFCHUNK];
static double zero = 0.;
#if DEBUG_NML
static void show_namelist(int file);
static void show_entry(NML_Entry *ne);
#endif
static int lineno = 0;
static const char *nml_cur_fname = "?";   /* D2: the file being parsed, for messages */
static const char *nml_cur_section = "?"; /* D5: the section being parsed, for messages */

/* D2 2026-09-15: a value of the wrong type is a configuration error, stated - never read as another type's
 * bytes through the NML_Value union. */
static const char *nml_type_name(int t)
{
    switch (t & MASK_TYPE) {
        case TYPE_INT    : return "an integer";
        case TYPE_DOUBLE : return "a real";
        case TYPE_STR    : return "a string";
        case TYPE_BOOL   : return "a logical";
    }
    return "no value";
}
static void nml_range_fatal(int file, const char *section, const char *key, long long v)
{
    fprintf(stderr, "FATAL: namelist \"%s\": &%s %s = %lld does not fit an integer\n",
            file_list[file].fname, section, key, v);
    exit(1);
}
static void nml_type_fatal(int file, const char *section, const char *key, int want, int got)
{
    fprintf(stderr, "FATAL: namelist \"%s\": &%s %s must be %s, but the file gives %s\n",
            file_list[file].fname, section, key, nml_type_name(want), nml_type_name(got));
    exit(1);
}

/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static char *readline(FILE *inf, char *buf)
{
    char *ln = NULL, *s, term;
    int size = BUFCHUNK;

    if ( feof(inf) ) {
        fprintf(stderr, "Early end of file\n");
        return NULL;
    }

    ln = buf;

    do  {
        ln = buf;
        ln[0] = 0;
        if ( feof(inf) || fgets(ln, size, inf) == NULL) return NULL;

        // strip off any LF or CR characters
        while ( ln[0] != 0 && (ln[strlen(ln)-1] == '\n' || ln[strlen(ln)-1] == '\r' ) )
            ln[strlen(ln)-1] = 0;

        if ( !strlen(ln) && feof(inf) ) {
            return NULL;
        }

        if ( ln[0] == 0 ) continue; // skip empty lines

        s = ln;
        while (*s) {
            /* D4 2026-09-15: one escape rule, inside and outside a string, as extract_values and grab_quoted use:
             * a backslash takes the next character literally (never past the end). This scan used to end a
             * string at an escaped quote, and outside a string not to protect the next character. A doubled
             * quote closes and reopens, which leaves the string balanced. */
            if (*s == '"' || *s == '\'' ) {
                term = *s++;
                while (*s && *s != term) s += (*s == '\\' && s[1]) ? 2 : 1;
                if (*s != term) {
                    fprintf(stderr, "FATAL: namelist \"%s\" line %d: unterminated string\n",
                            nml_cur_fname, lineno + 1);
                    exit(1);
                }
            }
            if (*s == '\\') s += s[1] ? 2 : 1;
            else if (*s == '!' || *s == '#') *s = 0;
            else s++;
        }

        s = ln;
        while (*s && ( *s == ' ' || *s == '\t' ) ) s++;
        if ( s != ln ) memmove(ln, s, strlen(s)+1);

        s = &ln[strlen(ln)-1];
        while (s >= ln && ( *s == ' ' || *s == '\t' ) ) *s-- = 0;
    }
    while (ln[0] == 0);

    lineno++;
    return buf;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static char *trim_buf_name(char *buf)
{
    char *e = strchr(buf, '=');
    char *r = NULL;
    if ( e == NULL ) {
        fprintf(stderr, "syntax error in file \"%s\" at %d\n",buf,lineno);
        exit(1);
    }
    r = e;
    do { *e-- = 0; }
    while ((*e == ' ' || *e == '\t') && e > buf);

    do { r++; }
    while ((*r == ' ' || *r == '\t') && *r);

    return r;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static int decode_buf(const char *s, double *r, long long int *i, int *b)
{
    if ( strlen(s) <= 0 ) {
        *b = FALSE;
        return TYPE_NODATA;
    }

    /* D2 2026-09-15: a logical is one whole token, case-insensitive, trailing blanks ignored. The prefix test
     * took ".trueXYZ" as true, and T, F, true, false, .t., .f. fell through to strings. */
    {
        static const char *t_tok[] = { ".true.", ".t.", "t", "true" };
        static const char *f_tok[] = { ".false.", ".f.", "f", "false" };
        size_t n = strlen(s);
        int k;
        while ( n > 0 && (s[n-1] == ' ' || s[n-1] == '\t') ) n--;
        for (k = 0; k < 4; k++) {
            if ( strlen(t_tok[k]) == n && strncasecmp(s, t_tok[k], n) == 0 ) { *b = TRUE;  return TYPE_BOOL; }
            if ( strlen(f_tok[k]) == n && strncasecmp(s, f_tok[k], n) == 0 ) { *b = FALSE; return TYPE_BOOL; }
        }
    }

    /* D3 2026-09-15: a number is ONE whole token - nothing after it but blanks, finite, not hex - read with the
     * same sscanf conversions as before, so every value that parsed before parses to the same bits. sscanf alone
     * read the longest numeric prefix ("12abc" was 12, "1.5abc" 1.5), accepted nan and inf, and dropped a
     * Fortran d exponent ("1.5d2" read 1.5); d/D is now read as e. Anything else is a string, and a numeric
     * key given a string stops in get_namelist. */
    {
        char tmp[128];
        size_t n = strlen(s), k;
        int used;
        while ( n > 0 && (s[n-1] == ' ' || s[n-1] == '\t') ) n--;
        if ( n > 0 && n < sizeof(tmp) && strpbrk(s, "xX") == NULL ) {
            for (k = 0; k < n; k++) tmp[k] = (s[k] == 'd' || s[k] == 'D') ? 'e' : s[k];
            tmp[n] = '\0';
            used = -1;
            if ( strpbrk(tmp, ".Ee") == NULL && sscanf(tmp, "%lld%n", i, &used) == 1 && used == (int)n ) {
                sscanf(tmp, "%lf", r);   /* as before: an integer fills the real too - extraction stores a
                                            * list's later integers from *r once a real has promoted it */
                return TYPE_INT;
            }
            used = -1;
            if ( sscanf(tmp, "%lf%n", r, &used) == 1 && used == (int)n && isfinite(*r) )
                return TYPE_DOUBLE;
        }
    }
    *r = 0.0/zero;
    return TYPE_STR;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static char *grab_substring(const char *s, int n)
{
    char *d, *e;

    e = ( d = malloc(n+1) );
    while ( *s && n-- > 0 ) {
        if ( *s == '\\' ) { s++; n--; }
        *e++ = *s++;
    }
    *e = 0;
    return d;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
/* D4 2026-09-15: a quoted value - a backslash takes the next character literally (as grab_substring does, but
 * never past the end of the value) and a doubled quote is one literal quote. */
static char *grab_quoted(const char *s, size_t n, char term)
{
    char *d = malloc(n+1), *e = d;
    const char *end = s + n;
    while ( s < end ) {
        if ( *s == '\\' && s + 1 < end ) { s++; *e++ = *s++; continue; }
        if ( *s == term && s + 1 < end && s[1] == term ) { *e++ = term; s += 2; continue; }
        *e++ = *s++;
    }
    *e = 0;
    return d;
}

static int extract_values(NML_Entry *entry, char *r)
{
    char *s, *d, term;
    size_t n;
    int comma = FALSE, type;
    long long int ires = 0;
    int bd = FALSE;
    double rres = 0.;

    do  {
        s = r;
        if (*r == '"' || *r == '\'' ) { // a string
            /* D4 2026-09-15: a trailing backslash no longer steps past the end of the line; a doubled quote is one
             * literal quote (it used to end the value, and the rest was skipped); only blanks may follow the
             * closing quote (text there used to be skipped silently). */
            term = *r++; s++;
            while (*r) {
                if (*r == '\\' && r[1]) { r += 2; continue; }
                if (*r == term && r[1] == term) { r += 2; continue; }
                if (*r == term) break;
                r++;
            }
            if ( *r != term ) {
                fprintf(stderr, "FATAL: namelist \"%s\" line %d: %s: unmatched %c\n",
                        nml_cur_fname, lineno, entry->name, term);
                exit(1);
            }

            n = r++ - s;

            d = grab_quoted(s, n, term);

            type = TYPE_STR;

            while (*r == ' ' || *r == '\t') r++;
            if (*r && *r != ',') {
                fprintf(stderr, "FATAL: namelist \"%s\" line %d: %s: text after the closing quote: %s\n",
                        nml_cur_fname, lineno, entry->name, r);
                exit(1);
            }
        } else {
            while (*r && *r != ',' ) r++;

            n = r - s;
            if ( *r ) r++;

            d = grab_substring(s, n);

            type = decode_buf(d, &rres, &ires, &bd);
            if ( type == TYPE_INT && entry->type == TYPE_DOUBLE ) {
                type = entry->type;
                // rres = ires;
            } else if ( type == TYPE_DOUBLE && entry->type == TYPE_INT ) {
                // This is a fix if the first item of a list was made an int, but there are reals in the
                // list meaning the whole list should have been reals.
                int i;
                for (i = 0; i < entry->count; i++) {
                    double tr = entry->data[i].i;
                    entry->data[i].r = tr;
                }
                entry->type = TYPE_DOUBLE;
            }

            /* D1 2026-09-15: a value that decoded as a STRING is stored below (entry->data[].s = d), so its
             * buffer must live until close_namelist frees it. Freeing it here left a dangling pointer - the
             * caller read freed memory and close_namelist freed it again. Numbers and logicals are copied by
             * value, so their buffer is freed exactly as before. */
            if ( type != TYPE_STR ) free(d);
        }
        /* D2: every element of a list has the list's type (integers and reals promote, above) */
        if ( type != TYPE_NODATA && entry->type != 0 && (entry->type & MASK_TYPE) != type ) {
            fprintf(stderr, "FATAL: namelist \"%s\" line %d: %s mixes %s and %s values\n",
                    nml_cur_fname, lineno, entry->name, nml_type_name(entry->type), nml_type_name(type));
            exit(1);
        }
        if ( entry->type == 0 ) entry->type = type;
        entry->data = realloc(entry->data, sizeof(NML_Value)*(entry->count+1));
        memset(&entry->data[entry->count], 0, sizeof(NML_Value));
        switch (type) {
            case TYPE_STR :
                entry->data[entry->count].s = d;
                break;
            case TYPE_INT :
                entry->data[entry->count].i = ires;
                break;
            case TYPE_DOUBLE :
                entry->data[entry->count].r = rres;
                break;
            case TYPE_BOOL :
                entry->data[entry->count].b = bd;
                break;
        }
        entry->count++;

        comma = FALSE;
        while ( *r && ( *r == ' ' || *r == '\t' ) ) r++; // skip blanks
        if ( *r == ',' ) {
            r++;
            while ( *r && ( *r == ' ' || *r == '\t' ) ) r++; // skip blanks
            comma = TRUE;
        }
    } while (*r);

    return comma;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static int get_entry(FILE *f, NML_Entry *entry, char *name)
{
    char *r = trim_buf_name(name);

    entry->name = strdup(name);
    entry->type = 0;
    entry->count = 0;
    entry->data = NULL;
    entry->used = 0;

    do  {
        if (r[0] != 0) extract_values(entry, r);

        if ( (r = readline(f, buf) ) ) {
            if ( strcmp(buf, "/") == 0 ) return 1;
            /* D5 2026-09-15: a new section before this one's '/' used to be read as one more value of the
             * previous key, and the keys after it as keys of this section - the next section vanished. */
            if ( buf[0] == '&' ) {
            fprintf(stderr, "FATAL: namelist \"%s\" line %d: &%s is not closed with / before \"%s\"\n",
                    nml_cur_fname, lineno, nml_cur_section, buf);
            exit(1);
            }
        } else return -1;
    }
    while ( strchr(buf, '=') == NULL );

    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static int get_section(FILE *f, NML_Section *section, const char *name)
{
    section->name = strdup(name);
    section->entry = NULL;
    section->count = 0;
    section->requested = 0;
    nml_cur_section = section->name;   /* D5 */

    readline(f, buf);
    if ( strcmp(buf, "/") == 0 ) return 1;
    if ( buf[0] == '&' ) {   /* D5: the section opened and never closed */
            fprintf(stderr, "FATAL: namelist \"%s\" line %d: &%s is not closed with / before \"%s\"\n",
                    nml_cur_fname, lineno, nml_cur_section, buf);
            exit(1);
    }

    do  {
        section->entry = realloc(section->entry, sizeof(NML_Entry)*(section->count+1));
        get_entry(f, &section->entry[section->count++], buf);
    }
    while ( strcmp(buf, "/") != 0 );

    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static int npush = 0;
static FILE *fs[10];
static const char *fn[10];

int push_file(FILE *f, const char *fname)
{
    if ( npush > 9 ) return -1;
    fs[npush] = f;
    fn[npush] = fname;
    npush++;
    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


int pop_file(FILE **f, const char **fname)
{
    if ( npush <= 0 ) return -1;
    npush--;
    if (npush) free((void*)*fname);
    *f = fs[npush];
    *fname = fn[npush];
    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


int get_new_name(const char *buf, const char **fname)
{
    char *s = (char*)buf;
    size_t l = strlen(s);
    char *tname = malloc((l+1)*sizeof(char));
    int i;
    while ( *s != '"' && *s != '\'' && *s != 0 ) s++;
    if (*s == 0 ) {
        fprintf(stderr, "Include file declaration must start with a \" or \'\n");
        free(tname); *fname = NULL;
        return -1;
    }
    s++;
    i = 0;
    while ( *s != '"' && *s != '\'' && *s != 0 ) tname[i++] = *s++;
    if ( *s == 0 ) {
        fprintf(stderr, "Include file declaration must end with a \" or \'\n");
        free(tname); *fname = NULL;
        return -1;
    }
    tname[i] = 0;
    *fname = tname;
    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
int open_namelist(const char *fname)
{
    int nml = -1;
    FILE *f = fopen(fname, "r");
    NML *fl = NULL;

    if ( f == NULL ) {
        fprintf(stderr, "Could not open \"%s\"\n", fname);
        return -1;
    }

    nml = list_count++;
    file_list = realloc(file_list, sizeof(NML)*list_count);
    fl = &file_list[nml];
    fl->count = 0; fl->section = NULL;
    fl->fname = strdup(fname);
    nml_cur_fname = fl->fname;   /* D2 */

    lineno = 0;
    do  {
        while ( readline(f, buf) ) {
            if (strncasecmp(buf, "include ", 8) == 0 ) {
                push_file(f, fname);
                get_new_name(buf, &fname);
                f = fopen(fname, "r");
                if ( f == NULL ) {
                    fprintf(stderr, "Could not open include file \"%s\"\n", fname);
                    return -1;
                }
                continue;
            } else if (buf[0] != '&') {
                fprintf(stderr, "Error in %sfile \"%s\"\n", (npush)?"included ":"", fname);
                fprintf(stderr, "\"%s\"\n",buf);
                list_count--;
                file_list = realloc(file_list, sizeof(NML)*list_count);
                nml = -1;
                break;
            }
            fl->count++;
            fl->section = realloc(fl->section, sizeof(NML_Section)*fl->count);
            get_section(f, &fl->section[fl->count-1], &buf[1]);
        }
    } while ( ! pop_file(&f, &fname) );

    fclose(f);
#if DEBUG_NML
    show_namelist(nml);
    exit(0);
#endif

    return nml;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static NML_Entry *find_namelist_entry(int file, const char *section, const char *entry)
{
    NML *fl = &file_list[file];
    int i, j;

    for (i = 0; i < fl->count; i++) {
        NML_Section *ns = &fl->section[i];
        if ( strcasecmp(section, ns->name) != 0 ) continue;

        for (j = 0; j < ns->count; j++) {
             NML_Entry *ne = &ns->entry[j];

             if ( strcasecmp(entry, ne->name) == 0 ) { ne->used = 1; return ne; }   /* D5 */
        }
    }
    return NULL;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
int get_nml_listlen(int file, const char *section, const char *entry)
{
    NML_Entry *ne = find_namelist_entry(file, section, entry);
    if ( ne == NULL) return 0;
    return ne->count;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
int get_namelist(int file, NAMELIST *nl)
{
    const char *section;
    int i, ret = -1;

    if (nl->type != TYPE_START) return -1;
    section = nl->name;
    nl++;
    for (i = 0; i < file_list[file].count; i++)   /* D5: a section asked for is judged by check_namelist_unused */
        if ( strcasecmp(section, file_list[file].section[i].name) == 0 ) file_list[file].section[i].requested = 1;

    while (nl->type != TYPE_END) {
        NML_Entry *ne = find_namelist_entry(file, section, nl->name);

        if (ne != NULL) {
            ret = 0;

            // this is a fudge for the case where we've asked for reals but the config
            // has forgotten to put in a '.' so we've seen it as ints.
            if ( (nl->type & MASK_TYPE) == TYPE_DOUBLE &&
                 (ne->type & MASK_TYPE) == TYPE_INT ) {
                NML_Value *tr = malloc((ne->count+2)*sizeof(NML_Value));
                for (i = 0; i < ne->count; i++) tr[i].r = ne->data[i].i;
                free(ne->data);
                ne->data = tr;
                ne->type = TYPE_DOUBLE | (ne->type & MASK_LIST);
            }

            if ( (nl->type & MASK_LIST) ) {
                int count = ne->count, nofree=FALSE;
                switch (nl->type & MASK_TYPE) {
                    case TYPE_INT :
                        if ( (ne->type & MASK_TYPE) != TYPE_INT )   /* D3: read a real's bits as an integer */
                            nml_type_fatal(file, section, nl->name, TYPE_INT, ne->type);
                        for (i = 0; i < count; i++)
                            if ( ne->data[i].i < INT_MIN || ne->data[i].i > INT_MAX )
                                nml_range_fatal(file, section, nl->name, ne->data[i].i);
                        *((void**)(nl->data)) = malloc((count+2)*sizeof(int));
                        for (i = 0; i < count; i++) (*((int**)(nl->data)))[i] = ne->data[i].i;
                        break;
                    case TYPE_DOUBLE :
                        if ( (ne->type & MASK_TYPE) != TYPE_DOUBLE )   /* D3 (integers were widened above) */
                            nml_type_fatal(file, section, nl->name, TYPE_DOUBLE, ne->type);
                        *((void**)(nl->data)) = malloc((count+2)*sizeof(double));
                        for (i = 0; i < count; i++) (*((double**)(nl->data)))[i] = ne->data[i].r;
                        break;
                    case TYPE_STR :
                        if ( (ne->type & MASK_TYPE) != TYPE_STR )   /* D2 */
                            nml_type_fatal(file, section, nl->name, TYPE_STR, ne->type);
                        *((void**)(nl->data)) = malloc((count+2)*sizeof(char**));
                        for (i = 0; i < count; i++)
                            (*((char***)(nl->data)))[i] = (ne->data[i].s != NULL) ? strdup(ne->data[i].s) : NULL;
                        break;
                    case TYPE_BOOL :
                        if ( (ne->type & MASK_TYPE) != TYPE_BOOL )   /* D2 */
                            nml_type_fatal(file, section, nl->name, TYPE_BOOL, ne->type);
                        *((void**)(nl->data)) = malloc((count+2)*sizeof(int));
                        for (i = 0; i < count; i++) (*((int**)(nl->data)))[i] = ne->data[i].b;
                        break;
                    default :
                        fprintf(stderr, "    Value of unknown type %d\n", ne->type);
                        nofree=TRUE;
                        break;
                }
            } else {
                switch (nl->type & MASK_TYPE) {
                    case TYPE_INT :   /* D3: a non-integer used to leave the default silently */
                        if ( (ne->type & MASK_TYPE) == TYPE_BOOL ) {
                            //# accept .true./.false. for integer option flags (upstream c8fe70e)
                            *((int*)(nl->data)) = ne->data[0].b ? 1 : 0;
                        } else {
                            if ( (ne->type & MASK_TYPE) != TYPE_INT )
                                nml_type_fatal(file, section, nl->name, TYPE_INT, ne->type);
                            if ( ne->data[0].i < INT_MIN || ne->data[0].i > INT_MAX )
                                nml_range_fatal(file, section, nl->name, ne->data[0].i);
                            *((int*)(nl->data)) = ne->data[0].i;
                        }
                        break;
                    case TYPE_DOUBLE :   /* D3: a non-number used to leave the default silently */
                        if ( (ne->type & MASK_TYPE) != TYPE_DOUBLE )
                            nml_type_fatal(file, section, nl->name, TYPE_DOUBLE, ne->type);
                        *((double*)(nl->data)) = ne->data[0].r;
                        break;
                    case TYPE_STR :
                        if ( (ne->type & MASK_TYPE) != TYPE_STR )   /* D2 */
                            nml_type_fatal(file, section, nl->name, TYPE_STR, ne->type);
                        *((char**)(nl->data)) = ne->data[0].s;
                        break;
                    case TYPE_BOOL :
                        if ( (ne->type & MASK_TYPE) == TYPE_INT ) {
                            //# explicit coercion (upstream c8fe70e): a number for a logical key
                            *((_Bool*)(nl->data)) = (ne->data[0].i != 0);
                        } else {
                            if ( (ne->type & MASK_TYPE) != TYPE_BOOL )   /* D2 */
                                nml_type_fatal(file, section, nl->name, TYPE_BOOL, ne->type);
                            *((_Bool*)(nl->data)) = ne->data[0].b;
                        }
                        break;
                    default :
                        fprintf(stderr, "    Value of unknown type %d\n", ne->type);
                        break;
                }
            }
        }
        nl++;
    }

    return ret;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#if DEBUG_NML
/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static void show_entry(NML_Entry *ne)
{
    int k;
    char *ts = NULL;
    switch (ne->type & MASK_TYPE) {
        case TYPE_INT    : ts = "TYPE_INT"; break;
        case TYPE_DOUBLE : ts = "TYPE_DOUBLE"; break;
        case TYPE_STR    : ts = "TYPE_STR"; break;
        case TYPE_BOOL   : ts = "TYPE_BOOL"; break;
        default          : ts = "TYPE_UNKNOWN"; break;
    }
    fprintf(stderr, "  Entry %s has %d %s values\n", ne->name, ne->count, ts);
    for (k = 0; k < ne->count; k++) {
        switch (ne->type) {
            case TYPE_INT    : fprintf(stderr, "   Value : %ld\n", ne->data[k].i); break;
            case TYPE_DOUBLE : fprintf(stderr, "   Value : %12.4f\n", ne->data[k].r); break;
            case TYPE_STR    : fprintf(stderr, "   Value : \"%s\"\n", (ne->data[k].s)?ne->data[k].s:"NULL"); break;
            case TYPE_BOOL   : fprintf(stderr, "   Value : %s\n", (ne->data[k].b)?"TRUE":"FALSE"); break;
            default          : fprintf(stderr, "   Value of unknown type %d\n", ne->type); break;
        }
    }
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
static void show_namelist(int file)
{
    NML *fl = &file_list[file];
    int i, j;

    for (i = 0; i < fl->count; i++) {
        NML_Section *ns = &fl->section[i];
        fprintf(stderr, "Section %s has %d entries\n", ns->name, ns->count);
        for (j = 0; j < ns->count; j++)
            show_entry(&ns->entry[j]);
    }
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#endif


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
/* D5 2026-09-15: one WARNING per key of a section that get_namelist was asked for, but which nothing read - a
 * misspelt key used to be stored and ignored without a word. Sections never asked for are not judged (another
 * reader may own them). Returns the number of unread keys. */
int check_namelist_unused(int file)
{
    NML *fl = &file_list[file];
    int i, j, n = 0;

    for (i = 0; i < fl->count; i++) {
        NML_Section *ns = &fl->section[i];
        if ( !ns->requested ) continue;
        for (j = 0; j < ns->count; j++)
            if ( !ns->entry[j].used ) {
                fprintf(stderr, "WARNING: namelist \"%s\": &%s %s is not a key GLM reads - ignored\n",
                        fl->fname, ns->name, ns->entry[j].name);
                n++;
            }
    }
    return n;
}

void close_namelist(int file)
{
    NML *fl = &file_list[file];
    int i, j, k, err = 0;

    free(fl->fname);
    for (i = 0; i < fl->count; i++) {
        NML_Section *ns = &fl->section[i];

        for (j = 0; j < ns->count; j++) {
            NML_Entry *ne = &ns->entry[j];
            if ( ne->data != NULL ) {
                if ( ne->type == TYPE_STR ) {
                    NML_Value *nv;
                    for (k = 0; k < ne->count; k++) {
                        nv = &ne->data[k];
                        if (nv->s != NULL) free(nv->s);
                    }
                }
                free(ne->data);
            }
            free(ne->name);
        }
        free(ns->entry);
        free(ns->name);
    }
    free(fl->section);

    if (err) exit(1);
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
