//  -----------------------------------------------------------------------------
//
//  Copyright (C) 2009-2011 Fons Adriaensen <fons@linuxaudio.org>
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
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <math.h>
#include "audiofile.h"


enum { HELP, CAF, WAV, AMB, BIT16, BIT24, FLOAT, GAIN, FROM, UPTO };

enum { BLOCK = 4096, MAXINP = 64 };


static struct option options [] = 
{
    { "help",  0, 0, HELP  },
    { "caf",   0, 0, CAF   },
    { "wav",   0, 0, WAV   },
    { "amb",   0, 0, AMB   },
    { "16bit", 0, 0, BIT16 },
    { "24bit", 0, 0, BIT24 },
    { "float", 0, 0, FLOAT },
    { "gain",  1, 0, GAIN  },
    { "from",  1, 0, FROM  },
    { "upto",  1, 0, UPTO  },
    { 0, 0, 0, 0 }
};


static void help (void)
{
    fprintf (stderr, "\nmakemulti %s\n", VERSION);
    fprintf (stderr, "(C) 2009 Fons Adriaensen  <fons@linuxaudio.org>\n");
    fprintf (stderr, "Combine audio files into a multichannel file.\n");
    fprintf (stderr, "Usage: makemulti <options> <input files> <output file>.\n");
    fprintf (stderr, "Options:\n");
    fprintf (stderr, "  --help  : Display this text.\n");
    fprintf (stderr, "  Output file type:      --caf, --wav, --amb\n");
    fprintf (stderr, "  Output sample format:  --16bit, --24bit, --32bit, --float\n");
    fprintf (stderr, "  The default output file format is caf, 24-bit.\n");
    fprintf (stderr, "  Input range selection: --from <time> --upto <time> (h:m:s)\n");
    fprintf (stderr, "  Signal modification:   --gain <gain> (dB)\n");
    exit (1);
}


static int        type = Audiofile::TYPE_CAF;
static int        form = Audiofile::FORM_24BIT;
static float      gain = 1;
static double     t_from = -1;
static double     t_upto = -1;
static Audiofile  *inp_file [MAXINP];
static Audiofile  *out_file;
static float      *inp_buff [MAXINP];
static float      *out_buff;



static int parsegain (const char *arg)
{
    if (! arg) return -1;
    if (sscanf (arg, "%f", &gain) != 1)
    {
	fprintf (stderr, "Can't parse gain parameter '%s'.\n", arg);
        return 1;
    }
    gain = powf (10.0f, 0.05f * gain);
    return 0;
}


static int parsetime (const char *arg, double *t)
{
    int    h, m, n;
    double s;

    if (! arg) return -1;
    if (sscanf (arg, "%d:%d:%lf%n", &h, &m, &s, &n) != 3)
    {
	fprintf (stderr, "Can't parse time format: '%s'.\n", arg);
        return 1;
    }
    if (arg [n]) return 1;
    s += 3600 * h + 60 * m;
    if (s < 0) return 1;
    *t = s;
    return 0;
}


static void procoptions (int ac, char *av [])
{
    int k;

    while ((k = getopt_long (ac, av, "", options, 0)) != -1)
    {
	switch (k)
	{
        case '?':
	case HELP:
	    help ();
	    break;

	case CAF:
	    type = Audiofile::TYPE_CAF;
	    break;

	case WAV:
	    type = Audiofile::TYPE_WAV;
	    break;

	case AMB:
	    type = Audiofile::TYPE_AMB;
	    break;

	case BIT16:
	    form = Audiofile::FORM_16BIT;
	    break;

	case BIT24:
	    form = Audiofile::FORM_24BIT;
	    break;

	case FLOAT:
	    form = Audiofile::FORM_FLOAT;
	    break;

	case GAIN:
	    if (parsegain (optarg)) exit (1);
	    break;

	case FROM:
	    if (parsetime (optarg, &t_from)) exit (1);
	    break;

	case UPTO:
	    if (parsetime (optarg, &t_upto)) exit (1);
	    break;
 	}
    }
}


