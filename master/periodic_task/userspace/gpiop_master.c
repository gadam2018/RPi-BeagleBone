// ****************************************************************
// MASTER PERIODIC TASK MODULE FOR USER SPACE MEASUREMENTS
// COMPILE: gcc gpiop_master.c -o gpiop_master -lm -lrt   // link to math library or   gcc -Wall -w gpiop1.c -std=gnu99 -lrt  
// RUN: sudo chrt -f 99 ./gpiop1_master <number-of-loops>// with priority 99
// Master and slave code must have the same number-of-loops (as runs)
// ****************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <poll.h>
#include <math.h>
#include <stdint.h> // for uint64_t

#define GPIO_in 27  // means (BCM) GPIO27 is (BOARD) pin 13 -for RASPEBRRY PI
#define GPIO_out 17 // means (BCM) GPIO17 is (BOARD) pin 11 -for RASPEBRRY PI

#define GPIO_in 13  // means GPIO1_13 is (pin 11) -for BEAGLEBONE BLACK
#define GPIO_out 16 // means GPIO1_16 is (pin 15) -for BEAGLEBONE BLACK

#define ON "1"
#define OFF "0"

int main(int argc, char *argv[])
{

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
 
long int choice=1;
 
struct timespec tms, ts_diff, ts_begin, ts_end;
struct timespec start_time, finish_time;
struct timeval initial_overall_time, final_overall_time, start_time2, finish_time2;

unsigned int i=0, next, i1, i2, mean_greater=0, mean_smaller=0;
long int *delays = (long int *) malloc(200010 * sizeof(long int));
//float *delays = (float *) malloc(atoi(argv[1])*2 * sizeof(float)); // desmeuei mnimi gia osa samples dosoume
float sum=0.0, median, mean, min=0, max=0;

int cycles;
char* setting;

int result;


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
// PREPARE TO SET OUTPUT
sprintf(GPIOValue_out, "/sys/class/gpio/gpio%d/value", GPIO_out);
if ((fdout = open(GPIOValue_out, O_WRONLY)) < 0) // apo O_WRONLY
  {
      fprintf(stderr, "Failed to open GPIO_out %d value.\n", GPIO_out);
      exit(1);
   }

//GPIO_in 27  means (BCM) GPIO27 is (BOARD) pin 13
//GPIO_out 17 means (BCM) GPIO17 is (BOARD) pin 11
/* ****************** READ INPUT VALUE  ****************** */
// Polling the input pin
	sprintf(GPIOValue_in, "/sys/class/gpio/gpio%d/value", GPIO_in);
if ((fd = open(GPIOValue_in, O_RDONLY)) < 0)  // apo O_RDONLY
  {
      fprintf(stderr, "Failed to open GPIO_in %d value.\n", GPIO_in);
      exit(1);
   }   

//struct timeval tval;
//uint64_t now;

pfd.fd = fd;
pfd.events = POLLPRI; //type of event to be monitored, POLLPRI for urgent data, POLLIN to read operations, POLLET edge triggered
gettimeofday(&initial_overall_time); 
while (choice <= atoi(argv[1])) 
{	
	lseek(fd, 0, SEEK_SET); // consume any prior interrupt
	read(fd, buf, sizeof buf);
	//printf("\n WAITING: Input pin %d value currently is %d\n", GPIO_in, atoi(buf));
	
	poll(&pfd, 1, -1);    // wait for (reads 1) interrupt 
	lseek(fd, 0, SEEK_SET); // consume interrupt
	read(fd, buf, sizeof buf);
	//printf("Input pin %d value now after 1st interrupt happened is %d\n",GPIO_in, atoi(buf));
	
	clock_gettime(CLOCK_REALTIME, &tms); ts_begin=tms; //returns the amount of secs and nsecs since 1-1-1970 at UTC (UNIX Epoch)
	//gettimeofday(&start_time);  //as timespec
	//gettimeofday(&start_time2); //as timeval
		
	poll(&pfd, 1, -1);    // wait for (reads 0) interrupt 
	lseek(fd, 0, SEEK_SET); // consume interrupt
	read(fd, buf, sizeof buf);
	//printf("Input pin %d value now after 2nd interrupt happened is %d\n",GPIO_in, atoi(buf));
    	
	clock_gettime(CLOCK_REALTIME, &tms);  ts_end=tms;  
    //gettimeofday(&finish_time);  // as timespec
    //gettimeofday(&finish_time2); // as timeval
   
    //printf("gettimeofday1: elapsed in (nsecs) usecs = %ld\n", \
    ((finish_time.tv_sec *1000000000 + finish_time.tv_nsec) - (start_time.tv_sec * 1000000000 + start_time.tv_nsec)));    
   // printf("gettimeofday2: elapsed in usecs = %ld\n", \
    ((finish_time2.tv_sec *1000000 + finish_time2.tv_usec) - (start_time2.tv_sec * 1000000 + start_time2.tv_usec)));    
    //printf("clock_gettime: elapsed in usecs = %ld\n", \
    ((ts_end.tv_sec*1000000 + ts_end.tv_nsec/1000) - (ts_begin.tv_sec * 1000000 + ts_begin.tv_nsec/1000)));
               
	 delays[i]= (ts_end.tv_sec*1000000 + ts_end.tv_nsec/1000) - (ts_begin.tv_sec * 1000000 + ts_begin.tv_nsec/1000); //in microseconds 
   
    i++;    
    next =i;
       

