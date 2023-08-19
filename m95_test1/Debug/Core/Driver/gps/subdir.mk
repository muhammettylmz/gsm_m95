################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Driver/gps/gps.c 

C_DEPS += \
./Core/Driver/gps/gps.d 

OBJS += \
./Core/Driver/gps/gps.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Driver/gps/%.o Core/Driver/gps/%.su Core/Driver/gps/%.cyclo: ../Core/Driver/gps/%.c Core/Driver/gps/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/bluetooth" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/gps" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/uart_debug" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mems" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mqtt" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/m95" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Driver-2f-gps

clean-Core-2f-Driver-2f-gps:
	-$(RM) ./Core/Driver/gps/gps.cyclo ./Core/Driver/gps/gps.d ./Core/Driver/gps/gps.o ./Core/Driver/gps/gps.su

.PHONY: clean-Core-2f-Driver-2f-gps

