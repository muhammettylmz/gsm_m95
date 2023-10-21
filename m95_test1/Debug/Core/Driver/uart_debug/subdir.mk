################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Driver/uart_debug/uart_debug.c 

C_DEPS += \
./Core/Driver/uart_debug/uart_debug.d 

OBJS += \
./Core/Driver/uart_debug/uart_debug.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Driver/uart_debug/%.o Core/Driver/uart_debug/%.su Core/Driver/uart_debug/%.cyclo: ../Core/Driver/uart_debug/%.c Core/Driver/uart_debug/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu99 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/VTS" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/obd2" -I../Core/Inc -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/bluetooth" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/gps" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/uart_debug" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mems" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mqtt" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/m95" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -Ofast -ffunction-sections -fdata-sections -Wall -Wfatal-errors -Wmissing-include-dirs -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Driver-2f-uart_debug

clean-Core-2f-Driver-2f-uart_debug:
	-$(RM) ./Core/Driver/uart_debug/uart_debug.cyclo ./Core/Driver/uart_debug/uart_debug.d ./Core/Driver/uart_debug/uart_debug.o ./Core/Driver/uart_debug/uart_debug.su

.PHONY: clean-Core-2f-Driver-2f-uart_debug

