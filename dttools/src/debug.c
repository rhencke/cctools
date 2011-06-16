/*
Copyright (C) 2003-2004 Douglas Thain and the University of Wisconsin
Copyright (C) 2005- The University of Notre Dame
This software is distributed under the GNU General Public License.
See the file COPYING for details.
*/

#include "debug.h"
#include "domain_name_cache.h"
#include "full_io.h"
#include "macros.h"
#include "stringtools.h"
#include "xmalloc.h"

#include <unistd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <pthread.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAS_ALLOCA_H
#include <alloca.h>
#endif

static pid_t(*debug_getpid) () = getpid;

struct debug_settings {
	pthread_mutex_t mutex;
	int fd;
	char output[PATH_MAX];
	size_t output_size;
	INT64_T flags;
	char program_name[1024];
} *D;

struct fatal_callback {
	void (*callback) ();
	struct fatal_callback *next;
};

struct fatal_callback *fatal_callback_list = 0;

struct flag_info {
	const char *name;
	INT64_T flag;
};

static struct flag_info table[] = {
	{"syscall", D_SYSCALL},
	{"notice", D_NOTICE},
	{"channel", D_CHANNEL},
	{"process", D_PROCESS},
	{"resolve", D_RESOLVE},
	{"libcall", D_LIBCALL},
	{"tcp", D_TCP},
	{"dns", D_DNS},
	{"auth", D_AUTH},
	{"local", D_LOCAL},
	{"http", D_HTTP},
	{"ftp", D_FTP},
	{"nest", D_NEST},
	{"chirp", D_CHIRP},
	{"landlord", D_LANDLORD},
	{"multi", D_MULTI},
	{"dcap", D_DCAP},
	{"rfio", D_RFIO},
	{"glite", D_GLITE},
	{"lfc", D_LFC},
	{"gfal", D_GFAL},
	{"grow", D_GROW},
	{"pstree", D_PSTREE},
	{"alloc", D_ALLOC},
	{"cache", D_CACHE},
	{"poll", D_POLL},
	{"hdfs", D_HDFS},
	{"bxgrid", D_BXGRID},
	{"debug", D_DEBUG},
	{"login", D_LOGIN},
	{"irods", D_IRODS},
	{"wq", D_WQ},
	{"user", D_USER},
	{"xrootd", D_XROOTD},
	{"remote", D_REMOTE},
	{"all", ~0},
	{"time", 0},		/* backwards compatibility */
	{"pid", 0},		/* backwards compatibility */
	{0, 0}
};

static const char *flag_to_name(INT64_T flag)
{
	struct flag_info *i;

	for(i = table; i->name; i++) {
		if(i->flag & flag)
			return i->name;
	}

	return "debug";
}

int debug_flags_set(const char *flagname)
{
	struct flag_info *i;

	pthread_mutex_lock(&D->mutex);
	for(i = table; i->name; i++) {
		if(!strcmp(flagname, i->name)) {
			D->flags |= i->flag;
			pthread_mutex_unlock(&D->mutex);
			return 1;
		}
	}
	pthread_mutex_unlock(&D->mutex);

	return 0;
}

void debug_flags_print(FILE * stream)
{
	int i;

	pthread_mutex_lock(&D->mutex);
	for(i = 0; table[i].name; i++) {
		fprintf(stream, "%s ", table[i].name);
	}
	pthread_mutex_unlock(&D->mutex);
}

void debug_set_flag_name(INT64_T flag, const char *name)
{
	struct flag_info *i;

	pthread_mutex_lock(&D->mutex);
	for(i = table; i->name; i++) {
		if(i->flag & flag) {
			i->name = name;
			break;
		}
	}
	pthread_mutex_unlock(&D->mutex);
}

static void do_debug(int is_fatal, INT64_T flags, const char *fmt, va_list args)
{
	char newfmt[65536];
	char buffer[65536];
	int length;

	struct timeval tv;
	struct tm *tm;

	gettimeofday(&tv, 0);
	tm = localtime(&tv.tv_sec);

	sprintf(newfmt, "%04d/%02d/%02d %02d:%02d:%02d.%02ld [%d] %s: %s: %s", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, (long) tv.tv_usec / 10000, (int) debug_getpid(), D->program_name, is_fatal ? "fatal " : flag_to_name(flags), fmt);

	vsprintf(buffer, newfmt, args);
	string_chomp(buffer);
	strcat(buffer, "\n");
	length = strlen(buffer);

	if(strcmp(D->output, "") != 0) {
		struct stat info;

		fstat(D->fd, &info);
		if(S_ISREG(info.st_mode) && info.st_size >= D->output_size && D->output_size != 0) {
			close(D->fd);

			if(stat(D->output, &info) == 0) {
				if(info.st_size >= D->output_size) {
					char *newname = alloca(strlen(D->output) + 5);
					sprintf(newname, "%s.old", D->output);
					rename(D->output, newname);
				}
			}

			D->fd = open(D->output, O_CREAT | O_TRUNC | O_WRONLY, 0777);
			if(D->fd < 0) {
				pthread_mutex_unlock(&D->mutex);
				fatal("couldn't open %s: %s", D->output, strerror(errno));
            }
		}
	}

	full_write(D->fd, buffer, length);
}

