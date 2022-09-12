#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

/*
* Background Job structure to keep track of a background process, its process id and the command which it is executing
*/
struct BackgroundJob {
  pid_t pid;
  char *cmd;
  struct BackgroundJob* next;
};

/*
* Head of the Background Process Linked list
*/
struct BackgroundJob* head    = NULL;

/*
* PID of the current running process
* -1 if no process running
*/
pid_t activeChild  = -1;

/*
* Handle the CRTL-C signal by killing the active child process
*/
void handle_sigint(int sig) {
  
  if (activeChild != -1) {
    kill(activeChild, SIGQUIT);
  }
  activeChild = -1;

  signal(SIGINT, SIG_IGN);
}

/*
* Add a process executing a task in the background to the list of background processes
*/
void addBackgroundJob(pid_t pid, char *args[]) {

  int j, ndx;
  char str[80];

  // Allocate space for the new process to add to memory
  struct BackgroundJob* bgJob = (struct BackgroundJob*) malloc(sizeof(struct BackgroundJob));

  // Create a single command string from the list of arguments
  strcpy(str, args[0]);
  j = 1;
  while (args[j] != NULL) {
    strcat(str, " ");
    strcat(str, args[j]);
    j++;
  }

  // Initialize data values of the new background process
  bgJob->pid = pid;
  bgJob->cmd = strdup(str);
  bgJob->next = NULL;

  ndx = 0;

  // If list is empty, make new process head of the list
  if (head == NULL) {
    head = bgJob;
    printf("[%d] %d", ndx, bgJob->pid);
    return;
  }
  
  // Initialize the iterator
  struct BackgroundJob *last = head;

  // Otherwise iterate through the list until you arrive at the last element
  while (last->next != NULL) {
    last = last->next;
    ndx++;
  }

  // Append the new process to the end of the list
  last->next = bgJob;
  ndx++;
  printf("[%d] %d", ndx, bgJob->pid);
  return;
}

/*
* Parse a potential redirect command in the following steps
* 1) Search for a '>' character in the buffer string
* 2) Create a list of arguments for the arguments to the left of the delimiter
* 3) Create a list of arguments for the arguments to the right of the delimiter
* 4) If the second list is empty, this is not a redirect command
*/
int parseRedirect(char* buffer, char *redirectArgs[]) {

  char *token;
  int i;

  redirectArgs[0] = strtok(buffer, ">");
  redirectArgs[1] = strtok(NULL, ">");

  if (redirectArgs[1] == NULL || strlen(redirectArgs[1]) == 0) {
    return 0;
  }
  
  return 1;
}

/*
* Parse a potential pipe command in the following steps
* 1) Search for a '|' character in the buffer string
* 2) Create a list of arguments for the arguments to the left of the delimiter
* 3) Create a list of arguments for the arguments to the right of the delimiter
* 4) If the second list is empty, this is not a pipe command
*/
int parsePipe(char* buffer, char *pipeArgs[]) {

  int i;

  pipeArgs[0] = strtok(buffer, "|");
  pipeArgs[1] = strtok(NULL, "|");

  if (pipeArgs[1] == NULL || strlen(pipeArgs[1]) == 0) {
    return 0;
  }
  
  return 1;
}

/*
* Print the task executing in each background process, its index, and its command string
*/
int printJobs() {

  int i = 0;
  struct BackgroundJob* curJob = head;

  // Iterate through all Jobs in the linked list
  while (curJob != NULL) {
    
    if (i != 0) {
      printf("\n");
    }

    // Print the necessry information for the job
    printf("[%d] %d %s", i, curJob->pid, curJob->cmd);
    curJob = curJob->next;
    i++;
  }

  return 0;
}

