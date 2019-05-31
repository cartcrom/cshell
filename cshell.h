#ifndef CSHELL_H
#define CSHELL_H

typedef int fd;

typedef struct cmd {
   int complete;
   int hasNext;
   fd inFD;
   fd outFD;
   char *args[12];
} cmd;

void csh();

#endif
