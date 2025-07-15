/*
 * Copyright (c) 2025, OldTeam
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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdnoreturn.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <ncurses.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#include <getopt.h>

#define MODESHORT	0
#define MODELINE	1
#define MODELONG	2

#define SHORTOPTS	"sLn:lrhS"
#define DEFAULTMODE	MODESHORT
#define DEFAULTNPROMPT	30
#define MAXPATHS	128

typedef struct __exe_t {
	char	name[2048];
	char	path[2048];
	size_t	siz;
} exe_t;

int	nprompt;
int	mode;
char	*paths[MAXPATHS];
size_t	npaths;
exe_t	*execs;
exe_t	*lexec;
size_t	execs_cap;
size_t	totsiz;
size_t	nexecs;
u_char	Sflag;

inline static void noreturn finish(int sig)
{
	(void)sig;
	endwin();
	if (execs)
		free(execs);
	exit(0);
}

static const char *bytesfmt(size_t bytes)
{
	const char *sizes[]={
		"B", "KiB", "MiB", "GiB", "TiB",
		"PiB", "EiB"
	};
	static char	buffer[32];
	double		c=(double)bytes;
	int		i=0;

	for (;c>=1024&&i<6;i++) 
		c/=1024;

	snprintf(buffer,sizeof(buffer),
		"%.2f %s",c,sizes[i]);

	return buffer;
}

inline static void exec(void)
{
	pid_t pid;
	if (!lexec)
		return;
	pid=fork();
	if (pid==-1)
		finish(0);
	else if (pid==0) {
		if (setsid()==-1)
			_exit(1);
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		open("/dev/null",O_RDONLY);
		open("/dev/null",O_WRONLY);
		open("/dev/null",O_WRONLY);
		execl(lexec->path,lexec->path,(char*)NULL);
		if (execs)
			free(execs);
		endwin();
		perror("execl");
		_exit(127);
	}
	finish(0);
}

inline static void search(char *in)
{
	size_t	fi,n,sum;
	int	y,x;
	u_char	s;

	sum=s=0;
	getyx(stdscr,y,x);
	for (n=0;n<nexecs;n++)
		if (strstr(execs[n].name,in))
			++sum;
	fi=1;
	for (n=0;n<nexecs&&fi<=nprompt;n++) {
		if (!strcmp(execs[n].name,in)) {
			lexec=&execs[n];
			++s;
		}
		if (strstr(execs[n].name,in)) {
			if (!s)
				lexec=&execs[n];
			if (mode==MODELONG||mode==MODESHORT)
				mvprintw((Sflag)?0:1,0,"exec %s (%s)"
					" %ld\n",lexec->path,bytesfmt(execs[n].siz),sum);
			if (mode==MODELONG) {
				mvhline((Sflag)?2:3,0,ACS_HLINE,45);
				mvprintw(fi+((Sflag)?2:3),0,"%s\n",
					execs[n].name);
			}
			++fi;
		}
	}

	move(y,x);
}

inline static void init(void)
{
	exe_t		exe;
	char		buf[65535];
	struct stat	st;
	struct dirent	*d;
	DIR		*dir;
	size_t		n,t1;

	d=NULL;
	dir=NULL;

	for (n=0;n<npaths;n++) {
		if (!(dir=opendir(paths[n])))
			continue;
		for (;(d=readdir(dir));) {
			if (!strcmp(d->d_name, ".")||!strcmp(d->d_name, ".."))
				continue;

			bzero(buf,sizeof(buf));
			snprintf(buf,sizeof(buf),"%s/%s",paths[n],d->d_name);

			if (stat(buf,&st)==-1)
				continue;

			if (nexecs==execs_cap) {
#define EXECS_STEP 512
				t1=execs_cap+EXECS_STEP;
				exe_t *t=realloc(execs,t1*sizeof(exe_t));
				if (!t)
					finish(0);
				execs=t;
				execs_cap=t1;
			}

			bzero(&exe,sizeof(exe));
			snprintf(exe.name,sizeof(exe.name),"%s",d->d_name);
			snprintf(exe.path,sizeof(exe.path),"%s",buf);
			exe.siz=st.st_size;
			totsiz+=st.st_size;

			execs[nexecs++]=exe;
		}
		closedir(dir);
	}
}

inline static int loop(void)
{
	char	in[2048];
	chtype	c;
	int	n;
	int	cpos;

	bzero(in,sizeof(in));
	cpos=(mode==MODELINE)?0:(Sflag)?1:2;
	n=0;

	switch (mode) {
		case MODELONG:
		case MODESHORT:
			if (!Sflag)
				mvprintw(0, 0, "loaded %ld files from"
					" %ld paths (%s)\n",nexecs,
					npaths,bytesfmt(totsiz));
			mvprintw((Sflag)?0:1,0,"exec %s (%s)"
				" %ld\n",execs->path,
				bytesfmt(execs->siz),nexecs);
			break;
	}

	mvprintw(cpos,0,": ");
	refresh();

	for (;;) {
		c=getch();
		if (c=='\n')
			break;
		else if (c==KEY_BACKSPACE||c==127) {
			if (n>0) {
				in[--n]='\0';
				move(cpos,(2+n));
				delch();
			}
		} else if (n<sizeof(in)-1) {
			in[n++]=c;
			addch(c);
		}
		in[n]='\0';
		search(in);
	}
	exec();

	/* NOTREACHED */
	return 0;
}

