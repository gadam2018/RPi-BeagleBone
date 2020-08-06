// ****************************************************************
// SLAVE RESPONSE TASK MODULE FOR KERNEL SPACE MEASUREMENTS
// kernel module kgpio4_slave.c
// ****************************************************************

#include <linux/init.h>		// required for the module_init() and module_exit()macros
#include <linux/module.h>	// required by all modules
#include <linux/kernel.h>	// required for the macro expansion for the printk() log level KERN_INFO
#include <linux/gpio.h>		// Required for the GPIO functions
#include <linux/interrupt.h>  	// Required for the IRQ code
#include <linux/time.h>
#include <linux/delay.h>

static unsigned int gpio_out = 17; //  - means (BCM) GPIO11 is (BOARD) pin 11
static unsigned int gpio_in = 27;  // - means (BCM) GPIO27 is (BOARD) pin 13 

static unsigned int gpio_out = 13;    // means GPIO1_13 is (pin 11) -for BEAGLEBONE BLACK
static unsigned int gpio_in = 16;     // means GPIO1_16 is (pin 15) -for BEAGLEBONE BLACK

static unsigned int irqNumber; // share IRQ num within file
static bool ledOn = 0; // used to invert state of output test LED
static int val_in;
static struct timespec ts_previous, ts_last; //, ts_diff;

// prototype for the custom IRQ handler function
static irq_handler_t rpi_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs);

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

// This next call requests an interrupt line
result = request_irq(irqNumber, (irq_handler_t) rpi_gpio_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "rpi_gpio_handler",  NULL); 	

printk(KERN_INFO "IRQ request result is: %d\n", result);
	if(result) { printk(KERN_ERR "Cannot register IRQ %d\n", irqNumber);    return -EIO;}

 return result;
}

static irq_handler_t rpi_gpio_irq_handler(unsigned int irq, void *dev_id, struct pt_regs *regs) {
struct timeval tv2 ;
  uint64_t previous2, last2;
  //unsigned long flags;

/*  unsigned int interrupt_time = millis();

   if (interrupt_time - last_interrupt_time < 1000) //ypotheto oti if interrupts come faster than 1000ms (1sec), assume it's a bounce and ignore
   {
     printk(KERN_NOTICE "Ignored Interrupt!!!!! [%d]%s \n",
          irq, (char *) dev_id);
     return (irq_handler_t) IRQ_HANDLED;
   }
   last_interrupt_time = interrupt_time;	
*/	
	//disable hard interrupts (remember them in flag 'flags') // auta edo kai parakato, ta eixe o kodikas pou eida alla den ta energopoisa
    //local_irq_save(flags);

//ledOn = !ledOn; 				// invert the LED state

// METRISI TOU XRONOU EOS OTOU TETHEI I TIMI STIN EKSODO (1 OR 0)
/* EAN THELO METRISEIS EDO TIS ENERGOPOIO
do_gettimeofday(&tv2) ; //2os tropos metrisis. telika se usecs alla oxi akrivis
previous2  = (uint64_t)(tv2.tv_usec) ; //microseconds
getnstimeofday(&ts_previous); //1os tropos metrisis se nsecs (xreiazetai /1000 -> usecs) alla akrivis!
*/
val_in=gpio_get_value(gpio_in);
gpio_set_value(gpio_out, val_in);//ledOn); 		// set gpio_out (LED) accordingly			
/* EAN THELO METRISEIS EDO TIS ENERGOPOIO
getnstimeofday(&ts_last);
do_gettimeofday(&tv2) ;
last2  = (uint64_t)(tv2.tv_usec) ;
printk(KERN_NOTICE "\nInterrupt [%d] for device %s was triggered !.\n", irq, (char *) dev_id);
printk(KERN_INFO "Interrupt! (gpio_in is %d)\n", gpio_get_value(gpio_in));
printk(KERN_INFO  "time elapsed in nanosecs = %lu\n", ts_last.tv_nsec - ts_previous.tv_nsec);
printk(KERN_INFO  "time again elapsed in usecs= %lld\n", (long long) (last2-previous2)); // I CANNOT USE FP OPERATIONS IN KERNEL
*/
//msleep(100); // 100ms = 0.1sec gia na prolavo na do to LED //or? mdelay(1000); //mdelay(unsigned long ms) //1sec ????
//gpio_set_value(gpio_out, 0); //gia tis dokimes me to LED

   // restore hard interrupts
   //local_irq_restore(flags);

return (irq_handler_t) IRQ_HANDLED; // announce IRQ handled
}


// The exit function is run once, upon module unloading.
static void __exit kgpio_exit(void)
{
printk(KERN_INFO "gpio_in value is currently: %d\n", gpio_get_value(gpio_in));
gpio_set_value(gpio_out, 0); 		// turn the gpio_out (LED) off
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
MODULE_DESCRIPTION("RaspberryPi3 kgpio4.c ");