/*
* Remove a Background Process based on its index in the Job list
*/
pid_t removeJob_Ndx(int ndx) {

  pid_t rtn;
  int i = 0;

  struct BackgroundJob *cur = head;
  struct BackgroundJob *prev;

  // If the list is empty, return 0
  if (cur == NULL) {
    return 0;
  }
  
  // If the process to be deleted is the first, move pointers up by 1 
  // and free the memory associated with the deleted process
  if (ndx == 0) {
    head = cur->next;
    rtn = cur->pid;
    printf("%s", cur->cmd);
    free(cur);
    return rtn;
  }
  
  // Iterate through the list to find the index to be deleted if it exists
  while (i != ndx && cur != NULL) {
    prev = cur;
    cur  = cur->next;
    i++;
  }

  // If index did not exist return 0
  if (cur == NULL) {
    return 0;
  }
  // Otherwise delete reference to index
  else {
    prev->next = cur->next;
  }

  // Free memory space associated with process and return its pid
  rtn = cur->pid;
  printf("%s", cur->cmd);
  free(cur);
  return rtn;
}

/*
* Remove a background process by its process id
*/
void removeJob_PID(pid_t pid) {

  struct BackgroundJob *cur = head;
  struct BackgroundJob *prev;

  // If list is empty, exit method
  if (cur == NULL) {
    return;
  }
  // If process to be deleted is head move the pointer up
  // and free memory associated with deleted process
  else if (cur->pid == pid) {
    head = cur->next;
    free(cur);
    return;
  }

  // Iterate through the list to find the process to be deleted
  while (cur != NULL && cur->pid != pid) {
    prev = cur;
    cur  = cur->next;
  }

  // If process was not found, exit function
  if (cur == NULL) {
    return;
  }
  // If process was found, delete it from the list by removing all pointers to it
  else {
    prev->next = cur->next;
  }

  // Free memory space associated with the background process
  free(cur);
}

/*
* Parse a command into a list of Strings where
* a) args[0] = command
* b) args[i>0] = parameter
*/
void parseRegularCmd(char* buffer, char *args[]) {

  int i, j;
  char *token;

  for (i = 0; i<20; i++) {
    args[i] = " ";
  }

  for (i = 0; i < 20; i++) {

    token = strsep(&buffer, " ");

    if (token == NULL) {
      break;
    }

    // Null terminate the current String
    for (j = 0; j < strlen(token); j++) {
      if (token[j] <= 32) {
        token[j] = '\0';
      }
    }

    // If current string is non-empty, add it to the list of arguments
    if (strlen(token) > 0) {
      args[i] = token;
    }

    // If the argument is the background indicator, ignore it and end String parsing
    if ((strcmp(args[i], "&")) == 0) {  
      break;
    }
    // If the String is a space character, move the pointer back one to refill the array index
    else if ((strcmp(args[i], " ")) == 0) {
      i--;
    }
  }
  
  // Null terminate the array of Strings
  args[i] = NULL;
}

/*
* Parse the input String into the necessary format based on the type of command
*/
char* parseString(char* buffer, char *args[], char *args2[], int *redirect, int *pipe) {

  char* pipeArgs[2];
  char* redirectArgs[2];
  char* freeable = buffer;

  // Check if command is a redirect command
  if (parseRedirect(buffer, redirectArgs) == 1) {

    // Set redirect flag
    *redirect = 1;
    *pipe     = 0;

    // Parse both sides of the > delimiter
    parseRegularCmd(redirectArgs[0], args);
    parseRegularCmd(redirectArgs[1], args2);
  }
  // Check if command is a pipe command
  else if (parsePipe(buffer, pipeArgs) == 1) {

    // Set pipe flag
    *redirect = 0;
    *pipe     = 1;

    // Parse both sides of the | delimiter
    parseRegularCmd(pipeArgs[0], args);
    parseRegularCmd(pipeArgs[1], args2);
  }
  // Handle as a regular command
  else {

    // Set both pipe and redirect flag to false
    *redirect = 0;
    *pipe     = 0;

    // Parse the command
    parseRegularCmd(buffer, args);
  } 

  return freeable;
}