void cleanup (void)
{
    int i;

    for (i = 0; i < MAXINP; i++)
    {
	delete   inp_file [i];
	delete[] inp_buff [i];
    }
    delete   out_file;
    delete[] out_buff;
}


int main (int ac, char *av [])
{
    int i, j, k;

    Audiofile  *A;
    int         ninp;
    int         rate;
    int         chan;
    uint32_t    offset;
    uint32_t    frames;
    int         nf_todo, nf_done, inp_nch, out_ich; 
    float       *inp_ptr, *out_ptr;

    procoptions (ac, av);
    ninp = ac - optind - 1;
    if (ninp < 1)
    {
        fprintf (stderr, "Missing arguments, try --help.\n");
	return 1;
    }
    if (ninp > MAXINP)
    {
        fprintf (stderr, "Too many input files: maximum is 64.\n"); 
	return 1;
    }
    if ((t_from >= 0) && (t_upto >= 0) && (t_upto < t_from + 0.1))
    {
        fprintf (stderr, "Illegal from/upto range.\n"); 
	return 1;
    }
    for (i = 0; i < MAXINP; i++)
    {
	inp_file [i] = 0;
        inp_buff [i] = 0;
    }
    out_file = 0;
    out_buff = 0;
    rate = 0;
    chan = 0;

    av += optind;
    for (i = 0; i < ninp; i++)
    {
        A = new Audiofile;
	if (A->open_read (av [i]))
	{
	    fprintf (stderr, "Can't open input file '%s'.\n", av [i]);
	    cleanup ();
	    return 1;
	}
	if (i)
	{
	    if (A->rate () != rate)
	    {
	        fprintf (stderr, "Sample rate of '%s' (%d) does not match previous value (%d).\n",
                         av [i], A->rate (), rate);
	        cleanup ();
	        return 1;
	    }
	}
	else rate = A->rate ();
	inp_file [i] = A;
	inp_buff [i] = new float [A->chan () * BLOCK];
	chan += A->chan ();
    }

    if (t_from >= 0) offset = (uint32_t)(t_from * rate);        
    else offset = 0;
    frames = 0;
    for (i = 0; i < ninp; i++)
    {
	A = inp_file [i];
	if (A->size () < offset)
	{
	    fprintf (stderr, "Input file '%s' is too short.\n", av [i]);
	    cleanup ();
	    return 1;
	}
	else
	{
	    if (A->size () > frames) frames = A->size ();
            A->seek (offset);
	}
    }
    if (t_upto >= 0) frames = (uint32_t)((t_upto - t_from) * rate + 0.5);
    else frames -= offset;

    out_file = new Audiofile ();
    if (out_file->open_write (av [ninp], type, form, rate, chan))
    {
	fprintf (stderr, "Can't open output file '%s'.\n", av [ninp]);
        cleanup ();
	return 1;
    }
    out_buff = new float [chan * BLOCK];

    while (frames)
    {
	nf_todo = (frames > BLOCK) ? BLOCK : frames;
	out_ich = 0;
	for (i = 0; i < ninp; i++)
	{
	    A = inp_file [i];
	    inp_ptr = inp_buff [i];
	    inp_nch = A->chan ();
	    nf_done = A->read (inp_ptr, nf_todo);
	    for (j = 0; j < inp_nch; j++)
            {
	        inp_ptr = inp_buff [i] + j;
	        out_ptr = out_buff + out_ich + j;
		for (k = 0; k < nf_done; k++)
		{
		    *out_ptr = gain * *inp_ptr;
		    out_ptr += chan;
		    inp_ptr += inp_nch;
		}
		while (k++ < nf_todo)
		{
		    *out_ptr = 0;
		    out_ptr += chan;
		}
	    }	    
	    out_ich += inp_nch;
	}
	out_file->write (out_buff, nf_todo);
	frames -= nf_todo;
    }
    out_file->close ();

    cleanup ();
    return 0;
}

