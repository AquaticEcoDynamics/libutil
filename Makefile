###############################################################################
#                                                                             #
# Makefile for libutil                                                        #
#                                                                             #
#  Developed by :                                                             #
#      AquaticEcoDynamics (AED) Group                                         #
#      School of Earth & Environment                                          #
#      The University of Western Australia                                    #
#                                                                             #
#  Copyright 2013 - 2016 -  The University of Western Australia               #
#                                                                             #
#   libutil is free software: you can redistribute it and/or modify           #
#   it under the terms of the GNU General Public License as published by      #
#   the Free Software Foundation, either version 3 of the License, or         #
#   (at your option) any later version.                                       #
#                                                                             #
#   libutil is distributed in the hope that it will be useful,                #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of            #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             #
#   GNU General Public License for more details.                              #
#                                                                             #
#   You should have received a copy of the GNU General Public License         #
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.     #
#                                                                             #
###############################################################################


srcdir=src
objdir=objs
incdir=include

SRCS=${srcdir}/namelist.c \
     ${srcdir}/aed_csv.c \
     ${srcdir}/aed_futils.F90 \
     ${srcdir}/aed_time.c

OBJS=${objdir}/namelist.o \
     ${objdir}/aed_csv.o \
     ${objdir}/aed_futils.o \
     ${objdir}/aed_time.o

CFLAGS=-Wall -O3 -fPIC
INCLUDES=-I${incdir}
ifeq ($(F90),ifort)
  FFLAGS=-warn all -i-static -mp1 -stand f03 -fPIC
else
  ifeq ($(F90),)
    F90=gfortran
  endif
  FFLAGS=-fPIC -Wall -ffree-line-length-none -std=f2003 -fall-intrinsics
endif

all: libutil.a

libutil.a: ${objdir} ${OBJS}
	ar rv $@ ${OBJS}
	ranlib $@

clean: ${objdir}
	@touch ${objdir}/1.o
	@touch 1__genmod.1
	@/bin/rm ${objdir}/*.o *__genmod.*
	@/bin/rmdir ${objdir}

distclean: clean
	@touch libutil.a
	@/bin/rm libutil.a

${objdir}:
	mkdir ${objdir}

${objdir}/%.o: ${srcdir}/%.c ${incdir}/%.h ${incdir}/libutil.h
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@

${objdir}/%.o: ${srcdir}/%.F90 ${incdir}/%.h ${incdir}/libutil.h
	$(F90) $(FFLAGS) -D_FORTRAN_VERSION_ -c $< -o $@

${objdir}/%.o: ${srcdir}/%.F90
	$(F90) $(FFLAGS) -D_FORTRAN_VERSION_ -c $< -o $@