/*
* Get a command from Standard Input
*/
char* getcmd(char *args[], char *args2[], int *background, int *redirect, 
           int *pipe)
{

  int check, i;
  char *loc;
  char *buffer;
  size_t buffsize = 32;  

  // Allocate space to store input command
  buffer = (char *)malloc(buffsize * sizeof(char));

  // If memory failed to allocate exit the process
  if (buffer == NULL) {
    printf("Unable to allocate memory to buffer.");
    exit(-1);
  }
  
  // Retrieve the line from input
  if (getline(&buffer, &buffsize, stdin) == -1) {
    
    // If getline failed free the memory space before exiting
    free(buffer);
    return 0;
  }

  check = 0;

  // Check that the input String contains at least one valid character
  for (i = 0; buffer[i] != '\0'; i++) {
    if (buffer[i] > 32) {
      check = 1;
      break;
    }
  }

  // If input is empty, return in the loop
  if (check == 0) {
    free(buffer);
    return 0;
  }

  // Check if the background is specified
  if ((loc = strchr(buffer, '&')) != NULL) {
    *background = 1;
  } else {
    *background = 0;
  }

  return parseString(buffer, args, args2, redirect, pipe);
}

/*
* Print working directory
*/
int pwd() {

  char cwd[256];

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    printf("Error printing working directory.");
  }
  printf("%s", cwd);
  return 0;
}

/*
* Print the command line prompty
*/
void printPrompt() {
  
  char cwd[256];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    printf("Error printing working directory.");
  }

  printf("\n%s>", cwd);
}

/*
* Place a Job running in a background process into the foreground of the shell process
*/
int foregroundJob(char *args[]) {

  int ndx;
  pid_t pid;

  // If no index was passed foreground the first job in the process list
  if (args[1] == NULL) {
    
    // Remove job from the background process list
    pid = removeJob_Ndx(0);
    activeChild = pid;

    // Wait for process to complete
    if (pid > 0) {
      waitpid(pid, 0, 0);
    }
    return 0;
  }
  // If an index was passed, foreground the process at the specific index in the process list
  else {

    // Retrieve index from the argument list
    ndx = atoi(args[1]);

    // Remove job from the background process list if it exists
    pid = removeJob_Ndx(ndx);

    activeChild = pid;

    // Wait for process to complete
    if (pid > 0) {
      waitpid(pid, 0, 0);
    }
    return 0;
  }
}

/*
* Execute a command as a built in command if it is one, otherwise do nothing
*/
int builtInCommands(char *args[], int *shellFlag) {

  // If command is exit, cancel the shell loop
  if (strcmp(args[0], "exit") == 0) {
    *shellFlag = 0;
    return 0;
  }

  // Change directory
  if (strcmp(args[0], "cd") == 0) {
    
    if (args[1] == NULL) {
      pwd();
    } 
    else {
      if (chdir(args[1]) != 0) {
        printf("Error changing directory.");
      }
    } 
    return 0;
  }

  // Print the working directory
  if (strcmp(args[0], "pwd") == 0) {
    return pwd();
  }

  // List all background jobs
  if (strcmp(args[0], "jobs") == 0) {
    return printJobs();
  }

  // Foreground a task running in the background
  if (strcmp(args[0], "fg") == 0) {
    return foregroundJob(args);
  }
}

/*
* Execute basic command
*/
void executeArgs(char *args[], int bg, int *shellFlag) {

  // If command is built in execute it in the parent process and continue the shell loop
  if (builtInCommands(args, shellFlag) == 0) {
    return;
  }
  
  // Fork process
  pid_t pid = fork();

  // If fork failed exit the shell as something went wrong
  if (pid < 0) {
    printf("Failed to fork process.");
    exit(0);
  }
  if (pid == 0) { // Currently in child process

    // Execute command
    if (execvp(args[0], args) >= 0) {
      printf("\n Failed to execute the requested command.");
    }
    exit(0);
  }
  else { // Currenty in parent process

    // If background flag set to false wait for the child to finish executing
    if (bg != 1) {
      activeChild = pid;
      waitpid(pid, 0, 0);
    }
    // Otherwise add the process to the list of background jobs
    else {
      addBackgroundJob(pid, args);
    }
  }
}

