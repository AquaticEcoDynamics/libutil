/******************************************************************************
 *                                                                            *
 * aed_time.h                                                                 *
 *                                                                            *
 *   some time utility functions                                              *
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
#ifndef _AED_TIME_H_
#define _AED_TIME_H_

#include "libutil.h"

#ifdef __STDC__

  /******************************************************************************/
  typedef struct timefmt {
      int Ypos, Ydig;
      int Mpos, Mdig;
      int Dpos, Ddig;
      int hpos, hdig;
      int mpos, mdig;
      int spos, sdig;
      char *fmt;
  } timefmt;

  void calendar_date(int julian, int *yyyy, int *mm, int *dd);
  int julian_day(int y, int m, int d);
  void read_time_string(const char *timestr, int *jul, int *secs);
  void write_time_string(char *timestr, int jul, int secs);
  int time_diff(int jul1, int secs1, int jul2, int secs2);
  int day_of_year(int jday);

  timefmt *decode_time_format(const char *fmt);
  void read_time_formatted(const char *timestr, timefmt *tf, int *jul, int *secs);
  void write_time_formatted(char *timestr, timefmt *tf, int jul, int secs);

#else

  INTERFACE

     SUBROUTINE calendar_date(julian,yyyy,mm,dd) BIND(C, name="calendar_date_")
        USE ISO_C_BINDING
        CINTEGER,INTENT(in)  :: julian
        CINTEGER,INTENT(out) :: yyyy,mm,dd
     END SUBROUTINE calendar_date

     CINTEGER FUNCTION julian_day(yyyy,mm,dd) BIND(C, name="julian_day_")
        USE ISO_C_BINDING
        CINTEGER,INTENT(in) :: yyyy,mm,dd
     END FUNCTION julian_day

     SUBROUTINE read_time_string(timestr, jul, secs) BIND(C, name="read_time_string_")
        USE ISO_C_BINDING
        CCHARACTER,INTENT(in) :: timestr(*)
        CINTEGER,INTENT(out)  :: jul,secs
     END SUBROUTINE read_time_string

     SUBROUTINE write_time_string(timestr, jul, secs) BIND(C, name="write_time_string_")
        USE ISO_C_BINDING
        CCHARACTER,INTENT(out) :: timestr(*)
        CINTEGER,INTENT(in)    :: jul,secs
     END SUBROUTINE write_time_string

     CINTEGER FUNCTION time_diff(jul1,secs1,jul2,secs2) BIND(C, name="time_diff_")
        USE ISO_C_BINDING
        CINTEGER,INTENT(in) :: jul1,secs1,jul2,secs2
     END FUNCTION time_diff

     CINTEGER FUNCTION day_of_year(jday) BIND(C, name="day_of_year_")
        USE ISO_C_BINDING
        CINTEGER,INTENT(in) :: jday
     END FUNCTION day_of_year

 END INTERFACE

!+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#endif

#endif
