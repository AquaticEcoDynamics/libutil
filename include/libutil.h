/******************************************************************************
 *                                                                            *
 * libutil.h                                                                  *
 *                                                                            *
 * Developed by :                                                             *
 *     AquaticEcoDynamics (AED) Group                                         *
 *     School of Agriculture and Environment                                  *
 *     The University of Western Australia                                    *
 *                                                                            *
 * Copyright 2013 - 2019 -  The University of Western Australia               *
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
#ifndef _LIBUTIL_H_
#define _LIBUTIL_H_

#define LIB_UTIL_VERSION  "1.0.9"

#ifndef AED_REAL
   #if SINGLE
     #define AED_REAL float
   #else
     #define AED_REAL double
   #endif
#endif
#ifndef LOGICAL
#  define LOGICAL  int
#endif
#ifndef TRUE
#  define TRUE    1
#  define FALSE   0
#endif
#ifndef MISVAL
#  define MISVAL -9999.
#endif
#ifdef _WIN32
#  define DIRSEP "\\"
#  define snprintf _snprintf
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
   double fmod(double x, double y);
#else
#  define DIRSEP "/"
#endif

#if DEBUG
#define CRASH(s) ( { int *x = (int*)1; fputs(s, stderr); *x = 1; } )
#endif

#endif
