// ****************************************************************
// SLAVE PERIODIC TASK MODULE FOR USER SPACE MEASUREMENTS
// COMPILE: gcc gpiop_slave.c -o gpiop_slave -lrt   or   gcc -Wall -w gpiop1.c -std=gnu99 -lrt  
// RUN: sudo chrt -f 99 ./gpiop_slave <number-of-loops> <semi-period in nsecs>// with realime priority 99
// Master and slave code must have the same number-of-loops (as runs)
// ****************************************************************

#include <sys/timerfd.h> // for the timer file descriptor
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
// #include <sys/time.h> ???
#include <poll.h>
#include <math.h>
#include <stdint.h> // for the definition of uint64_t (for the return of read(timer file descriptor)

#define GPIO_in 27  // means (BCM) GPIO27 is (BOARD) pin 13
#define GPIO_out 17 //means (BCM) GPIO17 is (BOARD) pin 11

#define GPIO_in 13  // means GPIO1_13 is (pin 11) -for BEAGLEBONE BLACK
#define GPIO_out 16 // means GPIO1_16 is (pin 15) -for BEAGLEBONE BLACK

#define ON "1"
#define OFF "0"

#define handle_error(msg) \
	do {perror(msg); exit(EXIT_FAILURE); } while(0)

int main(int argc, char *argv[])
{
if(argc!=3) {printf("RUN: sudo chrt -f 99 ./gpiop1_slave <arithmo> <semi-period in nsecs>\n"); return 1;}
	
mlockall(MCL_CURRENT | MCL_FUTURE);  // requires <sys/mman.h>
	
struct timeval tv;
struct pollfd pfd;
int fd,fdout;
char buf[8];

char GPIOString_in[4];
char GPIODirection_in[64];
char GPIOValue_in[64];
char GPIOEdge_in[64];

char setdirection[4];
char setvalue[4];
char setedge[4];

char GPIOString_out[4];
char GPIODirection_out[64];
char GPIOValue_out[64];
 
struct timespec tms, ts_diff, ts_begin, ts_end;
struct timespec start_time, finish_time;

unsigned int i=0, next, i1, i2, mean_greater=0, mean_smaller=0;

float sum=0.0, median, mean, min=0, max=0;

int cycles;
char* setting;

int result, timer_fd;
uint64_t periods_elapsed; //unsigned long long periods_elapsed;
struct itimerspec timeout;
struct timespec now;

int choice =1;

sleep(1); //delay time to start for the test device to get ready

/* ***************************** INPUTS ******************************************* */

/* *************************** EXPORT PIN IN *************************************** */
/* instead of doing from a terminal: sudo sh -c "echo 27     >/sys/class/gpio/export" */
sprintf(GPIOString_in,"%d",GPIO_in);
if( (fd=open("/sys/class/gpio/export",O_WRONLY)) <0) // or "w"
{
      fprintf(stderr, "Failed to export gpio (for writing in pin).\n");
      exit(1);
   }
else
{
write (fd, GPIOString_in, sizeof(GPIOString_in));
close (fd);
}

/* ************************** DIRECTION IN **************************************** */
/* instead of doing from a terminal:  sudo sh -c "echo in     >/sys/class/gpio/gpio27/direction" */
sprintf(GPIODirection_in,"/sys/class/gpio/gpio%d/direction",GPIO_in); 
if( (fd=open(GPIODirection_in,O_WRONLY)) <0) // or "w"
{
      fprintf(stderr, "Failed to open GPIODirection_in.\n");
      exit(1);
   }
else
{
//strcpy(setdirection,"in"); write (fd, setdirection, sizeof(setdirection));
write(fd,"in",sizeof("in"));
close (fd);
}

/* ******************************* EDGE rising *********************************** */
/* instead of doing from a terminal:  sudo sh -c "echo rising >/sys/class/gpio/gpio27/edge" */
sprintf(GPIOEdge_in,"/sys/class/gpio/gpio%d/edge",GPIO_in); 
if( (fd=open(GPIOEdge_in,O_WRONLY)) <0) // or "w"
{
      fprintf(stderr, "Failed to open GPIOEdge_in.\n");
      exit(1);
   }
else
{
//strcpy(setedge,"rising"); write (fd, setedge, sizeof(setedge));
//write(fd,"rising",sizeof("rising"));
//write(fd,"falling",sizeof("falling"));
write(fd,"both",sizeof("both")); 
close (fd);
}



/* ***************************** OUTPUTS ******************************************* */
/* *************************** EXPORT PIN OUT *************************************** */
/* instead of doing from a terminal: sudo sh -c "echo 17     >/sys/class/gpio/export" */
sprintf(GPIOString_out,"%d",GPIO_out);
if( (fd=open("/sys/class/gpio/export",O_WRONLY)) <0) // or "w"
{
      fprintf(stderr, "Failed to export gpio (for writing out pin).\n");
      exit(1);
   }
else
{
write (fd, GPIOString_out, sizeof(GPIOString_out));
close (fd);
}

/* ************************** DIRECTION OUT **************************************** */
/* instead of doing from a terminal:  sudo sh -c "echo out     >/sys/class/gpio/gpio17/direction" */
sprintf(GPIODirection_out,"/sys/class/gpio/gpio%d/direction",GPIO_out); 
if( (fd=open(GPIODirection_out,O_WRONLY)) <0) // or "w"
{
      fprintf(stderr, "Failed to open GPIODirection_out.\n");
      exit(1);
   }
else
{
//strcpy(setdirection,"out"); write (fd, setdirection, sizeof(setdirection));
write(fd,"out",sizeof("out"));
close (fd);
}


/* ****************************************************************** */
// PREPARE TO SET OUTPUT - ONCE INTERRUPT HAPPENED
sprintf(GPIOValue_out, "/sys/class/gpio/gpio%d/value", GPIO_out);
if ((fdout = open(GPIOValue_out, O_WRONLY)) < 0) // apo O_WRONLY
  {
      fprintf(stderr, "Failed to open GPIO_out %d value.\n", GPIO_out);
      exit(1);
   } 

// GPIO_in 27   means (BCM) GPIO27 is (BOARD) pin 13
// GPIO_out 17  means (BCM) GPIO17 is (BOARD) pin 11
/* ***************** SET OUTPUT VALUES PERIODICALLY ****************** */
// Using a timer
 
    timer_fd = timerfd_create(CLOCK_REALTIME, 0); //  //timer_fd=timerfd_create(CLOCK_MONOTONIC,0);
    if (timer_fd == -1) handle_error("timerfd_create");
   
   if (clock_gettime(CLOCK_REALTIME, &now) == -1) handle_error("clock_gettime");
   timeout.it_value.tv_sec = now.tv_sec;//initial expiration time in secs 
   timeout.it_value.tv_nsec = now.tv_nsec + atoi(argv[2]); //e.g. 2000000nsecs == 2000usecs
   
   timeout.it_interval.tv_sec = 0; 
   timeout.it_interval.tv_nsec = atoi(argv[2]); 
   
   if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &timeout, NULL) == -1)  handle_error("timerfd_settime"); 
   //{printf("Failed to set the timer duration\n"); handle_error("timerfd_settime"); return 1;}
   printf("Timer started\n");

