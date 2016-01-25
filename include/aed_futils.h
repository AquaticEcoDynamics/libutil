!*******************************************************************************
!*                                                                             *
!* aed_futils.h                                                                *
!*                                                                             *
!* Developed by :                                                              *
!*     AquaticEcoDynamics (AED) Group                                          *
!*     School of Earth & Environment                                           *
!*     The University of Western Australia                                     *
!*                                                                             *
!* Copyright 2013 - 2016 -  The University of Western Australia                *
!*                                                                             *
!*  This file is part of GLM (General Lake Model)                              *
!*                                                                             *
!*  libutil is free software: you can redistribute it and/or modify            *
!*  it under the terms of the GNU General Public License as published by       *
!*  the Free Software Foundation, either version 3 of the License, or          *
!*  (at your option) any later version.                                        *
!*                                                                             *
!*  libutil is distributed in the hope that it will be useful,                 *
!*  but WITHOUT ANY WARRANTY; without even the implied warranty of             *
!*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
!*  GNU General Public License for more details.                               *
!*                                                                             *
!*  You should have received a copy of the GNU General Public License          *
!*  along with this program.  If not, see <http://www.gnu.org/licenses/>.      *
!*                                                                             *
!*******************************************************************************
#ifndef _AED_FUTILS_H_
#define _AED_FUTILS_H_

   INTERFACE

      !#########################################################################
      FUNCTION MYTRIM(str) RESULT(res)
      !-------------------------------------------------------------------------
      CHARACTER(*),TARGET :: str
      CHARACTER(:),POINTER :: res
      INTEGER :: len
      END FUNCTION MYTRIM
      !+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


      !#########################################################################
      SUBROUTINE STOPIT(message)
      !------------------------------------------------------------------------
         CHARACTER(*) :: message
      END SUBROUTINE STOPIT
      !+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


      !#########################################################################
      INTEGER FUNCTION f_get_lun()
      !-------------------------------------------------------------------------
         INTEGER :: lun
         LOGICAL :: opened
      END FUNCTION f_get_lun
      !+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

   END INTERFACE

#endif