void debug(INT64_T flags, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	pthread_mutex_lock(&D->mutex);
	if(flags & D->flags) {
		int save_errno = errno;
		do_debug(0, flags, fmt, args);
		errno = save_errno;
	}
	pthread_mutex_unlock(&D->mutex);

	va_end(args);
}

void fatal(const char *fmt, ...)
{
	struct fatal_callback *f;
	va_list args;
	va_start(args, fmt);

	pthread_mutex_lock(&D->mutex);
	do_debug(1, 0, fmt, args);
	pthread_mutex_unlock(&D->mutex);

	for(f = fatal_callback_list; f; f = f->next) {
		f->callback();
	}

	while(1) {
		kill(getpid(), SIGTERM);
		kill(getpid(), SIGKILL);
	}

	va_end(args);
}

void debug_config_fatal(void (*callback) ())
{
	struct fatal_callback *f;
	f = xxmalloc(sizeof(*f));
	f->callback = callback;
	f->next = fatal_callback_list;
	fatal_callback_list = f;
}

void debug_config_file(const char *f)
{
	pthread_mutex_lock(&D->mutex);
	if(f) {
		if(*f == '/')
			strcpy(D->output, f);
		else {
			char path[8192];
			if(getcwd(path, sizeof(path)) == NULL)
				assert(0);
			assert(strlen(path) + strlen(f) + 1 < 8192);
			strcat(path, "/");
			strcat(path, f);
			strcpy(D->output, path);
		}
		D->fd = open(f, O_CREAT | O_APPEND | O_WRONLY, 0777);
		if(D->fd < 0) {
            pthread_mutex_unlock(&D->mutex);
			fatal("couldn't open %s: %s", f, strerror(errno));
        }
	} else {
		D->fd = STDERR_FILENO;
	}
	pthread_mutex_unlock(&D->mutex);
}

void debug_config_file_size(size_t size)
{
	pthread_mutex_lock(&D->mutex);
	D->output_size = size;
	pthread_mutex_unlock(&D->mutex);
}

void debug_config_getpid(pid_t(*getpidfunc) ())
{
	debug_getpid = getpidfunc;
}

INT64_T debug_flags_clear()
{
	INT64_T result;
	pthread_mutex_lock(&D->mutex);
	result = D->flags;
	D->flags = 0;
	pthread_mutex_unlock(&D->mutex);
	return result;
}

void debug_flags_restore(INT64_T fl)
{
	pthread_mutex_lock(&D->mutex);
	D->flags = fl;
	pthread_mutex_unlock(&D->mutex);
}

void debug_config(const char *name)
{
	int fd = open("/dev/zero", O_RDWR);
	if(fd == -1) {
		fprintf(stderr, "couldn't open /dev/zero: %s\n", strerror(errno));
		_exit(1);
	}
	D = (struct debug_settings *) mmap(NULL, sizeof(struct debug_settings), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if(D == MAP_FAILED) {
		fprintf(stderr, "could not allocate shared memory page: %s\n", strerror(errno));
		_exit(1);
	}

	D->fd = STDERR_FILENO;
	memset(D->output, 0, PATH_MAX);
	D->output_size = 10485760;
	D->flags = D_NOTICE;

	if(strlen(name) >= 1024) {
		fprintf(stderr, "program name is too long\n");
		_exit(1);
	}
	const char *end = strrchr(name, '/');
	if(end) {
		strcpy(D->program_name, end+1);
	} else {
		strcpy(D->program_name, name);
	}

	pthread_mutexattr_t attr;
	int result = pthread_mutexattr_init(&attr);
	if (result != 0) {
		fprintf(stderr, "pthread_mutexattr_init failed: %s\n", strerror(result));
		_exit(1);
	}
	result = pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	if (result != 0) {
		fprintf(stderr, "pthread_mutexattr_setpshared failed: %s\n", strerror(result));
		_exit(1);
	}
	result = pthread_mutex_init(&D->mutex, &attr);
	if (result != 0) {
		fprintf(stderr, "pthread_mutex_init failed: %s\n", strerror(result));
		_exit(1);
	}
	pthread_mutexattr_destroy(&attr);
}