choice++; 
}//end of WHILE

gettimeofday(&final_overall_time);
printf("Time elapsed in secs = %ld\n", final_overall_time.tv_sec - initial_overall_time.tv_sec);
printf("Time elapsed in secs = %ld\n", (final_overall_time.tv_sec +final_overall_time.tv_usec/1000000) - (initial_overall_time.tv_sec + initial_overall_time.tv_usec/1000000));

// *************************** MEASUREMENTS **************************

// *** SUM of SAMPLES ***
for(i=0; i<next; i++) 
{	
	//printf("delays[%u]=%ld, ",i,delays[i]); 
	sum=sum+delays[i]; 
 }
 
// *** SAMPLES and MEAN ***
mean=sum/next;
printf("\nSamples: %u, average (mean) latency = %.01fusecs, ", next, mean);

// *** VARIANCE and STANDARD DEVIATION ***
float variance, varsum=0, std_deviation;
for(i=0; i<next; i++) 
{
		varsum=varsum+pow((delays[i]-mean),2);
} 
variance=varsum/(next);
std_deviation=sqrt(variance);
printf("\nVariance is: %.01f, and standard deviation is: %.01f ", variance, std_deviation);

// *** MEDIAN ***
if (i%2==0) 	
{i1=(i/2)-1; i2=i1+1; median=(delays[i1]+delays[i2])/2;} 
else 			//odd no of samples
{i1=(i/2)-1; median=delays[i1];}
printf("\nmedian latency = %.01fusecs ", median);

// *** MAX ***
long unsigned int max_i=0;
for(i=0; i<next; i++) 
	if (delays[i]>=max) {max=delays[i]; max_i=i;}

// *** MIN and % smaller and % greater than mean***
long unsigned int min_i=0;
min=max;
for(i=0; i<next; i++) 
{
if (delays[i]<=min) {min=delays[i]; min_i=i;}
if (delays[i]>mean) mean_greater++; else mean_smaller++;
}
printf("min latency = %.01fusecs at i=%lu, max latency = %.01fusecs at i=%lu\n", min, min_i, max, max_i);
printf("The amount greater than mean is %u (%u%) and the amount smaller or equal is %u (%u%)\n", mean_greater, (mean_greater * 100)/next, mean_smaller, (mean_smaller * 100)/next);

//}//end of WHILE

close(fd);
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
