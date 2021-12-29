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
#include "config.h"

#define ZITA_CONVOLVER_VERSION  0
#if ZITA_CONVOLVER_MAJOR_VERSION == 3
#undef ZITA_CONVOLVER_VERSION
#define ZITA_CONVOLVER_VERSION  3
#elif ZITA_CONVOLVER_MAJOR_VERSION == 4
#undef ZITA_CONVOLVER_VERSION
#define ZITA_CONVOLVER_VERSION  4
#else
#error "This version of IR requires zita-convolver 3.x.x or 4.x.x"
#endif

extern Jclient     *jclient;
extern unsigned int fragm;


static char *inp_name [Convproc::MAXINP];
static char *out_name [Convproc::MAXOUT];
static char *inp_conn [Convproc::MAXINP];
static char *out_conn [Convproc::MAXOUT];


int convnew (const char *line, int lnum)
{
    unsigned int part;
    float        dens;
    int          r;

    memset (inp_name, 0, Convproc::MAXINP * sizeof (char *));
    memset (inp_conn, 0, Convproc::MAXINP * sizeof (char *));
    memset (out_name, 0, Convproc::MAXOUT * sizeof (char *));
    memset (out_conn, 0, Convproc::MAXOUT * sizeof (char *));

    r = sscanf (line, "%u %u %u %u %f", &ninp, &nout, &part, &size, &dens);
    if (r < 4) return ERR_PARAM;
    if (r < 5) dens = 0;

    if ((ninp == 0) || (ninp > Convproc::MAXINP))
    {
        fprintf (stderr, "Line %d: Number of inputs (%d) is out of range.\n", lnum, ninp);
        return ERR_OTHER;
    }
    if ((nout == 0) || (nout > Convproc::MAXOUT))
    {
        fprintf (stderr, "Line %d: Number of outputs (%d) is out of range.\n", lnum, nout);
        return ERR_OTHER;
    }
    if  ((part & (part -1)) || (part < Convproc::MINPART) || (part > Convproc::MAXPART))
    {
        fprintf (stderr, "Line %d: Partition size (%d) is illegal.\n", lnum, part);
        return ERR_OTHER;
    }
    if (part > Convproc::MAXDIVIS * fragm)
    {
        part = Convproc::MAXDIVIS * fragm; 
        fprintf (stderr, "Line %d: partition size adjusted to %d.\n", lnum, part);
    }
    if (part < fragm)
    {
        part = fragm; 
        fprintf (stderr, "Line %d: partition size adjusted to %d.\n", lnum, part);
    }
    if (size > MAXSIZE)
    {
        fprintf (stderr, "Line %d: Convolver size (%d) is out of range.\n", lnum, size);
        return ERR_OTHER;
    }
    if ((dens < 0.0f) || (dens > 1.0f))
    {
        fprintf (stderr, "Line %d: Density parameter is out of range.\n", lnum);
        return ERR_OTHER;
    }

    convproc->set_options (options);

#if ZITA_CONVOLVER_VERSION == 3
    convproc->set_density (dens);
    if (convproc->configure (ninp, nout, size, fragm, part, Convproc::MAXPART))
    {   
        fprintf (stderr, "Can't initialise convolution engine.\n");
        return ERR_OTHER;
    }

    return 0;
#elif ZITA_CONVOLVER_VERSION == 4
    if (convproc->configure (ninp, nout, size, fragm, part, Convproc::MAXPART, dens))
    {   
        fprintf (stderr, "Can't initialise convolution engine.\n");
        return ERR_OTHER;
    }

    return 0;
#endif
}

int inpname (const char *line)
{
    unsigned int  i, n;
    char          s [256];

    if (sscanf (line, "%u %n", &i, &n) != 1) return ERR_PARAM;
    if (--i >= ninp) return ERR_IONUM;
    line += n;
    n = sstring (line, s, 256);
    if (n) inp_name [i] = strdup (s);
    else return ERR_PARAM;
    line += n;
    n = sstring (line, s, 256);
    if (n) inp_conn [i] = strdup (s);
    return 0;
}


int outname (const char *line)
{
    unsigned int  i, n;
    char          s [256];

    if (sscanf (line, "%u %n", &i, &n) != 1) return ERR_PARAM;
    if (--i >= nout) return ERR_IONUM;
    line += n;
    n = sstring (line, s, 256);
    if (n) out_name [i] = strdup (s);
    else return ERR_PARAM;
    line += n;
    n = sstring (line, s, 256);
    if (n) out_conn [i] = strdup (s);
    return 0;
}


void makeports (void)
{
    unsigned int  i;
    char          s [16];

    for (i = 0; i < ninp; i++)
    {
	if (inp_name [i])
	{
            jclient->add_input_port (inp_name [i], inp_conn [i]);
	}
	else
	{
            sprintf (s, "In-%d", i + 1);
            jclient->add_input_port (s);
	}
	free (inp_name [i]);
	free (inp_conn [i]);
    }
    for (i = 0; i < nout; i++)
    {
	if (out_name [i])
	{
            jclient->add_output_port (out_name [i], out_conn [i]);
	}
	else
	{
            sprintf (s, "Out-%d", i + 1);
            jclient->add_output_port (s);
	}
	free (out_name [i]);
	free (out_conn [i]);
    }
}

