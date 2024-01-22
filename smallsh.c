// Include necessary header files
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

// Command lines are allowed to have a maximum of 2048 characters
#define CLLENGTH 2048

// The variable backgroundOn indicates whether background mode is on or off.  It is initialized to 1, which indicates that background mode is initially on.
int backgroundOn = 1;

// Function prototypes
void parseUserInput(char*[], int*, char[], char[], int);
void executeCommand(char*[], int*, struct sigaction, int*, char[], char[]);
void checkSIGTSTP(int);
void displayExit(int);

// The function parseUserInput gets the input and parses it
void parseUserInput(char* arr1[], int* backGround, char inName[], char outName[], int pid) {
	char input[CLLENGTH];
	int a;
	int b;

	// Get input
	printf(": ");
	fflush(stdout);
	fgets(input, CLLENGTH, stdin);

	// Remove newline character
	int found = 0;
	for (a=0; !found && a<CLLENGTH; a++) {
		if (input[a] == '\n') {
			input[a] = '\0';
			found = 1;
		}
	}

	// If the input is blank, return a blank character
	if (!strcmp(input, "")) {
		arr1[0] = strdup("");
		return;
	}

	// The function strtok parses the input into multiple strings
	const char space[2] = " ";
	char *token = strtok(input, space);

	for (a=0; token; a++) {
		// If the string contains an "&", there is a background process
		if (!strcmp(token, "&")) {
			*backGround = 1;
		}
		// If the string contains an "<", the file is an input file
		else if (!strcmp(token, "<")) {
			token = strtok(NULL, space);
			strcpy(inName, token);
		}
		// If the string contains an ">", the file is an output file
		else if (!strcmp(token, ">")) {
			token = strtok(NULL, space);
			strcpy(outName, token);
		}
		// The remainder of the string is the command
		else {
			arr1[a] = strdup(token);
			// This for loop replaces $$ with the pid
			for (b=0; arr1[a][b]; b++) {
				if (arr1[a][b] == '$' &&
					 arr1[a][b+1] == '$') {
					arr1[a][b] = '\0';
					snprintf(arr1[a], 256, "%s%d", arr1[a], pid);
				}
			}
		}
		// Process the next token in the input string
		token = strtok(NULL, space);
	}
}

