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


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include "audiofile.h"
#include "config.h"


#if ZITA_CONVOLVER_MAJOR_VERSION < 3
#error "This program requires zita-convolver version 3 or higher."
#endif


Convproc      *convproc = 0;
unsigned int   latency = 0;
unsigned int   options = 0;
unsigned int   fsamp = 0;
unsigned int   fragm = 0;
unsigned int   ninp = 0;
unsigned int   nout = 0;
unsigned int   size = 0;


static const char *clopt = "hvMV";
static bool M_opt = false;
static bool V_opt = false;
static bool stop = false;


static void help (void)
{
    fprintf (stderr, "\nFconvolver %s\n", VERSION);
    fprintf (stderr, "(C) 2006-2011 Fons Adriaensen  <fons@linuxaudio.org>\n\n");
    fprintf (stderr, "Usage: fconvolver <configuration file> <input file> <output file>\n");
    fprintf (stderr, "Options:\n");
    fprintf (stderr, "  -h                 Display this text\n");
    fprintf (stderr, "  -M                 Use the FFTW_MEASURE option [off]\n");   
    fprintf (stderr, "  -V                 Use vector mode processing [off]\n");   
    exit (1);
}


static void procoptions (int ac, char *av [], const char *where)
{
    int k;
    
    optind = 1;
    opterr = 0;
    while ((k = getopt (ac, av, (char *) clopt)) != -1)
    {
        if (optarg && (*optarg == '-'))
        {
            fprintf (stderr, "\n%s\n", where);
	    fprintf (stderr, "  Missing argument for '-%c' option.\n", k); 
            fprintf (stderr, "  Use '-h' to see all options.\n");
            exit (1);
        }
	switch (k)
	{
        case 'h' : help (); exit (0);
        case 'M' : M_opt = true; break;
        case 'V' : V_opt = true; break;
        case '?':
            fprintf (stderr, "\n%s\n", where);
            if (optopt != ':' && strchr (clopt, optopt))
	    {
                fprintf (stderr, "  Missing argument for '-%c' option.\n", optopt);
	    } 
            else if (isprint (optopt))
	    {
                fprintf (stderr, "  Unknown option '-%c'.\n", optopt);
	    }
            else
	    {
                fprintf (stderr, "  Unknown option character '0x%02x'.\n", optopt & 255);
	    }
            fprintf (stderr, "  Use '-h' to see all options.\n");
            exit (1);
        default:
            abort ();
 	}
    }
}


static void sigint_handler (int)
{
    stop = true;
}


int main (int ac, char *av [])
{
    Audiofile      Ainp, Aout;
    unsigned int   i, j, k;
    unsigned long  nf;
    float          *buff, *p, *q;

    if (zita_convolver_major_version () != ZITA_CONVOLVER_MAJOR_VERSION)
    {
	fprintf (stderr, "Zita-convolver does not match compile-time version.\n");
	return 1;
    }

    procoptions (ac, av, "On command line:");
    if (ac != optind + 3) help ();
    av += optind;

    if (Ainp.open_read (av [1]))
    {
	fprintf (stderr, "Unable to read input file '%s'\n", av [1]);
        return 1;
    } 
    fsamp = Ainp.rate ();

    convproc = new Convproc;
    if (M_opt) options |= Convproc::OPT_FFTW_MEASURE;
    if (V_opt) options |= Convproc::OPT_VECTOR_MODE;
    if (config (av [0]))
    {
	Ainp.close ();
	delete convproc;
        return 1;
    }

    if (Ainp.chan () != (int) ninp)
    {
	fprintf (stderr, "Number of inputs (%d) in config file doesn't match input file (%d)'\n",
                 ninp, Ainp.chan ());
	Ainp.close ();
	delete convproc;
        return 1;
    }

    if (Aout.open_write (av [2], Audiofile::TYPE_WAV, Audiofile::FORM_FLOAT, fsamp, nout))   
    {
	fprintf (stderr, "Unable to create output file '%s'\n", av [2]);
	Ainp.close ();
	delete convproc;
        return 1;
    } 

    nf = Ainp.size () + size - 1;
    buff = new float [fragm * ((ninp > nout) ? ninp : nout)];
    signal (SIGINT, sigint_handler); 

    convproc->start_process (0, 0);
    while (nf)
    {    
	if (stop) break;
	k = Ainp.read (buff, fragm);
	if (k < fragm) memset (buff + k * ninp, 0, (fragm - k) * ninp * sizeof (float));
	for (i = 0; i < ninp; i++)
	{
	    p = buff + i;
	    q = convproc->inpdata (i);
	    for (j = 0; j < k; j++) q [j] = p [j * ninp];
	}
	convproc->process ();
	k = fragm;
	if (k > nf) k = nf;
	for (i = 0; i < nout; i++)
	{
	    p = convproc->outdata (i);
	    q = buff + i;
	    for (j = 0; j < k; j++) q [j * nout] = p [j];
	}
        Aout.write (buff, k);
	nf -= k;
    }

    convproc->stop_process ();
    convproc->cleanup ();
    Ainp.close ();
    Aout.close ();
    if (stop) unlink (av [2]);
    delete[] buff;
    delete convproc;

    return stop ? 1 : 0;
}

