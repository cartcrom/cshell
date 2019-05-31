#include "filec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

void copyFile()
{
   int c;

   while (EOF != (c = getchar()))
      putchar(c);
}

int openFile(const char *fileName, const char *mode)
{
   int fd, flags;

   if (0 == strcmp("r", mode))
      flags = O_RDONLY;
   else if (0 == strcmp("w", mode))
      flags = O_WRONLY | O_CREAT | O_TRUNC;
   else {
      fprintf(stderr, "Unknown openFile mode %s\n", mode);
      exit(EXIT_FAILURE);
   }

   fd = open(fileName, flags, 0600);

   return fd;
}