// Enhanced Shell for xv6 RISC-V

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10
#define MAXHISTORY 50
#define MAXCMDLEN 100

// Global variables for shell features
static char history[MAXHISTORY][MAXCMDLEN];
static int history_count = 0;
static int history_index = 0;
static int from_file = 0;  // Flag to track if reading from file

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

// Background process tracking for wait command
#define MAX_BG_PROCS 20
static int bg_pids[MAX_BG_PROCS];
static int bg_count = 0;

int fork1(void);
void panic(char*);
struct cmd *parsecmd(char*);
void runcmd(struct cmd*) __attribute__((noreturn));

// History functions
void add_to_history(char *cmd);
void print_history(void);

// Tab completion functions
void tab_complete(char *buf, int *pos);
int find_matches(char *prefix, char matches[][32], int max_matches);

// Background process management
void add_bg_process(int pid);
void wait_for_bg_processes(void);

// Add command to history
void
add_to_history(char *cmd)
{
  // Don't add empty commands or commands that start with space
  if (cmd[0] == '\0' || cmd[0] == ' ' || cmd[0] == '\n')
    return;
  
  // Remove trailing newline
  int len = strlen(cmd);
  if (len > 0 && cmd[len-1] == '\n')
    cmd[len-1] = '\0';
  
  // Don't add duplicate consecutive commands
  if (history_count > 0 && strcmp(history[(history_index - 1 + MAXHISTORY) % MAXHISTORY], cmd) == 0)
    return;
  
  // Manual string copy instead of strncpy
  int i;
  for (i = 0; i < MAXCMDLEN-1 && cmd[i] != '\0'; i++) {
    history[history_index][i] = cmd[i];
  }
  history[history_index][i] = '\0';
  
  history_index = (history_index + 1) % MAXHISTORY;
  if (history_count < MAXHISTORY)
    history_count++;
}

// Print command history
void
print_history(void)
{
  int start = (history_count < MAXHISTORY) ? 0 : history_index;
  int i;
  for (i = 0; i < history_count; i++) {
    int idx = (start + i) % MAXHISTORY;
    fprintf(1, "%d: %s\n", i + 1, history[idx]);
  }
}

// Simple tab completion for common commands
void
tab_complete(char *buf, int *pos)
{
  char *common_commands[] = {
    "ls", "cat", "echo", "grep", "wc", "rm", "mkdir", "cd", "pwd",
    "ps", "kill", "cp", "mv", "chmod", "head", "tail", "sort", "uniq",
    "find", "history", "wait", "exit", 0
  };
  
  // Find the start of the current word
  int word_start = *pos;
  while (word_start > 0 && buf[word_start-1] != ' ' && buf[word_start-1] != '\t')
    word_start--;
  
  int word_len = *pos - word_start;
  if (word_len == 0)
    return;
  
  char prefix[32];
  int i;
  for (i = 0; i < word_len && i < 31; i++) {
    prefix[i] = buf[word_start + i];
  }
  prefix[i] = '\0';
  
  // Find matches
  char matches[10][32];
  int match_count = 0;
  
  for (i = 0; common_commands[i] && match_count < 10; i++) {
    // Manual string comparison instead of strncmp
    int j;
    int match = 1;
    for (j = 0; j < word_len; j++) {
      if (common_commands[i][j] != prefix[j]) {
        match = 0;
        break;
      }
    }
    if (match && common_commands[i][word_len] != '\0') {
      strcpy(matches[match_count], common_commands[i]);
      match_count++;
    }
  }
  
  if (match_count == 1) {
    // Single match - complete it
    int completion_len = strlen(matches[0]) - word_len;
    if (*pos + completion_len < MAXCMDLEN - 1) {
      strcpy(buf + *pos, matches[0] + word_len);
      *pos += completion_len;
      buf[*pos] = ' ';  // Add space after completion
      (*pos)++;
      buf[*pos] = '\0';
      fprintf(2, "\r$ %s", buf);
    }
  } else if (match_count > 1) {
    // Multiple matches - show them
    fprintf(2, "\n");
    for (i = 0; i < match_count; i++) {
      fprintf(2, "%s  ", matches[i]);
    }
    fprintf(2, "\n$ %s", buf);
  }
}

