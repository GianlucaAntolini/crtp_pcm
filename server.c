#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>

// CONSTANTS & STRUCTS
// Maximum wait time for producer and consumers in nanoseconds
#define MAX_WAIT_TIME_PROD 200000000
#define MAX_WAIT_TIME_CONS 1000000000

#define SERVER_PORT 49200
#define MAX_CLIENTS 10

// Structure to setup remote connection with monitoring process, used by monitor thread
struct monitor_data {
    // Sampling interval
    int interval;
    // Number of consumers
    int consumers;
};

// Structure to hold client information
struct client_data {
    int sockfd;
    struct sockaddr_in addr;
};

// SHARED VARS
// Messages
int msgs;
// Buffer size
int buff_sz;

// INTER-PROCESS COMMUNICATION VARS
// Mutex
pthread_mutex_t mutex;
// Condition variables (space and data in buffer)
pthread_cond_t buff_space, buff_data;
// Buffer
int *buff;
// Write index, next free slot in buffer
int w_idx = 0;
// Read index, next slot to be read by a consumer
int r_idx = 0;
// N of produced messages
int p = 0;
// Array of n of consumed messages by consumer
int *c;
// Flag to signal end of the production of messages by produver
char end = 0;
// Array to store active clients
struct client_data clients[MAX_CLIENTS];
// Number of active clients
int active_clients = 0;
// Mutex for handling client array
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


// FUNCTIONS
// Sleep thread for a random time in ns in the range [0 - max time (depends if producer or consumer))
static inline void bounded_nanosleep(const long bound) {
    static struct timespec time;
    time.tv_sec = 0;
    time.tv_nsec = rand() % bound;
    nanosleep(&time, NULL);
}

// Function to handle a client connection
void *client_handler(void *arg) {
    int client_index = *((int *)arg);
    free(arg);

    int sockfd = clients[client_index].sockfd;
    struct sockaddr_in client_addr = clients[client_index].addr;

    printf("CLIENT HANDLER ---> handling client %d\n", client_index);

    //infinite loop
    while (1){
        // When client sends message "close") it means it wants to disconnect, so exit the loop
        char msg[6];
        if (recv(sockfd, &msg, sizeof(msg), 0) < 0) {
            perror("CLIENT HANDLER ---> ERROR! DATA (CLOSE MESSAGE) RECEIVE FAILED FROM CLIENT, CONTINUING");
            continue;
        }

        if (strcmp(msg, "close") == 0) {
            printf("CLIENT HANDLER ---> client %d disconnected\n", client_index);

            break;
        }
        
  
    }


    // Close the client socket and remove it from the array when done
    close(sockfd);

    pthread_mutex_lock(&clients_mutex);
    active_clients--;
    // Fix the clients array
    for (int i = client_index; i < active_clients; i++) {
        clients[i] = clients[i + 1];
    }
    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}

// Server thread function
void *server_thread() {
    int server_sockfd, client_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Create socket for server
    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("SERVER THREAD ---> ERROR! SOCKET CREATION FAILED, EXITING");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the server address
    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("SERVER THREAD ---> ERROR! BINDING FAILED, EXITING");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_sockfd, MAX_CLIENTS) < 0) {
        perror("SERVER THREAD ---> ERROR! LISTEN FAILED, EXITING");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Accept a connection from a client
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);

        if (client_sockfd < 0) {
            perror("SERVER THREAD ---> ERROR! ACCEPT FAILED, CONTINUING");
            continue;
        }

        printf("SERVER THREAD ---> accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_mutex_lock(&clients_mutex);

        // Add the client to the array
        if (active_clients < MAX_CLIENTS) {
            int *client_index = malloc(sizeof(*client_index));
            *client_index = active_clients;
            clients[active_clients].sockfd = client_sockfd;
            clients[active_clients].addr = client_addr;
            active_clients++;

            // Create a thread to handle the client
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, client_handler, client_index);
        } else {
            // Reject the client if the array is full
            close(client_sockfd);
            printf("SERVER THREAD ---> Maximum clients reached. Rejecting connection.\n");
        }

        pthread_mutex_unlock(&clients_mutex);
    }

    // Close the server socket (this part will never be reached)
    close(server_sockfd);

    return NULL;
}





