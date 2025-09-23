#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Forward declaration of regex matcher
int match(char*, char*);

// Find function
void find(char *path, char *pattern) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, O_RDONLY)) < 0) {
        printf("find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE: {
        char *name = path + strlen(path) - 1;
        while (name > path && *name != '/') name--;
        if (*name == '/') name++;
        if (match(pattern, name))
            printf("%s\n", path);
        break;
    }

    case T_DIR:
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';

        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0)
                continue;

            // Extract directory entry name safely and trim trailing nulls/spaces
            char name[DIRSIZ+1];
            int i;
            for(i=0; i<DIRSIZ && de.name[i] != 0; i++)
                name[i] = de.name[i];
            name[i] = '\0';

            if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
                continue;

            // Build full path
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = '\0';

            // Trim trailing nulls/spaces in buf
            for(int j=strlen(buf)-1; j>=0; j--) {
                if(buf[j] == 0 || buf[j] == ' ')
                    buf[j] = '\0';
                else
                    break;
            }

            // Debug log
            printf("DEBUG: Checking path: '%s', name: '%s'\n", buf, name);

            if (stat(buf, &st) < 0) {
                printf("find: cannot stat %s\n", buf);
                continue;
            }

            if (st.type == T_DIR) {
                find(buf, pattern);
            } else {
                if(match(pattern, name))
                    printf("%s\n", buf);
            }
        }
        break;
    }

    close(fd);
}

// Main function
int main(int argc, char *argv[]) {
    if(argc < 3){
        printf("usage: find path regex_pattern\n");
        exit(1);
    }

    find(argv[1], argv[2]);
    exit(0);
}

// =========================
// Regex matcher (from grep.c)
// =========================

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int match(char *re, char *text) {
    if(re[0] == '^')
        return matchhere(re+1, text);
    do {
        if(matchhere(re, text))
            return 1;
    } while(*text++ != '\0');
    return 0;
}

int matchhere(char *re, char *text) {
    if(re[0] == '\0')
        return 1;
    if(re[1] == '*')
        return matchstar(re[0], re+2, text);
    if(re[0] == '$' && re[1] == '\0')
        return *text == '\0';
    if(*text != '\0' && (re[0] == '.' || re[0] == *text))
        return matchhere(re+1, text+1);
    return 0;
}

int matchstar(int c, char *re, char *text) {
    do {
        if(matchhere(re, text))
            return 1;
    } while(*text != '\0' && (*text++ == c || c == '.'));
    return 0;
}
