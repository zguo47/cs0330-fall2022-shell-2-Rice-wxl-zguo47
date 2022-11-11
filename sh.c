#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "./jobs.h"

/*
 * parse()
 *
 * - Description: creates the tokens, argv, and redirect arrays from the buffer
 * character array
 *
 * - Arguments: buffer: a char array representing user input, tokens: the
 * tokenized input, argv: the argument array eventually used for execv(),
 * redirect: the array storing the redirection information
 *
 * - Return: a number indicating whether there is an error when parsing the
 * input. 0 is returned if the parse succeeds, and 1 is returned if there is an
 * error.
 */

int parse(char buffer[1024], char *tokens[512], char *argv[512],
          char *redirect[512]) {
    char *str = buffer;
    char *token;
    // a variable representing the size of argv
    unsigned int i = 0;
    // a variable representing the size of redirect
    unsigned int size_re = 0;

    // tokenize
    while ((token = strtok(str, " \t\n")) != NULL) {
        tokens[i] = token;
        str = NULL;
        i += 1;
    }

    // when there is no user input, and return is hit
    if (tokens[0] == NULL) {
        return 1;
    }

    // put tokens into argv
    for (int j = 0; tokens[j] != NULL; j++) {
        argv[j] = tokens[j];
    }

    // error checkings for cases with nothing after "<", ">", or ">>"
    if (strcmp(argv[i - 1], ">") == 0) {
        fprintf(stderr, "syntax error: no output file\n");
        return 1;
    }

    if (strcmp(argv[i - 1], "<") == 0) {
        fprintf(stderr, "syntax error: no input file\n");
        return 1;
    }

    if (strcmp(argv[i - 1], ">>") == 0) {
        fprintf(stderr, "syntax error: no output file\n");
        return 1;
    }

    // handle the redirections by moving them from argv to redirect
    for (unsigned int k = 0; k < i - 1; k++) {
        if (strcmp(argv[k], ">") == 0 || strcmp(argv[k], "<") == 0 ||
            strcmp(argv[k], ">>") == 0) {
            // adding to redirect
            redirect[size_re] = argv[k];
            redirect[size_re + 1] = argv[k + 1];
            // increasing the total size of redirect by 2
            size_re += 2;
            // removing from argv
            memmove(argv + k, argv + k + 2, 8 * (i - k - 2));
            argv[i - 2] = NULL;
            argv[i - 1] = NULL;
            // decreasing the total size of argv by 2
            i -= 2;
            // k now refers to the next string to be parsed
            k -= 1;
            // handle redirections with no command
            if (i == 0) {
                fprintf(stderr, "error: redirects with no command\n");
                return 1;
            }
        }
    }

    int input_num = 0;
    int output_num = 0;
    // count the number of the same kinds of redirections
    for (unsigned int m = 0; m < size_re; m += 2) {
        if (strcmp(redirect[m], "<") == 0) {
            input_num += 1;
        } else {
            output_num += 1;
        }
    }

    // handle multiple same redirections
    if (input_num >= 2) {
        fprintf(stderr, "syntax error: multiple input files\n");
        return 1;
    } else if (output_num >= 2) {
        fprintf(stderr, "syntax error: multiple output files\n");
        return 1;
    }

    return 0;
}


/*
 * reap()
 *
 * - Description: reap the background processes. When there is a background 
 * process experience a status change (i.e. be suspended), reap will record
 * that change and handle it. 
 *
 * - Arguments: job_list_t *list: a pointer to the shell's job list which 
 * contains all the background processes and suspended foreground processes.
 *
 */
