//  -----------------------------------------------------------------------------
//
//  Copyright (C) 2006-2011 Fons Adriaensen <fons@linuxaudio.org>
//  
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  -----------------------------------------------------------------------------


#ifndef __JCLIENT_H
#define __JCLIENT_H


#include <jack/jack.h>
#include "zita-convolver.h"


class Jclient
{
public:

    Jclient (const char *jname, const char *jserv, Convproc *convproc);
    ~Jclient (void);

    enum { FL_EXIT = 0x80000000, FL_BUFF = 0x40000000 };

    const char  *jname (void) const { return _jname; } 
    unsigned int fsamp (void) const { return _fsamp; } 
    unsigned int fragm (void) const { return _fragm; } 
    unsigned int state (void) { return _convproc->state (); } 

    unsigned int flags (void) { _clear = true; return _flags; }

    int delete_ports (void);
    int add_input_port  (const char *name, const char *conn = 0);
    int add_output_port (const char *name, const char *conn = 0);

    void start (void) { _convproc->start_process (_abspri, _policy); }
    void stop (void)  { _convproc->stop_process (); }

private:

    void  init_jack (const char *jname, const char *jserv);
    void  close_jack (void);
    void  jack_process (void);

    jack_client_t  *_jack_client;
    jack_port_t    *_jack_inppp [Convproc::MAXINP];
    jack_port_t    *_jack_outpp [Convproc::MAXOUT];
    const char     *_jname;
    int             _abspri;
    int             _policy;
    unsigned int    _fsamp;
    unsigned int    _fragm;
    unsigned int    _flags;
    bool            _clear;
    unsigned int    _ninp;
    unsigned int    _nout;
    Convproc       *_convproc;
    bool            _convsync;

    static void jack_static_shutdown (void *arg);
    static void jack_static_freewheel (int state, void *arg);
    static int  jack_static_buffsize (jack_nframes_t nframes, void *arg);
    static int  jack_static_process (jack_nframes_t nframes, void *arg);
};


#endif
