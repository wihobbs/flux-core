/*****************************************************************************\
 *  Copyright (c) 2014 Lawrence Livermore National Security, LLC.  Produced at
 *  the Lawrence Livermore National Laboratory (cf, AUTHORS, DISCLAIMER.LLNS).
 *  LLNL-CODE-658032 All rights reserved.
 *
 *  This file is part of the Flux resource manager framework.
 *  For details, see https://github.com/flux-framework.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the license, or (at your option)
 *  any later version.
 *
 *  Flux is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the terms and conditions of the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *  See also:  http://www.gnu.org/licenses/
\*****************************************************************************/

/* flux-info.c - flux info subcommand */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <getopt.h>
#include <json/json.h>
#include <assert.h>
#include <libgen.h>
#include <zmq.h>
#include <czmq.h>
#include <stdarg.h>
#include <stdbool.h>

#include "log.h"

#include "flux.h"

#define OPTIONS "h"
static const struct option longopts[] = {
    {"help",       no_argument,        0, 'h'},
    { 0, 0, 0, 0 },
};

void usage (void)
{
    fprintf (stderr, 
"Usage: flux-info\n"
);
    exit (1);
}

int main (int argc, char *argv[])
{
    flux_t h;
    int ch;
    int rank, size;
    bool treeroot;

    log_init ("flux-info");

    while ((ch = getopt_long (argc, argv, OPTIONS, longopts, NULL)) != -1) {
        switch (ch) {
            case 'h': /* --help */
                usage ();
                break;
            default:
                usage ();
                break;
        }
    }
    if (optind != argc)
        usage ();

    if (!(h = flux_api_open ()))
        err_exit ("flux_api_open");

    if (flux_info (h, &rank, &size, &treeroot) < 0)
        err_exit ("flux_info");
    printf ("rank=%d\n", rank);
    printf ("size=%d\n", size);
    printf ("treeroot=%s\n", treeroot ? "true" : "false");

    flux_api_close (h);
    log_fini ();
    return 0;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
