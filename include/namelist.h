/******************************************************************************
 *                                                                            *
 * namelist.h                                                                 *
 *                                                                            *
 * Read fortran style namelists.                                              *
 *                                                                            *
 * Developed by :                                                             *
 *     AquaticEcoDynamics (AED) Group                                         *
 *     School of Agriculture and Environment                                  *
 *     The University of Western Australia                                    *
 *                                                                            *
 * Copyright 2013 - 2023 -  The University of Western Australia               *
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
#ifndef _NAMELIST_H_
#define _NAMELIST_H_

#define TYPE_START  0
#define TYPE_END    0

#define TYPE_NODATA 0
#define TYPE_INT    1
#define TYPE_DOUBLE 2
#define TYPE_STR    3
#define TYPE_BOOL   4
#define MASK_TYPE   0x0F
#define MASK_LIST   0x80

/******************************************************************************/

typedef struct _namelist_ {
    const char *name;
    int   type;
    void *data;
} NAMELIST;

/******************************************************************************/

int open_namelist(const char *fname);
int get_namelist(int file, NAMELIST *nl);
int get_nml_listlen(int file, const char *section, const char *entry);
void close_namelist(int file);

#endif
