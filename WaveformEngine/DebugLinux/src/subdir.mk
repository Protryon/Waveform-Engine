################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/font.c \
../src/glew.c \
../src/gui.c \
../src/json.c \
../src/main.c \
../src/pqueue.c \
../src/queue.c \
../src/render.c \
../src/smem.c \
../src/vector.c \
../src/waveform.c \
../src/xstring.c 

OBJS += \
./src/font.o \
./src/glew.o \
./src/gui.o \
./src/json.o \
./src/main.o \
./src/pqueue.o \
./src/queue.o \
./src/render.o \
./src/smem.o \
./src/vector.o \
./src/waveform.o \
./src/xstring.o 

C_DEPS += \
./src/font.d \
./src/glew.d \
./src/gui.d \
./src/json.d \
./src/main.d \
./src/pqueue.d \
./src/queue.d \
./src/render.d \
./src/smem.d \
./src/vector.d \
./src/waveform.d \
./src/xstring.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -std=gnu11 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


