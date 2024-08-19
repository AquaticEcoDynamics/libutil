###############################################################################
#                                                                             #
# Makefile for libutil                                                        #
#                                                                             #
#  Developed by :                                                             #
#      AquaticEcoDynamics (AED) Group                                         #
#      School of Agriculture and Environment                                  #
#      The University of Western Australia                                    #
#                                                                             #
#  Copyright 2013 - 2024 -  The University of Western Australia               #
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
ifeq ($(MDEBUG),true)
  DEBUG=true
endif

OBJS=${objdir}/namelist.o \
     ${objdir}/aed_csv.o \
     ${objdir}/aed_time.o

CFLAGS=-Wall -O3
INCLUDES=-I${incdir}
ifeq ($(F90),ifort)
  FFLAGS=-warn all -module ${moddir} -static-intel -mp1 -stand f03
else ifeq ($(F90),pgfortran)
  FFLAGS=-module ${moddir} -O3
else
  FFLAGS=-Wall -J ${moddir} -std=f2003
  ifeq ($(F90),)
    F90=gfortran
    FFLAGS+=-ffree-line-length-none -fall-intrinsics
  endif
endif
ifeq ($(DEBUG),true)
  CFLAGS+=-g
endif
ifeq ($(MDEBUG),true)
  CFLAGS+=-fsanitize=address
endif

CFLAGS+=-fPIE
FFLAGS+=-fPIE

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
