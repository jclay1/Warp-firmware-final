# Coursework - Sensor fusion for drone tracking in 3 dimensions
Name: **John Clay**. College: **Clare**. CRSID: **jc953**.

## Code layout
The most important files in this project are `warp-kl03-ksdk1.1-boot.c`, `devMPU6050.c` and `devMPU6050.h`. These are all in `src/boot/ksdk1.1.0/`. `CMakeLists.txt` and `build/ksdk1.1/build.sh` have also been edited to include the new driver files in the build.
`devMPU6050.h` and `devMPU6050.c` are the header file and device driver for the MPU-6050 inertial measurement unit (IMU) respectively. They enable the device to be read over I2C, and initialise the sensor properly for operation.

`warp-kl03-ksdk1.1-boot.c` implements the program, first reading from the IMU to obtain readings for angular and linear acceleration. These readings are then used to calculate the objective change in position as follows:
First the orientatation of the deviec is found. This is done by integrating the angular acceleration to find the angular velocity, and premultiplying by a conversion matrix to obtain the orientation of the sensor.
This orientation is used to calculate the rotation matrix between the body centred frame in which the sensor is reading, and a global frame, which is selected such that at power up the frames are identical. The system then calculates the position change, and prints the new final coordinate each iteration. 

## Hardware
The required hardware is:
* 1 x FRDM KL03
* 1 x MPU-6050 IMU
* 1 x Breadboard
* 5 x jumper wires
* 1 x USB mini to USB A cable for laptop connection (if printed output)
