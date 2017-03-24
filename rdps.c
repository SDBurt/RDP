#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "rdpFunctions.h"
#include "rdpStruct.h"

// stop a warning
int checkArguments(int, int);

// send size in bytes
int send_size;

// connection
int sock;
char* receiver_ip;
char* sender_ip;
char* receiver_port;
char* sender_port;
struct sockaddr_in receiver_address, sender_address;
socklen_t receiverLen, senderLen;
ssize_t recsize;

// file management
long int file_size;
long int file_left_to_send;
char event_type = 's';
int first_seq_num = 0;
char flag[4];

// state enum
rdp_state current_state;

// more file
char receiver_buffer[MAX_BUFFER_LENGTH];
char input_buffer[6500000];
char* file_path;
size_t bytes_read;
char payload_buffer[MAX_BUFFER_LENGTH];

// summary
long int total_payload_size;
int total_data_bytes_sent = 0;
int unique_data_bytes_sent = 0;
int total_data_packets_sent = 0;
int unique_data_packets_sent = 0;
int SYN_sent = 0;
int FIN_sent = 0;
int RST_sent = 0;
int ACK_received = 0;
int RST_received = 0;

// window
int acked = 0;
int window = 10240;
int payload_pos = 0;
int current_seqno = 0;

// of last acked
int last_seq_sent = 0;
int packets_sent = 0;

// time
struct timespec stopTime, startTime;
int timeout_counter = 0;
struct timeval timeout;

/*
* Backtrack the position pointers and file amount by "num" packets
*/

void backtrack_packets(int num) {
	payload_pos -= (984 * num);
	file_left_to_send += (984 * num);
}

/*
    open file and put it in the buffer
*/
void file_to_buffer() {

	// open file
    FILE *input = fopen(file_path, "r");
    if (input == NULL) {
        close(sock);
        error("error: unable to open file");
    }

	// determine the size of the file
	fseek(input, 0L, SEEK_END);
	file_size = ftell(input);
	fseek(input, 0L, SEEK_SET);

	// set the file size left to send
	file_left_to_send = file_size;

	// read the file into the buffer
	bytes_read = fread(input_buffer, sizeof(char), file_size, input);

	// check for error
	if (bytes_read < 0) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	// close the file
	fclose(input);
}
/*
* Send the segment to the receiver.
* input: rdp struct packet
*/
void send_segment(struct rdp_struct send_packet) {

	char segment_flag[4];
	get_flag(send_packet, segment_flag);

	if (strcmp(segment_flag, "SYN") == 0) {
		SYN_sent += 1;
	}

	else if (strcmp(segment_flag, "FIN") == 0) {
		// set a timeout
		FIN_sent += 1;
	}

	// create the header to send
	char header[MAX_BUFFER_LENGTH];
	sprintf(header, "%s %d %d %d %d %d %d %d %d %d ",
		send_packet.magic, send_packet.DAT,
		send_packet.ACK, send_packet.SYN,
		send_packet.FIN, send_packet.RST,
		send_packet.seqno, send_packet.ackno,
		send_packet.length, send_packet.window
	);

	if (strcmp(segment_flag, "DAT") == 0) {

		if (file_left_to_send > 0) {

			// hardcode header size for simplicity
			int header_size = 40;

			// hardcode the size of the payload (because padding)
			long int payload_size = (1024-40);

				// ref: http://stackoverflow.com/questions/6205195/
				// how-can-i-copy-part-of-another-string-in-c-given-a-starting-and-ending-index#6205241
			if (file_left_to_send < payload_size) {

				memcpy(payload_buffer, input_buffer + payload_pos, file_left_to_send);
				memset(payload_buffer+file_left_to_send, 0, (1024-40)-file_left_to_send);
				if (event_type == 's') {
					unique_data_bytes_sent += file_left_to_send;
				}
				total_payload_size += file_left_to_send;
				file_left_to_send -= file_left_to_send;
				//printf("last payload, payload now %ld\n", file_left_to_send);



			} else {

				if (event_type == 's') {
					unique_data_bytes_sent += payload_size;
				}
				memcpy(payload_buffer, input_buffer + payload_pos, payload_size);
				file_left_to_send -= payload_size;
				total_payload_size += payload_size;
			}

			size_t len = strlen(header);
			memcpy(header + len , payload_buffer, (1024-40));
			payload_pos+=payload_size;

			send_size = sendto(sock, header, sizeof(header), 0,(struct sockaddr *)&receiver_address, sizeof(receiver_address));
			if (send_size < 0){
				error("error: send size\n");
			}

				total_data_packets_sent += 1;

		 	if (event_type == 's'){
				total_data_packets_sent += 1;
				unique_data_packets_sent += 1;
			}

			total_data_bytes_sent = total_payload_size;
		}

		if (last_seq_sent < send_packet.seqno) {
			last_seq_sent = send_packet.seqno;
		}

		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;

	}

	// SYN and FIN packets go here
	else {
		// send the reply
		send_size = sendto(sock, header, sizeof(header), 0,
		(struct sockaddr *)&receiver_address, sizeof(receiver_address));
		if (send_size < 0) {
			error("error: send size\n");
		}

		timeout.tv_sec = 0;
		timeout.tv_usec = 50000;
	}

	// log the reply
	printLog(sender_address, receiver_address, event_type, segment_flag, send_packet);

	// clear the header
	memset(header, 0, MAX_BUFFER_LENGTH*sizeof(char));
}


