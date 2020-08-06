// ****************************************************************
// MASTER RESPONSE TASK MODULE FOR USER & KERNEL SPACE MEASUREMENTS
//COMPILE: gcc gpio_master.c -o gpio_master -lm -lrt  -lpthread // link to math library or   gcc -Wall -w gpio1.c -std=gnu99 -lrt  
//RUN: sudo ./gpio1_master <number-of-loops>// x 2 => runs, with FIFO & priority 99
// ****************************************************************

#define _GNU_SOURCE // for affinity routines -> sched_setaffinity()

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#if defined (_POSIX_MEMLOCK)
    #include <sys/mman.h>
#else
    #warning mlockall is not available
#endif    
#include <time.h>
#include <signal.h> 
#include <poll.h>
#include <math.h>
#include <sched.h> // for affinity sched_getaffinity() and sched_setaffinity() routines set_scheduler
#include <errno.h>
#include <pthread.h>
#include <string.h>

#define GPIO_in 27  // means (BCM) GPIO27 is (BOARD) pin 13 -for RASPEBRRY PI
#define GPIO_out 17 // means (BCM) GPIO17 is (BOARD) pin 11 -for RASPEBRRY PI

#define GPIO_in 13  // means GPIO1_13 is (pin 11) -for BEAGLEBONE BLACK
#define GPIO_out 16 // means GPIO1_16 is (pin 15) -for BEAGLEBONE BLACK

#define ON "1"
#define OFF "0"


void *thread_function(void *);


// Setting process scheduling algorithm to FIFO with priority 99
set_process_scheduler()
{
	struct sched_param param;
	const struct sched_param priority = {99};
	param.sched_priority = 99; //max_prio; 
		
   //if (sched_setscheduler(0, SCHED_FIFO, &priority) == -1) //SCHED_FIFO
   if (sched_setscheduler(getpid(), SCHED_FIFO, &priority) == -1) //SCHED_FIFO
	    fprintf(stderr,"%s Error setting scheduler ... (root privileges needed)\n",strerror(errno));
    
     //if (sched_getparam(0, &param) == -1) 
        // fprintf(stderr,"%s Error getting priority\n",strerror(errno));
	 printf("Scheduler priority is %d\n",param.sched_priority );
    
       // Set the priority of the process, 
    //param.sched_priority = 1;
   // if (sched_setparam(0, &param) == -1) 
     //   fprintf(stderr,"%s Error setting priority\n",strerror(errno));
    // printf("Scheduler current priority is %d\n",param.sched_priority );
}


// Getting process scheduling algorithm
get_process_scheduler()
{
switch(sched_getscheduler(getpid()))
{
	case SCHED_OTHER: printf("PROCESS scheduler is SCHED_OTHER scheduler\n"); break; // ROUND ROBIN POLICY
	case SCHED_FIFO: printf("PROCESS scheduler is SCHED_FIFO scheduler\n"); break; 
	case SCHED_RR: printf("PROCESS scheduler is SCHED_RR scheduler\n"); break; 
	default:  printf("Not known scheduler\n"); break;
} 
} 


// Getting process affinity
void get_process_affinity()
{
char str[80];
int pid=getpid();
int count=0, k;
cpu_set_t myset;
CPU_ZERO(&myset);
sched_getaffinity(0,sizeof(myset),&myset); // 0 - means current process
strcpy(str," ");
for(k=0; k<CPU_SETSIZE;++k)
{
	if(CPU_ISSET(k,&myset))
	{
		++count;
		char cpunum[3];
		sprintf(cpunum,"%d",k);
		strcat(str,cpunum);
	}
}
printf("The process with pid %d has affinity %d CPUs ... %s\n",pid, count, str);
}

//Setting process affinity e.g. to core 0 
void set_process_affinity()
{
cpu_set_t myset;
CPU_ZERO(&myset);
CPU_SET(0,&myset);  //0 
sched_setaffinity(0,sizeof(myset),&myset); 
}

// Getting thread affinity
void get_thread_affinity()
{
int pid, k, count=0;
char str[80];
cpu_set_t myset;
CPU_ZERO(&myset);
pthread_getaffinity_np(pthread_self(),sizeof(myset),&myset);
pid=pthread_self();
strcpy(str," ");
for(k=0; k<CPU_SETSIZE;++k)
{
	if(CPU_ISSET(k,&myset))
	{
		++count;
		char cpunum[3];
		sprintf(cpunum,"%d",k);
		strcat(str,cpunum);
	}
}
printf("The thread %d has affinity %d CPUs ... %s\n",pid, count, str);
}

// Setting thread affinity
void set_thread_affinity(int core)
{
cpu_set_t myset;
CPU_ZERO(&myset);
CPU_SET(core,&myset);
pthread_setaffinity_np(pthread_self(),sizeof(myset),&myset); 
}


