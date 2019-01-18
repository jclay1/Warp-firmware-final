// Device driver for the MPU-6050 IMU sensor

#include <stdlib.h>

#include "fsl_misc_utilities.h"
#include "fsl_device_registers.h"
#include "fsl_i2c_master_driver.h"
#include "fsl_spi_master_driver.h"
#include "fsl_rtc_driver.h"
#include "fsl_clock_manager.h"
#include "fsl_power_manager.h"
#include "fsl_mcglite_hal.h"
#include "fsl_port_hal.h"

#include "gpio_pins.h"
#include "SEGGER_RTT.h"
#include "warp.h"

extern volatile WarpI2CDeviceState	deviceMPU6050State;
extern volatile uint32_t		gWarpI2cBaudRateKbps;


void initMPU6050(const uint8_t i2cAddress, WarpI2CDeviceState volatile * deviceStatePointer){
	deviceStatePointer->i2cAddress = i2cAddress;
	deviceStatePointer->signalType = (	kWarpTypeMaskAccelerationX | 
						kWarpTypeMaskAccelerationY |
						kWarpTypeMaskAccelerationZ |
						kWarpTypeMaskAngularRateX  |
						kWarpTypeMaskAngularRateY  |
						kWarpTypeMaskAngularRateZ 
					 );
	return;
}

WarpStatus readSensorRegisterMPU6050(uint8_t deviceRegister){
	uint8_t         txBuf[2]        = {0x6B, 0x0}; // This is the power management register address and byte to send to it
	uint8_t		cmdBuf[1]	= {0xFF};
	i2c_status_t	returnValue;	

	i2c_device_t slave =
	{
		.address = deviceMPU6050State.i2cAddress,
		.baudRate_kbps = gWarpI2cBaudRateKbps
	};

	// Set the power management register to make sure the MPU is not in sleep mode.
	returnValue = I2C_DRV_MasterSendDataBlocking(
                                                        0 /* I2C peripheral instance */,
                                                        &slave,
                                                        NULL,
                                                        0,
                                                        txBuf,
                                                        2,
                                                        500 /* Timeout in ms*/);
	
	
	cmdBuf[0] = deviceRegister;
	// Then read from register
	returnValue = I2C_DRV_MasterReceiveDataBlocking(
							0 /* I2C peripheral instance */,
							&slave,
							cmdBuf,
							1,
							(uint8_t *)deviceMPU6050State.i2cBuffer,
							1,
							500 /*timeout in ms*/);
	
	if (returnValue == kStatus_I2C_Success)
	{
//		SEGGER_RTT_printf(0, "\r[0x%02x] 	0x%02x\n", cmdBuf[0], deviceMPU6050State.i2cBuffer[0]);
	}
	else
	{
	//	SEGGER_RTT_printf(0, kWarpConstantStringI2CFailure, cmdBuf[0], returnValue);
		SEGGER_RTT_printf(0, "\r[0x%02x]        0x%02x\n", cmdBuf[0], deviceMPU6050State.i2cBuffer[0]);
		SEGGER_RTT_WriteString(0, "Didn't work\n");
		return 0xFF;
	}
	
	// Return register contents
	return deviceMPU6050State.i2cBuffer[0];
}

WarpStatus writeSensorRegisterMPU6050(uint8_t deviceRegister, uint8_t register_data){ /* Write function for configuration of accel and gyro*/
	uint8_t 	cmdBuf[1]	= {0xFF};
	uint8_t		txBuf[2]	= {deviceRegister, register_data};
	i2c_status_t			returnValue;
	i2c_device_t slave = 
	{
		.address = deviceMPU6050State.i2cAddress,
		.baudRate_kbps = gWarpI2cBaudRateKbps
	};
	returnValue = I2C_DRV_MasterSendDataBlocking(
                                                        0 /* I2C peripheral instance */,
                                                        &slave,
                                                        NULL,
                                                        0,
                                                        txBuf,
                                                        2,
                                                        500 /* Timeout in ms*/);
	
	if (returnValue == kStatus_I2C_Success)
	{
		SEGGER_RTT_printf(0, "\r[0x%02x]  0x%02x\n", cmdBuf[0], txBuf[0]);
		SEGGER_RTT_WriteString(0, "Sent\n");
		return kWarpStatusOK;
	}
	else
	{
		return kWarpStatusDeviceCommunicationFailed;
		SEGGER_RTT_WriteString(0, "Didn't send\n");
	}
}