// Add background process
void
add_bg_process(int pid)
{
  if (bg_count < MAX_BG_PROCS) {
    bg_pids[bg_count++] = pid;
  }
}

// Wait for background processes
void
wait_for_bg_processes(void)
{
  int i;
  for (i = 0; i < bg_count; i++) {
    int status;
    // Use wait() instead of waitpid() since xv6 doesn't have waitpid
    int pid = wait(&status);
    if (pid > 0) {
      fprintf(1, "Background process %d completed\n", pid);
    }
  }
  bg_count = 0;  // Reset background process count
}

// Execute cmd. Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);
    
    // Handle built-in commands
    if(strcmp(ecmd->argv[0], "history") == 0) {
      print_history();
      exit(0);
    }
    if(strcmp(ecmd->argv[0], "wait") == 0) {
      wait_for_bg_processes();
      exit(0);
    }
    
    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

// Enhanced getcmd with tab completion support
int
getcmd(char *buf, int nbuf)
{
  // Only print $ prompt when reading from console (not from file)
  if (!from_file) {
    write(2, "$ ", 2);
  }
  
  memset(buf, 0, nbuf);
  
  if (from_file) {
    // Simple gets() when reading from file
    gets(buf, nbuf);
    if(buf[0] == 0) // EOF
      return -1;
    return 0;
  }
  
  // Interactive mode with tab completion
  int pos = 0;
  char c;
  
  while (pos < nbuf - 1) {
    if (read(0, &c, 1) != 1)
      break;
    
    if (c == '\t') {
      // Tab completion
      buf[pos] = '\0';
      tab_complete(buf, &pos);
      continue;
    } else if (c == '\n') {
      buf[pos] = c;
      buf[pos + 1] = '\0';
      write(2, "\n", 1);
      break;
    } else if (c == 127 || c == '\b') {  // Backspace
      if (pos > 0) {
        pos--;
        write(2, "\b \b", 3);  // Erase character
      }
      continue;
    } else if (c >= ' ' && c <= '~') {  // Printable characters
      buf[pos] = c;
      pos++;
      write(2, &c, 1);  // Echo character
    }
  }
  
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(int argc, char *argv[])
{
  static char buf[100];
  int fd;

  // Check if we're reading from a file
  if (argc > 1) {
    // Reading from file - redirect stdin
    if ((fd = open(argv[1], O_RDONLY)) < 0) {
      fprintf(2, "shell: cannot open %s\n", argv[1]);
      exit(1);
    }
    close(0);
    dup(fd);
    close(fd);
    from_file = 1;
  } else {
    // Ensure that three file descriptors are open for interactive mode
    while((fd = open("console", O_RDWR)) >= 0){
      if(fd >= 3){
        close(fd);
        break;
      }
    }
  }

  // Initialize history
  memset(history, 0, sizeof(history));

  // Read and run input commands
  while(getcmd(buf, sizeof(buf)) >= 0){
    char *cmd = buf;
    while (*cmd == ' ' || *cmd == '\t')
      cmd++;
    if (*cmd == '\n') // is a blank command
      continue;
      
    // Add to history (only for interactive mode)
    if (!from_file) {
      add_to_history(cmd);
    }
      
    if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' '){
      // Chdir must be called by the parent, not the child.
      cmd[strlen(cmd)-1] = 0;  // chop \n
      if(chdir(cmd+3) < 0)
        fprintf(2, "cannot cd %s\n", cmd+3);
    } else if(strcmp(cmd, "history\n") == 0) {
      // Handle history command in parent
      print_history();
    } else if(strcmp(cmd, "wait\n") == 0) {
      // Handle wait command in parent
      wait_for_bg_processes();
    } else if(strcmp(cmd, "exit\n") == 0) {
      // Handle exit command
      exit(0);
    } else {
      int pid = fork1();
      if (pid == 0) {
        runcmd(parsecmd(cmd));
      } else {
        // Check if command ends with &
        char *amp = strchr(cmd, '&');
        if (amp && amp[1] == '\n') {
          // Background process
          add_bg_process(pid);
          fprintf(1, "[%d] %d\n", bg_count, pid);
        } else {
          // Foreground process
          wait(0);
        }
      }
    }
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
