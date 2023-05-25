
/********************************************************************************************
This is a template for assignment on writing a custom Shell.

Students may change the return types and arguments of the functions given in this template,
but do not change the names of these functions.

Though use of any extra functions is not recommended, students may use new functions if they need to,
but that should not make code unnecessorily complex to read.

Students should keep names of declared variable (and any new functions) self explanatory,
and add proper comments for every logical step.

Students need to be careful while forking a new process (no unnecessory process creations)
or while inserting the single handler code (should be added at the correct places).

Finally, keep your filename as myshell.c, do not change this name (not even myshell.cpp,
as you not need to use any features for this assignment that are supported by C++ but not by C).

Name - Khushi Dave
Roll No - BT20CSE031
OS - Assignment 1
*********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>	  // to use exit() function
#include <unistd.h>	  // to use fork(), getpid(), exec() functions
#include <sys/wait.h> // to use wait() function
#include <signal.h>	  // to use signal() function
#include <fcntl.h>	  // to use close(), open() functions

// for type of commands
enum type_of_command
{
	SINGLE,
	SEQUENTIAL,
	PARALLEL,
	REDIRECTION
};

// colors can be used to color terminal like real
void cyan()
{
	printf("\e[1;36m");
}

void red()
{
	printf("\033[1;31m");
}

void yellow()
{
	printf("\033[1;33m");
}

void reset()
{
	printf("\033[0m");
}

// but using colors autograder was showing errors so commented out

// This function will parse the input string into multiple commands or a single command depending on the delimiter (&&, ##, >, or spaces).
void parseInput(char *command_line, char **command_words)
{

	int i = 0;
	char *str;
	int flag = 1;
	while ((str = strsep(&command_line, " ")) != NULL) // strsep to separate the inputs  by space delimiter
	{
		if (strlen(str) == 0) // for handling the null string
		{
			continue;
		}
		command_words[i] = str;
		i++;
	}
	command_words[i] = NULL;
}

void cd_command(char **command_words)
{

	int result;
	if (command_words[1] == NULL)
			result = chdir(getenv("HOME"));
		else
			result = chdir(command_words[1]);
	if (command_words[2] != NULL) // handling case of too many arguments
	{
		// red();
		printf("shell: cd: too many arguments\n");
		// reset();
		return;
	}
	if (result == -1)
	{
		// red();
		printf("shell: cd: %s: No such file or directory\n", command_words[1]); // when no such path exists
																				// reset();
	}
}

// This function will fork a new process to execute a command.
void executeCommand(char **command_words)
{

	if (strcmp(command_words[0], "cd") == 0)
	{
		cd_command(command_words); // To change the current working Directory.
	}
	else if (strcmp(command_words[0], "exit") == 0)
	{
		// yellow();

		printf("Exiting shell...\n");
		// reset();
		exit(1);
	}
	else
	{
		size_t pid;
		int status;
		pid = fork();
		if (pid < 0)
		{
			// red();   // colors can be used for real appearance
			// Error in forking.
			printf("Fork failed");
			// reset();
			exit(1);
		}
		else if (pid == 0)
		{
			// Child Process.
			signal(SIGINT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);
			if (execvp(command_words[0], command_words) < 0) // in case of wrong command.
			{
				// red();
				printf("Shell: Incorrect command\n");
				// reset();
				exit(1);
			}
		}
		else
		{
			waitpid(pid, &status, WUNTRACED); // wait system call works even if the child has stopped
		}
	}
}

//  This function will run multiple commands in parallel.
void executeParallelCommands(char **command_words)
{
	pid_t pid, pidt;
	int status = 0;
	int i = 0;
	int earlier_index = i;
	while (command_words[i] != NULL)
	{
		while (command_words[i] != NULL && strcmp(command_words[i], "&&") != 0)
		{
			i++;
		}
		command_words[i] = NULL;
		pid = fork();
		if (pid < 0)
		{
			exit(1);
		}
		if (pid == 0)
		{
			// child process.
			if (execvp(command_words[earlier_index], &command_words[earlier_index]) < 0)
			{
				// red();
				printf("Shell:Incorrect command\n");
				// reset();
				exit(1);
			}
		}
		else
		{
			waitpid(pid, &status, WUNTRACED); // wait system call works even if the child has stopped
		}
		i++;
		earlier_index = i;
	}
}

//  This function will run multiple commands in a sequence.
void executeSequentialCommands(char **command_words)
{
	size_t command_size = 64;
	char **command;
	command = malloc(command_size * sizeof(char *));
	int i = 0;
	while ((i == 0 && command_words[i] != NULL) || (command_words[i - 1] != NULL && command_words[i] != NULL))
	{
		int j = i, pos = 0;
		while (command_words[j] != NULL && strcmp(command_words[j], "##") != 0)
		{
			command[pos] = command_words[j];
			pos++;
			j++;
		}
		command[pos] = NULL;
		executeCommand(&command[0]);
		i = j + 1;
	}
}

void executeCommandRedirection(char **command_words)
{
	pid_t pid;
	int status = 0;
	int i = 0;
	while (command_words[i] != NULL)
	{
		while (command_words[i] != NULL && strcmp(command_words[i], ">") != 0)
		{
			i++;
		}
		command_words[i] = NULL;
		pid = fork();
		if (pid < 0)
		{
			// red();
			printf("Fork failed");
			// reset();
			exit(1);
		}
		if (pid == 0)
		{
			signal(SIGINT, SIG_DFL);											// enable SIGINT
			signal(SIGTSTP, SIG_DFL);											// enable SIGTSTP
			close(STDOUT_FILENO);												// close the standard output
			open(command_words[i + 1], O_CREAT | O_WRONLY | O_APPEND, S_IRWXU); // open the requested file

			// child process.
			if (execvp(command_words[0], command_words) < 0) // in case of wrong command.
			{
				// red();
				printf("Shell: Incorrect command\n");
				// reset();
				exit(1);
			}
		}
		else
		{
			waitpid(pid, &status, WUNTRACED); // this version of wait system call works even if the child has stopped
		}
	}
}

int main()
{
	// Initial Declarations
	char *command_line;
	size_t cmdsize = 64;
	command_line = (char *)malloc(cmdsize * sizeof(char));

	signal(SIGINT, SIG_IGN); // Used in order to ignore both Ctrl+C(SIGINT) and Ctrl+Z(SIGKILL) in the shell
	signal(SIGTSTP, SIG_IGN);

	while (1) // This loop will keep your shell running until user enters exit i.e will create infinite loop.
	{
		// Print the prompt in format - currentWorkingDirectory$
		char dir[1000];
		// cyan();  // colors arent working in autograd hence commented out
		printf("%s$", getcwd(dir, 1000));
		// reset();

		// Taking Input.

		getline(&command_line, &cmdsize, stdin); // using getline function to take input.

		int len = strlen(command_line);
		command_line[len - 1] = '\0'; // getline is terminated by '\n' so replace it with'\0'.

		if (strcmp(command_line, "") == 0) // When user enters just enter button.
		{
			continue; // we do nothing and just continue.
		}

		char **command_words;
		command_words = malloc(cmdsize * sizeof(char *));

		// Parse input with 'strsep()' for different symbols (&&, ##, >) and for spaces.
		parseInput(command_line, command_words);

		if (strcmp(command_words[0], "exit") == 0) // When user enters exit command.
		{
			// yellow();
			printf("Exiting shell...\n");
			// reset();
			break;
		}

		int i = 0;
		int type_of_command = SINGLE;	 // enumeration for checking type of process
		while (command_words[i] != NULL) // checking for parallel and sequential processes and redirection
		{
			if (strcmp(command_words[i], "&&") == 0)
			{
				type_of_command = PARALLEL;
				break;
			}
			if (strcmp(command_words[i], "##") == 0)
			{
				type_of_command = SEQUENTIAL;
				break;
			}
			if (strcmp(command_words[i], ">") == 0)
			{
				type_of_command = REDIRECTION;
				break;
			}
			i++;
		}

		if (type_of_command == PARALLEL)
			executeParallelCommands(command_words); // user wants to run multiple commands in parallel (commands separated by &&).
		else if (type_of_command == SEQUENTIAL)
			executeSequentialCommands(command_words); // user wants to run multiple commands sequentially (commands separated by ##).
		else if (type_of_command == REDIRECTION)
			executeCommandRedirection(command_words); // user wants redirect output of a single command to and output file specificed by user.
		else
			executeCommand(command_words); // user wants to run a single commands.
	}
}
