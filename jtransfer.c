/*
 * Copyright (c) 2026, Jesús Daniel Colmenares Oviedo <DtxdF@disroot.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/jail.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <fcntl.h>
#include <jail.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#ifndef JTRANSFER_VERSION
#   define JTRANSFER_VERSION "dev"
#endif

#define DEFAULT_UID ((uid_t)0)
#define DEFAULT_GID ((gid_t)0)
#define DEFAULT_FILE_MODE "0640"

static void version(void);
static void usage(void);
static ssize_t copy_fallback(int from_fd, int to_fd);

int
main(int argc, char **argv)
{
    bool nofollow = false;
    bool read_mode, write_mode;
	ssize_t wcount;
    int ch;
    int jid;
    int from_fd, to_fd, close_fd;
    int rc = EX_OK;
    const char *file;
    const char *jail;
    char *workdir;
    char *endptr;
    char *term;
    char *username, *group;
    char *pw_dir;
    long luid, lgid;
    uid_t uid;
    gid_t gid;
    mode_t mode;
    mode_t *set;
    struct passwd *pw;
    struct group *gr;

    read_mode = write_mode = false;
    ch = 0;
    file = jail = username = workdir = NULL;
    pw = NULL;
    pw_dir = "/";

    (void)umask(0);

    if ((set = setmode(DEFAULT_FILE_MODE)) == NULL)
        err(EX_SOFTWARE, "setmode");

    while ((ch = getopt(argc, argv, "hrwvd:f:j:m:u:")) != -1) {
        switch (ch) {
        case 'h':
            nofollow = true;
            break;
        case 'r':
            read_mode = true;
            break;
        case 'w':
            write_mode = true;
            break;
        case 'v':
            version();
            break;
        case 'd':
            workdir = optarg;
            break;
        case 'f':
            file = optarg;
            break;
        case 'j':
            jail = optarg;
            break;
        case 'm':
            free(set);
            if ((set = setmode(optarg)) == NULL)
                err(EX_DATAERR, "setmode");
            break;
        case 'u':
            username = optarg;
            break;
        default:
            usage();
        }
    }

    if (read_mode == write_mode)
        usage();

    if (file == NULL || file[0] == '\0')
        usage();
    
    if (jail == NULL || jail[0] == '\0')
        usage();

    mode = getmode(set, 0);

    /* Attach to the jail */
    jid = jail_getid(jail);
    if (jid < 0)
        errx(EX_SOFTWARE, "%s", jail_errmsg);
    if (jail_attach(jid) == -1)
        err(EX_SOFTWARE, "jail_attach(%d)", jid);

    if (username == NULL) {
        uid = DEFAULT_UID;
        gid = DEFAULT_GID;
    } else {
        if (username[0] == '\0')
            usage();

        group = strchr(username, ':');
        if (group != NULL)
            *group++ = '\0';

        uid = DEFAULT_UID;
        if (username[0] != '\0') {
            luid = strtol(username, &endptr, 10);

            if (*endptr == '\0') {
                if (luid < 0 || luid >= (uid_t)-1)
                    errx(EX_DATAERR, "bad user id");

                uid = (uid_t)luid;
            } else {
                pw = getpwnam(username);

                if (pw == NULL)
                    err(EX_OSERR, "getpwnam(): %s", username);
            }
        }

        if (pw == NULL)
            pw = getpwuid(uid);

        if (pw != NULL) {
            uid = pw->pw_uid;
            gid = pw->pw_gid;
        } else {
            gid = uid;
        }

        pw_dir = (pw != NULL ? pw->pw_dir : pw_dir);

        if (group != NULL && group[0] != '\0') {
            endptr = NULL;

            lgid = strtol(group, &endptr, 10);

            if (*endptr == '\0') {
                if (lgid < 0 || lgid >= (gid_t)-1)
                    errx(EX_DATAERR, "bad group id");

                gid = (gid_t)lgid;
            } else {
                gr = getgrnam(group);

                if (gr == NULL)
                    err(EX_OSERR, "getgrnam(): %s", group);

                gid = gr->gr_gid;
            }
        }
        endpwent();
    }

    if (workdir == NULL)
        workdir = pw_dir;

    if (chdir(workdir) == -1)
        err(EX_SOFTWARE, "chdir(): %s", workdir);

    if (setgroups(0, NULL) != 0)
        err(EX_OSERR, "setgroups");
    if (setgid(gid) != 0)
        err(EX_OSERR, "setgid");
    if (setuid(uid) != 0)
        err(EX_OSERR, "setuid");

    if (read_mode) { /* AKA read mode */
        to_fd = STDOUT_FILENO;

        if ((from_fd = open(file, O_RDONLY | (nofollow ? O_NOFOLLOW : 0), 0)) == -1)
            err(EX_SOFTWARE, "open(): %s", file);

        close_fd = from_fd;
    } else {
        from_fd = STDIN_FILENO;

		to_fd = open(file, O_WRONLY | O_TRUNC | O_CREAT | (nofollow ? O_NOFOLLOW : 0), mode);
        if (to_fd == -1)
            err(EX_SOFTWARE, "open(): %s", file);

        close_fd = to_fd;
    }

	do {
        wcount = copy_fallback(from_fd, to_fd);
		if (wcount < 0 && errno != EINTR)
			break;
	} while (wcount != 0);
	if (wcount < 0) {
		warn("%s", file);

        rc = EX_SOFTWARE;
    }

    /* Report errors on write-mode only. */
    if (close(close_fd) == -1 && write_mode) {
        warn("close()");

        rc = EX_SOFTWARE;
    }

    return (rc);
}

static void
version(void)
{
    fprintf(stderr, "%s\n", JTRANSFER_VERSION);
    exit(0);
}

static void
usage(void)
{
    fprintf(stderr, "%s\n%s\n%s\n",
        "usage: jtransfer -v",
        "       jtransfer [-r|-w] [-h] [-d <workdir>] [-m <mode>] [-u <uid>[:<gid>]] -f <file>",
        "                 -j <jid>");
    exit(EX_USAGE);
}

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Memory strategy threshold, in pages: if physmem is larger then this, use a 
 * large buffer.
 */
#define PHYSPAGES_THRESHOLD (32*1024)

/* Maximum buffer size in bytes - do not allow it to grow larger than this. */
#define BUFSIZE_MAX (2*1024*1024)

/*
 * Small (default) buffer size in bytes. It's inefficient for this to be
 * smaller than MAXPHYS.
 */
#define BUFSIZE_SMALL (MAXPHYS)

static ssize_t
copy_fallback(int from_fd, int to_fd)
{
    static char *buf = NULL;
    static size_t bufsize;
    ssize_t rcount, wresid, wcount = 0;
    char *bufp;

    if (buf == NULL) {
        if (sysconf(_SC_PHYS_PAGES) > PHYSPAGES_THRESHOLD)
            bufsize = MIN(BUFSIZE_MAX, MAXPHYS * 8);
        else
            bufsize = BUFSIZE_SMALL;
        buf = malloc(bufsize);
        if (buf == NULL)
            err(EX_OSERR, "Not enough memory");
    }
    do {
        rcount = read(from_fd, buf, bufsize);
    } while (rcount == -1 && errno == EINTR);
    if (rcount <= 0) {
        return (rcount);
    }
    for (bufp = buf, wresid = rcount; ; bufp += wcount, wresid -= wcount) {
        do {
            wcount = write(to_fd, bufp, wresid);
        } while (wcount == -1 && errno == EINTR);
        if (wcount <= 0)
            break;
        if (wcount >= wresid)
            break;
    }
    return (wcount < 0 ? wcount : rcount);
}
