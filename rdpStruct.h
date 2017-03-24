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
#include <math.h>

#define TRUE 1
#define FALSE 0
#define MAX_BUFFER_LENGTH 1024

void printTime();

typedef enum {
    waiting_to_synchronize,// this is the first element by default
    waiting_for_handshake_reply,
    sending,
    waiting_to_receive,
    receiving,
	end,
	finish
} rdp_state;

typedef struct rdp_struct {
    char* magic;
    int DAT;
    int ACK;
    int SYN;
    int FIN;
    int RST;
    int seqno;
    int ackno;
    int length; //payload length
    int window; // window size
    char* payload;
} rdp_struct;

void initialize_rdp (struct rdp_struct* this_rdp) {
    this_rdp->magic = "CSC361";
    this_rdp->DAT = 0;
    this_rdp->ACK = 0;
    this_rdp->SYN = 0;
    this_rdp->FIN = 0;
    this_rdp->RST = 0;
    this_rdp->seqno = 0;
    this_rdp->ackno = 0;
    this_rdp->length = 0; //payload length
    this_rdp->window = 10240; // window size
    this_rdp->payload = "";
}

void rdp_header_transfer(struct rdp_struct* to_rdp,  struct rdp_struct from_rdp) {
    to_rdp->magic = from_rdp.magic;
    to_rdp->DAT = from_rdp.DAT;
    to_rdp->ACK = from_rdp.ACK;
    to_rdp->SYN = from_rdp.SYN;
    to_rdp->FIN = from_rdp.FIN;
    to_rdp->RST = from_rdp.RST;
    to_rdp->seqno = from_rdp.seqno;
    to_rdp->ackno = from_rdp.ackno;
    to_rdp->length = from_rdp.length;
    to_rdp->window = from_rdp.window;
    to_rdp->payload = from_rdp.payload;
}

void rdp_set_payload(struct rdp_struct* rdp,  char* payload) {
    rdp->payload = payload;
    rdp->length = strlen(payload);
}

void header_from_string(char* receiver_buffer, struct rdp_struct* this_rdp) {
    char* pch;

    // get the first token
    pch = strtok(receiver_buffer, " ");

    this_rdp->magic = pch;

    pch = strtok(NULL, " ");
    this_rdp->DAT = atoi(pch);

    pch = strtok(NULL, " ");
    this_rdp->ACK = atoi(pch);

    pch = strtok(NULL, " ");
    this_rdp->SYN = atoi(pch);

    pch = strtok(NULL, " ");
    this_rdp->FIN = atoi(pch);

    pch = strtok(NULL, " ");
    this_rdp->RST = atoi(pch);

    pch = strtok(NULL, " ");
    this_rdp->seqno = atoi(pch);

    pch = strtok(NULL, " ");
    this_rdp->ackno = atoi(pch);

    pch = strtok(NULL, " ");
    this_rdp->length = atoi(pch);

    pch = strtok(NULL, " ");
    this_rdp->window = atoi(pch);

    pch = strtok(NULL, "\0");
    this_rdp->payload = pch;

    //pch = strtok(NULL, "");

}

int seq_from_string(char* receiver_buffer) {

	char* pch;

	int seq = 0;
    // get the first token
    pch = strtok(receiver_buffer, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");

	seq = atoi(pch);
	return seq;

}

void payload_from_string(char* receiver_buffer, char* temp) {
    char* pch;

    // get the first token
    pch = strtok(receiver_buffer, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, " ");
    pch = strtok(NULL, "\0");

    memcpy(temp,pch,985);
	memset(temp + 985, '\0', 1);

}

/*
    Print the log summary
*/
void printLog(struct sockaddr_in sender, struct sockaddr_in receiver, char event_type, char* flag, rdp_struct rdp){
    fflush(stdout);
    //inet_ntoa(sender_address.sin_addr), ntohs(sender_address.sin_port))

    fflush(stdout);
    printTime();
    printf("%c ", event_type);
    printf("%s:%d ", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
    printf("%s:%d ", inet_ntoa(receiver.sin_addr), ntohs(receiver.sin_port));
    printf("%s ", flag);

    if (strcmp(flag, "SYN") == 0) {
        printf("%d ", rdp.seqno);
        printf("%d \n", rdp.length);
    }
    else if (strcmp(flag, "ACK") == 0) {
        printf("%d ", rdp.ackno);
        printf("%d \n", rdp.window);
    }

    else if (strcmp(flag, "DAT") == 0) {
        printf("%d ", rdp.seqno);
        printf("%d \n", rdp.length);
    }

	else if (strcmp(flag, "FIN") == 0) {
		printf("%d ", rdp.seqno);
		printf("%d \n", rdp.length);
	}
    fflush(stdout);
}

void get_flag(struct rdp_struct packet, char flag[]) {
	if (packet.SYN == TRUE) {strcpy(flag,"SYN");}
	else if (packet.ACK == TRUE) {strcpy(flag,"ACK");}
	else if (packet.DAT == TRUE) {strcpy(flag,"DAT");}
	else if (packet.RST== TRUE) {strcpy(flag,"RST");}
	else if (packet.FIN == TRUE) {strcpy(flag,"FIN");}
	else {strcpy(flag,"ERR");}
}
