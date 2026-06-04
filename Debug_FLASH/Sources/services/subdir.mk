################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/services/com_gateway.c \
../Sources/services/logger.c \
../Sources/services/monitoring.c \
../Sources/services/ms_scheduler.c \
../Sources/services/system_init.c 

OBJS += \
./Sources/services/com_gateway.o \
./Sources/services/logger.o \
./Sources/services/monitoring.o \
./Sources/services/ms_scheduler.o \
./Sources/services/system_init.o 

C_DEPS += \
./Sources/services/com_gateway.d \
./Sources/services/logger.d \
./Sources/services/monitoring.d \
./Sources/services/ms_scheduler.d \
./Sources/services/system_init.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/services/%.o: ../Sources/services/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	powerpc-eabivle-gcc "@Sources/services/com_gateway.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


