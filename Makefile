###############################################################################
#                                                                             #
# Makefile for libutil                                                        #
#                                                                             #
#  Developed by :                                                             #
#      AquaticEcoDynamics (AED) Group                                         #
#      School of Agriculture and Environment                                  #
#      The University of Western Australia                                    #
#                                                                             #
#  Copyright 2013 - 2018 -  The University of Western Australia               #
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
incdir=include
ifeq ($(SINGLE),true)
  objdir=obj_s
  moddir=mod_s
  TARGET=lib/libutil_s.a
else
  objdir=obj
  moddir=mod
  TARGET=lib/libutil.a
endif

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
  FFLAGS=-warn all -i-static -module ${moddir} -mp1 -stand f03 -fPIC
else ifeq ($(F90),pgfortran)
  FFLAGS=-fPIC -module ${moddir} -O3
else
  ifeq ($(F90),)
    F90=gfortran
  endif
  FFLAGS=-fPIC -Wall -J ${moddir} -ffree-line-length-none -std=f2003 -fall-intrinsics
endif

all: ${TARGET}

${TARGET}: ${objdir} ${OBJS} lib
	ar rv $@ ${OBJS}
	ranlib $@

clean: ${objdir}
	@touch ${objdir}/1.o
	@touch 1__genmod.1
	@/bin/rm ${objdir}/*.o *__genmod.*
	@/bin/rmdir ${objdir}

distclean: clean
	@touch lib mod mod_s
	@/bin/rm -rf lib mod mod_s

lib:
	@mkdir lib

${moddir}:
	@mkdir ${moddir}

${objdir}:
	@mkdir ${objdir}

${objdir}/%.o: ${srcdir}/%.c ${incdir}/%.h ${incdir}/libutil.h
	$(CC) $(CFLAGS) $(INCLUDES) -g -c $< -o $@

${objdir}/%.o: ${srcdir}/%.F90 ${incdir}/%.h ${incdir}/libutil.h ${moddir}
	$(F90) $(FFLAGS) -D_FORTRAN_SOURCE_ -c $< -o $@
