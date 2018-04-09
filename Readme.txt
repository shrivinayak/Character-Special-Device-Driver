Assignment 5

Copy the files char_driver.c, Makefile and cdriver.c to a virtual linux machine
and follow the following steps:

1) Compile driver module : $ make

2) Load module : $ sudo insmod cdriver.ko NUM_DEVICES=<num_devices>

3) Test driver :
	1) Compile userapp : $ make app
	2) Run userapp : $ sudo ./userapp <device_number>			
		where device_number identifies the id number of the device to be tested.   

	Note : userapp has to be executed with sudo privilege as the device files
		   in /dev/ are created in the driver with root privileges.
		   
4) Unload module : $ sudo rmmod cdriver.ko
