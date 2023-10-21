################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Driver/bluetooth/bluetooth.c 

C_DEPS += \
./Core/Driver/bluetooth/bluetooth.d 

OBJS += \
./Core/Driver/bluetooth/bluetooth.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Driver/bluetooth/%.o Core/Driver/bluetooth/%.su Core/Driver/bluetooth/%.cyclo: ../Core/Driver/bluetooth/%.c Core/Driver/bluetooth/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu99 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/VTS" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/obd2" -I../Core/Inc -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/bluetooth" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/gps" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/uart_debug" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mems" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mqtt" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/m95" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -Ofast -ffunction-sections -fdata-sections -Wall -Wfatal-errors -Wmissing-include-dirs -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Driver-2f-bluetooth

clean-Core-2f-Driver-2f-bluetooth:
	-$(RM) ./Core/Driver/bluetooth/bluetooth.cyclo ./Core/Driver/bluetooth/bluetooth.d ./Core/Driver/bluetooth/bluetooth.o ./Core/Driver/bluetooth/bluetooth.su

.PHONY: clean-Core-2f-Driver-2f-bluetooth

