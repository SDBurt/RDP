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

#define TRUE 1
#define FALSE 0

/*
    Should check the arguments and see if they are set to the required
    If it is not at the required amount, return FALSE
    else return TRUE
*/
int checkArguments(int argc, int required) {
  //printf("num args: %d\n", argc);
  if (argc != required) {
      return FALSE;
  } else {
      return TRUE;
  }
}

/*
    Print the time
*/
void printTime() {
    fflush(stdout);

    time_t time_full;
    struct tm * time_tag;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    double time_in_milli = tv.tv_usec;

    time (&time_full);
    time_tag = localtime( &time_full );

    printf("%.2d:%.2d:%.2d.%.f ",
			time_tag->tm_hour,
            time_tag->tm_min,
			time_tag->tm_sec,
			time_in_milli
		);
}


void error(char* error_text) {
    printf("%s", error_text);
    exit(EXIT_FAILURE);
    return;
}

int min(int a, int b) {
  if (a < b) {
    return a;
  }
  else {
    return b;
  }
}
