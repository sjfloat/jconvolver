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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jclient.h"


Jclient::Jclient (const char *jname, const char *jserv, Convproc *convproc) :
    _jack_client (0),
    _jname (0),
    _fsamp (0),
    _fragm (0),
    _flags (0),
    _clear (false),
    _ninp (0),
    _nout (0),
    _convproc (convproc),
    _convsync (false)

{
    init_jack (jname, jserv);  
}


Jclient::~Jclient (void)
{
    if (_jack_client) close_jack ();
}


int Jclient::delete_ports (void)
{
    unsigned int k;

    if (_convproc->state () != Convproc::ST_IDLE) return -1;
    _ninp = 0;
    _nout = 0;

    for (k = 0; k < Convproc::MAXINP; k++)
    {
	if (_jack_inppp [k])
	{
	    jack_port_unregister (_jack_client, _jack_inppp [k]);
	    _jack_inppp [k] = 0;
	}
    }

    for (k = 0; k < Convproc::MAXOUT; k++)
    {
	if (_jack_outpp [k])
	{
	    jack_port_unregister (_jack_client, _jack_outpp [k]);
	    _jack_outpp [k] = 0;
	}
    }

    return 0;
}


void Jclient::init_jack (const char *jname, const char *jserv)
{
    struct sched_param  spar;
    jack_status_t       stat;
    int                 opts;

    opts = JackNoStartServer;
    if (jserv) opts |= JackServerName;
    if ((_jack_client = jack_client_open (jname, (jack_options_t) opts, &stat, jserv)) == 0)
    {
        fprintf (stderr, "Can't connect to JACK\n");
        exit (1);
    }

    jack_on_shutdown (_jack_client, jack_static_shutdown, (void *) this);
    jack_set_freewheel_callback (_jack_client, jack_static_freewheel, (void *) this);
    jack_set_buffer_size_callback (_jack_client, jack_static_buffsize, (void *) this);
    jack_set_process_callback (_jack_client, jack_static_process, (void *) this);

    if (jack_activate (_jack_client))
    {
        fprintf(stderr, "Can't activate JACK.\n");
        exit (1);
    }

    _jname = jack_get_client_name (_jack_client);
    _fsamp = jack_get_sample_rate (_jack_client);
    _fragm = jack_get_buffer_size (_jack_client);
    if (_fragm < 32)
    {
	fprintf (stderr, "Fragment size is too small\n");
	exit (0);
    }
    if (_fragm > 4096)
    {
	fprintf (stderr, "Fragment size is too large\n");
	exit (0);
    }

    pthread_getschedparam (jack_client_thread_id (_jack_client), &_policy, &spar);
    _abspri = spar.sched_priority;

    memset (_jack_inppp, 0, Convproc::MAXINP * sizeof (jack_port_t *));
    memset (_jack_outpp, 0, Convproc::MAXOUT * sizeof (jack_port_t *));
}


void Jclient::close_jack ()
{
    jack_deactivate (_jack_client);
    jack_client_close (_jack_client);
}


void Jclient::jack_static_shutdown (void *arg)
{
    Jclient *C = (Jclient *) arg;

    C->_flags = FL_EXIT;
}


void Jclient::jack_static_freewheel (int state, void *arg)
{
    Jclient *C = (Jclient *) arg;

    C->_convsync = state ? true : false;
}


int Jclient::jack_static_buffsize (jack_nframes_t nframes, void *arg)
{
    Jclient *C = (Jclient *) arg;

    if (C->_fragm)
    {
        C->stop ();
        C->_flags = FL_BUFF;
    }
    return 0;
}


int Jclient::jack_static_process (jack_nframes_t nframes, void *arg)
{
    Jclient *C = (Jclient *) arg;

    C->jack_process ();
    return 0;
}


void Jclient::jack_process (void)
{
    unsigned int  i;
    float         *inpp [Convproc::MAXINP];
    float         *outp [Convproc::MAXOUT];

    for (i = 0; i < _nout; i++)
    {
        outp [i] = (float *) jack_port_get_buffer (_jack_outpp [i], _fragm);
    }
    for (i = 0; i < _ninp; i++)
    {
        inpp [i] = (float *) jack_port_get_buffer (_jack_inppp [i], _fragm);
    }

    if (_clear)
    {
	_flags = 0;
	_clear = false;
    }
	
    if (_convproc->state () == Convproc::ST_WAIT) _convproc->check_stop ();

    if (_convproc->state () != Convproc::ST_PROC)
    {
	for (i = 0; i < _nout; i++) memset (outp [i], 0, _fragm * sizeof (float));
	return;
    }
    for (i = 0; i < _ninp; i++) memcpy (_convproc->inpdata (i), inpp [i], _fragm * sizeof (float));
    _flags |= _convproc->process (_convsync);
    for (i = 0; i < _nout; i++)	memcpy (outp [i], _convproc->outdata (i), _fragm * sizeof (float));
}


int Jclient::add_input_port (const char *name, const char *conn)
{
    char s [512];

    if ((_convproc->state () > Convproc::ST_STOP) || (_ninp == Convproc::MAXINP)) return -1;
    _jack_inppp [_ninp] = jack_port_register (_jack_client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (conn)
    {
	sprintf (s, "%s:%s", _jname, name);
	jack_connect (_jack_client, conn, s);
    }
    return _ninp++;
}


int Jclient::add_output_port (const char *name, const char *conn)
{
    char s [512];

    if ((_convproc->state () > Convproc::ST_STOP) || (_nout == Convproc::MAXOUT)) return -1;
     _jack_outpp [_nout] = jack_port_register (_jack_client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (conn)
    {
	sprintf (s, "%s:%s", _jname, name);
	jack_connect (_jack_client, s, conn);
    }
    return _nout++;
}

