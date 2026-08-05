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
} NML_Entry;

typedef struct _nml_sect {
    char *name;
    int   count;
    NML_Entry *entry;
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
            if (*s == '"' || *s == '\'' ) {
                term = *s++;
                while (*s && *s != term) s++;
                if (*s != term) { fprintf(stderr, "Unterminated string\n"); exit(1); }
            }
            if (*s == '\\') s++;
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

    if ( strncasecmp(s, ".true.", 6) == 0 ) {
        *b = TRUE;
        return TYPE_BOOL;
    }
    if ( strncasecmp(s, ".false.", 7) == 0 ) {
        *b = FALSE;
        return TYPE_BOOL;
    }

    if ( sscanf(s, "%lf", r) == 0 ) *r = 0.0/zero;

    if ( strpbrk(s, ".Ee") == NULL ) {
        if ( sscanf(s, "%lld", i) == 1 ) {
            return TYPE_INT;
        }
    }
    if ( sscanf(s, "%lf", r) == 1 )
        return TYPE_DOUBLE;

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
            term = *r++; s++;
            while (*r && *r != term) {
                if (*r == '\\' ) r++;
                r++;
            }
            if ( *r != term ) { fprintf(stderr, "unmatched '%c'\n", term); exit(1); }

            n = r++ - s;

            d = grab_substring(s, n);

            type = TYPE_STR;

            while (*r && *r != ',' ) r++;
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

            free(d);
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

    do  {
        if (r[0] != 0) extract_values(entry, r);

        if ( (r = readline(f, buf) ) ) {
            if ( strcmp(buf, "/") == 0 ) return 1;
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

    readline(f, buf);
    if ( strcmp(buf, "/") == 0 ) return 1;

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

             if ( strcasecmp(entry, ne->name) == 0 ) return ne;
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
                        *((void**)(nl->data)) = malloc((count+2)*sizeof(int));
                        for (i = 0; i < count; i++) (*((int**)(nl->data)))[i] = ne->data[i].i;
                        break;
                    case TYPE_DOUBLE :
                        *((void**)(nl->data)) = malloc((count+2)*sizeof(double));
                        for (i = 0; i < count; i++) (*((double**)(nl->data)))[i] = ne->data[i].r;
                        break;
                    case TYPE_STR :
                        *((void**)(nl->data)) = malloc((count+2)*sizeof(char**));
                        for (i = 0; i < count; i++) (*((char***)(nl->data)))[i] = ne->data[i].s;
                        break;
                    case TYPE_BOOL :
                        *((void**)(nl->data)) = malloc((count+2)*sizeof(int));
                        for (i = 0; i < count; i++) (*((int**)(nl->data)))[i] = ne->data[i].b;
                        break;
                    default :
                        fprintf(stderr, "    Value of unknown type %d\n", ne->type);
                        nofree=TRUE;
                        break;
                }
                if (!nofree) { free(ne->data); ne->data = *((void**)(nl->data)); }
            } else {
                switch (nl->type & MASK_TYPE) {
                    case TYPE_INT :
                        if ( (ne->type & MASK_TYPE) == TYPE_INT )
                            *((int*)(nl->data)) = ne->data[0].i;
                        else if ( (ne->type & MASK_TYPE) == TYPE_BOOL )
                            //# accept .true./.false. for integer option flags
                            *((int*)(nl->data)) = ne->data[0].b ? 1 : 0;
                        break;
                    case TYPE_DOUBLE :
                        if ( (ne->type & MASK_TYPE) == TYPE_DOUBLE )
                            *((double*)(nl->data)) = ne->data[0].r;
                        break;
                    case TYPE_STR :
                        *((char**)(nl->data)) = ne->data[0].s;
                        break;
                    case TYPE_BOOL :
                        if ( (ne->type & MASK_TYPE) == TYPE_INT )
                            //# explicit coercion: numeric bools were previously
                            //  read via union punning of the low bytes
                            *((_Bool*)(nl->data)) = (ne->data[0].i != 0);
                        else
                            *((_Bool*)(nl->data)) = ne->data[0].b;
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
 * Write out a NAMELIST table's current values in the same &section/entry=/  *
 * syntax get_namelist() reads. Scalars print whatever is presently in the   *
 * target variable; MASK_LIST (array) entries print a commented-out          *
 * placeholder, since this table alone carries no reliable array length.     *
 ******************************************************************************/
void write_namelist(FILE *out, NAMELIST *nl, NML_Describe_Fn describe)
{
    char note[512];

    if (nl == NULL || nl->type != TYPE_START || nl->name == NULL) return;

    fprintf(out, "&%s\n", nl->name);
    nl++;

    while (nl->type != TYPE_END) {
        const char *key = nl->name;
        const char *desc = (describe != NULL) ? describe(key) : NULL;

        if (nl->type & MASK_LIST) {
            const char *sample;
            switch (nl->type & MASK_TYPE) {
                case TYPE_STR  : sample = "'a', 'b'";        break;
                case TYPE_BOOL : sample = ".true., .false."; break;
                default        : sample = "0, 0";            break;
            }
            if (desc != NULL)
                snprintf(note, sizeof(note), "%s (comma-separated list; unset by default)", desc);
            else
                snprintf(note, sizeof(note), "list (comma-separated); unset by default");
            fprintf(out, "!  %-22s = %-18s !# %s\n", key, sample, note);
        } else {
            switch (nl->type & MASK_TYPE) {
                case TYPE_INT :
                    if (desc != NULL) fprintf(out, "   %-22s = %-18d !# %s\n", key, *((int*)(nl->data)), desc);
                    else               fprintf(out, "   %-22s = %d\n", key, *((int*)(nl->data)));
                    break;
                case TYPE_DOUBLE :
                    if (desc != NULL) fprintf(out, "   %-22s = %-18.6g !# %s\n", key, *((double*)(nl->data)), desc);
                    else               fprintf(out, "   %-22s = %.6g\n", key, *((double*)(nl->data)));
                    break;
                case TYPE_STR : {
                    char *v = *((char**)(nl->data));
                    if (v != NULL) {
                        if (desc != NULL) fprintf(out, "   %-22s = '%s'   !# %s\n", key, v, desc);
                        else               fprintf(out, "   %-22s = '%s'\n", key, v);
                    } else {
                        if (desc != NULL) snprintf(note, sizeof(note), "%s (unset by default)", desc);
                        else               snprintf(note, sizeof(note), "unset by default");
                        fprintf(out, "!  %-22s = %-18s !# %s\n", key, "''", note);
                    }
                    break;
                }
                case TYPE_BOOL : {
                    const char *bv = (*((_Bool*)(nl->data))) ? ".true." : ".false.";
                    if (desc != NULL) fprintf(out, "   %-22s = %-18s !# %s\n", key, bv, desc);
                    else               fprintf(out, "   %-22s = %s\n", key, bv);
                    break;
                }
                default :
                    fprintf(out, "!  %-22s = ?                  !# unknown type, please check\n", key);
                    break;
            }
        }
        nl++;
    }
    fprintf(out, "/\n\n");
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
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
