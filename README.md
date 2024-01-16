Exercise for Concurrent and Real Time Programming Course in Computer Engineering Master's Degree at UNIPD




**EXCERCISE 3** of the proposed excercises:

Producer-(multiple) consumers program with remote status monitoring. The producer and 
the consumers shall be implemented as threads and the message queue will be hosted in 
shared memory. Another thread, separate from the producer and the consumers, shall 
monitor the message queue length, the number of produced messages and the number of 
received messages for every consumer. A TCP/IP server will allow one or more clients to 
connect. When a client connects, a new thread is created, handling communication with that 
client and periodically sending the information collected by the monitor thread.





**SOLUTION:**




**ARCHITECTURE:**

![alt text](https://github.com/GianlucaAntolini/crtp_pcm/blob/main/architecture.png)
                           



**MAKEFILE:**

Compiles main.c and monitor.c with also -phtreads flag

launch 'make' command to compile
launch 'rm' command to remove all compiled files



**SERVER:**

Parameters:
- sampling interval in seconds
- number of consumers
- buffer size
- number of messages

Starts TCP/IP server that listens to incoming connections and for each connection received,
if limit of number of clients is not already reached, creates a thread for that client.
Creates one producer thread and consumers threads depending on number received from arguments,
both work on the shared buffer to produce and consume messages (number of messages to produce
received from arguments).
Creates a monitor thread that every n seconds (number received from arguments) looks at the shared data
and shared buffer, prepares a message containing number of messages produced, in queue, consumed total
and consumed by each consumer and sends it to all connected clients.

launch command './server 5 3 10 1000' to execute the server (example parameters)





**CLIENT:**

Parameters:
- number of consumers
- max number of message client wants to receive
Connects to server via TCP/IP (if limit of number of clients on server list is not already reached)
and whenever it receives data from it, it prints it, for the number of times (maximum) specified
in arguments, then it closes the connection.

launch command './client 3 100' to start a client (example parameters, NOTE: number of consumers of client must be equal of number of consumers of server)