// PRODUCER (thread)
void *producer() {

    // Loop for the number of messages to produce
    for (int i = 0; i < msgs; i++) {
        
        // Wait random time to simulate work
        bounded_nanosleep(MAX_WAIT_TIME_PROD);
    
        // Lock the mutex
        pthread_mutex_lock(&mutex);

        // Wait until there is space in the buffer (to do it check if the next slot is free)
        while ((w_idx + 1) % buff_sz == r_idx) {
            // Unlock mutex and wait on condition atomically
            pthread_cond_wait(&buff_space, &mutex);
        }

        // Put the message in buffer
        buff[w_idx] = i;
        // Update write index and number of produced messages
        w_idx = (w_idx + 1) % buff_sz;
        p += 1;

        printf("PRODUCER THREAD ---> produced message %d\n", i);

        // Signal to consumers that there is new message
        pthread_cond_signal(&buff_data);
        // Unlock mutex
        pthread_mutex_unlock(&mutex);
    }

    // Lock mutex
    pthread_mutex_lock(&mutex);
    // Set end flag
    end = 1;
    // Broadcast all consumers end of production
    pthread_cond_broadcast(&buff_data);
    // Unlock mutex
    pthread_mutex_unlock(&mutex);

    return NULL;
}


// CONSUMER (thread)
void *consumer(void *arg) {

    // Get id of this consumer from arguments (cast pointer)
    const int id = *((int *)arg);
    // Free memory allocated for id
    free(arg);

    // Init message variable
    int item;
    // Infinite loop
    while (1) {
        // Lock mutex
        pthread_mutex_lock(&mutex);

        // Check if producer finished producing (end flag set and write index reached read index)
        if (end && r_idx == w_idx) {
            // Unlock mutex
            pthread_mutex_unlock(&mutex);
            // Exit loop
            break;
        }

        // Wait for new message (end flag not set and read index reached write index)
        while (!end && r_idx == w_idx) {
            // Unlock mutex and wait on condition atomically
            pthread_cond_wait(&buff_data, &mutex);
        }

        // Set item as message in buffer in read index
        item = buff[r_idx];
        // Update read index and number of consumed messages for this consumer
        r_idx = (r_idx + 1) % buff_sz;
        c[id] += 1;

        printf("CONSUMER THREAD ---> consumer thread with id %d consumed message %d\n", id, item);

        // Signal to producer that there is space in buffer
        pthread_cond_signal(&buff_space);
        // Unlock mutex
        pthread_mutex_unlock(&mutex);

        // Wait random time to simulate work
        bounded_nanosleep(MAX_WAIT_TIME_CONS);
    }

    return NULL;
}


// MONITOR (thread)
static void *monitor(void *arg) {
    const struct monitor_data *data = (struct monitor_data *)arg;
    const int interval = data->interval;
    const int consumers = data->consumers;

    while (1) {
        pthread_mutex_lock(&mutex);

        const int q_length = (w_idx - r_idx + buff_sz) % buff_sz;
        char last_msg = end && (q_length == 0);

        int monitor_msg[consumers + 2];
        monitor_msg[0] = htonl(p);
        monitor_msg[1] = htonl(q_length);

        for (int i = 0; i < consumers; i++) {
            monitor_msg[i + 2] = htonl(c[i]);
        }

        pthread_mutex_unlock(&mutex);

        pthread_mutex_lock(&clients_mutex);
        printf("MONITOR THREAD ---> sending monitor server message to %d clients\n", active_clients);
        // Send message to all active clients
        for (int i = 0; i < active_clients; i++) {
            if (send(clients[i].sockfd, &monitor_msg, sizeof(monitor_msg), 0) < 0) {
                perror("MONITOR THREAD ---> ERROR! DATA (MONITOR MESSAGE) SEND FAILED TO CLIENT, CONTINUING");
            }


            
            // total number of consumed messages
            int total_consumed = 0;
            for (int j = 0; j < consumers; j++) {
                total_consumed += ntohl(monitor_msg[j + 2]);
            }

            printf("MONITOR THREAD ---> sent monitor server message to client %d with %d produced messages, %d messages in queue and %d messages consumed by consumers\n",
                   i, ntohl(monitor_msg[0]), ntohl(monitor_msg[1]), total_consumed);
                   for (int j = 0; j < consumers; j++) {
                printf("MONITOR THREAD ---> in message: consumer thread with id %d consumed %d messages\n", j, ntohl(monitor_msg[j + 2]));
            }
        }

        pthread_mutex_unlock(&clients_mutex);

        if (last_msg) {
            printf("MONITOR THREAD ---> last message sent, exiting\n");
            break;
        }

        sleep(interval);
    }

    return NULL;
}


