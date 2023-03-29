#linking no-os driver 

#no-os headers
NOOS_INC 	:= $(NOOS)/include/

#no-os API
NOOS_SRC 	:= 	$(NOOS)/drivers/api/no_os_spi.c \
			    $(NOOS)/drivers/api/no_os_gpio.c \
			    $(NOOS)/drivers/api/no_os_i2c.c

#linking platform
NOOS_INC 	+= 	$(NOOS)/drivers/platform/chibios/

NOOS_SRC 	+= 	$(NOOS)/drivers/platform/chibios/chibios_spi.c \
				$(NOOS)/drivers/platform/chibios/chibios_i2c.c \
 	   	       	$(NOOS)/drivers/platform/chibios/chibios_gpio.c \
				$(NOOS)/drivers/platform/chibios/chibios_delay.c \
				$(NOOS)/drivers/platform/chibios/chibios_alloc.c 
				
#link utils
NOOS_SRC 	+= 	$(NOOS)/util/no_os_util.c

#linking drivers
NOOS_INC 	+= 	$(NOOS)/drivers/accel/adxl355/
NOOS_SRC 	+= 	$(NOOS)/drivers/accel/adxl355/adxl355.c

#linking all
ALLINC  	+= $(NOOS_INC)
ALLCSRC 	+= $(NOOS_SRC)
