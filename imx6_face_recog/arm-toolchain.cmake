SET(CMAKE_SYSTEM_NAME Linux) 
SET(TOOLCHAIN_DIR "/opt/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf")
SET(CMAKE_FIND_ROOT_PATH ${TOOLCHAIN_DIR})
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}/bin/arm-linux-gnueabihf-g++)
