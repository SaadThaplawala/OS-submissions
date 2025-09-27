// user/find.c -- find with -exec and regex-friendly pattern normalization
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/param.h"   // for MAXARG


// ---------------- REGEX FUNCTIONS (from grep.c) ----------------
int match(char*, char*);
int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

int
matchhere(char *re, char *text)
{
  if(re[0] == '\0')
    return 1;
  if(re[1] == '*')
    return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0')
    return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

int
matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

// ---------------- helper: filename from path ----------------
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

// ---------------- helper: normalize pattern ----------------
// - strip surrounding ' or " if present
// - remove backslashes '\' (so '\.' -> '.')
void
normalize_pattern(char *s)
{
  if(!s || !*s) return;

  int len = strlen(s);

  // Strip surrounding single or double quotes
  if(len >= 2 && ((s[0] == '\'' && s[len-1] == '\'') ||
                  (s[0] == '"'  && s[len-1] == '"'))) {
    // shift left by 1 and null-terminate at len-2
    int i;
    for(i = 0; i < len-2; i++)
      s[i] = s[i+1];
    s[i] = '\0';
    len = strlen(s);
  }

  // Remove backslashes: transform in-place
  int ri = 0;
  for(int wi = 0; wi < len; wi++){
    if(s[wi] == '\\') {
      // skip backslash, copy next char if any
      if(wi + 1 < len) {
        s[ri++] = s[wi+1];
        wi++; // consume escaped char
      }
      // if backslash is last char, drop it
    } else {
      s[ri++] = s[wi];
    }
  }
  s[ri] = '\0';
}

// ---------------- CORE FIND ----------------
void
find(char *path, char *target, int use_regex, int exec_flag, char **cmd)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    {
      char *name = fmtname(path);
      int matched = 0;
      if(use_regex){
        matched = match(target, name);
      } else {
        matched = (strcmp(name, target) == 0);
      }
      if(matched){
        if(exec_flag){
          // build argv: [cmd...] path 0
          char *argv[MAXARG];
          int i = 0;
          if(cmd){
            while(cmd[i] != 0 && i < MAXARG-2){
              argv[i] = cmd[i];
              i++;
            }
          }
          argv[i++] = path; // append the matched filename
          argv[i] = 0;
          int pid = fork();
          if(pid == 0){
            exec(argv[0], argv);
            fprintf(2, "find: exec %s failed\n", argv[0]);
            exit(1);
          } else {
            wait(0);
          }
        } else {
          printf("%s\n", path);
        }
      }
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      // stat/check inside recursion will handle files and directories
      find(buf, target, use_regex, exec_flag, cmd);
    }
    break;
  }
  close(fd);
}

// ---------------- MAIN ----------------
int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(2, "usage: find path pattern [-exec cmd...]\n");
    exit(1);
  }

  // normalize pattern (strip quotes/backslashes added by the shell)
  normalize_pattern(argv[2]);

  // detect regex usage (after normalization)
  int use_regex = 0;
  for(char *p = argv[2]; *p; p++){
    if(*p == '.' || *p == '*' || *p == '^' || *p == '$'){
      use_regex = 1;
      break;
    }
  }

  // detect -exec
  int exec_flag = 0;
  char **cmd = 0;
  for(int i = 3; i < argc; i++){
    if(strcmp(argv[i], "-exec") == 0){
      exec_flag = 1;
      cmd = &argv[i+1]; // remainder are command tokens, null-terminated by shell
      break;
    }
  }

  find(argv[1], argv[2], use_regex, exec_flag, cmd);
  exit(0);
}