// This function executes the command that is passed into the arr[] parameter (array)
void executeCommand(char* arr2[], int* statusOrSignal, struct sigaction sa, int* backGround, char inName[], char outName[]) {
	int input;
	int output;
	int result;
	pid_t spawnPid = -5;

	// The function fork() forks off a child when the program receives a command that is not built in
	spawnPid = fork();
	switch (spawnPid) {
		case -1:	;
			perror("Error!\n");
			exit(1);
			break;

		case 0:	;
			// ^C is now the default
			sa.sa_handler = SIG_DFL;
			sigaction(SIGINT, &sa, NULL);

			// Open file for input
			if (strcmp(inName, "")) {
				input = open(inName, O_RDONLY);
				if (input == -1) {
					perror("Cannot open file for input\n");
					exit(1);
				}
				// Assign input file
				result = dup2(input, 0);
				if (result == -1) {
					perror("Cannot assign input file\n");
					exit(2);
				}
				// Close input file
				fcntl(input, F_SETFD, FD_CLOEXEC);
			}

			// Open output file
			if (strcmp(outName, "")) {
				output = open(outName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (output == -1) {
					perror("Cannot open file for output\n");
					exit(1);
				}
				// Assign output file
				result = dup2(output, 1);
				if (result == -1) {
					perror("Cannot assign output file\n");
					exit(2);
				}
				// Close output file
				fcntl(output, F_SETFD, FD_CLOEXEC);
			}

			// If a file name or directory name is not found, print an error message
			if (execvp(arr2[0], (char* const*)arr2)) {
				printf("%s: no such file or directory\n", arr2[0]);
				fflush(stdout);
				exit(2);
			}
			break;

		default:	;
			// If background mode is on (foreground-only mode is off), execute a process in the background
			if (*backGround && backgroundOn) {
				pid_t actualPid = waitpid(spawnPid, statusOrSignal, WNOHANG);
				printf("background pid is %d\n", spawnPid);
				fflush(stdout);
			}
			// Otherwise terminate the signal
			else {
				pid_t actualPid = waitpid(spawnPid, statusOrSignal, 0);
                if(WIFSIGNALED(*statusOrSignal)){
                    printf("terminated by signal %d\n", *statusOrSignal);
                    fflush(stdout);
                }
			}

		// Display the exit status or signal responsible for termination
		while ((spawnPid = waitpid(-1, statusOrSignal, WNOHANG)) > 0) {
			displayExit(*statusOrSignal);
			fflush(stdout);
		}
	}
}

// This function receives a SIGTSTP signal, and then either enters or exits foreground-only mode
void checkSIGTSTP(int sig) {
	// If commands are currently allowed in the background, enter foreground-only mode
	if (backgroundOn == 1) {
		char* msg = "\nEntering foreground-only mode (& is now ignored)\n";
		write(1, msg, 49);
		fflush(stdout);
		backgroundOn = 0;
	}

	// If commands are currently not allowed in the background, exit foreground-only mode
	else {
		char* msg = "\nExiting foreground-only mode\n";
		write (1, msg, 29);
		fflush(stdout);
		backgroundOn = 1;
	}
}

// The exit function displays the exit value or signal responsible for termination when a command fails
void displayExit(int statusOrSignal) {
	if (WIFEXITED(statusOrSignal)) {
		printf("exit value %d\n", WEXITSTATUS(statusOrSignal));
	} else {
		printf("terminated by signal %d\n", WTERMSIG(statusOrSignal));
	}
}

int main() {
    // Define necessary variables and arrays
	int backGround = 0;
	int exitStatus = 0;
	int cont = 1;
	int pid = getpid();
	int a;

	char inFile[256] = "";
	char outFile[256] = "";
	char* input[512];

	for (a=0; a<512; a++) {
		input[a] = NULL;
	}

	// Ignore the signal ^C
	struct sigaction sa_sigint = {0};
	sa_sigint.sa_handler = SIG_IGN;
	sigfillset(&sa_sigint.sa_mask);
	sa_sigint.sa_flags = 0;
	sigaction(SIGINT, &sa_sigint, NULL);

	// Use the function checkSIGTSTP() for the signal ^Z
	struct sigaction sa_sigtstp = {0};
	sa_sigtstp.sa_handler = checkSIGTSTP;
	sigfillset(&sa_sigtstp.sa_mask);
	sa_sigtstp.sa_flags = 0;
	sigaction(SIGTSTP, &sa_sigtstp, NULL);

	do {
		// Get the input and parse it
		parseUserInput(input, &backGround, inFile, outFile, pid);

		// Ignore comments and blank lines
		if (input[0][0] == '#' ||
					input[0][0] == '\0') {
			continue;
		}

		// If the user types "exit", exit the program
		else if (strcmp(input[0], "exit") == 0) {
			cont = 0;
		}

		// Change directory
		else if (strcmp(input[0], "cd") == 0) {
			if (input[1]) {
                // Print error message if directory is not found
				if (chdir(input[1]) == -1) {
					printf("Directory not found.\n");
					fflush(stdout);
				}
			} else {
			// If directory is not specified, the default directory is "HOME"
				chdir(getenv("HOME"));
			}
		}

		// If the exit status is 0, call the function displayExit to print the exit value or signal responsible for termination, but do not exit the program
		else if (strcmp(input[0], "status") == 0) {
			displayExit(exitStatus);
		}

		// Execute a command
		else {
			executeCommand(input, &exitStatus, sa_sigint, &backGround, inFile, outFile);
		}

		// Reset the input array, the first element of the inputFile and outputFile arrays, and the variable background
		for (a=0; input[a]; a++) {
			input[a] = NULL;
		}
		backGround = 0;
		inFile[0] = '\0';
		outFile[0] = '\0';

	} while (cont);

	return 0;
}