while(choice <=atoi(argv[1]))
{   
   setting=ON;     
   read(timer_fd, &periods_elapsed, sizeof(periods_elapsed));
   write(fdout, setting, sizeof (setting)); //set the VALUE
   //printf("Just WROTE %s\n", setting);	//sleep(1); // for testing purposes with a LED
   setting=OFF;

   read(timer_fd, &periods_elapsed, sizeof(periods_elapsed));
   //printf("read: periods_elapsed=%llu\n", (unsigned long long) periods_elapsed);
   write(fdout, setting, sizeof (setting)); //set the VALUE 
   //printf("Just WROTE %s\n", setting);	//sleep(1); // for testing purposes with a LED

choice++;
} // end of WHILE

close(fdout);



// ******************************************************************* 
// *************************** UNEXPORT PIN IN *************************************** 
// instead of doing from a terminal: sudo sh -c "echo 21     >/sys/class/gpio/unexport" 
if ((fd = open ("/sys/class/gpio/unexport", O_WRONLY)) <0)  //remove the mapping - UNEXPORT the GPIO Pin
{
      fprintf(stderr, "Failed to unexport gpio pin in.\n");
      exit(1);
   }
else
{
write (fd, GPIOString_in, sizeof(GPIOString_in));
close (fd);
}

// *************************** UNEXPORT PIN OUT *************************************** 
// instead of doing from a terminal: sudo sh -c "echo 4     >/sys/class/gpio/unexport" 
if ((fd = open ("/sys/class/gpio/unexport", O_WRONLY)) <0)  //remove the mapping - UNEXPORT the GPIO Pin
{
      fprintf(stderr, "Failed to unexport gpio pin out.\n");
      exit(1);
   }
else
{
write (fd, GPIOString_out, sizeof(GPIOString_out));
close (fd);
}

return 0; //   exit(0);
}
