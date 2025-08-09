/******************************************************************************
 *                                                                            *
 * aed_csv.c                                                                  *
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "libutil.h"
#include "aed_csv.h"
#include "aed_time.h"


/*----------------------------------------------------------------------------*/

typedef struct _AED_CSV_OUT {
    FILE    *f;
    char     time[20];
    int      n_cols;
    char   **header;
    AED_REAL buff[MAX_OUT_VALUES+4];
} AED_CSV_OUT;

static int _n_outf = 0;
static AED_CSV_OUT csv_of[MAX_OUT_FILES];

typedef struct _AED_CSV_IN {
    FILE  *f;
    int    n_cols;
    char **header;
    AED_REAL *curLine;
    timefmt  *tf;
} AED_CSV_IN;

static int _n_inf = -1;
static AED_CSV_IN csv_if[MAX_IN_FILES];


static const AED_REAL missing = MISVAL;
//static const AED_REAL zero = 0.;
// VS C compiler doesnt like the first for, but is OK with t'other
//static const AED_REAL NaN = missing / zero;
static const AED_REAL NaN = MISVAL / 0.;

#define BUFCHUNK    10240


/*============================================================================*/

/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
static char *_ln = NULL;
static char *read_line(FILE *inf)
{
    char *ln = _ln;
    int size = BUFCHUNK;

    if ( feof(inf) ) return NULL;

    _ln = (ln = (char *)realloc(ln, size));
    ln[0] = 0;
    if ( feof(inf) || fgets(ln, size, inf) == NULL) return NULL;

    while ( !feof(inf) && strlen(ln) && ln[strlen(ln)-1] != '\n' ) {
        _ln = (ln = (char *)realloc(ln, size+BUFCHUNK));
        if (fgets(&ln[size-1], BUFCHUNK+1, inf) == NULL) {
            ln[size-1] = 0;
            return ln;
        }
        size += BUFCHUNK;
    }

    // strip off any LF or CR characters
    while ( ln[0] != 0 && (ln[strlen(ln)-1] == '\n' || ln[strlen(ln)-1] == '\r' ) )
        ln[strlen(ln)-1] = 0;

    if ( !strlen(ln) && feof(inf) ) {
        free(ln);
        ln = NULL;
    }

    return ln;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************/
static char **break_line(const char *b, int *n)
{
    char **ret = NULL;
    char *t = NULL, *s;
    char term;
    int  n_strs = 0;
    size_t len;

    while (b && *b ) {
        term = 0;
        if ( *b == '"' || *b == '\'' )
           term = *b++;
        t = (char*)b;
        while ( *b ) {
            if ( *b == '\\' ) b+=2;
            else if ( term && *b != term ) b++;
            else if ( *b != ',' ) b++;
            else break;
        }
        while (*t && (*t == ' ' || *t == '\t')) t++;
        len = b - t;
        if ( term && *b == term ) len--;
        while (len && (t[len-1] == ' ' || t[len-1] == '\t')) len--;
        s = malloc(len+1);
        strncpy(s, t, len); s[len] = 0;
        n_strs++;
        ret = realloc(ret, (n_strs+1)*sizeof(char*));
        ret[n_strs-1] = s;
        ret[n_strs] = NULL;
        if ( *b ) b++;
    }

    *n = n_strs;
    return ret;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************/
static int check_it(int csv, int idx)
{
    return (csv >= 0 && csv < _n_inf &&
            idx >= 0 && idx < csv_if[csv].n_cols);
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int open_csv_input(const char *fname, const char *timefmt)
{
    FILE *f = NULL;
    int cols, i;

    if ( _n_inf < 0 ) {
        for (i = 0; i < MAX_IN_FILES; i++) {
            csv_if[i].f = NULL;
            csv_if[i].n_cols = 0;
            csv_if[i].header = NULL;
            csv_if[i].curLine = NULL;
            csv_if[i].tf = NULL;
        }
        _n_inf = 0;
    }

    if ( _n_inf >= MAX_IN_FILES ) {
        fprintf(stderr, "Too many csv_files open\n");
        return -1;
    }

    if ( (f = fopen(fname, "r")) == NULL ) {
        fprintf(stderr, "Cannot find file \"%s\"\n", fname);
        return -1;
    }

    csv_if[_n_inf].f = f;
    csv_if[_n_inf].header = break_line( read_line(f), &cols );
    csv_if[_n_inf].n_cols = cols;
    csv_if[_n_inf].curLine = malloc(sizeof(AED_REAL)*cols);
    if (timefmt != NULL)
        csv_if[_n_inf].tf = decode_time_format(timefmt);
    else
        csv_if[_n_inf].tf = NULL;

    load_csv_line(_n_inf);
    return _n_inf++;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int close_csv_input(int csvf)
{
    int i;

    if ( csvf < 0 || csvf > _n_inf ) {
        fprintf(stderr, "Request close for invalid csv file number\n");
        return -1;
    }

    if ( csv_if[csvf].f != NULL ) fclose(csv_if[csvf].f);
    csv_if[csvf].f = NULL;
    if ( csv_if[csvf].header != NULL ) {
        for (i = 0; i < csv_if[csvf].n_cols; i++ )
            free(csv_if[csvf].header[i]);
        free(csv_if[csvf].header);
    }
    csv_if[csvf].n_cols = 0;
    csv_if[csvf].header = NULL;
    if ( csv_if[csvf].curLine != NULL ) free(csv_if[csvf].curLine);
    csv_if[csvf].curLine = NULL;
    if ( csv_if[csvf].tf != NULL) {
        free(csv_if[csvf].tf->fmt);
        free(csv_if[csvf].tf);
    }
    csv_if[csvf].tf = NULL;

    if ( csvf == _n_inf-1 ) _n_inf--;

    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int count_lines(const char *fname)
{
    FILE *f = NULL;
    int count = -1;    /* start from -1 because we don't count the first line */

    if ( (f = fopen(fname, "r")) == NULL ) {
        fprintf(stderr, "Cannot find file \"%s\"\n", fname);
        return -1;
    }

    while (read_line(f) != NULL)
        count++;

    fclose(f);

    return count;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int find_csv_var(int csv, const char *name)
{
    int i;

    if (csv >= _n_inf) return -1;

    for (i = 0; i < csv_if[csv].n_cols; i++) {
        if ( strcasecmp(name, csv_if[csv].header[i]) == 0 ) return i;
    }

    return -1;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int load_csv_line(int csv)
{
    char **b = NULL;
    int    count, i, ret = TRUE;
    double num;
    int    jul, secs;

    if ( csv < 0 || csv > _n_inf ) {
        fprintf(stderr, "Request load for invalid csv file number\n");
#if DEBUG
        CRASH("load_csv_line");
#else
        exit(1);
#endif
    }

    b = break_line(read_line(csv_if[csv].f), &count);

    if ( b == NULL || count != csv_if[csv].n_cols )
        ret = FALSE;
    else {
        if (csv_if[csv].tf != NULL)
            read_time_formatted(b[0], csv_if[csv].tf, &jul, &secs);
        else
            read_time_string(b[0], &jul, &secs);
        num = secs; num /= 86400.0; num += jul;
        csv_if[csv].curLine[0] = num;
        free(b[0]);

        for (i = 1; i < count; i++) {
            if (strlen(b[i]) > 0)
                sscanf(b[i], "%lf", &csv_if[csv].curLine[i]);
            else
                csv_if[csv].curLine[i] = NaN;
            free(b[i]);
        }
    }
    if ( b != NULL ) free(b);

    return ret;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int get_csv_type(int csv, int idx)
{
    if ( check_it(csv,idx) ) return csv_if[csv].curLine[idx];
    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int get_csv_val_i(int csv, int idx)
{
    if ( check_it(csv,idx) ) return csv_if[csv].curLine[idx];
    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
AED_REAL get_csv_val_r(int csv, int idx)
{
    if ( check_it(csv,idx) ) return csv_if[csv].curLine[idx];
    return 0.;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int get_csv_val_s(int csv, int idx, char *s)
{
    if ( check_it(csv,idx) ) return csv_if[csv].curLine[idx];
    return 0;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
const char *get_csv_colname(int csv, int idx)
{
    if ( csv >= _n_inf ) return NULL;
    if ( idx >= csv_if[csv].n_cols || idx < 0 ) return NULL;

    return (const char*)(csv_if[csv].header[idx]);
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int open_csv_output(const char *out_dir, const char *fname)
{
    char *path = NULL;
    size_t len;
    int ret = -1;

    if ( out_dir != NULL && strcmp(out_dir, ".") != 0 ) {
        len = strlen(out_dir) + strlen(DIRSEP) + strlen(fname) + 5;
        path = malloc(len);
        snprintf(path, len, "%s%s%s.csv", out_dir, DIRSEP, fname);
    } else {
        len = strlen(fname) + 5;
        path = malloc(len);
        snprintf(path, len, "%s.csv", fname);
    }

    if ( (csv_of[_n_outf].f = fopen(path, "w")) == NULL ) {
        fprintf(stderr, "Failed to open \"%s\"\n", path);
        ret = -1;
    } else {
#ifndef _WIN32
        struct stat stat;
        fstat(fileno(csv_of[_n_outf].f), &stat);
        if ( S_ISFIFO(stat.st_mode) ) {
            // at most buffer only lines in fifo pipes
        //  setvbuf(csv_of[_n_outf].f, NULL, _IONBF, 0);
            setlinebuf(csv_of[_n_outf].f);
        }
#endif
        ret = _n_outf++;
    }
    free(path);
    return ret;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
int close_csv_output(int outf)
{
    int ret;

    if ( outf < 0 || outf >= MAX_OUT_FILES ) return -1;
    ret = fclose(csv_of[outf].f);
    csv_of[outf].f = NULL;
    return ret;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
static void _add_header(char ***hdrs, int *n, const char *name)
{
    char **l_hdrs = *hdrs;
    int l_n = *n + 1;
    if ( (l_hdrs = realloc(l_hdrs, sizeof(char*)*l_n)) == NULL ) {
        fprintf(stderr, "Out of memory error\n");
        return;
    }
    l_hdrs[*n] = (char*)name;
    *n = l_n;
    *hdrs = l_hdrs;
}
/*----------------------------------------------------------------------------*/
void csv_header_start(int f)
{
    fputs("time", csv_of[f].f);
    csv_of[f].n_cols = 0;
    _add_header(&csv_of[f].header, &csv_of[f].n_cols, "time");
    strcpy(csv_of[f].time, "INVALID");
}
/*----------------------------------------------------------------------------*/
void csv_header_var(int f, const char *v)
{
    fprintf(csv_of[f].f, ",%s", v);
    csv_of[f].buff[csv_of[f].n_cols] = missing;
    _add_header(&csv_of[f].header, &csv_of[f].n_cols, v);
}
/*----------------------------------------------------------------------------*/
void csv_header_var2(int f, const char *v, const char *units)
{
    fprintf(csv_of[f].f, ",%s [%s]", v, units);
    csv_of[f].buff[csv_of[f].n_cols] = missing;
    _add_header(&csv_of[f].header, &csv_of[f].n_cols, v);
}
/*----------------------------------------------------------------------------*/
void csv_header_end(int f)
{
    fputc('\n', csv_of[f].f);
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
void write_csv_start(int f, const char *cval) { fputs(cval, csv_of[f].f); }
void write_csv_val(int f, AED_REAL val) { fprintf(csv_of[f].f, ",%15.6f", val); }
void write_csv_end(int f) { fputc('\n', csv_of[f].f); }
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 ******************************************************************************/
void write_csv_var(int f, const char *name, AED_REAL val, const char *cval, int last)
{
    int i;

    if ( csv_of[f].f == NULL ) return;

    if (strcasecmp(name, "time") == 0) {
        strncpy(csv_of[f].time, cval, 19); csv_of[f].time[19] = 0;
    } else if ( *name != 0) {
        for (i = 0; i < csv_of[f].n_cols; i++) {
            if ( strcasecmp(name, csv_of[f].header[i]) == 0 ) {
                csv_of[f].buff[i] = val;
                if ( !last ) return;
                break;
            }
        }
    }

    if (last && (strcasecmp(csv_of[f].time, "INVALID") != 0)) {
        fputs(csv_of[f].time, csv_of[f].f);

        for (i = 1; i < csv_of[f].n_cols; i++)
            fprintf(csv_of[f].f, ",%12.6f", csv_of[f].buff[i]);

        fputc('\n', csv_of[f].f);

        strcpy(csv_of[f].time, "INVALID");
        for (i = 0; i < csv_of[f].n_cols; i++)
            csv_of[f].buff[i] = missing;
    }
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *                                                                            *
 *                                                                            *
 ******************************************************************************/
void find_day(int csv, int time_idx, int jday)
{
    int y,m,d;
    AED_REAL tr;

    if ( !check_it(csv, time_idx) ) {
        fprintf(stderr, "Fatal error in find_day: file %d index %d\n", csv, time_idx);
#if DEBUG
        CRASH("find_day");
#else
        exit(1);
#endif
    }

    while( (tr = get_csv_val_r(csv, time_idx)) < jday) {
        if ( !load_csv_line(csv) ) {
            calendar_date(jday, &y, &m, &d);
            fprintf(stderr,"Day %d (%d-%02d-%02d) not found\n", jday, y, m, d);
#if DEBUG
            CRASH("find_day");
#else
            exit(1);
#endif
        }
    }
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
