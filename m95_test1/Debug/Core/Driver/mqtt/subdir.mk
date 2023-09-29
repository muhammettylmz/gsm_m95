################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Driver/mqtt/cert.c \
../Core/Driver/mqtt/mqtt.c 

C_DEPS += \
./Core/Driver/mqtt/cert.d \
./Core/Driver/mqtt/mqtt.d 

OBJS += \
./Core/Driver/mqtt/cert.o \
./Core/Driver/mqtt/mqtt.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Driver/mqtt/%.o Core/Driver/mqtt/%.su Core/Driver/mqtt/%.cyclo: ../Core/Driver/mqtt/%.c Core/Driver/mqtt/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu99 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/VTS" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/obd2" -I../Core/Inc -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/bluetooth" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/gps" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/uart_debug" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mems" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mqtt" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/m95" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -Ofast -ffunction-sections -fdata-sections -Wall -Wfatal-errors -Wmissing-include-dirs -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Driver-2f-mqtt

clean-Core-2f-Driver-2f-mqtt:
	-$(RM) ./Core/Driver/mqtt/cert.cyclo ./Core/Driver/mqtt/cert.d ./Core/Driver/mqtt/cert.o ./Core/Driver/mqtt/cert.su ./Core/Driver/mqtt/mqtt.cyclo ./Core/Driver/mqtt/mqtt.d ./Core/Driver/mqtt/mqtt.o ./Core/Driver/mqtt/mqtt.su

.PHONY: clean-Core-2f-Driver-2f-mqtt

