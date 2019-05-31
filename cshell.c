#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "cshell.h"
#include "filec.h"

#define MAX_LINE 1024
#define MAX_CMDS 20
#define MAX_ARGS 11
#define DELIM " \t\r\n\a"

#define INVALIDPIPE() {fprintf(stderr, "cshell: Invalid pipe\n");\
return 0;}

#define LINETOOLONG() {fprintf(stderr, "cshell: Command line too long\n");\
readRest();\
return 0;}

void PrintError(const char* message) {
   fprintf(stderr, "%s", message);
}

void fail() {
   perror(NULL);
   exit(EXIT_FAILURE);
}

pid_t LaunchCommand(cmd command, fd inter, int hasNext)
{
   pid_t pid;

   pid = fork();
   if (pid == 0) {
      /* Child process */
      if (hasNext)
         close(inter);
      if (-1 == dup2(command.inFD, STDIN_FILENO))
         fail();
      if (-1 == dup2(command.outFD, STDOUT_FILENO))
         fail();

      if (execvp(command.args[0], command.args) == -1)
         fprintf(stderr, "cshell: %s: Command not found\n", command.args[0]);

      exit(EXIT_FAILURE);
   }
   else if (pid < 0) {
      /* Error forking */
      perror("cshell");
   }
   else {
      /* Parent process */
   }

   return pid;
}

void waitChildren(int numChildren, pid_t pids[MAX_CMDS]) {
   int i;
   int status;
   for (i = 0; i < numChildren; i++) {
      do {
         waitpid(pids[i], &status, WUNTRACED);
      } while (!WIFEXITED(status) && !WIFSIGNALED(status));
   }
}

int ExecutePipeline(cmd pipeline[MAX_CMDS]) {
   int i = 0;
   pid_t pids[MAX_CMDS];
   fd cfd[2];
   /* Originally set first cmd in pipeline to read from stdin */
   fd inter = STDIN_FILENO;

   if (strcmp(pipeline[0].args[0], "exit") == 0)
      exit(EXIT_SUCCESS);

   do {
      /* if no input redirection has been set, use the prev cmd's read pipe */
      if (pipeline[i].inFD == STDIN_FILENO)
         pipeline[i].inFD = inter;

      /* If this cmd starts a pipe, */
      if (pipeline[i].hasNext) {
         if (pipe(cfd) < 0)
            fail();
         /* If no output redirection has been set, write to new pipe */
         if (pipeline[i].outFD == STDOUT_FILENO)
            pipeline[i].outFD = cfd[1];
      }
      pids[i] = LaunchCommand(pipeline[i], cfd[0], pipeline[i].hasNext);

      if (i > 0)
         close(inter);
      if (pipeline[i].hasNext)
         close(cfd[1]);

      inter = cfd[0];

   } while(pipeline[i++].hasNext);

   waitChildren(i, pids);

   return 1;
}

fd replaceFile(const char *fileName, int *redir) {
   fd new = openFile(fileName, (*redir == -1) ? "r" : "w");

   if (new == -1) {
      fprintf(stderr, "cshell: Unable to open file for input\n");
      *redir = 0;
      return 0;
   }

   *redir = 0;
   return new;
}

int checkRedir(int *redir, char *token, cmd *command, int i) {
   int found = 0;

   if (!strcmp(token, "<"))
      found = -1;
   else if (!strcmp(token, ">"))
      found = 1;

   /* if < or > is found when we are already redirecting, error */
   if (*redir) {
      if (found)
         return 0;
      else if (*redir == -1 && !(command->inFD = replaceFile(token, redir)))
         return 0;
      else if (*redir == 1 && !(command->outFD = replaceFile(token, redir)))
         return 0;
   }
   else {
      command->args[i] = token;
   }
   *redir = found;
   return 1;
}

cmd parseCommand(char *token)
{
   int i = 0;
   cmd newcmd = {0, 0, STDIN_FILENO, STDOUT_FILENO};

   /* -1 = input/<, 0 = none, 1 = output/> */
   int redir = 0;

   while (token != NULL && strcmp(token, "|") != 0) {
      int redirected = redir;
      /* If redirection syntax error is present */
      if (!checkRedir(&redir, token, &newcmd, i))
         break;

      if (!redir && !redirected)
         newcmd.args[i++] = token;

      if (i > MAX_ARGS) {
         fprintf(stderr, "cshell: test: Too many arguments\n");
         break;
      }

      token = strtok(NULL, DELIM);
   }

   /* if we read all arguments & are not expecting redirection, success */
   newcmd.complete = (!redir && (token == NULL || strcmp(token, "|") == 0));
   if (redir)
      fprintf(stderr, "cshell: Syntax error\n");

   /* NULL terminate args */
   newcmd.args[i] = NULL;

   newcmd.hasNext = (token != NULL && strcmp(token, "|") == 0) ? 1 : 0;

   return newcmd;
}

int ParseLine(char *line, cmd pipeline[MAX_CMDS]) {
   int i = 0;
   char *token = strtok(line, DELIM);

   pipeline[i] = parseCommand(token);
   if (!pipeline[i].complete)
      return 0;

   if (pipeline[i].args[0] == NULL)
      INVALIDPIPE();

   while (pipeline[i].hasNext) {
      token = strtok(NULL, DELIM);
      pipeline[++i] = parseCommand(token);

      if (pipeline[i].args[0] == NULL)
         INVALIDPIPE();

      if (i >= MAX_CMDS) {
         fprintf(stderr, "cshell: Too many commands\n");
         return 0;
      }

      if (!pipeline[i].complete)
         return 0;
   }

   /* if there was an empty pipe on the last command, error */
   return 1;
}

void readRest() {
   int c = getchar();
   while (c != EOF && c != '\n')
      c = getchar();
}

int ReadLine(char *line)
{
   int i = 0;
   int c;

   while (1) {
      c = getchar();

      if (feof(stdin)) {
         printf("exit\n");
         exit(EXIT_SUCCESS);
      }

      if (c == EOF || c == '\n') {
         line[i] = '\0';
         return 1;
      }
      else {
         line[i] = c;
      }
      i++;
      if (i >= MAX_LINE)
         LINETOOLONG();
   }
}

void csh() {
   char line[MAX_LINE];
   int status = 1;
   cmd pipeline[MAX_CMDS];

   setbuf(stdout, NULL);

   do {
      printf(":-) ");
      if (ReadLine(line) && strlen(line) > 0 && ParseLine(line, pipeline))
         status = ExecutePipeline(pipeline);

   } while (status);

}