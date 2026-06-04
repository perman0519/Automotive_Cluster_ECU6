################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Sources/drivers/can.c \
../Sources/drivers/gpio.c \
../Sources/drivers/uart.c 

OBJS += \
./Sources/drivers/can.o \
./Sources/drivers/gpio.o \
./Sources/drivers/uart.o 

C_DEPS += \
./Sources/drivers/can.d \
./Sources/drivers/gpio.d \
./Sources/drivers/uart.d 


# Each subdirectory must supply rules for building sources it contributes
Sources/drivers/%.o: ../Sources/drivers/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Standard S32DS C Compiler'
	powerpc-eabivle-gcc "@Sources/drivers/can.args" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


