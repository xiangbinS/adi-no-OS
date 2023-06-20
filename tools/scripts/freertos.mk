ifeq ($(FREERTOS_PLUS),)
$(error $(ENDL)$(ENDL)FREERTOS_PLUS not defined\
		$(ENDL)$(ENDL)\
		Please run command "export FREERTOS_PLUS=/path/to/freeRTOS-Plus"$(ENDL))
endif # FREERTOS_PLUS check

ifeq ($(FREERTOS_KERNEL),)
$(error $(ENDL)$(ENDL)FREERTOS_KERNEL not defined\
		$(ENDL)$(ENDL)\
		Please run command "export FREERTOS_KERNEL=/path/to/FreeRTOS-Kernel"$(ENDL))
endif # FREERTOS_KERNEL check

# link freeRTOS kernel files
RTOS_INCS = $(FREERTOS_KERNEL)/include/projdefs.h \
        $(FREERTOS_KERNEL)/include/FreeRTOS.h \
        $(FREERTOS_KERNEL)/include/list.h \
        $(FREERTOS_KERNEL)/include/portable.h \
        $(FREERTOS_KERNEL)/include/deprecated_definitions.h \
        $(FREERTOS_KERNEL)/include/mpu_wrappers.h \
        $(FREERTOS_KERNEL)/include/newlib-freertos.h \
        $(FREERTOS_KERNEL)/include/queue.h \
        $(FREERTOS_KERNEL)/include/stack_macros.h \
        $(FREERTOS_KERNEL)/include/semphr.h \
        $(FREERTOS_KERNEL)/include/task.h \
        $(FREERTOS_KERNEL)/include/timers.h \
        /home/robi/Workspace_no-os/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/portmacro.h # TO DO: make port finder generic, based on platform


#Link heap implementation
RTOS_SRCS = $(FREERTOS_KERNEL)/portable/MemMang/heap_2.c

RTOS_SRCS += $(FREERTOS_KERNEL)/tasks.c \
        $(FREERTOS_KERNEL)/list.c \
        $(FREERTOS_KERNEL)/queue.c \
        $(FREERTOS_KERNEL)/timers.c 


# TO DO: make freeRTOSConfig file platform specific
RTOS_INCS += /home/robi/Workspace_no-os/no-os/projects/iio_demo_freeRTOS_CLI/src/platform/maxim/FreeRTOSConfig.h

RTOS_SRCS += /home/robi/Workspace_no-os/FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c

# linking freeRTOS Command Line-Interface files
RTOS_INCS += $(FREERTOS_PLUS)/Source/FreeRTOS-Plus-CLI/FreeRTOS_CLI.h 
RTOS_SRCS += $(FREERTOS_PLUS)/Source/FreeRTOS-Plus-CLI/FreeRTOS_CLI.c


RTOS_SRCS += /home/robi/Downloads/SEGGER_SysView/Src/Sample/FreeRTOSV10.4/Config/Cortex-M/SEGGER_SYSVIEW_Config_FreeRTOS.c
RTOS_INCS += /home/robi/Downloads/SEGGER_SysView/Src/Sample/FreeRTOSV10.4/SEGGER_SYSVIEW_FreeRTOS.h

# System view - SEGGER

RTOS_SRCS += $(wildcard /home/robi/Downloads/SEGGER_SysView/Src/SEGGER/*.c)
RTOS_SRCS += $(wildcard /home/robi/Downloads/SEGGER_SysView/Src/SEGGER/Syscalls/SEGGER_RTT_Syscalls_GCC.c)
RTOS_INCS += $(wildcard /home/robi/Downloads/SEGGER_SysView/Src/SEGGER/*.h)

RTOS_SRCS += $(wildcard /home/robi/Downloads/SEGGER_SysView/Src/Sample/FreeRTOSV10.4/*.c)
RTOS_INCS += $(wildcard /home/robi/Downloads/SEGGER_SysView/Src/Sample/FreeRTOSV10.4/*.h)

RTOS_INCS += $(wildcard /home/robi/Downloads/SEGGER_SysView/Src/Config/*.h)

ASM_SRCS += /home/robi/Downloads/SEGGER_SysView/Src/SEGGER/SEGGER_RTT_ASM_ARMv7M.S

# END System view

SRCS += $(RTOS_SRCS)
INCS += $(RTOS_INCS)
