#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#include "include/jail.h"
#include "include/str.h"
#include "include/process.h"

#define BUFFERSIZE 10000

struct cmdQueue {
    int rfd;
    int pipeN;
};

struct cmdQueue cmdq[1000];

int pipeNumberTable[1000][2];

    void
prompt()
{
    if ((write(STDOUT_FILENO, "% ", 2)) < 0)
        perror("write prompt");
}

    void
welcome_message()
{
    const char msg[3][42] = 
    {
        {"****************************************\r\n"},
        {"** Welcome to the information server. **\r\n"},
        {"****************************************\r\n"}
    };
    if ((write(STDOUT_FILENO, msg, 126)) < 0)
        perror("write prompt");
}

void
unknown_command(char *cmd)
{
    char msg[256] = {0};

    snprintf(msg, 256, "Unknown command: [%s].\n", cmd);
    if ((write(STDOUT_FILENO, msg, strlen(msg))) < 0)
        perror("write prompt");
}

    int
environment_command(char *cmd[])
{
    char    msg[256] = {0};
    int     flag = 0;

    if (strncmp(cmd[0], "exit", 4) == 0)
        flag = 1;
    else if (strncmp(cmd[0], "setenv", 6) == 0) {
        setenv(cmd[1], cmd[2], 1);
        flag = 2;
    }
    else if (strncmp(cmd[0], "printenv", 8) == 0) {
        snprintf(msg, BUFFERSIZE, "PATH=%s\n", getenv(cmd[1]));
        write(STDIN_FILENO, msg, strlen(msg));
        flag = 2;
    }
    return flag;
}

    int
find_empty_entry()
{
    for (int i = 0; i < 1000; i++) {
        if (pipeNumberTable[i][1] == 0)
            return i;
    }
    return -1;
}

    int
pip_number(int pipeN)
{
    int pipefd[2];
    int index = -1;

    if ((pipe(pipefd)) == -1) {
        perror("ERROR on pipe.\n");
        return -1;
    } 

    index = find_empty_entry();
    pipeNumberTable[index][0] = pipefd[0];
    if (pipeN == 0)
        pipeNumberTable[index][1] = 2;
    else
        pipeNumberTable[index][1] = pipeN + 1;

    return pipefd[1];
}

    int
get_in_fd()
{
    int pipefd[2];
    char buffer[BUFFERSIZE] = {'\0'};

    if ((pipe(pipefd)) == -1) {
        perror("ERROR on pipe.\n");
        return -1;
    }

    for (int i = 0; i < 1000; i++) {
        if (pipeNumberTable[i][1] > 0) {
            if (pipeNumberTable[i][1] == 1) {
                read(pipeNumberTable[i][0], buffer, BUFFERSIZE);
                close(pipeNumberTable[i][0]);
                pipeNumberTable[i][0] = 0;
                write(pipefd[1], buffer, strlen(buffer));
            }
            pipeNumberTable[i][1]--;
        }
        memset(buffer, '\0', BUFFERSIZE);
    }
    close(pipefd[1]);
    return pipefd[0];
}

    int
process_cmd(const char* cmd)
{
    int     status = 0, flag;
    int     tok_count = 0, argc = 0;
    char    **cmd_split;
    char    *argv[256] = {0};
    int     rfd, wfd, efd;
    int     pipeN = 0;
    FILE    *fp = 0;

    cmd_split = strtok_bychar(cmd, ' ', &tok_count);

    status = environment_command(cmd_split);
    if (status == 1)
        return 1;
    else if (status == 2)
        return 0;

    for (int i = 0; i < tok_count; ++i) {
        flag = 0;
        rfd = STDIN_FILENO;
        wfd = STDOUT_FILENO;
        efd = STDERR_FILENO;

        switch (cmd_split[i][0]) {
            case '|':
                pipeN = atoi(&cmd_split[i][1]);
                wfd = pip_number(pipeN);
                flag = 1;
                break;
            case '>':
                fp = fopen(cmd_split[++i], "w");
                wfd = fileno(fp);
                flag = 2;
                break;
            default:
                argv[argc++] = strdup(cmd_split[i]);
                break;
        }

        if ((flag != 0) || (i == tok_count - 1)) {
            rfd = get_in_fd();
            status = process(argv, rfd, wfd, efd, 1);
            if (status == 255) {
                unknown_command(argv[0]);
                break;
            }
            /* reset argv, argc */
            free_2Darray(argv, argc);
            argc = 0;
            /* close file descriptor */
            if (rfd != STDIN_FILENO)
                close(rfd);
            if (wfd != STDOUT_FILENO)
                close(wfd);
            if (flag == 2)
                fclose(fp);
        }
    }
    free_2Darray(cmd_split, tok_count);
    free(cmd_split);
    return 0;
}

    int
main()
{
    char    buf[10000] = {0};
    int     status = 0;

    jail("ras_dir");
    setenv("PATH", "bin:.", 1);

    welcome_message();

    for (;;) {
        memset(buf, 0, BUFFERSIZE);

        prompt();

        readLine(STDIN_FILENO, buf, BUFFERSIZE);

        /* echo server */
        /* write(STDOUT_FILENO, buf, strlen(buf));
           write(STDOUT_FILENO, "\n", 1); */

        status = process_cmd(buf);
        if (status) break;
    }
}
