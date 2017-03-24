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

int checkArguments(int, int);

// socket stuff
char event_type = 's';
int sock;
char* receiver_ip;
char* sender_ip;
char* receiver_port;
char* sender_port;
struct sockaddr_in sender_address;
socklen_t receiverLen, senderLen;
ssize_t recsize;

struct timespec stopTime, startTime;

// file stuff
int send_size;
int payload_pos = 0;
FILE* writeFile;
char* fileName;
char send_buffer[MAX_BUFFER_LENGTH];
char receiver_buffer[MAX_BUFFER_LENGTH];

// for summary
int total_data_bytes_received = 0;
int unique_data_bytes_received = 0;
int total_data_packets_received = 0;
int unique_data_packets_received = 0;
int SYN_received = 0;
int FIN_received = 0;
int RST_received = 0;
int ACK_sent = 0;
int RST_sent = 0;

int lowest_payload = 984;
int repeat_payload = 0;

// acked so far
int first_DAT_received = TRUE;
int current_ack = 0;
int next_ack = 0;
int window = 10240;

// flag stuff
char flag[4];


// buffer stuff and packet control
char packets [ 10 ][ 1024 ];
int num_packets = 0;
char received_seg[1024];
char temp[1024];
char temp_DAT_seg[984];
int bytes_written = 0;
int initial_seq = 0;

// connection
int first_packet = TRUE;
int seq_acked = 0;

void send_response(struct rdp_struct reply_packet, char event) {

	// create the header to send
	char header[MAX_BUFFER_LENGTH];
	sprintf(header, "%s %d %d %d %d %d %d %d %d %d ",
		reply_packet.magic, reply_packet.DAT,
		reply_packet.ACK, reply_packet.SYN,
		reply_packet.FIN, reply_packet.RST,
		reply_packet.seqno, reply_packet.ackno,
		reply_packet.length, reply_packet.window
	);

	// send the reply
	send_size = sendto(sock, header, sizeof(header), 0,
	(struct sockaddr *)&sender_address, sizeof(sender_address));
	if (send_size < 0){
		error("error: send size\n");
	}

	// log the reply
	printLog(sender_address, sender_address, event, "ACK", reply_packet);

	ACK_sent += 1;

	// clear the header
	memset(header, 0, MAX_BUFFER_LENGTH*sizeof(char));
}

void printSummary() {
	printf("total data bytes received: %d\n", total_data_bytes_received);
	printf("unique data bytes received: %d\n", unique_data_bytes_received);
	printf("total data packets received: %d\n", total_data_packets_received);
	printf("unique data packets received: %d\n", unique_data_packets_received);
	printf("SYN packets received: %d\n", SYN_received);
	printf("FIN packets received: %d\n", FIN_received);
	printf("RST packets received: %d\n", RST_received);
	printf("ACK packets sent: %d\n", ACK_sent);
	printf("RST packets sent: %d\n", RST_sent);
	printf("total time duration(second): %lu.%lu\n", (stopTime.tv_sec - startTime.tv_sec), (stopTime.tv_nsec - startTime.tv_nsec));
}

