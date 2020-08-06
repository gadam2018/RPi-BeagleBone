// ****************************************************************
// SLAVE PERIODIC TASK MODULE FOR KERNEL SPACE MEASUREMENTS
// kernel module kgpiop2_slave.c
//INSMOD: e.g. sudo insmod kgpiop2.ko  (runs with default value for reps=2 and default value for period_nsecs=7500000 == 7.500usecs)
//INSMOD: e.g. sudo insmod kgpiop2.ko reps=4 period_nsecs=7500000 (or for new values for the parameters inside the module)
// The number of reps must be always even (e.g. 2, 4, 6, etc.) and twice as large as the number-of-loops in master module kgpiop4_master

// Another device is used to generate an interrupt on the rising edge.
// (output is OFF - gpio_out pin is 0)
// ****************************************************************

#include <linux/init.h>		      // required for the module_init() and module_exit()macros
#include <linux/module.h>	      // required by all modules
#include <linux/moduleparam.h>	 // in order to pass command line arguments to module's parameters
#include <linux/kernel.h>		 // required for the macro expansion for the printk() log level KERN_INFO
#include <linux/gpio.h>		     // Required for the GPIO functions
#include <linux/interrupt.h>  	 // Required for the IRQ code
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>

static unsigned  int reps=2; //default is 2 reps meaning 1 ON & OFF cycle
static unsigned  int period_nsecs=7500000; // default is period_nsecs= 7500000nsecs == 7.500usecs == 7.5ms
module_param(reps, int, 0644); // permission=0644 - the owner (root) can both read and write, and everyone else can read
module_param(period_nsecs, int, 0644); 

static unsigned int gpio_out = 17; //  (BCM) GPIO11 is (BOARD) pin 11
static unsigned int gpio_in = 27;  // (BCM) GPIO27 is (BOARD) pin 13 

static unsigned int gpio_out = 13;    // means GPIO1_13 is (pin 11) -for BEAGLEBONE BLACK
static unsigned int gpio_in = 16;     // means GPIO1_16 is (pin 15) -for BEAGLEBONE BLACK

static unsigned int irqNumber; // share IRQ num within file
static bool ledOn = 0; // used to invert state of LED
//static struct timespec ts_previous, ts_last; //, ts_diff;

// prototype for the custom IRQ handler function
static irq_handler_t rpi_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);

static struct hrtimer high_res_timer;
ktime_t ktime;

static unsigned long int j=0;
//static unsigned long int reps=0;


// High res timer's function
static enum hrtimer_restart timer_func(struct hrtimer* timer)
{
 ktime_t now;
 int overrun;
// printk(KERN_INFO "This is timer function\n");	
 
ledOn = !ledOn; 
gpio_set_value(gpio_out, ledOn); 		// set gpio_out (LED) accordingly			
//printk(KERN_INFO  "Output is %d\n",ledOn); //ON/OFF

 ktime=ktime_set(0, period_nsecs); //set timer in nsecs
 now=hrtimer_cb_get_time(timer);
 overrun=hrtimer_forward(timer,now,ktime);
 j++; 
 if(j>=reps) { 
	 int cancelled = hrtimer_cancel(timer);
	 if(cancelled) printk(KERN_INFO "Timer still running\n"); else printk(KERN_INFO "Timer cancelled\n");
	 return HRTIMER_NORESTART;
	 }
 
 return HRTIMER_RESTART; 
}


//****************************************************************************
// Interrupts variables block                                               
//****************************************************************************
unsigned int last_interrupt_time = 0;
static uint64_t epochMilli;

unsigned int millis (void)
{
  struct timeval tv ;
  uint64_t now ;

  do_gettimeofday(&tv) ;
  now  = (uint64_t)tv.tv_sec * (uint64_t)1000 + (uint64_t)(tv.tv_usec / 1000) ;

  return (uint32_t)(now - epochMilli) ;
}
//****************************************************************************

