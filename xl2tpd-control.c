/*
 * Layer Two Tunnelling Protocol Daemon Control Utility
 * Copyright (C) 2011 Alexander Dorokhov
 *
 * This software is distributed under the terms
 * of the GPL, which you should have received
 * along with this source.
 *
 * xl2tpd-control client main file
 *
 */
 
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "l2tp.h"

/* Paul: Alex: can we change this to use stdout, and let applications using
 * xl2tpd-control capture the output, instead of creating tmp files?
 */
/* result filename format including absolute path and formatting %i for pid */
#define RESULT_FILENAME_FORMAT "/var/run/xl2tpd/xl2tpd-control-%i.out"

#define ERROR_LEVEL 1
#define DEBUG_LEVEL 2

#define TUNNEL_REQUIRED 1
#define TUNNEL_NOT_REQUIRED 0

int log_level = ERROR_LEVEL;

void print_error (int level, const char *fmt, ...);

int read_result(int result_fd, char* buf, ssize_t size);

/* Definition of a command */
struct command_t
{
    char *name;
    int (*handler) (int socket_fd, char* tunnel, int optc, char *optv[]);
    int requires_tunnel;
};

int command_add_lac (int, char* tunnel, int optc, char *optv[]);
int command_connect_lac (int, char* tunnel, int optc, char *optv[]);
int command_disconnect_lac (int, char* tunnel, int optc, char *optv[]);
int command_remove_lac (int, char* tunnel, int optc, char *optv[]);
int command_add_lns (int, char* tunnel, int optc, char *optv[]);
int command_status_lac (int, char* tunnel, int optc, char *optv[]);
int command_status_lns (int, char* tunnel, int optc, char *optv[]);
int command_remove_lns (int, char* tunnel, int optc, char *optv[]);
int command_available (int, char* tunnel, int optc, char *optv[]);

struct command_t commands[] = {
    /* Keep this command mapping for backwards compat */
    {"add", &command_add_lac, TUNNEL_REQUIRED},
    {"connect", &command_connect_lac, TUNNEL_REQUIRED},
    {"disconnect", &command_disconnect_lac, TUNNEL_REQUIRED},
    {"remove", &command_remove_lac, TUNNEL_REQUIRED},

    /* LAC commands */
    {"add-lac", &command_add_lac, TUNNEL_REQUIRED},
    {"connect-lac", &command_connect_lac, TUNNEL_REQUIRED},
    {"disconnect-lac", &command_disconnect_lac, TUNNEL_REQUIRED},
    {"remove-lac", &command_remove_lac, TUNNEL_REQUIRED},

    /* LNS commands */
    {"add-lns", &command_add_lns, TUNNEL_REQUIRED},
    {"remove-lns", &command_remove_lns, TUNNEL_REQUIRED},

    /* Generic commands */
    {"status", &command_status_lac, TUNNEL_REQUIRED},
    {"status-lns", &command_status_lns, TUNNEL_REQUIRED},
    {"available", &command_available, TUNNEL_NOT_REQUIRED},
    {NULL, NULL}
};

void usage()
{
    printf ("\nxl2tpd server version %s\n", SERVER_VERSION);
    printf ("Usage: xl2tpd-control [-c <PATH>] <command> <tunnel name> [<COMMAND OPTIONS>]\n"
            "\n"
            "    -c\tspecifies xl2tpd control file\n"
            "    -d\tspecify xl2tpd-control to run in debug mode\n"
            "--help\tshows extended help\n"
            "Available commands: add, connect, disconnect, remove, add-lns\n"
    );
}

void help()
{
    usage();
    printf (
        "\n"
        "Commands help:\n"
        "\tadd\tadds new or modify existing lac configuration.\n"
        "\t\tConfiguration must be specified as command options in\n"
        "\t\t<key>=<value> pairs format.\n"
        "\t\tSee available options in xl2tpd.conf(5)\n"
        "\tconnect\ttries to activate the tunnel.\n"
        "\t\tUsername and secret for the tunnel can be passed as\n"
        "\t\tcommand options.\n"
        "\tdisconnect\tdisconnects the tunnel.\n"
        "\tremove\tremoves lac configuration from xl2tpd.\n"
        "\t\txl2tpd disconnects the tunnel before removing.\n"
        "\n"
        "\tadd-lns\tadds new or modify existing lns configuration.\n"
        "See xl2tpd-control man page for more help\n"
    );
}