/*
    Main
*/
int main(int argc, char* argv[]) {

    char* file_path;
    int send_size;

    // Check the arguments
    int requiredArgs = 4;
    int argValid = checkArguments(argc, requiredArgs);
    if (argValid == FALSE) {
        error("Error: Invalid Arguments");
    }

    // ./rdpr 10.10.1.100 8080 received.dat
    receiver_ip = argv[1];
    receiver_port = argv[2];
    fileName = argv[3];


    // create socket
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    // set options
    int opt = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    // set receiver address
    memset(&sender_address, 0, sizeof sender_address);
    sender_address.sin_family = AF_INET;
    sender_address.sin_addr.s_addr = inet_addr(receiver_ip);
    sender_address.sin_port = htons(atoi(receiver_port));
    senderLen = sizeof(sender_address);

    // bind sender address
    if (bind(sock, (struct sockaddr *)&sender_address, sizeof(sender_address)) < 0) {
        close(sock);
        printf("ip: %s port: %s\n", receiver_ip, receiver_port);
        error("error: bind failed\n");
    }

    /*
        http://stackoverflow.com/questions/11573974/write-to-txt-file
    */
    // open the file to read data to

    writeFile = fopen(fileName, "w");

    // descriptors
    fd_set read_fds;
    int select_result;
    struct timeval timeout;

	timeout.tv_sec = 360;
	timeout.tv_usec = 0;

    while(1) {

        fflush(stdout);

        FD_ZERO(&read_fds); // clear
        FD_SET(sock, &read_fds); // descriptor from socket

        // test and run select
        select_result = (select(sock+1, &read_fds, NULL, NULL, &timeout));

		switch(select_result) {
			// error
			case -1:
				printf( "Error\n" );
	        break;
			// timeout

			case 0:
				fflush(stdout);

				memcpy(temp, packets[0], 1024);
				current_ack = seq_from_string(temp);

				if (repeat_payload > 0) {
					unique_data_bytes_received -= 984*repeat_payload;
					unique_data_packets_received -= 1*repeat_payload;
				}


				int i = 0;
				int j = 0;
				int k = 0;

				// super inefficient but whatever
				while (i < num_packets){
					while (j < num_packets) {

						memcpy(temp, packets[j], 1024);
						current_ack = seq_from_string(temp);

						// if the current ack is the next expected ack in order
						if (seq_acked == current_ack) {

							memcpy(temp, packets[j], 1024);
							payload_from_string(temp, temp_DAT_seg);


							fwrite(temp_DAT_seg , 1 , strlen(temp_DAT_seg) , writeFile);
							bytes_written += 1024;

							unique_data_bytes_received += strlen(temp_DAT_seg);
							unique_data_packets_received += 1;


							// increase the seq acked
							seq_acked += 1024;
							k++; // packet added

						}
						j++;
					}
					i++;
				}


				num_packets = 0;
				k = 0;

				// process the packet from string
				struct rdp_struct SEND_packet;
				initialize_rdp(&SEND_packet);
				header_from_string(received_seg, &SEND_packet);

				SEND_packet.window = 10240;
				SEND_packet.seqno = initial_seq + bytes_written - 1024;
				SEND_packet.ackno = initial_seq + bytes_written;
				SEND_packet.ACK = TRUE;

				send_response(SEND_packet, event_type);
				window = 10240;
				timeout.tv_sec = 360;
				timeout.tv_usec = 0;

				event_type = 's';

			break;

			// receiving
			case 1:

				recsize = recvfrom(sock, (void*)receiver_buffer, sizeof(receiver_buffer), 0, (struct sockaddr*)&sender_address, &senderLen);
				if (recsize < 0) {
					printf("error with receive\n");
				}

				if ( window > 0 ) {

					// process the packet from string
					struct rdp_struct received_packet;
					initialize_rdp(&received_packet);
					memcpy(received_seg, receiver_buffer, 1024);
					header_from_string(receiver_buffer, &received_packet);
					get_flag(received_packet, flag);
					window -= 1024;

					if (received_packet.seqno < seq_acked && received_packet.FIN != TRUE) {
						// print and drop
						printLog(sender_address, sender_address, 'R', flag, received_packet);
						event_type = 'S';


					} else {
						printLog(sender_address, sender_address, 'r', flag, received_packet);
						event_type = 's';

						repeat_payload = 0;

					}

					if (strcmp(flag, "SYN") == 0) {

						SYN_received += 1;
						window += 1024;

						// initialize the new sequence and acknologment numbers
						struct rdp_struct SYN_packet;
						initialize_rdp(&SYN_packet);

						if (seq_acked != received_packet.seqno + 1) {
							seq_acked = received_packet.seqno + 1;
							initial_seq = seq_acked;
							// modify the fields
							SYN_packet.window = 10240;
							SYN_packet.ackno = received_packet.seqno + 1;
							SYN_packet.ACK = TRUE;
							clock_gettime(CLOCK_MONOTONIC_RAW, &startTime);

							// send the response
							send_response(SYN_packet, event_type);
						} else {

							// modify the fields
							SYN_packet.window = 10240;
							SYN_packet.ackno = received_packet.seqno + 1;
							SYN_packet.ACK = TRUE;

							// send the response
							send_response(SYN_packet, 'S');

						}
					}

					if (strcmp(flag, "FIN") == 0) {

						window += 1024;
						FIN_received += 1;

						// create a packet for the reply
						struct rdp_struct FIN_packet;
						initialize_rdp(&FIN_packet);

						// modify the packet fields
						FIN_packet.ACK = TRUE;
						FIN_packet.seqno = received_packet.ackno;
						FIN_packet.ackno = received_packet.seqno + 1;
						FIN_packet.window = 10240;

						send_response(FIN_packet, 's');
						clock_gettime(CLOCK_MONOTONIC_RAW, &stopTime);

						printSummary();
						fclose(writeFile);
						close(sock);
						return 0;

					}

					if (strcmp(flag, "DAT") == 0) {

						memcpy(packets[num_packets], received_seg, 1024);
						num_packets++;
						total_data_bytes_received += strlen(received_packet.payload);
						total_data_packets_received += 1;

						timeout.tv_sec = 0;
						timeout.tv_usec = 2000;
					}

				} else {

					timeout.tv_sec = 0;
					timeout.tv_usec = 0;
				}
				
			break;
		}

        memset(receiver_buffer, 0, MAX_BUFFER_LENGTH*sizeof(char));
        fflush(stdout);
    }

    // end
    close(sock);
    return(0);
}
