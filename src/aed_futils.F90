!******************************************************************************
!*                                                                            *
!* aed_futils.F90                                                             *
!*                                                                            *
!* Developed by :                                                             *
!*     AquaticEcoDynamics (AED) Group                                         *
!*     School of Agriculture and Environment                                  *
!*     The University of Western Australia                                    *
!*                                                                            *
!* Copyright 2013 - 2018 -  The University of Western Australia               *
!*                                                                            *
!*  This file is part of GLM (General Lake Model)                             *
!*                                                                            *
!*  libutil is free software: you can redistribute it and/or modify           *
!*  it under the terms of the GNU General Public License as published by      *
!*  the Free Software Foundation, either version 3 of the License, or         *
!*  (at your option) any later version.                                       *
!*                                                                            *
!*  libutil is distributed in the hope that it will be useful,                *
!*  but WITHOUT ANY WARRANTY; without even the implied warranty of            *
!*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
!*  GNU General Public License for more details.                              *
!*                                                                            *
!*  You should have received a copy of the GNU General Public License         *
!*  along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
!*                                                                            *
!******************************************************************************


!###############################################################################
FUNCTION MYTRIM(str) RESULT(res)
!-------------------------------------------------------------------------------
! Useful for passing string arguments to C functions
!-------------------------------------------------------------------------------
   CHARACTER(*),TARGET :: str
   CHARACTER(:),POINTER :: res
   INTEGER :: len

   len = LEN_TRIM(str)+1
   str(len:len) = achar(0)
   res => str
END FUNCTION MYTRIM
!+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


!###############################################################################
SUBROUTINE STOPIT(message)
!-------------------------------------------------------------------------------
!ARGUMENTS
   CHARACTER(*) :: message
!-------------------------------------------------------------------------------
   PRINT *,message
   STOP
END SUBROUTINE STOPIT
!+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


!###############################################################################
INTEGER FUNCTION f_get_lun()
!-------------------------------------------------------------------------------
! Find the first free logical unit number
!-------------------------------------------------------------------------------
!ARGUMENTS
   INTEGER :: lun
   LOGICAL :: opened
!
!-------------------------------------------------------------------------------
!BEGIN
   DO lun = 10,99
      inquire(unit=lun, opened=opened)
      IF ( .not. opened ) THEN
         f_get_lun = lun
         RETURN
      ENDIF
   ENDDO
   f_get_lun = -1
END FUNCTION f_get_lun
!+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

