# --------------------------------------------------------------------------------
#
#  Copyright (C) 2005-2011 Fons Adriaensen <fons@linuxaudio.org>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# --------------------------------------------------------------------------------


PREFIX = /usr/local
SUFFIX := $(shell uname -m | sed -e 's/^unknown/$//' -e 's/^i.86/$//' -e 's/^x86_64/$/64/')
LIBDIR = lib$(SUFFIX)
DISTDIR = jconvolver-$(VERSION)
VERSION = 0.9.2
LDFLAGS += -L$(PREFIX)/$(LIBDIR)
CPPFLAGS += -O3 -Wall -MMD -MP -DVERSION=\"$(VERSION)\"
CPPFLAGS += -march=native


all:	jconvolver fconvolver makemulti


JCONVOLVER_O =	jconvolver.o config.o jconfig.o audiofile.o sstring.o jclient.o
jconvolver:	LDLIBS += -lzita-convolver -lfftw3f -lsndfile -lclthreads -ljack -lpthread -lrt
jconvolver:	$(JCONVOLVER_O)
	g++ $(LDFLAGS) -o $@ $(JCONVOLVER_O) $(LDLIBS)
$(JCONVOLVER_O):
-include $(JCONVOLVER_O:%.o=%.d)



FCONVOLVER_O =	fconvolver.o config.o fconfig.o audiofile.o sstring.o
fconvolver:	LDLIBS += -lzita-convolver -lfftw3f -lsndfile -lpthread -lrt
fconvolver:	$(FCONVOLVER_O)
	g++ $(LDFLAGS) -o $@ $(FCONVOLVER_O) $(LDLIBS)
$(FCONVOLVER_O):
-include $(FCONVOLVER_O:%.o=%.d)



MAKEMULTI_O =	makemulti.o audiofile.o
makemulti : LDLIBS += -lsndfile -lrt
makemulti:	$(MAKEMULTI_O)
	g++ $(LDFLAGS) -o $@ $(MAKEMULTI_O) $(LDLIBS)


install:	all
	install -Dm 755 jconvolver $(DESTDIR)$(PREFIX)/bin/jconvolver
	install -Dm 755 fconvolver $(DESTDIR)$(PREFIX)/bin/fconvolver
	install -Dm 755 makemulti  $(DESTDIR)$(PREFIX)/bin/makemulti


clean:
	/bin/rm -f *~ *.o *.a *.d *.so jconvolver fconvolver makemulti

