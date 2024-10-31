/*
 * Copyright (c) 2024, OldTeam
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

#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <map>
#include <fcntl.h>
#include <vector>
#include <filesystem>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define cls(u0) \
  printf("\r\033[K");

typedef void           u0;
typedef unsigned char  u8;
typedef unsigned int   u32;
typedef size_t         usize;
typedef std::string    str;
class                  target;
namespace fs=          std::filesystem;

/* paths for search */
std::vector<str> paths={
  "/bin", "/usr/bin", "/sbin", "/usr/sbin",
  "/usr/local/bin", "/usr/local/sbin"
};

class target {
  str name, path;
  usize len;

  public:
  usize getlen(u0) { return this->len; }
  str getname(u0) { return this->name; }
  str getpath(u0) { return this->path; }
  u0 setname(str name) { this->name=name; }
  u0 setlen(usize len) { this->len=len; }
  u0 setpath(str path) { this->path=path; }
};

std::vector<target> nodes={};
str                 buf="";
char                c=0;
size_t              num=0, totlen=0;
struct termios      original;
target              last;
bool                long_m=0, line_m=0;

static str bytes_c(usize bytes)
{
  /* nesca4-dev/blob/main/nescadata.cc */
  const char *sizes[]={
    "B", "KiB", "MiB", "GiB",
    "TiB", "PiB", "EiB"
  };
  double c=0;
  int i=0;

  c=static_cast<double>(bytes);
  while (c>=1024&&i<6) {
    c/=1024;
    i++;
  }

  std::ostringstream result;
  result<<std::fixed<<std::setprecision(2)
    <<c<<" "<<sizes[i];
  return result.str();
}

static bool is_exec(const fs::path &path)
{
  struct stat file_stat;
  if (stat(path.c_str(), &file_stat)==0)
    return (file_stat.st_mode&S_IXUSR)!=0;
  return 0;
}

static u0 importnodes(u0)
{
  for (const auto&p:paths) {
    if (!fs::exists(p)||!fs::is_directory(p))
      continue;
    for (const auto&e:fs::directory_iterator(p)) {
       if (is_exec(e.path())&&fs::is_regular_file(e)) {
        target is_res;
        is_res.setname(e.path().filename().string());
        is_res.setpath(e.path().string());
        is_res.setlen(fs::file_size(e));
        nodes.push_back(is_res);
        totlen+=is_res.getlen();
        num++;
      }
    }
  }
  std::cout<<"loaded "<<num<<" files from "<<
    paths.size()<<" paths ("<<bytes_c(totlen)<<")\n";
}

static u0 fucksearch(const str &buf)
{
  std::map<str,target> res;
  for (auto&node:nodes)
    if (node.getname().find(buf)!=std::string::npos)
      res[(node.getname())]=node;
  if (res.empty()) {
    cls(u0);
    if (long_m)
      std::cout << "0";
    std::cout << ": " << buf;
  }
  if (!res.empty()) {
    if (long_m) {
      cls(u0);
      for (const auto&s:res)
        std::cout << s.first << std::endl;
      last=res.begin()->second;
      std::cout << res.size();
      std::cout << ": " << buf;
    }
    else if (line_m) {
      cls(u0);
      std::cout << ": "<<res.begin()->first << " ";
      std::cout << res.size();
      last=res.begin()->second;
    }
  }
}

static u0 noncanonmode(struct termios &orig, bool notrules)
{
  struct termios tty;
  if (notrules) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig);
    return;
  }
  tcgetattr(STDIN_FILENO, &orig);
  tty=original;
  tty.c_lflag&=~(ICANON|ECHO);
  tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

static /* noreturn */ u0 goodbye(int sig)
{
  noncanonmode(original, 1);
  exit(0);
}

static u0 runbaby(u0)
{
  pid_t pid;
  pid=fork();
  if (pid==-1)
    goodbye(0); /* baby not work */
  else if (pid==0) {
    std::cout << ": exec " << last.getpath() << " " << bytes_c(last.getlen()) << std::endl;
    if (setsid()==-1)
      goodbye(0);
    /* baby, baby... */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
    execl(last.getpath().c_str(), last.getpath().c_str(), (char*)NULL);
  }
  goodbye(0); /* baby fulfilled */
}

int main(int argc, char **argv)
{
  if (argc<=1) {
needhelp:
    fprintf(stdout, "Usage %s <mode>\n", argv[0]);
    fprintf(stdout, "- random  random mode \n");
    fprintf(stdout, "- line    search in one line\n");
    fprintf(stdout, "- long    search with more lines\n");
    return 0;
  }
  if (!strcmp(argv[1], "long"))
    long_m=1;
  else if (!strcmp(argv[1], "line"))
    line_m=1;
  else if (!strcmp(argv[1], "random")) {
    std::srand(static_cast<u32>
      (std::time(nullptr)));
    if (std::rand()%2==0)
      line_m=1;
    else
      long_m=1;
  }
  else goto needhelp;

  signal(SIGINT, goodbye);
  importnodes();
  noncanonmode(original, 0);
  std::cin>>std::noskipws;
  fprintf(stdout, ": ");

  while (std::cin.get(c)&&c!='\n') {
    if (c=='\b'||c==127) {
      if (!buf.empty()) {
        buf.pop_back();
        std::cout<<"\b \b";
      }
    }
    else
      buf+=c;
    /* call fucksearch */
    fucksearch(buf);
  }

  cls(u0);
  runbaby();
  putchar('\n');

  /* NOTREACHED */
}
