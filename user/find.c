#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/param.h"   // <-- needed for MAXARG

char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // find last slash
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  buf[strlen(p)] = 0;
  return buf;
}

void
find(char *path, char *target, int exec_flag, char **cmd)
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
    if(strcmp(fmtname(path), target) == 0){
      if(exec_flag){
        int pid = fork();
        if(pid == 0){
          // child: prepare argv for exec
          char *args[MAXARG];
          int i = 0;
          while(cmd[i] != 0 && i < MAXARG-1){
            args[i] = cmd[i];
            i++;
          }
          args[i++] = path;   // append filename
          args[i] = 0;
          exec(args[0], args);
          fprintf(2, "find: exec %s failed\n", args[0]);
          exit(1);
        } else {
          wait(0);
        }
      } else {
        printf("%s\n", path);
      }
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      if(de.inum == 0)
        continue;
      if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      find(buf, target, exec_flag, cmd);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  if(argc < 3){
    fprintf(2, "usage: find path filename [-exec cmd]\n");
    exit(1);
  }

  int exec_flag = 0;
  char **cmd = 0;

  for(int i = 3; i < argc; i++){
    if(strcmp(argv[i], "-exec") == 0){
      exec_flag = 1;
      cmd = &argv[i+1]; // command starts after -exec
      break;
    }
  }

  find(argv[1], argv[2], exec_flag, cmd);
  exit(0);
}
