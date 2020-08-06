// ****************************************************************
// SLAVE RESPONSE TASK MODULE FOR USER SPACE MEASUREMENTS
// COMPILE: gcc gpio_slave.c -o gpio_slave -lrt  -lpthread    or   gcc -Wall -w gpio1.c -std=gnu99 -lrt  
// RUN: sudo ./gpio_slave <number-of-loops> // x 2 => runs, with FIFO & priority 99
// ****************************************************************

#define _GNU_SOURCE // for affinity routines -> sched_setaffinity()
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#if defined(_POSIX_MEMLOCK)
   #include <sys/mman.h>
#else
   #warning mlockall is not available
#endif      
#include <time.h>
// #include <sys/time.h> ???
#include <signal.h>
#include <poll.h>
#include <math.h>
#include <sched.h> // for affinity sched_getaffinity() and sched_setaffinity() routines, set_scheduler
#include <errno.h>
#include <pthread.h>
#include <string.h>

//error codes
#ifdef CODES
#define EPERM        1  /* Operation not permitted */
#define ENOENT       2  /* No such file or directory */
#define ESRCH        3  /* No such process */
#define EINTR        4  /* Interrupted system call */
#define EIO          5  /* I/O error */
#define ENXIO        6  /* No such device or address */
#define E2BIG        7  /* Argument list too long */
#define ENOEXEC      8  /* Exec format error */
#define EBADF        9  /* Bad file number */
#define ECHILD      10  /* No child processes */
#define EAGAIN      11  /* Try again */
#define ENOMEM      12  /* Out of memory */
#define EACCES      13  /* Permission denied */
#define EFAULT      14  /* Bad address */
#define ENOTBLK     15  /* Block device required */
#define EBUSY       16  /* Device or resource busy */
#define EEXIST      17  /* File exists */
#define EXDEV       18  /* Cross-device link */
#define ENODEV      19  /* No such device */
#define ENOTDIR     20  /* Not a directory */
#define EISDIR      21  /* Is a directory */
#define EINVAL      22  /* Invalid argument */
#define ENFILE      23  /* File table overflow */
#define EMFILE      24  /* Too many open files */
#define ENOTTY      25  /* Not a typewriter */
#define ETXTBSY     26  /* Text file busy */
#define EFBIG       27  /* File too large */
#define ENOSPC      28  /* No space left on device */
#define ESPIPE      29  /* Illegal seek */
#define EROFS       30  /* Read-only file system */
#define EMLINK      31  /* Too many links */
#define EPIPE       32  /* Broken pipe */
#define EDOM        33  /* Math argument out of domain of func */
#define ERANGE      34  /* Math result not representable */
#endif

#define GPIO_in 27  // means (BCM) GPIO27 is (BOARD) pin 13 -for RASPEBRRY PI
#define GPIO_out 17 // means (BCM) GPIO17 is (BOARD) pin 11 -for RASPEBRRY PI

#define GPIO_in 13  // means GPIO1_13 is (pin 11) -for BEAGLEBONE BLACK
#define GPIO_out 16 // means GPIO1_16 is (pin 15) -for BEAGLEBONE BLACK

#define ON "1"
#define OFF "0"

#define LOW 0
#define HIGH 1

void *thread_function(void *);