static int __init kgpio_init(void)
{
int result = 0;       
printk(KERN_INFO "kernel gpio module initialization \n"); 

printk(KERN_INFO "Value of reps=%d\n", reps);
if (!gpio_is_valid(gpio_out)) { printk(KERN_INFO "Invalid output GPIO\n"); return -ENODEV;}
ledOn = false;//ledOn = true;
gpio_request(gpio_out, "sysfs"); 			// request gpio_out (LED GPIO)
gpio_direction_output(gpio_out, ledOn); 	// set in output mode and on
// gpio_set_value(gpio_out, ledOn); 		// not required - see line above
gpio_export(gpio_out, false); 				// appears in /sys/class/gpio
											// false prevents in/out change
gpio_request(gpio_in, "sysfs"); 			// set up gpio_in (Button)
gpio_direction_input(gpio_in); 				// set up as input

gpio_export(gpio_in, false); 				// appears in /sys/class/gpio

printk(KERN_INFO "gpio_in value is currently: %d\n", gpio_get_value(gpio_in));
irqNumber = gpio_to_irq(gpio_in); 		// map GPIO to IRQ number
printk(KERN_INFO "gpio_in mapped to IRQ: %d\n", irqNumber);

// interrupt line request
result = request_irq(irqNumber, (irq_handler_t) rpi_gpio_irq_handler, IRQF_TRIGGER_RISING, "rpi_gpio_handler",  NULL); 		
printk(KERN_INFO "IRQ request result is: %d\n", result);
	if(result) { printk(KERN_ERR "Cannot register IRQ %d\n", irqNumber);    return -EIO;}

// Initialization of the timer
ktime=ktime_set(0, period_nsecs); //set timer in nsecs
hrtimer_init(&high_res_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
high_res_timer.function = &timer_func;  // when the timer expires
printk(KERN_INFO "High resolution timer initialized\n");

return result;
} // END of kgpio_init


static irq_handler_t rpi_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs) {
  //struct timeval tv2 ;
  //uint64_t previous2, last2;
  //unsigned long flags;


/* TESTING MEASUREMENTS
do_gettimeofday(&tv2) ; //2os tropos metrisis. telika se usecs alla oxi akrivis
previous2  = (uint64_t)(tv2.tv_usec) ; //microseconds
getnstimeofday(&ts_previous); //1os tropos metrisis se nsecs (xreiazetai /1000 -> usecs) alla akrivis!
*/

hrtimer_start(&high_res_timer, ktime, HRTIMER_MODE_REL);

/* TESTING MEASUREMENTS
getnstimeofday(&ts_last);
do_gettimeofday(&tv2) ;
last2  = (uint64_t)(tv2.tv_usec) ;
printk(KERN_NOTICE "\nInterrupt [%d] for device %s was triggered !.\n", irq, (char *) dev_id);
printk(KERN_INFO "Interrupt! (gpio_in is %d)\n", gpio_get_value(gpio_in));
printk(KERN_INFO  "time elapsed in nanosecs = %lu\n", ts_last.tv_nsec - ts_previous.tv_nsec);
printk(KERN_INFO  "time again elapsed in usecs= %lld\n", (long long) (last2-previous2)); // I CANNOT USE FP OPERATIONS IN KERNEL
*/

return (irq_handler_t) IRQ_HANDLED; // announce IRQ handled
}


// The exit function is run once, upon module unloading.
static void __exit kgpio_exit(void)
{
//printk(KERN_INFO "gpio_in value is currently: %d\n", gpio_get_value(gpio_in));
//gpio_set_value(gpio_out, 0); 		// turn the gpio_out (LED) off
gpio_unexport(gpio_out); 			// unexport the gpio_out (LED GPIO)
free_irq(irqNumber, NULL); 		// free the IRQ number, no *dev_id
gpio_unexport(gpio_in); 			// unexport the gpio_in (Button GPIO)
gpio_free(gpio_out); 			// free the gpio_out (LED GPIO)
gpio_free(gpio_in); 			// free the gpio_in (Button GPIO)
printk(KERN_INFO "Finish\n");
}

module_init(kgpio_init);
module_exit(kgpio_exit);

MODULE_AUTHOR("George K. Adam");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RaspberryPi3 kgpio2.c ");