// MAIN
int main(int argc, char *args[]) {
    // Arguments check
    // 4 arguments: sampling interval, consumers number, buffer size, messages number
     if (argc != 5) {
        printf("MAIN ---> ERROR, WRONG NUMBER OF ARGUMENTS, EXITING\n");
        exit(EXIT_FAILURE);
    }

    // Get all data from arguments

    // Sampling interval
    const int interval = strtol(args[1], NULL, 10);
    printf("MAIN ---> sampling interval set to %d seconds\n", interval);

    // N of consumers
    const int consumers = (int)strtol(args[2], NULL, 10);
    printf("MAIN ---> number of consumers set to %d\n", consumers);

    // Buffer size
    buff_sz = (int)strtol(args[3], NULL, 10);
    // Allocate buffer, check error
    buff = malloc(sizeof(int) * buff_sz);
    if (buff == NULL) {
        perror("MAIN ---> ERROR, BUFFER MEMORY ALLOCATION FAILED, EXITING");
        exit(EXIT_FAILURE);
    }
    printf("MAIN ---> allocated buffer for %d messages\n", buff_sz);

    // N of messages to produce
    msgs = (int)strtol(args[4], NULL, 10);
    printf("MAIN ---> production of %d messages set up\n", msgs);

    // Initialize mutex and condition variables
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&buff_space, NULL);
    pthread_cond_init(&buff_data, NULL);
    pthread_mutex_init(&clients_mutex, NULL);

    // Create server thread
    printf("MAIN ---> creating server thread\n");
    pthread_t server_t;
    pthread_create(&server_t, NULL, server_thread, NULL);

    // Create producer thread
    printf("MAIN ---> creating producer thread\n");
    pthread_t prod_t;
    pthread_create(&prod_t, NULL, producer, NULL);

    // Consumer counter, check error
    c = malloc(sizeof(int) * consumers);
    if (c == NULL) {
        perror("MAIN ---> ERROR, CONSUMER COUNTER MEMORY ALLOCATION FAILED, EXITING");
        exit(EXIT_FAILURE);
    }
    

    // Create consumer cons_t (n of consumers)
    printf("MAIN ---> creating %d consumer thread\n", consumers);
    pthread_t cons_t[consumers];
    for (int i = 0; i < consumers; i++) {

        // Allocate ID var of the consumer on heap, check error
        int *id = malloc(sizeof(*id));
        if (id == NULL) {
            perror("MAIN ---> ERROR, CONSUMER ID MEMORY ALLOCATION FAILED, EXITING");
            exit(EXIT_FAILURE);
        }

        // Set  ID of the consumer as index of loop
        *id = i;

        pthread_create(&cons_t[i], NULL, consumer, id);
    }

    // Send data to monitor
    struct monitor_data setup_data;
    setup_data.interval = interval;
    setup_data.consumers = consumers;

    // Create monitor thread
    printf("MAIN ---> creating monitor thread\n");
    pthread_t monitor_t;
    pthread_create(&monitor_t, NULL, monitor, &setup_data);

    // Wait for all consumer cons_t to finish
    for (int i = 0; i < consumers; i++) {
        // Wait for thread termination
        pthread_join(cons_t[i], NULL);
    }

    // Wait for monitor thread to finish
    pthread_join(monitor_t, NULL);

    printf("MAIN ---> all consumer threads terminated\n");
    
    // Free memory (buffer and consumers counter)
    free(buff);
    free(c);

    // Wait for server thread to finish (this will never happen)
    pthread_join(server_t, NULL);

    return 0;
}