// Setting process scheduling algorithm to FIFO with priority 1
set_process_scheduler()
{
	struct sched_param param;
	const struct sched_param priority = {99}; //1
	param.sched_priority = 99; //max_prio; //1
		
	//if (sched_setscheduler(0, SCHED_FIFO, &priority) == -1) //SCHED_FIFO
	  if (sched_setscheduler(getpid(), SCHED_FIFO, &priority) == -1) //SCHED_FIFO
	    fprintf(stderr,"%s Error setting scheduler ... (root privileges needed)\n",strerror(errno));
    
     //if (sched_getparam(0, &param) == -1) 
        // fprintf(stderr,"%s Error getting priority\n",strerror(errno));
	 printf("Scheduler priority is %d\n",param.sched_priority );
	 printf("FIFO scheduler max priority is:%d\n",sched_get_priority_max(SCHED_FIFO));
    
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
sched_getaffinity(0,sizeof(myset),&myset); // 0 means current process
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


//SET PROCESS AFFINITY
void set_process_affinity() //ONLY TO FIRST CORE
{
	cpu_set_t myset;
	CPU_ZERO(&myset);
	CPU_SET(0,&myset);
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
 
#if defined(_POSIX_MEMLOCK)
 if(mlockall(MCL_CURRENT | MCL_FUTURE)<0)  // requires <sys/mman.h>
   {perror("Could not lock process in memory"); exit(2);}
#else
   #warning mlockall is not available
#endif

// Setting process scheduling algorithm to FIFO with priority 1
set_process_scheduler();
get_process_scheduler();
get_process_affinity();
set_process_affinity();
get_process_affinity();

long int arg=atoi(argv[1]);

// Creating the thread
	pthread_create( &thread_id, NULL, thread_function, (void *) arg );  
	//pthread_create( &thread_id, NULL, thread_function, NULL );   
// Waiting for the thread to finish
    pthread_join( thread_id, NULL); 

}
	
//void *thread_function(void *dummyPtr){
void *thread_function(void *arg2){

#if defined(_POSIX_MEMLOCK)
 if(mlockall(MCL_CURRENT | MCL_FUTURE)<0)  // requires <sys/mman.h>
   {perror("Could not lock thread in memory"); exit(2);}
#else
   #warning mlockall is not available
#endif

long int arg=(long int) arg2;
printf("arg=%ld\n",arg);

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
 
time_t currenttime;



//Setting THREAD scheduling algorithm to FIFO and priority 99
struct sched_param the_priority;
the_priority.sched_priority = 99; //1
pthread_setschedparam(pthread_self(), SCHED_FIFO, &the_priority);

//Setting THREAD AFFINITY
get_thread_affinity();
set_thread_affinity(0);
get_thread_affinity();
   
   
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


//* ******************************* EDGE both or rising or falling *********************************** *
//* instead of doing from a terminal:  sudo sh -c "echo rising >/sys/class/gpio/gpio27/edge" *
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
      fprintf(stderr, "Failed to open GPIO_out %d value for writing.\n", GPIO_out);
      exit(1);
   }

//while(choice<=arg){


// GPIO_in 27   means (BCM) GPIO27 is (BOARD) pin 13
// GPIO_out 17  means (BCM) GPIO17 is (BOARD) pin 11
/* ***************** READ INPUT VALUE & SET OUTPUT ****************** */
// Polling the input pin
sprintf(GPIOValue_in, "/sys/class/gpio/gpio%d/value", GPIO_in);
if ((fd = open(GPIOValue_in, O_RDONLY|O_NONBLOCK)) < 0)  //O_RDWR|O_NONBLOCK)) < 0) 
  {
      fprintf(stderr, "Failed to open GPIO_in %d value for reading.\n", GPIO_in);
      exit(1);
   }

static const char buf2[]="01";  
int bytes,resp;
pfd.fd = fd;
pfd.events = POLLPRI; //type of event to be monitored, POLLPRI  for urgent data, POLLIN to read operations, POLLET edge triggered   
//lseek(fd, 0, SEEK_SET); // consume any prior interrupt
//read(fd, buf, sizeof buf);

lseek(fd, 0, SEEK_SET); // consume any prior interrupt
read(fd, buf, sizeof buf);


while(choice<=arg)
{ 
	
poll(&pfd, 1, -1);    	// wait for interrupt 

lseek(fd, 0, SEEK_SET); // consume interrupt
read(fd, buf, sizeof buf);
//if(bytes=read(fd, buf, sizeof(buf))==-1) fprintf(stderr, "Failed to read.\n"); //printf("Bytes read: %d\n",bytes);
printf("\rCycle:%u Input now after interrupt has happened is %d",choice-1, atoi(buf)); //itan APENERGOPOIIMENO   

//lseek(fd, 0, SEEK_SET); 

write(fdout, &buf2[LOW == atoi(buf) ? 0 : 1], 1);

//printf("Just WROTE %s\n", buf);	//sleep(1); // for testing with the LED  //itan APENERGOPOIIMENO
		//if (strcmp(buf,"1")==0) printf("1"); else printf("0"); thelei diorthosi den douleuei i sygkrisi
choice++; 
// close(fd);
 } // end of WHILE

close(fd);
close(fdout);

printf("\n");

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
//free(delays);
return 0; //   exit(0);
}