void reap(job_list_t *list) {
    int status_background;
    pid_t pid;
    while ((pid = waitpid(-1, &status_background,
                          WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        // background job suspended
        if (WIFSTOPPED(status_background)) {
            printf("[%d](%d) suspended by signal %d\n", get_job_jid(list, pid),
                   pid, WSTOPSIG(status_background));
            update_job_pid(list, pid, STOPPED);
        }

        // bg job continued
        if (WIFCONTINUED(status_background)) {
            printf("[%d](%d) resumed \n", get_job_jid(list, pid), pid);
            update_job_pid(list, pid, RUNNING);
        }

        // bg job terminated by signal
        if (WIFSIGNALED(status_background)) {
            printf("[%d](%d) terminated by signal %d\n", get_job_jid(list, pid),
                   pid, WTERMSIG(status_background));
            remove_job_pid(list, pid);
        }

        // bg job exit normally
        if (WIFEXITED(status_background)) {
            printf("[%d](%d) terminated with exit status %d\n",
                   get_job_jid(list, pid), pid, WEXITSTATUS(status_background));
            remove_job_pid(list, pid);
        }
    }
}

/*
 * bg()
 *
 * - Description: handle the built in command bg, which will resume the job 
 * associated with the given jid, and runs it in the background. 
 *
 * - Arguments: char *job_id, a pointer to the jid for the job that the user 
 * wants to resume; job_list_t *list, a pointer to the shell's job list which 
 * contains all the background processes and suspended foreground processes.
 *
 */
void bg(char *job_id, job_list_t *list) {
    int jid = atoi(job_id);
    pid_t id = get_job_pid(list, jid);
    // the job exist in the job list.
    if (id != -1) {
        if (kill(-id, SIGCONT) == -1) {
            perror("Kill");
        }
    } else {
        printf("job not found\n");
    }
}

/*
 * fg()
 *
 * - Description: handle the built in command fg, which will resume the job
 * associated with the given jid, and runs it in the foreground. 
 *
 * - Arguments: char *job_id, a pointer to the jid for the job that the user 
 * wants to resume; job_list_t *list, a pointer to the shell's job list which 
 * contains all the background processes and suspended foreground processes.
 *
 */
void fg(char *job_id, job_list_t *list) {
    int jid = atoi(job_id);
    pid_t id = get_job_pid(list, jid);
    // the job exist in the job list
    if (id != -1) {
        // To enable to job to interact with the terminal.
        if (tcsetpgrp(0, id) == -1) {
            perror("tcsetpgrp");
        }
        if (kill(-id, SIGCONT) == -1) {
            perror("Kill");
        }

    } else {
        printf("job not found\n");
    }

    // waitpid() for this process
    int status;
    pid_t pid = waitpid(id, &status, WUNTRACED);
    if (pid != -1) {
        if (WIFSTOPPED(status)) {
            update_job_pid(list, pid, STOPPED);
            printf("[%d](%d) suspended by signal %d\n", get_job_jid(list, pid),
                   pid, WSTOPSIG(status));
        }

        // remove it from job list when terminated by signal.
        if (WIFSIGNALED(status)) {
            printf("[%d](%d) terminated by signal %d\n", get_job_jid(list, id),
                   id, WTERMSIG(status));
            remove_job_pid(list, id);
        }

        // remove it from job list when it exit normally.
        if (WIFEXITED(status)) {
            remove_job_pid(list, id);
        }

        // give back the control to the parent. 
        if (tcsetpgrp(0, getpgrp()) == -1) {
            perror("tcsetpgrp");
        }
    }
}

/*
 * main()
 *
 * - Description: this is the main function where the shell is implemented.
 *
 */

int main() {
    // ignore the signals for the parent process
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    // used for saving the value returned from read
    ssize_t bytesRead;
    // global variable job list
    job_list_t *j_list = init_job_list();
    // get the value of jid
    int size_j = 0;
    // store the child process id
    pid_t curr_child_pid;
    // global variable status
    int status;

    while (1) {
        // reap() everytime to keep track of all the background processes.
        reap(j_list);
#ifdef PROMPT
        if (printf("33sh> ") < 0) {
            fprintf(stderr, "error when printing prompt.\n");
        }
        if (fflush(stdout) < 0) {
            fprintf(stderr, "error when using fflush.\n");
        }
#endif
        // initializing necessary arrays
        char buffer[1024];
        memset(buffer, 0, 1024);

        char *tokens[512];
        memset(tokens, 0, 512 * sizeof(char *));

        char *argv[512];
        memset(argv, 0, 512 * sizeof(char *));

        char *redirect[512];
        memset(redirect, 0, 512 * sizeof(char *));

        char buf[1024];
        memset(buf, 0, 1024);

        // reading from file descripter 0, which stores user input
        if ((bytesRead = read(0, buffer, 1024)) == -1) {
            perror("user input");
            continue;
        } else if (bytesRead == 0) {
            // the case where user inputs nothing and hits Command D
            cleanup_job_list(j_list);
            exit(1);
        } else {
            // handling the error produced by parse()
            if (parse(buffer, tokens, argv, redirect) == 1) {
                continue;
            }
        }

        // get size of argv
        int size_argv = 0;
        for (int j = 0; argv[j] != NULL; j++) {
            size_argv++;
        }

        // command exit
        if (strcmp(argv[0], "exit") == 0) {
            cleanup_job_list(j_list);
            exit(0);
        }

        // command cd
        if (strcmp(argv[0], "cd") == 0) {
            // no file
            if (argv[1] == NULL) {
                fprintf(stderr, "cd: syntax error \n");
            } else {
                // using chdir() system call
                int cd = chdir(
                    strcat(strcat(getcwd(buf, sizeof(buf)), "/"), argv[1]));
                if (cd == -1) {
                    perror("cd");
                }
            }
            continue;
        }

        // command ln
        else if (strcmp(argv[0], "ln") == 0) {
            // no file
            if (argv[1] == NULL || argv[2] == NULL) {
                fprintf(stderr, "ln: syntax error \n");
            } else {
                // using link() system call
                if (link(argv[1], argv[2]) == -1) {
                    perror("ln");
                }
            }
            continue;
        }

        // command rm
        else if (strcmp(argv[0], "rm") == 0) {
            // no file
            if (argv[1] == NULL) {
                fprintf(stderr, "rm: syntax error \n");
            } else {
                // using unlink() system call
                if (unlink(argv[1]) == -1) {
                    perror("rm");
                }
            }
            continue;
        }

        // command jobs
        else if (strcmp(argv[0], "jobs") == 0) {
            jobs(j_list);
            continue;
        }

        // command bg
        else if (strcmp(argv[0], "bg") == 0) {
            char *second_arg = argv[1];
            // check if second argument contains %
            if (second_arg[0] == 37) {
                bg(&second_arg[1], j_list);
            } else {
                fprintf(stderr, "bg: syntax error \n");
            }
            continue;
        }

        // command fg
        else if (strcmp(argv[0], "fg") == 0) {
            char *second_arg = argv[1];
            // check if second argument contains %
            if (second_arg[0] == 37) {
                fg(&second_arg[1], j_list);
            } else {
                fprintf(stderr, "fg: syntax error \n");
            }
            continue;
        }

        // get the pid of the child.
        curr_child_pid = fork();

        // identify a background process and add it to the job list.
        if (strcmp(argv[size_argv - 1], "&") == 0) {
            size_j += 1;
            add_job(j_list, size_j, curr_child_pid, RUNNING, argv[0]);
        }
        // child process
        if (curr_child_pid == 0) {
            if (setpgid(getpid(), getpid()) == -1) {
                perror("setpgid");
                exit(1);
            }

            // print the jid and pid of the background process
            if (strcmp(argv[size_argv - 1], "&") == 0) {
                argv[size_argv - 1] = NULL;
                printf("[%d](%d)\n", size_j, getpid());

            // if it is a foreground process, enable it to interact with terminal
            } else {
                if (tcsetpgrp(0, getpgrp()) == -1) {
                    perror("tcsetpgrp");
                    exit(1);
                }
            }

            // set the signal handler to react with default behavior.
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            // handling redirection. Note that the maximum size of
            // redirect is 4.
            for (int i = 0; i < 4; i += 2) {
                if (redirect[i] != NULL) {
                    // input file redirection
                    if (strcmp(redirect[i], "<") == 0) {
                        // close fd 0
                        if (close(0) == -1) {
                            perror("close");
                            exit(1);
                        }
                        // set flags to read only
                        int fd_0 = open(redirect[i + 1], O_RDONLY);
                        if (fd_0 == -1) {
                            perror("open");
                            exit(1);
                        }
                    }
                    // output file redirection with ">"
                    else if (strcmp(redirect[i], ">") == 0) {
                        // close fd 1
                        if (close(1) == -1) {
                            perror("close");
                            exit(1);
                        }
                        // set flags to write only, create, and truncate
                        int fd_1 = open(redirect[i + 1],
                                        O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        if (fd_1 == -1) {
                            perror("open");
                            exit(1);
                        }
                    }
                    // output file redirection with ">>"
                    else {
                        // close fd 1
                        if (close(1) == -1) {
                            perror("close");
                            exit(1);
                        }
                        // set flags to write only, create, and append
                        int fd_2 = open(redirect[i + 1],
                                        O_WRONLY | O_CREAT | O_APPEND, 0666);
                        if (fd_2 == -1) {
                            perror("open");
                            exit(1);
                        }
                    }
                }
            }

            // handling all other commands using execv() system call
            char *full_name = argv[0];
            // extracting the binary name
            if (strrchr(argv[0], '/') != NULL) {
                argv[0] = strrchr(argv[0], '/') + 1;
            }
            execv(full_name, argv);
            perror("execv");
            exit(1);
        }

        // wait for the foreground process
        if (strcmp(argv[size_argv - 1], "&") != 0) {
            pid_t pid = waitpid(curr_child_pid, &status, WUNTRACED);
            if (pid == -1) {
                perror("wait");
                continue;
            } else {
                // foreground process suspended
                if (WIFSTOPPED(status)) {
                    size_j += 1;
                    add_job(j_list, size_j, pid, STOPPED, argv[0]);
                    printf("[%d](%d) suspended by signal %d\n", size_j, pid,
                           WSTOPSIG(status));
                }
                // foreground process terminated by signal
                if (WIFSIGNALED(status)) {
                    printf("[%d](%d) terminated by signal %d\n", size_j + 1,
                           pid, WTERMSIG(status));
                }
            }

            // give back the control to parent
            if (tcsetpgrp(0, getpgrp()) == -1) {
                perror("tcsetpgrp");
                continue;
            }
        }
    }
    // cleanup job list before exit
    cleanup_job_list(j_list);
    return 0;
}
