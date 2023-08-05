################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (11.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Driver/m95/m95.c 

OBJS += \
./Core/Driver/m95/m95.o 

C_DEPS += \
./Core/Driver/m95/m95.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Driver/m95/%.o Core/Driver/m95/%.su Core/Driver/m95/%.cyclo: ../Core/Driver/m95/%.c Core/Driver/m95/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mems" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/mqtt" -I"C:/Users/muham/ws/m95_vehicle/firmware/gsm_m95/m95_test1/Core/Driver/m95" -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Driver-2f-m95

clean-Core-2f-Driver-2f-m95:
	-$(RM) ./Core/Driver/m95/m95.cyclo ./Core/Driver/m95/m95.d ./Core/Driver/m95/m95.o ./Core/Driver/m95/m95.su

.PHONY: clean-Core-2f-Driver-2f-m95

