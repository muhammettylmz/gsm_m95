################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Driver/VTS/vehicleTrackingSystem.c 

C_DEPS += \
./Core/Driver/VTS/vehicleTrackingSystem.d 

OBJS += \
./Core/Driver/VTS/vehicleTrackingSystem.o 


# Each subdirectory must supply rules for building sources it contributes
Core/Driver/VTS/%.o Core/Driver/VTS/%.su Core/Driver/VTS/%.cyclo: ../Core/Driver/VTS/%.c Core/Driver/VTS/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/VTS" -I../Core/Inc -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/bluetooth" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/gps" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/uart_debug" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mems" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mqtt" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/m95" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Driver-2f-VTS

clean-Core-2f-Driver-2f-VTS:
	-$(RM) ./Core/Driver/VTS/vehicleTrackingSystem.cyclo ./Core/Driver/VTS/vehicleTrackingSystem.d ./Core/Driver/VTS/vehicleTrackingSystem.o ./Core/Driver/VTS/vehicleTrackingSystem.su

.PHONY: clean-Core-2f-Driver-2f-VTS