int main(int c, char **av)
{
	int	rez;
	int	n;
	int	j;

	signal(SIGINT,finish);
	srand(time(NULL));
	if (c<=1) {
L0:		fprintf(stderr,"Usage %s [options] <path1 ...,>\n",*av);
		fprintf(stderr,"  -s \t\tenable short display mode\n");
		fprintf(stderr,"  -L \t\tenable long display mode\n");
		fprintf(stderr,"  -n <max> \tspecify the maximum number"
			" of prompts\n");
		fprintf(stderr,"  -l \t\tenable line display mode\n");
		fprintf(stderr,"  -r \t\tspecify random display mode\n");
		fprintf(stderr,"  -S \t\tskip the very first loading info\n");
		fprintf(stderr,"  -h \t\tshow this menu and exit\n");
		fprintf(stderr,"\nReleased in %s %s\n",__DATE__,__TIME__);
		finish(0);
	}

	mode=DEFAULTMODE;
	Sflag=0;
	totsiz=0;
	nexecs=0;
	nprompt=DEFAULTNPROMPT;
	lexec=NULL;

	while ((rez=getopt(c,av,SHORTOPTS))!=EOF) {
		switch (rez) {
			case 'S':
				++Sflag;
				break;
			case 's':
				mode=MODESHORT;
				break;
			case 'l':
				mode=MODELINE;
				break;
			case 'L':
				mode=MODELONG;
				break;
			case 'r':
				mode=rand()%3;
				break;
			case 'n': {
				unsigned long long val;
				char *endp;

				errno=0;
				val=strtoull(optarg,&endp,10);
				if (errno==ERANGE||val>
						(unsigned long long)	
						SIZE_MAX) {
L1:					fprintf(stderr,"Failed convert"
						" \"%s\" to num\n",optarg);
					finish(0);
				}
				while (isspace((u_char)*endp))
					endp++;
				if (*endp!='\0')
					goto L1;
				if (val<1||val>INT_MAX)
					goto L1;

				nprompt=(int)val;
				break;
			}
			case 'h': case '?': default:
				goto L0;
		}
	}

	n=c-optind;
	if (n<=0)
		goto L0;
	if (n>MAXPATHS) {
		fprintf(stderr,"Too many paths!\n");
		finish(0);
	}
	npaths=n;
	n=optind;
	j=0;
	while (n<c) {
		paths[j++]=av[n];
		++n;
	}

	init();
	if (nexecs==0) {
		fprintf(stderr,"Not found files in paths!\n");
		finish(0);
	}

	initscr();
	cbreak();
	noecho();
	keypad(stdscr,TRUE);
	
	return loop();
}
