################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/CustomDriverSet/SrcDriver/EMA_FILTER.c \
../Core/CustomDriverSet/SrcDriver/SMA_FILTER.c \
../Core/CustomDriverSet/SrcDriver/TwoDimSoundLoc.c 

OBJS += \
./Core/CustomDriverSet/SrcDriver/EMA_FILTER.o \
./Core/CustomDriverSet/SrcDriver/SMA_FILTER.o \
./Core/CustomDriverSet/SrcDriver/TwoDimSoundLoc.o 

C_DEPS += \
./Core/CustomDriverSet/SrcDriver/EMA_FILTER.d \
./Core/CustomDriverSet/SrcDriver/SMA_FILTER.d \
./Core/CustomDriverSet/SrcDriver/TwoDimSoundLoc.d 


# Each subdirectory must supply rules for building sources it contributes
Core/CustomDriverSet/SrcDriver/%.o Core/CustomDriverSet/SrcDriver/%.su Core/CustomDriverSet/SrcDriver/%.cyclo: ../Core/CustomDriverSet/SrcDriver/%.c Core/CustomDriverSet/SrcDriver/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -DUSE_FULL_LL_DRIVER -c -I../Core/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/glusc/zavRad/Sound Localization/Core/CustomDriverSet/IncDriver" -I"C:/Users/glusc/zavRad/Sound Localization/Core/CustomDriverSet/SrcDriver" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-CustomDriverSet-2f-SrcDriver

clean-Core-2f-CustomDriverSet-2f-SrcDriver:
	-$(RM) ./Core/CustomDriverSet/SrcDriver/EMA_FILTER.cyclo ./Core/CustomDriverSet/SrcDriver/EMA_FILTER.d ./Core/CustomDriverSet/SrcDriver/EMA_FILTER.o ./Core/CustomDriverSet/SrcDriver/EMA_FILTER.su ./Core/CustomDriverSet/SrcDriver/SMA_FILTER.cyclo ./Core/CustomDriverSet/SrcDriver/SMA_FILTER.d ./Core/CustomDriverSet/SrcDriver/SMA_FILTER.o ./Core/CustomDriverSet/SrcDriver/SMA_FILTER.su ./Core/CustomDriverSet/SrcDriver/TwoDimSoundLoc.cyclo ./Core/CustomDriverSet/SrcDriver/TwoDimSoundLoc.d ./Core/CustomDriverSet/SrcDriver/TwoDimSoundLoc.o ./Core/CustomDriverSet/SrcDriver/TwoDimSoundLoc.su

.PHONY: clean-Core-2f-CustomDriverSet-2f-SrcDriver

