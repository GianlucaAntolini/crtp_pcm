#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

// CONSTANTS
#define SERVER_PORT 49200
#define SERVER_IP "10.10.10.100"


// recv wrapper
// parameters: socket descriptor (with open TCP connection), buffer to store data, n of bytes to read from socket
// returns n of bytes read : >0 if no error, 0 if connection closed, <0 if error
static inline int recv_edit(const int sd, char *ret_buff, const int sz) {
    int n = 0, rcvd = 0;
    while (n < sz) {
        rcvd = recv(sd, &ret_buff[n], sz - n, 0);
        if (rcvd <= 0) {

            return rcvd;
        }
        n += rcvd;
    }

  
    return n;
}

int main(int argc, char *args[]) {

    // Check command line arguments  (n of consumers, n of max_msgs of life)
    if (argc != 3) {
        printf("MONITOR CLIENT ---> ERROR, WRONG NUMBER OF ARGUMENTS, EXITING\n");
        exit(EXIT_FAILURE);
    }

    // Default port number
    int port = SERVER_PORT;
    // Default server IP
    char *ip = SERVER_IP;

    // Create server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    // Create socket, check error
    const int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("MONITOR CLIENT ---> ERROR, SOCKET CREATION FAILED, EXITING");
        exit(EXIT_FAILURE);
    }

    // Connect to the server, check error
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("MONITOR CLIENT ---> ERROR, CONNECTION FAILED, EXITING");
        exit(EXIT_FAILURE);
    }

    printf("MONITOR CLIENT ---> CONNECTED TO SERVER\n");

    // Get number of consumers from command line, first argument
    int consumers = atoi(args[1]);


    // Array of integers: 1st element is the number of items produced by the producer, 2nd element is sz of queue, rest is n of items consumed by each consumer
    int monitor_msg[consumers + 2];

    // Get max number of messages client wants to receive from command line, second argument
    int max_msgs = atoi(args[2]);

    // Loop for the number of max_msgs specified in the command line
    while (max_msgs != 0) {
        // Receive data from the server, check error
        const int rcvd = recv_edit(socket_fd, (char *)&monitor_msg, sizeof(monitor_msg));
        if (rcvd < 0) {
            printf("MONITOR CLIENT ---> ERROR RECEIVING MESSAGES, CLOSING CONNECTION\n");
            break;
        }


        if (rcvd == 0) {
            // sleep 1 second and  continue
            sleep(1);
            max_msgs--;
            continue;
        }

        // Check if received data is correct
        int check = 0;
        int msg_produced = ntohl(monitor_msg[0]);
        int msg_queue = ntohl(monitor_msg[1]);

        check += msg_queue;

        printf("MONITOR CLIENT ---> n of messages produced: %d, n of messages in queue: %d", msg_produced, msg_queue);

        int tmp = 0;
        for (int i = 2; i < consumers + 2; i++) {
            tmp = ntohl(monitor_msg[i]);
            check += tmp;
            printf("\nMONITOR CLIENT ---> consumer thread with id %d consumed %d messages", i - 2, tmp);
        }

        // sleep for 1 second
        sleep(1);
        max_msgs--;

        printf("\n");
    }

    // Now the monitor client has to send a message to the server to close the connection
    // Send message to the server, check error
    const int sent = send(socket_fd, "close", 5, 0);
    if (sent < 0) {
        perror("MONITOR CLIENT ---> ERROR SENDING MESSAGES, CLOSING CONNECTION");
    }
    printf("MONITOR CLIENT ---> CONNECTION CLOSED\n");

    // Close the connection
    close(socket_fd);

    return 0;
}