int main (int argc, char *argv[])
{
    char* control_filename = NULL;
    char* tunnel_name = NULL;
    struct command_t* command = NULL;    
    int i; /* argv iterator */

    if (argv[1] && !strncmp (argv[1], "--help", 6))
    {
        help();
        return 0;
    }
    /* parse global options */
    for (i = 1; i < argc; i++)
    {

        if (!strncmp (argv[i], "-c", 2))
        {
            control_filename = argv[++i];
        } else if (!strncmp (argv[i], "-d", 2)) {
            log_level = DEBUG_LEVEL;
        } else {
            break;
        }
    }
    if (i >= argc)
    {
        print_error (ERROR_LEVEL, "error: command not specified\n");
        usage();
        return -1;
    }
    if (!control_filename)
    {
        control_filename = strdup (CONTROL_PIPE);
    }
    print_error (DEBUG_LEVEL, "set control filename to %s\n", control_filename);    

    /* parse command name */
    for (command = commands; command->name; command++)
    {
        if (!strcasecmp (argv[i], command->name))
        {
            i++;
            break;
        }
    }
    
    if (command->name)
    {
        print_error (DEBUG_LEVEL, "get command %s\n", command->name);
    } else {
        print_error (ERROR_LEVEL, "error: no such command %s\n", argv[i]);
        return -1;
    }
    
    /* get tunnel name */
    if(command->requires_tunnel){
        if (i >= argc)
        {
            print_error (ERROR_LEVEL, "error: tunnel name not specified\n");
            usage();
            return -1;
        }
        tunnel_name = argv[i++];    
        /* check tunnel name for whitespaces */
        if (strstr (tunnel_name, " "))
        {
            print_error (ERROR_LEVEL,
                "error: tunnel name shouldn't include spaces\n");
            usage();        
            return -1;
        }
    }
   
    int ctl_socket;
    struct sockaddr_un ctl_socket_addr;
    if((ctl_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        //error
    }

    ctl_socket_addr.sun_family = AF_UNIX;
    strcpy(ctl_socket_addr.sun_path, "socket1");
    if (connect(ctl_socket, (struct sockaddr *)&ctl_socket_addr, sizeof(ctl_socket_addr.sun_family) + strlen(ctl_socket_addr.sun_path)) == -1) {
        //error
    }

    char buf[CONTROL_PIPE_MESSAGE_SIZE] = "";
    int command_res = command->handler (
        ctl_socket, tunnel_name, argc - i, argv + i
    );

    if (command_res < 0)
    {
        print_error (ERROR_LEVEL, "error: command parse error\n");
        return -1;
    }

    char response_buf[CONTROL_PIPE_MESSAGE_SIZE];
    int response_len;
    for(;;){
        if((response_len = read_result(ctl_socket, &response_buf, CONTROL_PIPE_MESSAGE_SIZE)) > 0){
            printf("%s", response_buf);
        }else{
            break;
        }
    }

    close(ctl_socket); 
    return 0;
}

void print_error (int level, const char *fmt, ...)
{
    if (level > log_level) return;
    va_list args;
    va_start (args, fmt);
    vfprintf (stderr, fmt, args);
    va_end (args);
}

int read_result(int result_fd, char* buf, ssize_t size)
{
    return recv(result_fd, buf, size, 0);
}


int write_request(int socket_fd, const char *fmt, ...)
{
    va_list args;
    char buf[1024];

    va_start (args, fmt);
    vsprintf(buf, fmt, args);
    va_end (args);

    // Send this to the socket
    if (send(socket_fd, buf, strlen(buf), 0) < 0) {
        printf("send() failed\n");
        perror("send");
        //error
    }
    return 0;
}


int command_add
(int socket_fd, char* tunnel, int optc, char *optv[], int reqopt)
{
    if (optc <= 0)
    {
        print_error (ERROR_LEVEL, "error: tunnel configuration expected\n");
        return -1;
    }

    char buf[1024];
    sprintf (buf, "%c %s ", reqopt, tunnel);
    int i;
    int wait_key = 1;
    for (i = 0; i < optc; i++)
    {
        sprintf (buf, "%s", optv[i]);
        if (wait_key)
        {
            /* try to find '=' */
            char* eqv = strstr (optv[i], "=");
            if (eqv)
            {
                /* check is it not last symbol */
                if (eqv != (optv[i] + strlen(optv[i]) - 1))
                {
                    sprintf (buf, ";"); /* end up option */
                } else {
                    wait_key = 0; /* now we waiting for value */
                }
            } else { /* two-word key */
                sprintf(buf, " "); /* restore space */
            }
        } else {
            sprintf(buf, ";"); /* end up option */        
            wait_key = 1; /* now we again waiting for key */
        }
    }

    write_request(socket_fd, "%s", buf);
    return 0;
}

int command_add_lac
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    return command_add(socket_fd, tunnel, optc, optv, CONTROL_PIPE_REQ_LAC_ADD_MODIFY);
}

int command_add_lns
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    return command_add(socket_fd, tunnel, optc, optv, CONTROL_PIPE_REQ_LNS_ADD_MODIFY);
}


int command_connect_lac
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    /*fprintf (mesf, "%c %s", CONTROL_PIPE_REQ_LAC_CONNECT, tunnel);
    if (optc > 0) {
        if (optc == 1)
            fprintf (mesf, " %s", optv[0]);
        else // optc >= 2
            fprintf (mesf, " %s %s", optv[0], optv[1]);
    }
    */
    return 0;
}

int command_disconnect_lac
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    return write_request(socket_fd, "%c %s", CONTROL_PIPE_REQ_LAC_DISCONNECT, tunnel);
}

int command_remove_lac
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    return write_request(socket_fd, "%c %s", CONTROL_PIPE_REQ_LAC_REMOVE, tunnel);
}

int command_status_lns
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    return write_request(socket_fd, "%c %s", CONTROL_PIPE_REQ_LNS_STATUS, tunnel);
}

int command_status_lac
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    return write_request(socket_fd, "%c %s", CONTROL_PIPE_REQ_LAC_STATUS, tunnel);
}

int command_available
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    return write_request(socket_fd, "%c %s", CONTROL_PIPE_REQ_AVAILABLE, tunnel);
}

int command_remove_lns
(int socket_fd, char* tunnel, int optc, char *optv[])
{
    return write_request(socket_fd, "%c %s", CONTROL_PIPE_REQ_LNS_REMOVE, tunnel);
}

