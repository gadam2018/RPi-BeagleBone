# RPi-BeagleBone
PREEMPT_RT latency measurements in Linux kernels and distributions (Ubuntu, Arch Linux, and Debian) running on RPi and BeagleBone
The slave devices under test (RaspberryPi and BeagleBone) are connected to, and communicate with, another Raspberry (master) that performs the actual measurements. Measurements include the latency of response tasks in user and kernel space, the response at specific periodic rates on execution of periodic tasks in user and kernel space, the maximum sustained frequency, min, max, std.deviation and variance.

Installation (mainly for RPis)
Install the software source files found respectively in master and slave folders into the master and slave device respectively.
For user and kernel space response task measurements: in the master COMPILE: gcc gpio_master.c -o gpio_master -lm -lrt -lpthread in the slave COMPILE: gcc gpio_slave.c -o gpio_slave -lrt -lpthread
For user space periodic task measurements: in the master COMPILE: gcc gpiop_master.c -o gpiop_master -lm -lrt in the slave COMPILE: gcc gpiop_slave.c -o gpiop_slave -lrt
For kernel space periodic task measurements: in the master COMPILE: gcc kgpiop_master.c -o kgpiop_master -lm -lrt

Usage
Response task measurements at user space Run the slave first, then the master to perform the measurements. in the slave device run: sudo ./gpio_master in the master device run: RUN: sudo ./gpio_slave
Response task measurements at kernel space: Run the slave first, then the master to perform the measurements. in the slave device run: make and sudo insmod kgpio4_slave.ko in the master device run: RUN: sudo ./gpio_slave
Periodic task measurements at user space: in the slave device run: sudo chrt -f 99 ./gpiop_slave in the master device run: sudo chrt -f 99 ./gpiop1_master
Periodic task measurements at kernel space: in the slave device run: make and sudo insmod kgpiop2.ko in the master device run: sudo chrt -f 99 ./kgpiop_master 
