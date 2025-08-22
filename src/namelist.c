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
 * Copyright 2013 - 2025 - The University of Western Australia                *
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

#define BUFCHUNK      10240

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
                if ((nl->type & MASK_TYPE) == TYPE_INT) {
                    *((void**)(nl->data)) = malloc((ne->count+2)*sizeof(int));
                    for (i = 0; i < ne->count; i++) (*((int**)(nl->data)))[i] = ne->data[i].i;
                    free(ne->data); ne->data = *((void**)(nl->data));
                } else {
                    *((void**)(nl->data)) = (void*)(ne->data);
                }
            } else {
                switch (nl->type & MASK_TYPE) {
                    case TYPE_INT :
                        if ( (ne->type & MASK_TYPE) == TYPE_INT )
                            *((int*)(nl->data)) = ne->data[0].i;
                        break;
                    case TYPE_DOUBLE :
                        if ( (ne->type & MASK_TYPE) == TYPE_DOUBLE )
                            *((double*)(nl->data)) = ne->data[0].r;
                        break;
                    case TYPE_STR :
                        *((char**)(nl->data)) = ne->data[0].s;
                        break;
                    case TYPE_BOOL :
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
//              fprintf(stderr, "free entry \"%s\"in section \"%s\" - %p\n",
//                                                ne->name, ns->name, ne->data);
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
