/******************************************************************************
 *                                                                            *
 * aed_time.c                                                                 *
 *                                                                            *
 *   some time utility functions                                              *
 *                                                                            *
 * Developed by :                                                             *
 *     AquaticEcoDynamics (AED) Group                                         *
 *     School of Agriculture and Environment                                  *
 *     The University of Western Australia                                    *
 *                                                                            *
 * Copyright 2013 - 2024 -  The University of Western Australia               *
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
#include <math.h>

#include "libutil.h"
#include "aed_time.h"



/******************************************************************************
 *  Convert a true Julian day to a calendar date --- year, month and day.     *
 ******************************************************************************/
void calendar_date(int julian, int *yyyy, int *mm, int *dd)
{
    int j = julian;
    int y, m, d;

    j = j - 1721119 ;
    y = (4 * j - 1) / 146097;

    j = 4 * j - 1 - 146097 * y;
    d = j / 4;
    j = (4 * d + 3) / 1461;

    d = 4 * d + 3 - 1461 * j;
    d = (d + 4) / 4 ;
    m = (5 * d - 3) / 153;

    d = 5 * d - 3 - 153 * m;
    d = (d + 5) / 5 ;
    y = 100 * y + j ;

    if (m < 10)
        m = m + 3;
    else {
        m = m - 9;
        y = y + 1;
    }
    *yyyy = y;
    *mm = m;
    *dd = d;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 *  Convert a calendar date to true Julian day                                *
 ******************************************************************************/
int julian_day(int y, int m, int d)
{
    int ya, c;

    if (m > 2)
        m = m - 3;
    else {
        m = m + 9;
        y = y - 1;
    }

    c = y / 100;
    ya = y - 100 * c;
    return (146097 * c) / 4 + (1461 * ya) / 4 + (153 * m + 2) / 5 + d + 1721119;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 * Converts a time string to the true Julian day and seconds of that day.     *
 * The format of the time string must be:  YYYY-MM-DD hh:mm:ss .              *
 ******************************************************************************/
void read_time_string(const char *timestr, int *jul, int *secs)
{
    int yy,mm,dd,hh,min,ss,n;

    *jul = 0; *secs = 0;
    n = sscanf(timestr, "%4d-%2d-%2d %2d:%2d:%2d", &yy, &mm, &dd, &hh, &min, &ss);
    if ( n > 2 ) *jul = julian_day(yy, mm, dd);
    if ( n > 4 ) *secs = 3600 * hh + 60 * min;
    if ( n > 5 ) *secs += ss;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 * Formats Julian day and seconds of that day to a nice looking               *
 * character string.                                                          *
 ******************************************************************************/
void write_time_string(char *timestr, int jul, int secs)
{
    int ss,min,hh,dd,mm,yy;

    hh   = secs/3600;
    min  = (secs-hh*3600)/60;
    ss   = secs - 3600*hh - 60*min;

    calendar_date(jul,&yy,&mm,&dd);

    sprintf(timestr, "%04d-%02d-%02d %02d:%02d:%02d", yy,mm,dd,hh,min,ss);
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 * This functions returns the time difference between two                     *
 * dates in seconds. The dates are given as Julian day and seconds            *
 * of that day.                                                               *
 ******************************************************************************/
int time_diff(int jul1, int secs1, int jul2, int secs2)
{
    return 86400*(jul1-jul2) + (secs1-secs2);
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************/
int day_of_year(int jday)
{
    int y,m,d;
    calendar_date(jday,&y,&m,&d);
    return jday - julian_day(y,1,1);
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 * Decode a time format string into a timefmt structure.                      *
 ******************************************************************************/
timefmt *decode_time_format(const char *fmt)
{
    int l, pos;
    char *f, *s;
    char fmtbuf[80];
    timefmt tf, *t;

    tf.Ypos = -1; tf.Mpos = -1; tf.Dpos = -1;
    tf.hpos = -1; tf.mpos = -1; tf.spos = -1;

    f = (char*)fmt; s = fmtbuf;
    l = 0; pos = 0;
    while (*f) {
        switch (*f) {
            case 'Y' :
            case 'M' :
            case 'D' :
            case 'h' :
            case 'm' :
            case 's' :
                l = 1;
                while (f[1] == *f) {
                    f++; l++;
                }
                *s++ = '%'; if ( l > 1 ) *s++ = '0' + l; *s++ = 'd';
                switch (*f) {
                    case 'Y' : tf.Ypos = pos++; tf.Ydig = l; break;
                    case 'M' : tf.Mpos = pos++; tf.Mdig = l; break;
                    case 'D' : tf.Dpos = pos++; tf.Ddig = l; break;
                    case 'h' : tf.hpos = pos++; tf.hdig = l; break;
                    case 'm' : tf.mpos = pos++; tf.mdig = l; break;
                    case 's' : tf.spos = pos++; tf.sdig = l; break;
                }
                break;
            default:
                *s++ = *f;
                break;
        }
        f++; *s = 0;
    }
    tf.fmt = strdup(fmtbuf);

    t = malloc(sizeof(timefmt));
    memcpy(t, &tf, sizeof(timefmt));

    return t;
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 * Converts a time string to the true Julian day and seconds of that day.     *
 * The format of the time string is given in a timefmt structure.             *
 ******************************************************************************/
void read_time_formatted(const char *timestr, timefmt *tf, int *jul, int *secs)
{
    int n;
    int vals[7], *v = &vals[1];

    vals[0] = 0;
    *jul = 0; *secs = 0;

    n = sscanf(timestr, tf->fmt, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]);
    if ( n > 2 ) *jul = julian_day(v[tf->Ypos], v[tf->Mpos], v[tf->Dpos]);
    if ( n > 4 ) *secs = 3600 * v[tf->hpos] + 60 * v[tf->mpos];
    if ( n > 5 ) *secs += v[tf->spos];
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/******************************************************************************
 * Formats Julian day and seconds of that day to a nice looking               *
 * character string according to the format in the timefmt structure.         *
 ******************************************************************************/
void write_time_formatted(char *timestr, timefmt *tf, int jul, int secs)
{
    int ss,min,hh,dd,mm,yy;
    int vals[7], *v = &vals[1];

    hh   = secs/3600;
    min  = (secs-hh*3600)/60;
    ss   = secs - 3600*hh - 60*min;

    calendar_date(jul,&yy,&mm,&dd);

    v[tf->spos] = ss; v[tf->mpos] = min; v[tf->hpos] = hh;
    v[tf->Dpos] = dd; v[tf->Mpos] = mm;  v[tf->Ypos] = yy;

    sprintf(timestr, tf->fmt, v[0], v[1], v[2], v[3], v[4], v[5]);
}
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
