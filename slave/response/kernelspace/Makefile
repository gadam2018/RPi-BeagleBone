# There must be a Tab character in front of the calls to make in the Makefile
KERN_SRC:= /lib/modules/$(shell uname -r)/build
# KERN_SRC is the location of the kernel source.  
# The current standard is to link to the associated source tree from the directory containing the compiled modules.
PWD   := $(shell pwd)
# PWD is the current working directory and the location of our module source files.
obj-m := kgpio4_slave.o				# or obj-m += kgpio4_slave.o
# obj-m is a list of what kernel modules to build.  
# The .o and other objects will be automatically built from the corresponding .c file - no need to list the source files explicitly.
# default:
#	$(MAKE) -C $( KERN_SRC) M=$(PWD) modules
# default is the default make target. 
# The rule here says to run make with a working directory of the directory containing the kernel source
# and compile only the modules in the PWD (local) directory.
all:
	make -C $(KERN_SRC) M=$(PWD) modules # will build the module(s) located in current directory
# all output files will be located in the same directory as the module source
# -C $(KERN_SRC) is used to specify where to find the kernel source.
# M= is used to tell kbuild that an external module is being built.
# make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C $(KERN_SRC) M=$(PWD) clean # Remove all generated files in current directory
# make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