int main(int argc, char *argv[])
{

 pthread_t thread_id;
 
 //long int arg=(long int) atoi(argv[1]);

#if defined (_POSIX_MEMLOCK)
if( mlockall(MCL_CURRENT | MCL_FUTURE)<0)  // requires <sys/mman.h>
    { perror("Could not lock process in memory"); exit(2);}  
#else
    #warning mlockall is not available
#endif 

// Setting process scheduling algorithm to FIFO with priority 1
set_process_scheduler();
get_process_scheduler();
get_process_affinity();
set_process_affinity();
get_process_affinity();

long int arg=atoi(argv[1]); //printf("ärg=%ld\n",arg);

// Creating the thread
   pthread_create( &thread_id, NULL, thread_function, (void *) arg );   
// Waiting for the thread to finish
   pthread_join( thread_id, NULL); 

}
	
void *thread_function(void *arg2)
{
	
#if defined (_POSIX_MEMLOCK)
if( mlockall(MCL_CURRENT | MCL_FUTURE)<0)  // requires <sys/mman.h>
    { perror("Could not lock thread in memory"); exit(2);}  
#else
    #warning mlockall is not available
#endif 
	
long int arg=(long int) arg2; //printf("arg=%ld\n",arg2); //pthread_exit(NULL);
printf("arg=%ld\n",arg);

//struct timeval tv;
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
 
struct timespec tms, ts_diff, ts_begin, ts_end, stimuli;
struct timespec start_time, finish_time;

unsigned int next, i1, i2, mean_greater=0, mean_smaller=0;

float *delays = (float *) malloc(arg *2 * sizeof(float));

float sum=0.0, median, mean, min=0, max=0, badmax=0;

int cycles;
char* setting;

int result;

time_t currenttime;


//Setting THREAD scheduling algorithm to FIFO and priority 9
struct sched_param the_priority;
the_priority.sched_priority = 99;
pthread_setschedparam(pthread_self(), SCHED_FIFO, &the_priority);

//Setting THREAD AFFINITY
get_thread_affinity();
set_thread_affinity(0);
get_thread_affinity();

// short start delay
printf("\nWaiting a few secs for the other device to get ready\n");
sleep(4); //delay e.g. 4 sec to give time for the other device to get ready

currenttime=time(NULL);
printf(ctime(&currenttime));



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




//* ******************************* EDGE both or rising or falling *********************************** 
//* instead of doing from a terminal:  sudo sh -c "echo rising >/sys/class/gpio/gpio27/edge" 
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
/* ****************** READ INPUT VALUE & SET OUTPUT ****************** */
// Polling the input pin
	sprintf(GPIOValue_in, "/sys/class/gpio/gpio%d/value", GPIO_in);
if ((fd = open(GPIOValue_in, O_RDONLY|O_NONBLOCK)) < 0)  // apo O_RDONLY
  {
      fprintf(stderr, "Failed to open GPIO_in %d value.\n", GPIO_in);
      exit(1);
   }   

unsigned int i=0;

pfd.fd = fd;
//type of event to be monitored, POLLPRI for urgent data, POLLIN to read operations, POLLET edge triggered
pfd.events = POLLPRI; //POLLPRI; //POLLPRI|POLLIN; 

int bytes;
static const char newsetting[]="10";
gettimeofday(&start_time);
//int random_stimuli_time=50;  //50 microseconds
stimuli.tv_sec=0;  // for clock_nanosleep(CLOCK_REALTIME, 0,&stimuli, NULL); parakato

while(choice <= arg) 
{	
setting=ON; //write(fdout,"rising",sizeof("rising"));

for(cycles=0;cycles<2;cycles++)
{	
	
// TRIGGERING the other device (for CYCLE=0 sets Output=1, for CYCLE=1 sets Output=)
lseek(fd, 0, SEEK_SET);   //1 // consume interrupt //positions the seek offset at first char at beginning of file
read(fd, buf, sizeof buf); //2	

write(fdout, setting, sizeof (setting)); //3 //set the VALUE

//if(clock_gettime(CLOCK_REALTIME, &tms)==-1) printf("Failure to get start time\n"); else ts_begin=tms;
clock_gettime(CLOCK_REALTIME, &tms); ts_begin=tms;
poll(&pfd, 1, -1); 	
//if(clock_gettime(CLOCK_REALTIME, &tms)==-1) printf("Failure to get end time\n"); else  ts_end=tms;
clock_gettime(CLOCK_REALTIME, &tms);  ts_end=tms;

stimuli.tv_nsec=2000000; // ==2ms up to 10000000 ==10ms 

clock_nanosleep(CLOCK_REALTIME, 0,&stimuli, NULL);

delays[i]= (ts_end.tv_nsec - ts_begin.tv_nsec) ; 
//if ( (ts_end.tv_nsec - ts_begin.tv_nsec) <0)  
if ( delays[i] <0)   
delays[i]= delays[i] + 1000000000; 

lseek(fd, 0, SEEK_SET);    // consume interrupt
read(fd, buf, sizeof buf);

//printf("\rCycle:%u Input now after interrupt happened is %d",i, atoi(buf));  	
//delays[i]= (ts_end.tv_nsec - ts_begin.tv_nsec) ;// in microseconds  
//if ( (ts_end.tv_nsec - ts_begin.tv_nsec) <0)   
//delays[i]= delays[i] + 1000000000;              

i++;    
		//printf(" %u ",i);  
next =i;
       
setting=OFF; //write(fdout,"falling",sizeof("falling"));
} // end of FOR
choice++; 
} //end of WHILE


close(fd);
close(fdout);



//#ifdef MEASUREMENTS - FOR TESTING PURPOSES ONLY

// *************************** MEASUREMENTS **************************
#define BAD 500 // define the limit above any sample of interrupt latency is considered to be wrong measurement - not to take in consideration
gettimeofday(&finish_time);
printf("\nTotal testing Time elapsed in secs = %lu\n", finish_time.tv_sec - start_time.tv_sec);

float badsum=0.0, badmean;
long int L50to100=0, L100toBAD=0, L200toBAD=0;
// *** SUM of SAMPLES ***
long unsigned int bad_positive=0, bad_negative=0, bad_samples=0;

for(i=0; i<next; i++) //convert from nsecs to usecs
{
	delays[i] = delays[i] /1000;
	//printf("delay[%u] = %f usecs\n", i, delays[i]);
}

for(i=1; i<next; i++) //ALWAYS IGNORE THE FIRST DELAY, IS ALWAYS SOMETHING BAD
{	
	if(delays[i]>=0 && delays[i]<BAD) sum=sum+delays[i]; 
	else if (delays[i]>BAD) bad_positive++; else if (delays[i]<0) bad_negative++;
	//if (delays[i]>BAD || delays[i]<0) {printf(" delays[%u]=%ld, ",i,delays[i]);} //} //{printf(" delays[%u]=%.01f, ",i,delays[i]);} //printf(" stimulis[%u]=%ld usecs",i,stimulis[i]);	}
    badsum=badsum+delays[i]; 
   if (delays[i]<BAD && delays[i]>100) {L100toBAD++; }//printf(" [%u]=%ld, ",i,delays[i]);} 
    if (delays[i]<BAD && delays[i]>200) {L200toBAD++; } //printf(" [%u]=%ld, ",i,delays[i]);} 
    if (delays[i]<=100 && delays[i]>=50) L50to100++;
   //if (delays[i]>BAD) printf(" BAD[%u]=%ld, ",i,delays[i]); 
 }
 bad_samples=bad_positive+bad_negative;
 printf("\nTotal BAD<samples=%lu Bad_positive=%lu and Bad_negative=%lu", bad_samples, bad_positive,bad_negative);
 
 printf("\nL100toBAD=%ld",L100toBAD); 
 printf("\nL200toBAD=%ld",L200toBAD); 
 printf("\nL50to100=%ld",L50to100); 

 
// *** SAMPLES and MEAN ***
mean=sum/(next-bad_samples);
badmean=badsum/next;
printf("\nSamples: %u, average (mean) latency = %.01fusecs, ", next, mean);
printf("\nSamples: %u, average (badmean - with all bads) latency = %.01fusecs, ", next, badmean);

// *** VARIANCE and STANDARD DEVIATION ***
float variance, varsum=0, std_deviation;
for(i=1; i<next; i++) //ALWAYS IGNORE THE FIRST DELAY, IS ALWAYS SOMETHING BAD
{
	if(delays[i]>0 && delays[i]<BAD) 
		varsum=varsum+pow((delays[i]-mean),2);
} 
variance=varsum/(next-bad_samples);
std_deviation=sqrt(variance);
printf("\nVariance is: %.01f, and standard deviation is: %.01f ", variance, std_deviation);

// *** MEDIAN ***
if (i%2==0) 	// even no of samples, vasika always even tha einai
{i1=(i/2)-1; i2=i1+1; median=(delays[i1]+delays[i2])/2;} 
else 			//odd no of samples
{i1=(i/2)-1; median=delays[i1];}
printf("\nmedian latency = %.01fusecs ", median);

// *** MAX ***
long unsigned int max_i=0;
long unsigned int badmax_i=0;
for(i=1; i<next; i++) //ALWAYS IGNORE THE FIRST DELAY, IS ALWAYS SOMETHING BAD
{
	if (delays[i]>max && delays[i]<BAD) {max=delays[i]; max_i=i;}
	if (delays[i]>max) {badmax=delays[i]; badmax_i=i;}
}	

// *** MIN and % smaller and % greater than mean***
long unsigned int min_i=0;
min=max;
for(i=1; i<next; i++) //ALWAYS IGNORE THE FIRST DELAY, IS ALWAYS SOMETHING BAD
{
if (delays[i]<min && delays[i]>0) {min=delays[i]; min_i=i;}
if (delays[i]>mean) mean_greater++; else mean_smaller++;
}
printf("min latency = %.01fusecs, max latency [%ld]= %.01fusecs\n", min, max_i, max);
printf("badmax latency = %.01fusecs\n", badmax);
printf("The amount greater than mean is %u (%u%) and the amount smaller or equal is %u (%u%)\n", mean_greater, (mean_greater * 100)/next, mean_smaller, (mean_smaller * 100)/next);


//#endif /*  MEASUREMENTS */

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

free(delays);
return 0; //   exit(0);
}