/*
* Execute a pipe command
*/
int executeArgs_Piped(char *args[], char *args2[]) {

  int i, j;
  int fd[2];
  pid_t pid1, pid2;

  // Execute pipe system call and handle failure
  if (pipe(fd) < 0) {
    printf("Pipe failed to initialize.");
    exit(0);
  }

  // Fork process
  pid1 = fork();

  // Handle fork failure
  if (pid1 < 0) {
    printf("Failed to fork.");
    exit(0);
  }

  if (pid1 == 0) { // Currently in child process 1

    // Set up standard out side of the pipe
    close(STDOUT_FILENO);
    dup2(fd[1], STDOUT_FILENO);
    close(fd[0]);
    close(fd[1]);

    // Execute the command to the left of the pipe
    if (execvp(args[0], args) < 0 ) {
      printf("Failed to execute first command during pipe.");
      exit(0);
    }
    exit(0);
  }
  else {  

    // Fork the process
    pid2 = fork();

    // Handle fork failure
    if (pid2 < 0) {
      printf("Failed to fork.");
      exit(0);
    }

    if (pid2 == 0) { // Currently in child process

      // Set up standard in side of the pipe
      close(STDIN_FILENO); 
      dup2(fd[0], STDIN_FILENO);
      close(fd[1]);
      close(fd[0]);

      // Execute the command to the right of the pipe
      if (execvp(args2[0], args2) < 0 ) {
        printf("Failed to execute second command during pipe.");
        exit(0);
      }
      exit(0);
    }
    else {
      
      // Close pipe so program doesnt hang
      close(fd[0]);
      close(fd[1]);
      // Wait for both processes to complete
      waitpid(pid1, 0, 0);
      waitpid(pid2, 0, 0);
      return 0;
    }
  }
}

/*
* Execute redirect command
*/
void executeArgs_Redirect(char *args[], char *args2[]) {

  int fd;
  pid_t pid;

  // Fork process
  pid = fork();

  // Handle fork failure
  if (pid < 0) {
    printf("Failed to fork.");
    exit(0);
  }

  if (pid == 0) { // Currently in child process

    // Open file and create a connection
    fd = open(args2[0], O_WRONLY | O_TRUNC | O_CREAT, 0644);
    dup2(fd, 1);
    close(fd);

    // Execute command
    if (execvp(args[0], args) >= 0) {
      printf("\n Failed to execute the requested command.");
    }
    exit(0);
  }
  else { // Currently in parent process
    
    // Wait for process to complete
    waitpid(pid, 0, 0);
  }
}

int main(void)
{

  char *args[20], *args2[20];
  char *freeablebuffer;
  int bg, redirect, pipe, i, j;
  int shellFg = 1;
  pid_t pid;

  signal(SIGINT, handle_sigint);
  signal(SIGTSTP, SIG_IGN);
  printf("Welcome to Simple C Shell by Bela Zumbansen {260 865 725}");
  
  while (shellFg) {

    bg = 0;
    pid = waitpid(-1, 0, WNOHANG); 

    if (pid > 0) {
      removeJob_PID(pid);
    }
    
    printPrompt();

    freeablebuffer = getcmd(args, args2, &bg, &redirect, &pipe);
    if (freeablebuffer == 0) {
      continue;
    }
        
    if (redirect == 1) {
      executeArgs_Redirect(args, args2);
    } 
    else if (pipe == 1) {
      executeArgs_Piped(args, args2);
    }
    else {
      executeArgs(args, bg, &shellFg);
    }
    free(freeablebuffer);
  }

  exit(0);  
}
