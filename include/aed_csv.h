/******************************************************************************
 *                                                                            *
 * aed_csv.h                                                                  *
 *                                                                            *
 * Developed by :                                                             *
 *     AquaticEcoDynamics (AED) Group                                         *
 *     School of Earth & Environment                                          *
 *     The University of Western Australia                                    *
 *                                                                            *
 * Copyright 2013 - 2016 -  The University of Western Australia               *
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
#ifndef _AED_CSV_H_
#define _AED_CSV_H_

#include "libutil.h"

#define MAX_OUT_VALUES   40

#define bufsize 2048

#ifdef _FORTRAN_VERSION_

  INTERFACE

     CINTEGER FUNCTION open_csv_input(fname,len,tf,l2) BIND(C, name="open_csv_input_")
        USE ISO_C_BINDING
        CCHARACTER,INTENT(in)  :: fname(*)
        CINTEGER,INTENT(in) :: len
        CCHARACTER,INTENT(in)  :: tf(*)
        CINTEGER,INTENT(in) :: l2
     END FUNCTION open_csv_input

     CINTEGER FUNCTION find_csv_var(csv,name,len) BIND(C, name="find_csv_var_")
        USE ISO_C_BINDING
        CINTEGER,INTENT(in)   :: csv
        CCHARACTER,INTENT(in) :: name(*)
        CINTEGER,INTENT(in) :: len
     END FUNCTION find_csv_var

     CLOGICAL FUNCTION load_csv_line(csv)
        USE ISO_C_BINDING
        CINTEGER,INTENT(in) :: csv
     END FUNCTION load_csv_line

     CINTEGER FUNCTION get_csv_type(csv, idx)
        USE ISO_C_BINDING
        CINTEGER,INTENT(in) :: csv, idx
     END FUNCTION get_csv_type
     CINTEGER FUNCTION get_csv_val_i(csv, idx)
        USE ISO_C_BINDING
        CINTEGER,INTENT(in) :: csv, idx
     END FUNCTION get_csv_val_i
     AED_REAL FUNCTION get_csv_val_r(csv, idx)
        USE ISO_C_BINDING
        CINTEGER,INTENT(in) :: csv, idx
     END FUNCTION get_csv_val_r
     CINTEGER FUNCTION get_csv_val_s(csv, idx, s)
        USE ISO_C_BINDING
        CINTEGER,INTENT(in) :: csv, idx
        CCHARACTER,INTENT(out) :: s
     END FUNCTION get_csv_val_s

    !----------------------------------------------------

  END INTERFACE

#else

/*############################################################################*/

  int open_csv_input_(const char *fname, int *len, const char *timefmt, int *l2);
  int find_csv_var_(int *csv, const char *name, int *len);

  int open_csv_input(const char *fname, const char *timefmt);
  int count_lines(const char *fname);
  int find_csv_var(int csv, const char *name);

  int load_csv_line(int csv);
  int get_csv_type(int csv, int idx);
  int get_csv_val_i(int csv, int idx);
  AED_REAL get_csv_val_r(int csv, int idx);
  int get_csv_val_s(int csv, int idx, char *s);

  int close_csv_input(int csvf);

  int open_csv_output(const char *out_dir, const char *fname);
  int close_csv_output(int outf);

  void csv_header_start(int f);
  void csv_header_var(int f, const char *v);
  void csv_header_var2(int f, const char *v, const char *units);
  void csv_header_end(int f);

  void write_csv_start(int f, const char *cval);
  void write_csv_val(int f, AED_REAL val);
  void write_csv_end(int f);
  void write_csv_var(int f, const char *name, AED_REAL val, const char *cval, int last);

  void find_day(int csv, int time_idx, int jday);

#endif

#endif