/*
*	Prints the summary of total flags, packets, and bytes sent
*/
void printSummary() {
	printf("total data bytes send: %d\n", total_data_bytes_sent);
	printf("unique data bytes send: %d\n", unique_data_bytes_sent);
	printf("total data packets send: %d\n", total_data_packets_sent);
	printf("unique data packets send: %d\n", unique_data_packets_sent);
	printf("SYN packets send: %d\n", SYN_sent);
	printf("FIN packets send: %d\n", FIN_sent);
	printf("RST packets send: %d\n", RST_sent);
	printf("ACK packets received: %d\n", ACK_received);
	printf("RST packets received: %d\n", RST_received);
	printf("total time duration(second): %lu.%lu\n", (stopTime.tv_sec - startTime.tv_sec), (stopTime.tv_nsec - startTime.tv_nsec));
}

/*
    Main
*/
int main(int argc, char* argv[]) {

    char header[1024];

	srand(time(NULL));
	first_seq_num = rand()%500 + 1;

    // Check the arguments
    int required = 6;
    int argValid = checkArguments(argc, required);
    if (argValid == FALSE) {
        printf("Error: Invalid Arguments\n");
        return -1;
    }

    // parse the arguments
    // rdps <sender_ip> <sender_port> <receiver_ip> <receiver_port> <send file name> (6)
    sender_ip = argv[1];
    sender_port = argv[2];
    receiver_ip = argv[3];
    receiver_port = argv[4];
    file_path = argv[5];

    // create socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // set options
    int opt = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    // set sender address
    memset(&sender_address, 0, sizeof sender_address);
    sender_address.sin_family = AF_INET;
    sender_address.sin_addr.s_addr = inet_addr(sender_ip);
    sender_address.sin_port = htons(atoi(sender_port));
    senderLen = sizeof(sender_address);

    // set receiver address
    memset(&receiver_address, 0, sizeof receiver_address);
    receiver_address.sin_family = AF_INET;
    receiver_address.sin_addr.s_addr = inet_addr(receiver_ip);
    receiver_address.sin_port = htons(atoi(receiver_port));
    receiverLen = sizeof(receiver_address);

    // bind sender address
    if (bind(sock, (struct sockaddr *)&sender_address, sizeof(sender_address)) < 0) {
        close(sock);
        printf("ip: %s port: %s\n", sender_ip, sender_port);
        error("error: bind failed\n");;
    }

	// put the file into a buffer
	file_to_buffer();

    // descriptors
    fd_set read_fds;

    // select result storage
    int select_result;

	timeout.tv_sec = 360;
    timeout.tv_usec = 0;

	clock_gettime(CLOCK_MONOTONIC_RAW, &startTime);


    // infinit loop
    while(1) {

        fflush(stdout);
        FD_ZERO(&read_fds); // clear
        FD_SET(sock, &read_fds); // descriptor from socket

        // since the starting state is synchronizing
        if (current_state == waiting_to_synchronize) {
			struct rdp_struct sync;
			initialize_rdp(&sync);
			sync.seqno = first_seq_num;
			sync.SYN = TRUE;

			//printf("SYN.. \n");
			send_segment(sync);

        }

		if ((current_state == sending) && event_type != 'S') {

			// create a packet for the packet to send
			struct rdp_struct send_packet;
			initialize_rdp(&send_packet);

			// set the received packet fields
			send_packet.DAT = TRUE;

			while (window != 0 && (current_state == sending) && (file_left_to_send > 0)) {

				send_packet.seqno = current_seqno;
				send_packet.length = 1024;

				send_segment(send_packet);
				packets_sent++;
				last_seq_sent = current_seqno;
				current_seqno += 1024;
				window -= 1024;
			}
		}

		if (current_state == finish) {

			// create a packet for the reply
			struct rdp_struct fin_packet;
			initialize_rdp(&fin_packet);

			// modify the packet fields
			fin_packet.FIN = TRUE;
			fin_packet.seqno = 0;
			fin_packet.ackno = 0;

			send_segment(fin_packet);

			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

		}

		// test and run select (no timeout atm)
	    select_result = (select(sock+1, &read_fds, NULL, NULL, &timeout));

		switch(select_result) {
			case -1:
				printf( "Error\n" );
				break;

			case 0:
				event_type = 'S';

				if (current_state == sending) {

					// reset payload positioning and packets sent;
					current_seqno -= (1024*packets_sent);
					window = 10240;
					
					backtrack_packets(packets_sent);

					// create a packet for the packet to send
					struct rdp_struct REsend_packet;
					initialize_rdp(&REsend_packet);

					// set the received packet fields
					REsend_packet.DAT = TRUE;

					int i = 0;
					while (i < packets_sent) {
						//printf("packets sent: %d\n", packets_sent);

						REsend_packet.seqno = current_seqno;
						REsend_packet.length = 1024;
						send_segment(REsend_packet);

						current_seqno += 1024;
						window -= 1024;
						i++;
					}

					timeout.tv_sec = 0;
					timeout.tv_usec = 10000;
					break;
				}

				if (current_state == finish) {

					timeout_counter++;

					if (timeout_counter == 2) {
						clock_gettime(CLOCK_MONOTONIC_RAW, &stopTime);
						printSummary();
						close(sock);
						return(0);
					}

					timeout.tv_sec = 0;
					timeout.tv_usec = 50000;

					break;
				}
				break;

			default:
				// receive the message
				recsize = recvfrom(sock, (void*)receiver_buffer, sizeof(receiver_buffer), 0,
				(struct sockaddr*) &receiver_address, &receiverLen);

				if (recsize < 0) {
					close(sock);
					error("error: handshake reply\n");
				}

				// set timeout amount
				timeout.tv_sec = 360;
				timeout.tv_usec = 0;

				// create a packet for the received info
				struct rdp_struct received_packet;
				initialize_rdp(&received_packet);
				header_from_string(receiver_buffer, &received_packet);

				current_seqno = received_packet.ackno;

				// print the received packet log
				get_flag(received_packet, flag);
				printLog(sender_address, receiver_address, 'r', flag, received_packet);
				ACK_received += 1;
				packets_sent = 0;
				event_type = 's';

	            switch (current_state) {
	                case waiting_to_synchronize:
						current_state = sending;
						window = received_packet.window;
	                    break;

	                case sending:
						window = received_packet.window;

						int received_seqno = received_packet.seqno;
						current_seqno = received_packet.ackno;

						if ((received_seqno) < last_seq_sent) {
							int revert = (last_seq_sent - received_seqno)/1024;
							backtrack_packets(revert);

							unique_data_bytes_sent -= 984*revert;
							unique_data_packets_sent -= revert;

							current_seqno = (last_seq_sent - 1024*revert) + 1024;

						} else {
							current_seqno = received_packet.ackno;
						}

						if ((file_left_to_send == 0) && (received_seqno == last_seq_sent)) {
							current_state = finish;
						}

	                    break;

					case finish:

						if (received_packet.ackno == 1) {
							// receive the message
							clock_gettime(CLOCK_MONOTONIC_RAW, &stopTime);
							printSummary();
							close(sock);
							return(0);
						}
						break;
	            }
		}

        fflush(stdout);
		FD_ZERO(&read_fds); // clear
    }

    // end
    close(sock);
    return(0);
}
