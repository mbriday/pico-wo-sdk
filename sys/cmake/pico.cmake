#directory names
set(SYS_DIR ${CMAKE_SOURCE_DIR}/../sys)

enable_language(ASM)

include_directories(
    ${CMAKE_SOURCE_DIR}
	${SYS_DIR}/rp2040/hardware_structs/include
	${SYS_DIR}/rp2040/hardware_regs/include
	${SYS_DIR}/rp2040/hardware/include
)

set(SYSTEM_SRCS 
	${SYS_DIR}/boot2_w25q080_2_padded_checksum.s
	${SYS_DIR}/crt0.S
	${SYS_DIR}/runtime.c
)

# define CPU OPTIONS
set(CPU_OPTIONS -mthumb -mcpu=cortex-m0plus -march=armv6-m)

set(CMAKE_C_FLAGS_DEBUG   "-O0 -Wno-deprecated -Werror -DDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Wno-deprecated -Werror -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE   "-O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

#compiler options
add_compile_options(
    ${CPU_OPTIONS}
	$<$<COMPILE_LANGUAGE:C>:-std=c11>
	$<$<COMPILE_LANGUAGE:CXX>:-std=c++11>
    $<$<COMPILE_LANGUAGE:CXX>:-fms-extensions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
	#$<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
	#$<$<COMPILE_LANGUAGE:CXX>:-Wsuggest-override>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
    -fstrict-volatile-bitfields
    -ffunction-sections
    -Wall
    -Wextra
    -Wcast-align
	#-Wconversion
    #-Wsign-conversion
    -Wshadow
    -Wlogical-op
    -Wsuggest-final-types
    -Wsuggest-final-methods
    #-pedantic
    -fexceptions
    -g
)

#executable (should be after compile options)
add_executable(${PROJECT_NAME} ${SRCS} ${SYSTEM_SRCS})

# select linker script
set(LINKER_DIR ${SYS_DIR})
set(LINKER_SCRIPT "memmap_default.ld")
set(LINKER_FLAGS 
	-Wl,--start-group -Wl,--end-group -Wl,--gc-sections
	-lstdc++_nano
	--specs=nosys.specs
	-Xlinker -Map=${PROJECT_NAME}.map
)

#should be defined after 'add_executable'
target_link_libraries(${PROJECT_NAME}
	${CPU_OPTIONS}
	-T${LINKER_DIR}/${LINKER_SCRIPT}
	${LINKER_FLAGS}
    m
    #-nostartfiles
    #-nostdlib
)

set_property(TARGET ${PROJECT_NAME} PROPERTY LINK_DEPENDS ${LINKER_DIR}/${LINKER_SCRIPT})

#flashing rules
#include("../sys/cmake/flash.cmake") 
