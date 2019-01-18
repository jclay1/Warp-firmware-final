/*
	Authored 2016-2018. Phillip Stanley-Marbell.

	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions
	are met:

	*	Redistributions of source code must retain the above
		copyright notice, this list of conditions and the following
		disclaimer.

	*	Redistributions in binary form must reproduce the above
		copyright notice, this list of conditions and the following
		disclaimer in the documentation and/or other materials
		provided with the distribution.:q

	*	Neither the name of the author nor the names of its
		contributors may be used to endorse or promote products
		derived from this software without specific prior written
		permission.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
	BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

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

#include "devBMX055.h"
#include "devADXL362.h"
#include "devMMA8451Q.h"
#include "devLPS25H.h"
#include "devHDC1000.h"
#include "devMAG3110.h"
#include "devSI7021.h"
#include "devL3GD20H.h"
#include "devBME680.h"
#include "devTCS34725.h"
#include "devSI4705.h"
#include "devCCS811.h"
#include "devAMG8834.h"
#include "devPAN1326.h"
#include "devAS7262.h"
#include "devAS7263.h"
#include "devSSD1331.h"
#include "devINA219.h"
#include "devMPU6050.h"

#define					kWarpConstantStringI2cFailure		"\rI2C failed, reg 0x%02x, code %d\n"
#define					kWarpConstantStringErrorInvalidVoltage	"\rInvalid supply voltage [%d] mV!"
#define					kWarpConstantStringErrorSanity		"\rSanity Check Failed!"


volatile WarpSPIDeviceState		deviceADXL362State;
volatile WarpI2CDeviceState		deviceBMX055accelState;
volatile WarpI2CDeviceState		deviceBMX055gyroState;
volatile WarpI2CDeviceState		deviceBMX055magState;
volatile WarpI2CDeviceState		deviceMMA8451QState;
volatile WarpI2CDeviceState		deviceLPS25HState;
volatile WarpI2CDeviceState		deviceHDC1000State;
volatile WarpI2CDeviceState		deviceMAG3110State;
volatile WarpI2CDeviceState		deviceSI7021State;
volatile WarpI2CDeviceState		deviceL3GD20HState;
volatile WarpI2CDeviceState		deviceBME680State;
volatile WarpI2CDeviceState		deviceTCS34725State;
volatile WarpI2CDeviceState		deviceSI4705State;
volatile WarpI2CDeviceState		deviceCCS811State;
volatile WarpI2CDeviceState		deviceAMG8834State;
volatile WarpUARTDeviceState		devicePAN1326BState;
volatile WarpUARTDeviceState		devicePAN1323ETUState;
volatile WarpI2CDeviceState		deviceAS7262State;
volatile WarpI2CDeviceState		deviceAS7263State;
volatile WarpI2CDeviceState		deviceINA219currentState;
volatile WarpI2CDeviceState		deviceMPU6050State;

/*
 *	TODO: move this and possibly others into a global structure
 */
volatile i2c_master_state_t		i2cMasterState;
volatile spi_master_state_t		spiMasterState;
volatile spi_master_user_config_t	spiUserConfig;


/*
 *	TODO: move magic default numbers into constant definitions.
 */
volatile uint32_t			gWarpI2cBaudRateKbps	= 1;
volatile uint32_t			gWarpUartBaudRateKbps	= 1;
volatile uint32_t			gWarpSpiBaudRateKbps	= 1;
volatile uint32_t			gWarpSleeptimeSeconds	= 0;
volatile WarpModeMask			gWarpMode		= kWarpModeDisableAdcOnSleep;



void					sleepUntilReset(void);
void					lowPowerPinStates(void);
void					disableTPS82740A(void);
void					disableTPS82740B(void);
void					enableTPS82740A(uint16_t voltageMillivolts);
void					enableTPS82740B(uint16_t voltageMillivolts);
void					setTPS82740CommonControlLines(uint16_t voltageMillivolts);
void					printPinDirections(void);
void					dumpProcessorState(void);
void					repeatRegisterReadForDeviceAndAddress(WarpSensorDevice warpSensorDevice, uint8_t baseAddress, 
								uint16_t pullupValue, bool autoIncrement, int chunkReadsPerAddress, bool chatty,
								int spinDelay, int repetitionsPerAddress, uint16_t sssupplyMillivolts,
								uint16_t adaptiveSssupplyMaxMillivolts, uint8_t referenceByte);
int					char2int(int character);
void					enableSssupply(uint16_t voltageMillivolts);
void					disableSssupply(void);
void					activateAllLowPowerSensorModes(void);
void					powerupAllSensors(void);
uint8_t					readHexByte(void);
int					read4digits(void);


/*
 *	TODO: change the following to take byte arrays
 */
WarpStatus				writeByteToI2cDeviceRegister(uint8_t i2cAddress, bool sendCommandByte, uint8_t commandByte, bool sendPayloadByte, uint8_t payloadByte);
WarpStatus				writeBytesToSpi(uint8_t *  payloadBytes, int payloadLength);


void					warpLowPowerSecondsSleep(uint32_t sleepSeconds, bool forceAllPinsIntoLowPowerState);



/*
 *	From KSDK power_manager_demo.c <<BEGIN>>>
 */

clock_manager_error_code_t clockManagerCallbackRoutine(clock_notify_struct_t *  notify, void *  callbackData);

/*
 *	static clock callback table.
 */
clock_manager_callback_user_config_t		clockManagerCallbackUserlevelStructure =
									{
										.callback	= clockManagerCallbackRoutine,
										.callbackType	= kClockManagerCallbackBeforeAfter,
										.callbackData	= NULL
									};

static clock_manager_callback_user_config_t *	clockCallbackTable[] =
									{
										&clockManagerCallbackUserlevelStructure
									};

clock_manager_error_code_t
clockManagerCallbackRoutine(clock_notify_struct_t *  notify, void *  callbackData)
{
	clock_manager_error_code_t result = kClockManagerSuccess;

	switch (notify->notifyType)
	{
		case kClockManagerNotifyBefore:
			break;
		case kClockManagerNotifyRecover:
		case kClockManagerNotifyAfter:
			break;
		default:
			result = kClockManagerError;
		break;
	}

	return result;
}


/*
 *	Override the RTC IRQ handler
 */
void
RTC_IRQHandler(void)
{
	if (RTC_DRV_IsAlarmPending(0))
	{
		RTC_DRV_SetAlarmIntCmd(0, false);
	}
}

/*
 *	Override the RTC Second IRQ handler
 */
void
RTC_Seconds_IRQHandler(void)
{
	gWarpSleeptimeSeconds++;
}

/*
 *	Power manager user callback
 */
power_manager_error_code_t callback0(power_manager_notify_struct_t *  notify,
					power_manager_callback_data_t *  dataPtr)
{
	WarpPowerManagerCallbackStructure *		callbackUserData = (WarpPowerManagerCallbackStructure *) dataPtr;
	power_manager_error_code_t			status = kPowerManagerError;

	switch (notify->notifyType)
	{
		case kPowerManagerNotifyBefore:
			status = kPowerManagerSuccess;
			break;
		case kPowerManagerNotifyAfter:
			status = kPowerManagerSuccess;
			break;
		default:
			callbackUserData->errorCount++;
			break;
	}

	return status;
}

/*
 *	From KSDK power_manager_demo.c <<END>>>
 */



void
sleepUntilReset(void)
{
	while (1)
	{
		GPIO_DRV_SetPinOutput(kWarpPinSI4705_nRST);
		warpLowPowerSecondsSleep(1, false /* forceAllPinsIntoLowPowerState */);
		GPIO_DRV_ClearPinOutput(kWarpPinSI4705_nRST);
		warpLowPowerSecondsSleep(60, true /* forceAllPinsIntoLowPowerState */);
	}
}



void
enableSPIpins(void)
{
	CLOCK_SYS_EnableSpiClock(0);

	/*	Warp KL03_SPI_MISO	--> PTA6	(ALT3)		*/
	PORT_HAL_SetMuxMode(PORTA_BASE, 6, kPortMuxAlt3);

	/*	Warp KL03_SPI_MOSI	--> PTA7	(ALT3)		*/
	PORT_HAL_SetMuxMode(PORTA_BASE, 7, kPortMuxAlt3);

	/*	Warp KL03_SPI_SCK	--> PTB0	(ALT3)		*/
	PORT_HAL_SetMuxMode(PORTB_BASE, 0, kPortMuxAlt3);


	/*
	 *	Initialize SPI master. See KSDK13APIRM.pdf Section 70.4
	 *
	 */
	uint32_t			calculatedBaudRate;
	spiUserConfig.polarity		= kSpiClockPolarity_ActiveHigh;
	spiUserConfig.phase		= kSpiClockPhase_FirstEdge;
	spiUserConfig.direction		= kSpiMsbFirst;
	spiUserConfig.bitsPerSec	= gWarpSpiBaudRateKbps * 1000;
	SPI_DRV_MasterInit(0 /* SPI master instance */, (spi_master_state_t *)&spiMasterState);
	SPI_DRV_MasterConfigureBus(0 /* SPI master instance */, (spi_master_user_config_t *)&spiUserConfig, &calculatedBaudRate);
}



void
disableSPIpins(void)
{
	SPI_DRV_MasterDeinit(0);


	/*	Warp KL03_SPI_MISO	--> PTA6	(GPI)		*/
	PORT_HAL_SetMuxMode(PORTA_BASE, 6, kPortMuxAsGpio);

	/*	Warp KL03_SPI_MOSI	--> PTA7	(GPIO)		*/
	PORT_HAL_SetMuxMode(PORTA_BASE, 7, kPortMuxAsGpio);

	/*	Warp KL03_SPI_SCK	--> PTB0	(GPIO)		*/
	PORT_HAL_SetMuxMode(PORTB_BASE, 0, kPortMuxAsGpio);

	GPIO_DRV_ClearPinOutput(kWarpPinSPI_MOSI);
	GPIO_DRV_ClearPinOutput(kWarpPinSPI_MISO);
	GPIO_DRV_ClearPinOutput(kWarpPinSPI_SCK);


	CLOCK_SYS_DisableSpiClock(0);
}



void
enableI2Cpins(uint16_t pullupValue)
{
	CLOCK_SYS_EnableI2cClock(0);

	/*	Warp KL03_I2C0_SCL	--> PTB3	(ALT2 == I2C)		*/
	PORT_HAL_SetMuxMode(PORTB_BASE, 3, kPortMuxAlt2);

	/*	Warp KL03_I2C0_SDA	--> PTB4	(ALT2 == I2C)		*/
	PORT_HAL_SetMuxMode(PORTB_BASE, 4, kPortMuxAlt2);


	I2C_DRV_MasterInit(0 /* I2C instance */, (i2c_master_state_t *)&i2cMasterState);


	/*
	 *	TODO: need to implement config of the DCP
	 */
	//...
}



void
disableI2Cpins(void)
{
	I2C_DRV_MasterDeinit(0 /* I2C instance */);	


	/*	Warp KL03_I2C0_SCL	--> PTB3	(GPIO)			*/
	PORT_HAL_SetMuxMode(PORTB_BASE, 3, kPortMuxAsGpio);

	/*	Warp KL03_I2C0_SDA	--> PTB4	(GPIO)			*/
	PORT_HAL_SetMuxMode(PORTB_BASE, 4, kPortMuxAsGpio);


	/*
	 *	TODO: need to implement clearing of the DCP
	 */
	//...

	/*
	 *	Drive the I2C pins low
	 */
	GPIO_DRV_ClearPinOutput(kWarpPinI2C0_SDA);
	GPIO_DRV_ClearPinOutput(kWarpPinI2C0_SCL);


	CLOCK_SYS_DisableI2cClock(0);
}


void
lowPowerPinStates(void)
{
	/*
	 *	Following Section 5 of "Power Management for Kinetis L Family" (AN5088.pdf),
	 *	we configure all pins as output and set them to a known state. We choose
	 *	to set them all to '0' since it happens that the devices we want to keep
	 *	deactivated (SI4705, PAN1326) also need '0'.
	 */

	/*
	 *			PORT A
	 */
	/*
	 *	For now, don't touch the PTA0/1/2 SWD pins. Revisit in the future.
	 */
	/*
	PORT_HAL_SetMuxMode(PORTA_BASE, 0, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTA_BASE, 1, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTA_BASE, 2, kPortMuxAsGpio);
	*/

	/*
	 *	PTA3 and PTA4 are the EXTAL/XTAL
	 */
	PORT_HAL_SetMuxMode(PORTA_BASE, 3, kPortPinDisabled);
	PORT_HAL_SetMuxMode(PORTA_BASE, 4, kPortPinDisabled);

	PORT_HAL_SetMuxMode(PORTA_BASE, 5, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTA_BASE, 6, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTA_BASE, 7, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTA_BASE, 8, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTA_BASE, 9, kPortMuxAsGpio);
	
	/*
	 *	NOTE: The KL03 has no PTA10 or PTA11
	 */

	PORT_HAL_SetMuxMode(PORTA_BASE, 12, kPortMuxAsGpio);



	/*
	 *			PORT B
	 */
	PORT_HAL_SetMuxMode(PORTB_BASE, 0, kPortMuxAsGpio);
	
	/*
	 *	PTB1 is connected to KL03_VDD. We have a choice of:
	 *		(1) Keep 'disabled as analog'.
	 *		(2) Set as output and drive high.
	 *
	 *	Pin state "disabled" means default functionality (ADC) is _active_
	 */
	if (gWarpMode & kWarpModeDisableAdcOnSleep)
	{
		PORT_HAL_SetMuxMode(PORTB_BASE, 1, kPortMuxAsGpio);
	}
	else
	{
		PORT_HAL_SetMuxMode(PORTB_BASE, 1, kPortPinDisabled);
	}

	PORT_HAL_SetMuxMode(PORTB_BASE, 2, kPortMuxAsGpio);

	/*
	 *	PTB3 and PTB3 (I2C pins) are true open-drain
	 *	and we purposefully leave them disabled.
	 */
	PORT_HAL_SetMuxMode(PORTB_BASE, 3, kPortPinDisabled);
	PORT_HAL_SetMuxMode(PORTB_BASE, 4, kPortPinDisabled);


	PORT_HAL_SetMuxMode(PORTB_BASE, 5, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTB_BASE, 6, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTB_BASE, 7, kPortMuxAsGpio);

	/*
	 *	NOTE: The KL03 has no PTB8 or PTB9
	 */

	PORT_HAL_SetMuxMode(PORTB_BASE, 10, kPortMuxAsGpio);
	PORT_HAL_SetMuxMode(PORTB_BASE, 11, kPortMuxAsGpio);

	/*
	 *	NOTE: The KL03 has no PTB12
	 */
	
	PORT_HAL_SetMuxMode(PORTB_BASE, 13, kPortMuxAsGpio);



	/*
	 *	Now, set all the pins (except kWarpPinKL03_VDD_ADC, the SWD pins, and the XTAL/EXTAL) to 0
	 */
	
	
	
	/*
	 *	If we are in mode where we disable the ADC, then drive the pin high since it is tied to KL03_VDD
	 */
	if (gWarpMode & kWarpModeDisableAdcOnSleep)
	{
		GPIO_DRV_SetPinOutput(kWarpPinKL03_VDD_ADC);
	}
	
#ifdef WARP_FRDMKL03
	GPIO_DRV_ClearPinOutput(kWarpPinPAN1323_nSHUTD);
#else
	GPIO_DRV_ClearPinOutput(kWarpPinPAN1326_nSHUTD);
#endif
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740A_CTLEN);
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740B_CTLEN);
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL1);
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL2);
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL3);
	GPIO_DRV_ClearPinOutput(kWarpPinCLKOUT32K);
	GPIO_DRV_ClearPinOutput(kWarpPinTS5A3154_IN);
	GPIO_DRV_ClearPinOutput(kWarpPinSI4705_nRST);

	/*
	 *	Drive these chip selects high since they are active low:
	 */
	GPIO_DRV_SetPinOutput(kWarpPinISL23415_nCS);
	GPIO_DRV_SetPinOutput(kWarpPinADXL362_CS);

	/*
	 *	When the PAN1326 is installed, note that it has the
	 *	following pull-up/down by default:
	 *
	 *		HCI_RX / kWarpPinI2C0_SCL	: pull up
	 *		HCI_TX / kWarpPinI3C0_SDA	: pull up
	 *		HCI_RTS / kWarpPinSPI_MISO	: pull up
	 *		HCI_CTS / kWarpPinSPI_MOSI	: pull up
	 *
	 *	These I/Os are 8mA (see panasonic_PAN13xx.pdf, page 10),
	 *	so we really don't want to be driving them low. We
	 *	however also have to be careful of the I2C pullup and
	 *	pull-up gating. However, driving them high leads to
	 *	higher board power dissipation even when SSSUPPLY is off
	 *	by ~80mW on board #003 (PAN1326 populated).
	 *
	 *	In revB board, with the ISL23415 DCP pullups, we also
	 *	want I2C_SCL and I2C_SDA driven high since when we
	 *	send a shutdown command to the DCP it will connect
	 *	those lines to 25570_VOUT. 
	 *
	 *	For now, we therefore leave the SPI pins low and the
	 *	I2C pins (PTB3, PTB4, which are true open-drain) disabled.
	 */

	GPIO_DRV_ClearPinOutput(kWarpPinI2C0_SDA);
	GPIO_DRV_ClearPinOutput(kWarpPinI2C0_SCL);
	GPIO_DRV_ClearPinOutput(kWarpPinSPI_MOSI);
	GPIO_DRV_ClearPinOutput(kWarpPinSPI_MISO);
	GPIO_DRV_ClearPinOutput(kWarpPinSPI_SCK);

	/*
	 *	HCI_RX / kWarpPinI2C0_SCL is an input. Set it low.
	 */
	//GPIO_DRV_SetPinOutput(kWarpPinI2C0_SCL);

	/*
	 *	HCI_TX / kWarpPinI2C0_SDA is an output. Set it high.
	 */
	//GPIO_DRV_SetPinOutput(kWarpPinI2C0_SDA);

	/*
	 *	HCI_RTS / kWarpPinSPI_MISO is an output. Set it high.
	 */
	//GPIO_DRV_SetPinOutput(kWarpPinSPI_MISO);

	/*
	 *	From PAN1326 manual, page 10:
	 *
	 *		"When HCI_CTS is high, then CC256X is not allowed to send data to Host device"
	 */
	//GPIO_DRV_SetPinOutput(kWarpPinSPI_MOSI);
}



void
disableTPS82740A(void)
{
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740A_CTLEN);
}

void
disableTPS82740B(void)
{
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740B_CTLEN);
}


void
enableTPS82740A(uint16_t voltageMillivolts)
{
	setTPS82740CommonControlLines(voltageMillivolts);
	GPIO_DRV_SetPinOutput(kWarpPinTPS82740A_CTLEN);
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740B_CTLEN);

	/*
	 *	Select the TS5A3154 to use the output of the TPS82740
	 *
	 *		IN = high selects the output of the TPS82740B:
	 *		IN = low selects the output of the TPS82740A:
	 */
	GPIO_DRV_ClearPinOutput(kWarpPinTS5A3154_IN);
}


void
enableTPS82740B(uint16_t voltageMillivolts)
{
	setTPS82740CommonControlLines(voltageMillivolts);
	GPIO_DRV_ClearPinOutput(kWarpPinTPS82740A_CTLEN);
	GPIO_DRV_SetPinOutput(kWarpPinTPS82740B_CTLEN);

	/*
	 *	Select the TS5A3154 to use the output of the TPS82740
	 *
	 *		IN = high selects the output of the TPS82740B:
	 *		IN = low selects the output of the TPS82740A:
	 */
	GPIO_DRV_SetPinOutput(kWarpPinTS5A3154_IN);
}


void	
setTPS82740CommonControlLines(uint16_t voltageMillivolts)
{
	/*
	 *	 From Manual:
	 *
	 *		TPS82740A:	VSEL1 VSEL2 VSEL3:	000-->1.8V, 111-->2.5V
	 *		TPS82740B:	VSEL1 VSEL2 VSEL3:	000-->2.6V, 111-->3.3V
	 */

	switch(voltageMillivolts)
	{
		case 2600:
		case 1800:
		{
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL1);
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL2);
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL3);
			
			break;
		}

		case 2700:
		case 1900:
		{
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL1);
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL2);
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL3);
			
			break;
		}

		case 2800:
		case 2000:
		{
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL1);
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL2);
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL3);
			
			break;
		}

		case 2900:
		case 2100:
		{
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL1);
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL2);
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL3);
			
			break;
		}

		case 3000:
		case 2200:
		{
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL1);
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL2);
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL3);
			
			break;
		}

		case 3100:
		case 2300:
		{
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL1);
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL2);
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL3);
			
			break;
		}

		case 3200:
		case 2400:
		{
			GPIO_DRV_ClearPinOutput(kWarpPinTPS82740_VSEL1);
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL2);
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL3);
			
			break;
		}

		case 3300:
		case 2500:
		{
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL1);
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL2);
			GPIO_DRV_SetPinOutput(kWarpPinTPS82740_VSEL3);
			
			break;
		}

		/*
		 *	Should never happen, due to previous check in enableSssupply()
		 */
		default:
		{
			SEGGER_RTT_printf(0, RTT_CTRL_RESET RTT_CTRL_BG_BRIGHT_YELLOW RTT_CTRL_TEXT_BRIGHT_WHITE kWarpConstantStringErrorSanity RTT_CTRL_RESET "\n");
		}
	}


	/*
	 *	Vload ramp time of the TPS82740 is 800us max (datasheet, Section 8.5 / page 5)
	 */
	OSA_TimeDelay(1);
}



void
enableSssupply(uint16_t voltageMillivolts)
{
	if (voltageMillivolts >= 1800 && voltageMillivolts <= 2500)
	{
		enableTPS82740A(voltageMillivolts);
	}
	else if (voltageMillivolts >= 2600 && voltageMillivolts <= 3300)
	{
		enableTPS82740B(voltageMillivolts);
	}
	else
	{
		SEGGER_RTT_printf(0, RTT_CTRL_RESET RTT_CTRL_BG_BRIGHT_RED RTT_CTRL_TEXT_BRIGHT_WHITE kWarpConstantStringErrorInvalidVoltage RTT_CTRL_RESET "\n", voltageMillivolts);
	}
}



void
disableSssupply(void)
{
	disableTPS82740A();
	disableTPS82740B();
	
	/*
	 *	Clear the pin. This sets the TS5A3154 to use the output of the TPS82740B,
	 *	which shouldn't matter in any case. The main objective here is to clear
	 *	the pin to reduce power drain.
	 *
	 *		IN = high selects the output of the TPS82740B:
	 *		IN = low selects the output of the TPS82740A:
	 */
	GPIO_DRV_SetPinOutput(kWarpPinTS5A3154_IN);
}



void
warpLowPowerSecondsSleep(uint32_t sleepSeconds, bool forceAllPinsIntoLowPowerState)
{
	/*
	 *	Set all pins into low-power states. We don't just disable all pins,
	 *	as the various devices hanging off will be left in higher power draw
	 *	state. And manuals say set pins to output to reduce power.
	 */
	if (forceAllPinsIntoLowPowerState)
	{
		lowPowerPinStates();
	}

	warpSetLowPowerMode(kWarpPowerModeVLPR, 0);
	warpSetLowPowerMode(kWarpPowerModeVLPS, sleepSeconds);
}



void
printPinDirections(void)
{
	/*
	SEGGER_RTT_printf(0, "KL03_VDD_ADC:%d\n", GPIO_DRV_GetPinDir(kWarpPinKL03_VDD_ADC));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "I2C0_SDA:%d\n", GPIO_DRV_GetPinDir(kWarpPinI2C0_SDA));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "I2C0_SCL:%d\n", GPIO_DRV_GetPinDir(kWarpPinI2C0_SCL));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "SPI_MOSI:%d\n", GPIO_DRV_GetPinDir(kWarpPinSPI_MOSI));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "SPI_MISO:%d\n", GPIO_DRV_GetPinDir(kWarpPinSPI_MISO));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "SPI_SCK_I2C_PULLUP_EN:%d\n", GPIO_DRV_GetPinDir(kWarpPinSPI_SCK_I2C_PULLUP_EN));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "TPS82740A_VSEL2:%d\n", GPIO_DRV_GetPinDir(kWarpPinTPS82740_VSEL2));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "ADXL362_CS:%d\n", GPIO_DRV_GetPinDir(kWarpPinADXL362_CS));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "kWarpPinPAN1326_nSHUTD:%d\n", GPIO_DRV_GetPinDir(kWarpPinPAN1326_nSHUTD));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "TPS82740A_CTLEN:%d\n", GPIO_DRV_GetPinDir(kWarpPinTPS82740A_CTLEN));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "TPS82740B_CTLEN:%d\n", GPIO_DRV_GetPinDir(kWarpPinTPS82740B_CTLEN));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "TPS82740A_VSEL1:%d\n", GPIO_DRV_GetPinDir(kWarpPinTPS82740_VSEL1));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "TPS82740A_VSEL3:%d\n", GPIO_DRV_GetPinDir(kWarpPinTPS82740_VSEL3));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "CLKOUT32K:%d\n", GPIO_DRV_GetPinDir(kWarpPinCLKOUT32K));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "TS5A3154_IN:%d\n", GPIO_DRV_GetPinDir(kWarpPinTS5A3154_IN));
	OSA_TimeDelay(100);
	SEGGER_RTT_printf(0, "SI4705_nRST:%d\n", GPIO_DRV_GetPinDir(kWarpPinSI4705_nRST));
	OSA_TimeDelay(100);
	*/
}



void
dumpProcessorState(void)
{
	uint32_t	cpuClockFrequency;

	CLOCK_SYS_GetFreq(kCoreClock, &cpuClockFrequency);
	SEGGER_RTT_printf(0, "\r\n\n\tCPU @ %u KHz\n", (cpuClockFrequency / 1000));
	SEGGER_RTT_printf(0, "\r\tCPU power mode: %u\n", POWER_SYS_GetCurrentMode());
	SEGGER_RTT_printf(0, "\r\tCPU clock manager configuration: %u\n", CLOCK_SYS_GetCurrentConfiguration());
	SEGGER_RTT_printf(0, "\r\tRTC clock: %d\n", CLOCK_SYS_GetRtcGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tSPI clock: %d\n", CLOCK_SYS_GetSpiGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tI2C clock: %d\n", CLOCK_SYS_GetI2cGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tLPUART clock: %d\n", CLOCK_SYS_GetLpuartGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tPORT A clock: %d\n", CLOCK_SYS_GetPortGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tPORT B clock: %d\n", CLOCK_SYS_GetPortGateCmd(1));
	SEGGER_RTT_printf(0, "\r\tFTF clock: %d\n", CLOCK_SYS_GetFtfGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tADC clock: %d\n", CLOCK_SYS_GetAdcGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tCMP clock: %d\n", CLOCK_SYS_GetCmpGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tVREF clock: %d\n", CLOCK_SYS_GetVrefGateCmd(0));
	SEGGER_RTT_printf(0, "\r\tTPM clock: %d\n", CLOCK_SYS_GetTpmGateCmd(0));
}



int
main(void)
{
	uint8_t					key;
	WarpSensorDevice			menuTargetSensor = kWarpSensorADXL362;
	uint16_t				menuI2cPullupValue = 32768;
	uint8_t					menuRegisterAddress = 0x00;
	uint16_t				menuSupplyVoltage = 0;


	rtc_datetime_t				warpBootDate;

	power_manager_user_config_t		warpPowerModeWaitConfig;
	power_manager_user_config_t		warpPowerModeStopConfig;
	power_manager_user_config_t		warpPowerModeVlpwConfig;
	power_manager_user_config_t		warpPowerModeVlpsConfig;
	power_manager_user_config_t		warpPowerModeVlls0Config;
	power_manager_user_config_t		warpPowerModeVlls1Config;
	power_manager_user_config_t		warpPowerModeVlls3Config;
	power_manager_user_config_t		warpPowerModeRunConfig;

	const power_manager_user_config_t	warpPowerModeVlprConfig = {
							.mode			= kPowerManagerVlpr,
							.sleepOnExitValue	= false,
							.sleepOnExitOption	= false
						};

	power_manager_user_config_t const *	powerConfigs[] = {
							/*
							 *	NOTE: This order is depended on by POWER_SYS_SetMode()
							 *
							 *	See KSDK13APIRM.pdf Section 55.5.3
							 */
							&warpPowerModeWaitConfig,
							&warpPowerModeStopConfig,
							&warpPowerModeVlprConfig,
							&warpPowerModeVlpwConfig,
							&warpPowerModeVlpsConfig,
							&warpPowerModeVlls0Config,
							&warpPowerModeVlls1Config,
							&warpPowerModeVlls3Config,
							&warpPowerModeRunConfig,
						};

	WarpPowerManagerCallbackStructure			powerManagerCallbackStructure;

	/*
	 *	Callback configuration structure for power manager
	 */
	const power_manager_callback_user_config_t callbackCfg0 = {
							callback0,
							kPowerManagerCallbackBeforeAfter,
							(power_manager_callback_data_t *) &powerManagerCallbackStructure};

	/*
	 *	Pointers to power manager callbacks.
	 */
	power_manager_callback_user_config_t const *	callbacks[] = {
								&callbackCfg0
						};



	/*
	 *	Enable clock for I/O PORT A and PORT B
	 */
	CLOCK_SYS_EnablePortClock(0);
	CLOCK_SYS_EnablePortClock(1);



	/*
	 *	Setup board clock source.
	 */
	g_xtal0ClkFreq = 32768U;



	/*
	 *	Initialize KSDK Operating System Abstraction layer (OSA) layer.
	 */
	OSA_Init();



	/*
	 *	Setup SEGGER RTT to output as much as fits in buffers.
	 *
	 *	Using SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL can lead to deadlock, since
	 *	we might have SWD disabled at time of blockage.
	 */
	SEGGER_RTT_ConfigUpBuffer(0, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_TRIM);


	SEGGER_RTT_WriteString(0, "\n\n\n\rBooting Warp, in 3... ");
	OSA_TimeDelay(500);
	SEGGER_RTT_WriteString(0, "2... ");
	OSA_TimeDelay(500);
	SEGGER_RTT_WriteString(0, "1...\n\r");
	OSA_TimeDelay(500);



	/*
	 *	Configure Clock Manager to default, and set callback for Clock Manager mode transition.
	 *
	 *	See "Clocks and Low Power modes with KSDK and Processor Expert" document (Low_Power_KSDK_PEx.pdf)
	 */
	CLOCK_SYS_Init(	g_defaultClockConfigurations,
			CLOCK_CONFIG_NUM,
			&clockCallbackTable,
			ARRAY_SIZE(clockCallbackTable)
			);
	CLOCK_SYS_UpdateConfiguration(CLOCK_CONFIG_INDEX_FOR_RUN, kClockManagerPolicyForcible);



	/*
	 *	Initialize RTC Driver
	 */
	RTC_DRV_Init(0);



	/*
	 *	Set initial date to 1st January 2016 00:00, and set date via RTC driver
	 */
	warpBootDate.year	= 2016U;
	warpBootDate.month	= 1U;
	warpBootDate.day	= 1U;
	warpBootDate.hour	= 0U;
	warpBootDate.minute	= 0U;
	warpBootDate.second	= 0U;
	RTC_DRV_SetDatetime(0, &warpBootDate);



	/*
	 *	Setup Power Manager Driver
	 */
	memset(&powerManagerCallbackStructure, 0, sizeof(WarpPowerManagerCallbackStructure));


	warpPowerModeVlpwConfig = warpPowerModeVlprConfig;
	warpPowerModeVlpwConfig.mode = kPowerManagerVlpw;
	
	warpPowerModeVlpsConfig = warpPowerModeVlprConfig;
	warpPowerModeVlpsConfig.mode = kPowerManagerVlps;
	
	warpPowerModeWaitConfig = warpPowerModeVlprConfig;
	warpPowerModeWaitConfig.mode = kPowerManagerWait;
	
	warpPowerModeStopConfig = warpPowerModeVlprConfig;
	warpPowerModeStopConfig.mode = kPowerManagerStop;

	warpPowerModeVlls0Config = warpPowerModeVlprConfig;
	warpPowerModeVlls0Config.mode = kPowerManagerVlls0;

	warpPowerModeVlls1Config = warpPowerModeVlprConfig;
	warpPowerModeVlls1Config.mode = kPowerManagerVlls1;

	warpPowerModeVlls3Config = warpPowerModeVlprConfig;
	warpPowerModeVlls3Config.mode = kPowerManagerVlls3;

	warpPowerModeRunConfig.mode = kPowerManagerRun;

	POWER_SYS_Init(	&powerConfigs,
			sizeof(powerConfigs)/sizeof(power_manager_user_config_t *),
			&callbacks,
			sizeof(callbacks)/sizeof(power_manager_callback_user_config_t *)
			);



	/*
	 *	Switch CPU to Very Low Power Run (VLPR) mode
	 */
	warpSetLowPowerMode(kWarpPowerModeVLPR, 0);



	/*
	 *	Initialize the GPIO pins with the appropriate pull-up, etc.,
	 *	defined in the inputPins and outputPins arrays (gpio_pins.c).
	 *
	 *	See also Section 30.3.3 GPIO Initialization of KSDK13APIRM.pdf
	 */
	GPIO_DRV_Init(inputPins  /* input pins */, outputPins  /* output pins */);
	
	/*
	 *	Note that it is lowPowerPinStates() that sets the pin mux mode,
	 *	so until we call it pins are in their default state.
	 */
	lowPowerPinStates();



	/*
	 *	Toggle LED3 (kWarpPinSI4705_nRST)
	 */
	GPIO_DRV_SetPinOutput(kWarpPinSI4705_nRST);
	OSA_TimeDelay(500);
	GPIO_DRV_ClearPinOutput(kWarpPinSI4705_nRST);
	OSA_TimeDelay(500);
	GPIO_DRV_SetPinOutput(kWarpPinSI4705_nRST);
	OSA_TimeDelay(500);
	GPIO_DRV_ClearPinOutput(kWarpPinSI4705_nRST);
	OSA_TimeDelay(500);
	GPIO_DRV_SetPinOutput(kWarpPinSI4705_nRST);
	OSA_TimeDelay(500);
	GPIO_DRV_ClearPinOutput(kWarpPinSI4705_nRST);



	/*
	 *	Initialize all the sensors
	 */
	initINA219current( 	0x40    /* i2cAddress */, 	&deviceINA219currentState	);
	initMPU6050(	0X68	/* i2cAddress */, 	&deviceMPU6050State	);


	/*
	 *	Initialization: Devices hanging off SPI
	 */
	initADXL362(&deviceADXL362State);

	/*
	 *	Power down all sensors:
	 */
	//activateAllLowPowerSensorModes();

	/*
	 *	Initialization: the PAN1326, generating its 32k clock
	 */
	//Disable for now
	//initPAN1326B(&devicePAN1326BState);
#ifdef WARP_PAN1323ETU
	initPAN1323ETU(&devicePAN1323ETUState);
#endif



	/*
	 *	Make sure SCALED_SENSOR_SUPPLY is off
	 */
	disableSssupply();

	SEGGER_RTT_WriteString(0, "We made it this far 3\n");
	/*
	 *	TODO: initialize the kWarpPinKL03_VDD_ADC, write routines to read the VDD and temperature
	 */

	
	/*
	 *	Wait for supply and pull-ups to settle.
	 */
	OSA_TimeDelay(1000);
//	WarpStatus currentSensorStatus = readSensorRegisterINA219(0x01);

	SEGGER_RTT_WriteString(0, "Time delay finished \n");
	
	/* This is where we will insert OLED driver code */
	//devSSD1331init();

	SEGGER_RTT_WriteString(0, "We made it this far 4");
	

	enableI2Cpins(menuI2cPullupValue);

	// Initialise Kalman filter terms
	float dt = 0.526; float P[2][2] = {{3.0,0.0},{0.0,3.0}}; float innovation; float S; // dt was measured manually
	float K[2] = {0.0, 0.0}; float state_bias = 0.0; float state_angle = 0.0;

	// Declare uncertainties. Q is the process noise covariance matrix and is assumed to be diagonal. R is the observation noise covariance.
	float Q_angle = 0.0007; float Q_gyroBias = 0.003; float R_measure = 0.001;


	for (int a=0; a<1000; a++){

	SEGGER_RTT_printf(0, "\rIteration: %d\n", a);

	/* Check calibration settings*/
	uint8_t gyroCalib = readSensorRegisterMPU6050(0x1B);
	uint8_t accelCalib = readSensorRegisterMPU6050(0x1C);
	uint8_t powerMgmt = readSensorRegisterMPU6050(0x6B);
	uint8_t powerMgmt2 = readSensorRegisterMPU6050(0x6C);

	/* Read accelerometer registers */
	uint8_t X_acc_H = readSensorRegisterMPU6050(0x3B); 
	uint8_t X_acc_L = readSensorRegisterMPU6050(0x3C);
	uint8_t Y_acc_H = readSensorRegisterMPU6050(0x3D);
	uint8_t Y_acc_L = readSensorRegisterMPU6050(0x3E);
	uint8_t Z_acc_H = readSensorRegisterMPU6050(0x3F);
	uint8_t Z_acc_L = readSensorRegisterMPU6050(0x40);
	
	/* Read gyro registers */
	uint8_t X_gyro_H = readSensorRegisterMPU6050(0x43); 
	uint8_t X_gyro_L = readSensorRegisterMPU6050(0x44);
	uint8_t Y_gyro_H = readSensorRegisterMPU6050(0x45);
	uint8_t Y_gyro_L = readSensorRegisterMPU6050(0x46);
	uint8_t Z_gyro_H = readSensorRegisterMPU6050(0x47);
	uint8_t Z_gyro_L = readSensorRegisterMPU6050(0x48);
	
	/* Combine low and high bytes */
	uint16_t X_accel = (X_acc_H << 8 )| X_acc_L;
        uint16_t Y_accel = (Y_acc_H << 8 )| Y_acc_L;
        uint16_t Z_accel = (Z_acc_H << 8 )| Z_acc_L;
	uint16_t X_rate = (X_gyro_H << 8) | X_gyro_L;
        uint16_t Y_rate = (Y_gyro_H << 8) | Y_gyro_L;
        uint16_t Z_rate = (Z_gyro_H << 8) | Z_gyro_L;
	
	int X_accel_n; int Y_accel_n; int Z_accel_n;
	int X_rate_n; int Y_rate_n; int Z_rate_n;
	
	/* Convert from twos complement to signed integer*/
	if(X_accel >= 32768){
		X_accel_n = (int)(-(65535-X_accel)+1);
	} else {
		X_accel_n = (int)X_accel;
	}
     	if(Y_accel >= 32768){
                Y_accel_n = (int)(-(65535-Y_accel)+1);
        } else {
                Y_accel_n = (int)Y_accel;
        }
	if(Z_accel >= 32768){
                Z_accel_n = (int)(-(65535-Z_accel)+1);
        } else {
                Z_accel_n = (int)Z_accel;
        }
	if(X_rate >= 32768){
                X_rate_n = (int)(-(65535-X_rate)+1);
        } else {
                X_rate_n = (int)X_rate;
        }
	if(Y_rate >= 32768){
                Y_rate_n = (int)(-(65535-Y_rate)+1);
        } else {
                Y_rate_n = (int)Y_rate;
        }
	if(Z_rate >= 32768){
                Z_rate_n = (int)(-(65535-Z_rate)+1);
        } else {
                Z_rate_n = (int)Z_rate;
        }

/* Previously used to print intermediate values. No longer used.
	SEGGER_RTT_printf(0, "\rX_accel is %04x, Y_accel is %04x, Z_accel is %04x\n", X_accel, Y_accel, Z_accel);
	SEGGER_RTT_printf(0, "\rX_accel is %d, Y_accel is %d, Z_accel is %d\n", X_accel_n, Y_accel_n, Z_accel_n);
	SEGGER_RTT_printf(0, "\rX_rate is %d, Y_rate is %d, Z_rate is %d\n", X_rate_n, Y_rate_n, Z_rate_n); */

/* Convert gyroscope readings to degrees per second, assuming full scale range of +- 250 degrees/s from datasheet and 0x1B register reading*/
	float X_rate_final = (float)X_rate_n/131.072; // Scale factor of 131.072
	float Y_rate_final = (float)Y_rate_n/131.072;
	float Z_rate_final = (float)Z_rate_n/131.072;
	
/*Previously used to print intermediate values. No longer used.
	SEGGER_RTT_printf(0, "\rX_rate in hex is %04x", X_rate);
	SEGGER_RTT_printf(0, "\rX_rate in deg/s is %d\n", (int)X_rate_final); */

/* Find estimated tilt from accelerometer*/
	double x_rot = atan2((double)(-Y_accel_n), sqrt((double)(X_accel_n*X_accel_n + Z_accel_n*Z_accel_n)));
	double y_rot = atan2((double)(X_accel_n) , sqrt((double)(Y_accel_n*Y_accel_n + Z_accel_n*Z_accel_n)));
	

/* Previously used to print intermediate values. No longer used
	int x_rotation = (int)(x_rot*100);
	int y_rotation = (int)(y_rot*100);

	SEGGER_RTT_printf(0, "\rX_ROT in radians is %d, Y_ROT in radians is %d\n", x_rotation, y_rotation);

	// Convert from radians to degrees
	x_rotation = (int)(x_rot*572);
	y_rotation = (int)(y_rot*572);
	
	SEGGER_RTT_printf(0, "\rX_ROT in degrees is %d\n", x_rotation); */

/* NOW carry out Kalman filter operations*/

	// Step 0 - calculate sensor readings
	float gyroRate = X_rate_final;
	float accAngle = (float)x_rot*57.2;

	// Step 1 - predict new angle based on previous angle and gyroscope reading
	float rate = gyroRate - state_bias;
	state_angle += dt*rate;

	// Step 2 - update P
	P[0][0] = dt*(dt*P[1][1] - P[0][1] - P[1][0] + Q_angle);
	P[0][1] -= dt*P[1][1];
	P[1][0] -= dt*P[1][1];
	P[1][1] += Q_gyroBias*dt;
	
	// Step 3 - calculate innovation, the difference between prediction and reading
	innovation = accAngle - state_angle;

	// Step 4 - update S
	S = P[0][0] + R_measure;

	// Step 5 - update Kalman gain
	K[0] = P[0][0]/S;
	K[1] = P[1][0]/S;

	// Step 6 - update angle and bias based on Kalman gain and innovation
	state_angle += K[0]*innovation;
	state_bias += K[1]*innovation;

	// Step 7 - final update of P based on Kalman gain
	float p00_inter = P[0][0];
	float p01_inter = P[0][1];

	P[0][0] -= K[0]*p00_inter;
	P[0][1] -= K[0]*p01_inter;
	P[1][0] -= K[1]*p00_inter;
	P[1][1] -= K[1]*p01_inter;
	
	// Print readings
	int angle_int = (int)(state_angle);
	int angle_reading_int = (int)(accAngle);
	int bias_int = (int)(state_bias);

//	int p00 = (int)(P[0][0]*100); int p01 = (int)(P[0][1]*100); int p10 = (int)(P[1][0]*100); int p11 = (int)(P[1][1]*100);

	SEGGER_RTT_printf(0, "\rAngle reading = %d, Angle estimate = %d, Bias estimate = %d\n", angle_reading_int, angle_int, bias_int);

//	SEGGER_RTT_printf(0, "\rP = [%d , %d], [%d , %d]\n", p00, p01, p10, p11);

	}

	disableI2Cpins();

	while (1)
	{//Previously warp menu
	}
	
	return 0;
}



void
loopForSensor(	const char *  tagString,
		WarpStatus  (* readSensorRegisterFunction)(uint8_t deviceRegister),
		volatile WarpI2CDeviceState *  i2cDeviceState,
		volatile WarpSPIDeviceState *  spiDeviceState,
		uint8_t  baseAddress,
		uint8_t  minAddress,
		uint8_t  maxAddress,
		int  repetitionsPerAddress,
		int  chunkReadsPerAddress,
		int  spinDelay,
		bool  autoIncrement,
		uint16_t  sssupplyMillivolts,
		uint8_t  referenceByte,
		uint16_t adaptiveSssupplyMaxMillivolts,
		bool  chatty
		)
{
#ifndef WARP_FRDMKL03
	WarpStatus		status;
	uint8_t			address = min(minAddress, baseAddress);
	int			readCount = repetitionsPerAddress + 1;
	int			nSuccesses = 0;
	int			nFailures = 0;
	int			nCorrects = 0;
	int			nBadCommands = 0;
	uint16_t		actualSssupplyMillivolts = sssupplyMillivolts;
	uint16_t		voltageTrace[readCount];



	if (	(!spiDeviceState && !i2cDeviceState) ||
		(spiDeviceState && i2cDeviceState) )
	{
			SEGGER_RTT_printf(0, RTT_CTRL_RESET RTT_CTRL_BG_BRIGHT_YELLOW RTT_CTRL_TEXT_BRIGHT_WHITE kWarpConstantStringErrorSanity RTT_CTRL_RESET "\n");
	}

	memset(voltageTrace, 0, readCount*sizeof(uint16_t));
	enableSssupply(actualSssupplyMillivolts);
	OSA_TimeDelay(100);

	SEGGER_RTT_WriteString(0, tagString);
	while ((address <= maxAddress) && autoIncrement)
	{
		for (int i = 0; i < readCount; i++) for (int j = 0; j < chunkReadsPerAddress; j++)
		{
			voltageTrace[i] = actualSssupplyMillivolts;
			status = readSensorRegisterFunction(address+j);
			if (status == kWarpStatusOK)
			{
				nSuccesses++;
				if (actualSssupplyMillivolts > sssupplyMillivolts)
				{
					actualSssupplyMillivolts -= 100;
					enableSssupply(actualSssupplyMillivolts);
				}

				if (spiDeviceState)
				{
					if (referenceByte == spiDeviceState->spiSinkBuffer[2])
					{
						nCorrects++;
					}

					if (chatty)
					{
						SEGGER_RTT_printf(0, "\r0x%02x --> [0x%02x 0x%02x 0x%02x]\n",
							address+j,
							spiDeviceState->spiSinkBuffer[0],
							spiDeviceState->spiSinkBuffer[1],
							spiDeviceState->spiSinkBuffer[2]);
					}
				}
				else
				{
					if (referenceByte == i2cDeviceState->i2cBuffer[0])
					{
						nCorrects++;
					}

					if (chatty)
					{
						SEGGER_RTT_printf(0, "\r0x%02x --> 0x%02x\n",
							address+j,
							i2cDeviceState->i2cBuffer[0]);
					}
				}
			}
			else if (status == kWarpStatusDeviceCommunicationFailed)
			{
				SEGGER_RTT_printf(0, "\r0x%02x --> ----\n",
					address+j);

				nFailures++;
				if (actualSssupplyMillivolts < adaptiveSssupplyMaxMillivolts)
				{
					actualSssupplyMillivolts += 100;
					enableSssupply(actualSssupplyMillivolts);
				}
			}
			else if (status == kWarpStatusBadDeviceCommand)
			{
				nBadCommands++;
			}

			if (spinDelay > 0) OSA_TimeDelay(spinDelay);
		}

		if (autoIncrement)
		{
			address++;
		}
	}

	/*
	 *	To make printing of stats robust, we switch to VLPR (assuming we are not already in VLPR).
	 *
	 *	As of circa issue-58 implementation, RTT printing when in RUN mode was flaky (achievable SWD speed too slow for buffer fill rate?)
	 */
	warpSetLowPowerMode(kWarpPowerModeVLPR, 0 /* sleep seconds : irrelevant here */);

	SEGGER_RTT_printf(0, "\r\n\t%d/%d success rate.\n", nSuccesses, (nSuccesses + nFailures));
	SEGGER_RTT_printf(0, "\r\t%d/%d successes matched ref. value of 0x%02x.\n", nCorrects, nSuccesses, referenceByte);
	SEGGER_RTT_printf(0, "\r\t%d bad commands.\n\n", nBadCommands);
	SEGGER_RTT_printf(0, "\r\tVoltage trace:\n", nBadCommands);

	for (int i = 0; i < readCount; i++)
	{
		SEGGER_RTT_printf(0, "\r\t\t%d\t%d\n", i, voltageTrace[i]);
	}
#endif

	return;
}



//void
//repeatRegisterReadForDeviceAndAddress(WarpSensorDevice warpSensorDevice, uint8_t baseAddress, uint16_t pullupValue, bool autoIncrement, int chunkReadsPerAddress, bool chatty, int spinDelay, int repetitionsPerAddress, uint16_t sssupplyMillivolts, uint16_t adaptiveSssupplyMaxMillivolts, uint8_t referenceByte)
//{
//	if (warpSensorDevice != kWarpSensorADXL362)
//	{
//		enableI2Cpins(pullupValue);
//	}
//
//	switch (warpSensorDevice)
//	{
//		case kWarpSensorADXL362:
//		{
//			/*
//			 *	ADXL362: VDD 1.6--3.5
//			 */
//			loopForSensor(	"\r\nADXL362:\n\r",		/*	tagString			*/
//					&readSensorRegisterADXL362,	/*	readSensorRegisterFunction	*/
//					NULL,				/*	i2cDeviceState			*/
//					&deviceADXL362State,		/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x2E,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorMMA8451Q:
//		{
//			/*
//			 *	MMA8451Q: VDD 1.95--3.6
//			 */
//			loopForSensor(	"\r\nMMA8451Q:\n\r",		/*	tagString			*/
//					&readSensorRegisterMMA8451Q,	/*	readSensorRegisterFunction	*/
//					&deviceMMA8451QState,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x31,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorBME680:
//		{
//			/*
//			 *	BME680: VDD 1.7--3.6
//			 */
//			loopForSensor(	"\r\nBME680:\n\r",		/*	tagString			*/
//					&readSensorRegisterBME680,	/*	readSensorRegisterFunction	*/
//					&deviceBME680State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x1D,				/*	minAddress			*/
//					0x75,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorBMX055accel:
//		{
//			/*
//			 *	BMX055accel: VDD 2.4V -- 3.6V
//			 */
//			loopForSensor(	"\r\nBMX055accel:\n\r",		/*	tagString			*/
//					&readSensorRegisterBMX055accel,	/*	readSensorRegisterFunction	*/
//					&deviceBMX055accelState,	/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x39,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorBMX055gyro:
//		{
//			/*
//			 *	BMX055gyro: VDD 2.4V -- 3.6V
//			 */
//			loopForSensor(	"\r\nBMX055gyro:\n\r",		/*	tagString			*/
//					&readSensorRegisterBMX055gyro,	/*	readSensorRegisterFunction	*/
//					&deviceBMX055gyroState,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x39,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorBMX055mag:
//		{
//			/*
//			 *	BMX055mag: VDD 2.4V -- 3.6V
//			 */
//			loopForSensor(	"\r\nBMX055mag:\n\r",		/*	tagString			*/
//					&readSensorRegisterBMX055mag,	/*	readSensorRegisterFunction	*/
//					&deviceBMX055magState,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x40,				/*	minAddress			*/
//					0x52,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorMAG3110:
//		{
//			/*
//			 *	MAG3110: VDD 1.95 -- 3.6
//			 */
//			loopForSensor(	"\r\nMAG3110:\n\r",		/*	tagString			*/
//					&readSensorRegisterMAG3110,	/*	readSensorRegisterFunction	*/
//					&deviceMAG3110State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x11,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorL3GD20H:
//		{
//			/*
//			 *	L3GD20H: VDD 2.2V -- 3.6V
//			 */
//			loopForSensor(	"\r\nL3GD20H:\n\r",		/*	tagString			*/
//					&readSensorRegisterL3GD20H,	/*	readSensorRegisterFunction	*/
//					&deviceL3GD20HState,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x0F,				/*	minAddress			*/
//					0x39,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorLPS25H:
//		{
//			/*
//			 *	LPS25H: VDD 1.7V -- 3.6V
//			 */
//			loopForSensor(	"\r\nLPS25H:\n\r",		/*	tagString			*/
//					&readSensorRegisterLPS25H,	/*	readSensorRegisterFunction	*/
//					&deviceLPS25HState,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x08,				/*	minAddress			*/
//					0x24,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorTCS34725:
//		{
//			/*
//			 *	TCS34725: VDD 2.7V -- 3.3V
//			 */
//			loopForSensor(	"\r\nTCS34725:\n\r",		/*	tagString			*/
//					&readSensorRegisterTCS34725,	/*	readSensorRegisterFunction	*/
//					&deviceTCS34725State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x1D,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorSI4705:
//		{
//			/*
//			 *	SI4705: VDD 2.7V -- 5.5V
//			 */
//			loopForSensor(	"\r\nSI4705:\n\r",		/*	tagString			*/
//					&readSensorRegisterSI4705,	/*	readSensorRegisterFunction	*/
//					&deviceSI4705State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x09,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorHDC1000:
//		{
//			/*
//			 *	HDC1000: VDD 3V--5V
//			 */
//			loopForSensor(	"\r\nHDC1000:\n\r",		/*	tagString			*/
//					&readSensorRegisterHDC1000,	/*	readSensorRegisterFunction	*/
//					&deviceHDC1000State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x1F,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorSI7021:
//		{
//			/*
//			 *	SI7021: VDD 1.9V -- 3.6V
//			 */
//			loopForSensor(	"\r\nSI7021:\n\r",		/*	tagString			*/
//					&readSensorRegisterSI7021,	/*	readSensorRegisterFunction	*/
//					&deviceSI7021State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x09,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorCCS811:
//		{
//			/*
//			 *	CCS811: VDD 1.8V -- 3.6V
//			 */
//			loopForSensor(	"\r\nCCS811:\n\r",		/*	tagString			*/
//					&readSensorRegisterCCS811,	/*	readSensorRegisterFunction	*/
//					&deviceCCS811State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0xFF,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorAMG8834:
//		{
//			/*
//			 *	AMG8834: VDD ?V -- ?V
//			 */
//			loopForSensor(	"\r\nAMG8834:\n\r",		/*	tagString			*/
//					&readSensorRegisterAMG8834,	/*	readSensorRegisterFunction	*/
//					&deviceAMG8834State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0xFF,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorAS7262:
//		{
//			/*
//			 *	AS7262: VDD 2.7--3.6
//			 */
//			loopForSensor(	"\r\nAS7262:\n\r",		/*	tagString			*/
//					&readSensorRegisterAS7262,	/*	readSensorRegisterFunction	*/
//					&deviceAS7262State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x2B,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//
//		case kWarpSensorAS7263:
//		{
//			/*
//			 *	AS7263: VDD 2.7--3.6
//			 */
//			loopForSensor(	"\r\nAS7263:\n\r",		/*	tagString			*/
//					&readSensorRegisterAS7263,	/*	readSensorRegisterFunction	*/
//					&deviceAS7263State,		/*	i2cDeviceState			*/
//					NULL,				/*	spiDeviceState			*/
//					baseAddress,			/*	baseAddress			*/
//					0x00,				/*	minAddress			*/
//					0x2B,				/*	maxAddress			*/
//					repetitionsPerAddress,		/*	repetitionsPerAddress		*/
//					chunkReadsPerAddress,		/*	chunkReadsPerAddress		*/
//					spinDelay,			/*	spinDelay			*/
//					autoIncrement,			/*	autoIncrement			*/
//					sssupplyMillivolts,		/*	sssupplyMillivolts		*/
//					referenceByte,			/*	referenceByte			*/
//					adaptiveSssupplyMaxMillivolts,	/*	adaptiveSssupplyMaxMillivolts	*/
//					chatty				/*	chatty				*/
//					);
//			break;
//		}
//		case kWarpSensorINA219:
//		{
//			 loopForSensor(  "\r\nINA219:\n\r",              /*      tagString                       */
//                                        &readSensorRegisterINA219,      /*      readSensorRegisterFunction      */
//                                        &deviceINA219currentState,             /*      i2cDeviceState                  */
//                                        NULL,                           /*      spiDeviceState                  */
//                                        baseAddress,                    /*      baseAddress                     */
//                                        0x00,                           /*      minAddress                      */
//                                        0x2B,                           /*      maxAddress                      */
//                                        repetitionsPerAddress,          /*      repetitionsPerAddress           */
//                                        chunkReadsPerAddress,           /*      chunkReadsPerAddress            */
//                                        spinDelay,                      /*      spinDelay                       */
//                                        autoIncrement,                  /*      autoIncrement                   */
//                                        sssupplyMillivolts,             /*      sssupplyMillivolts              */
//                                        referenceByte,                  /*      referenceByte                   */
//                                        adaptiveSssupplyMaxMillivolts,  /*      adaptiveSssupplyMaxMillivolts   */
//                                        chatty                          /*      chatty                          */
//					);
//			break;
//
//		}
//
//		default:
//		{
//			SEGGER_RTT_printf(0, "\r\tInvalid warpSensorDevice [%d] passed to repeatRegisterReadForDeviceAndAddress.\n", warpSensorDevice);
//		}
//	}
//
//	if (warpSensorDevice != kWarpSensorADXL362)
//	{
//		disableI2Cpins();
//	}
//}



int
char2int(int character)
{
	if (character >= '0' && character <= '9')
	{
		return character - '0';
	}

	if (character >= 'a' && character <= 'f')
	{
		return character - 'a' + 10;
	}

	if (character >= 'A' && character <= 'F')
	{
		return character - 'A' + 10;
	}

	return 0;
}



uint8_t
readHexByte(void)
{
	uint8_t		topNybble, bottomNybble;

	topNybble = SEGGER_RTT_WaitKey();
	bottomNybble = SEGGER_RTT_WaitKey();

	return (char2int(topNybble) << 4) + char2int(bottomNybble);
}



int
read4digits(void)
{
	uint8_t		digit1, digit2, digit3, digit4;
	
	digit1 = SEGGER_RTT_WaitKey();
	digit2 = SEGGER_RTT_WaitKey();
	digit3 = SEGGER_RTT_WaitKey();
	digit4 = SEGGER_RTT_WaitKey();

	return (digit1 - '0')*1000 + (digit2 - '0')*100 + (digit3 - '0')*10 + (digit4 - '0');
}



WarpStatus
writeByteToI2cDeviceRegister(uint8_t i2cAddress, bool sendCommandByte, uint8_t commandByte, bool sendPayloadByte, uint8_t payloadByte)
{
	i2c_status_t	status;
	uint8_t		commandBuffer[1];
	uint8_t		payloadBuffer[1];
	i2c_device_t	i2cSlaveConfig =
			{
				.address = i2cAddress,
				.baudRate_kbps = gWarpI2cBaudRateKbps
			};

	commandBuffer[0] = commandByte;
	payloadBuffer[0] = payloadByte;

	/*
	 *	TODO: Need to appropriately set the pullup value here
	 */
	enableI2Cpins(65535 /* pullupValue*/);

	status = I2C_DRV_MasterSendDataBlocking(
						0	/* instance */,
						&i2cSlaveConfig,
						commandBuffer,
						(sendCommandByte ? 1 : 0),
						payloadBuffer,
						(sendPayloadByte ? 1 : 0),
						1000	/* timeout in milliseconds */);
	disableI2Cpins();

	return (status == kStatus_I2C_Success ? kWarpStatusOK : kWarpStatusDeviceCommunicationFailed);
}



WarpStatus
writeBytesToSpi(uint8_t *  payloadBytes, int payloadLength)
{
	uint8_t		inBuffer[payloadLength];
	spi_status_t	status;
	
	enableSPIpins();
	status = SPI_DRV_MasterTransferBlocking(0		/* master instance */,
						NULL		/* spi_master_user_config_t */,
						payloadBytes,
						inBuffer,
						payloadLength	/* transfer size */,
						1000		/* timeout in microseconds (unlike I2C which is ms) */);					
	disableSPIpins();

	return (status == kStatus_SPI_Success ? kWarpStatusOK : kWarpStatusCommsError);
}



//void
//powerupAllSensors(void)
//{
//	WarpStatus	status;

	/*
	 *	BMX055mag
	 *
	 *	Write '1' to power control bit of register 0x4B. See page 134.
	 */
//	status = writeByteToI2cDeviceRegister(	deviceBMX055magState.i2cAddress		/*	i2cAddress		*/,
//						true					/*	sendCommandByte		*/,
//						0x4B					/*	commandByte		*/,
//						true					/*	sendPayloadByte		*/,
//						(1 << 0)				/*	payloadByte		*/);
//	if (status != kWarpStatusOK)
//	{
//		SEGGER_RTT_printf(0, "\r\tPowerup command failed, code=%d, for BMX055mag @ 0x%02x.\n", status, deviceBMX055magState.i2cAddress);
//	}
//}



//void
//activateAllLowPowerSensorModes(void)
//{
//	WarpStatus	status;



	
	// *	ADXL362:	See Power Control Register (Address: 0x2D, Reset: 0x00).
	 //*
	 //*	POR values are OK.
	 



	
	 //*	BMX055accel: At POR, device is in Normal mode. Move it to Deep Suspend mode.
	// *
	 //*	Write '1' to deep suspend bit of register 0x11, and write '0' to suspend bit of register 0x11. See page 23.
	 
//	status = writeByteToI2cDeviceRegister(	deviceBMX055accelState.i2cAddress,	//	i2cAddress		
//						true				,	//	sendCommandByte		
//						0x11				,	//	commandByte		
//						true				,	//	sendPayloadByte		
//						(1 << 5)			,	//	payloadByte		
//						);
//	if (status != kWarpStatusOK)
//	{
//		SEGGER_RTT_printf(0, "\r\tPowerdown command failed, code=%d, for BMX055accel @ 0x%02x.\n", status, deviceBMX055accelState.i2cAddress);
//	}


	
//	 *	BMX055gyro: At POR, device is in Normal mode. Move it to Deep Suspend mode.
//	 *
//	 *	Write '1' to deep suspend bit of register 0x11. See page 81.
//	 
//	status = writeByteToI2cDeviceRegister(	deviceBMX055gyroState.i2cAddress	/*	i2cAddress		*/,
//						true					/*	sendCommandByte		*/,
//						0x11					/*	commandByte		*/,
//						true					/*	sendPayloadByte		*/,
//						(1 << 5)				/*	payloadByte		*/);
//	if (status != kWarpStatusOK)
//	{
//		SEGGER_RTT_printf(0, "\r\tPowerdown command failed, code=%d, for BMX055gyro @ 0x%02x.\n", status, deviceBMX055gyroState.i2cAddress);
//	}




	/*
	 *	BMX055mag: At POR, device is in Suspend mode. See page 121.
	 *
	 *	POR state seems to be powered down.
	 */



	/*
	 *	MMA8451Q: See 0x2B: CTRL_REG2 System Control 2 Register (page 43).
	 *
	 *	POR state seems to be not too bad.
	 */



	/*
	 *	LPS25H: See Register CTRL_REG1, at address 0x20 (page 26).
	 *
	 *	POR state seems to be powered down.
	 */



	/*
	 *	MAG3110: See Register CTRL_REG1 at 0x10. (page 19).
	 *
	 *	POR state seems to be powered down.
	 */



	/*
	 *	HDC1000: currently can't turn it on (3V)
	 */



	/*
	 *	SI7021: Can't talk to it correctly yet.
	 */



	/*
	 *	L3GD20H: See CTRL1 at 0x20 (page 36).
	 *
	 *	POR state seems to be powered down.
	 */
//	status = writeByteToI2cDeviceRegister(	deviceL3GD20HState.i2cAddress	/*	i2cAddress		*/,
//						true				/*	sendCommandByte		*/,
//						0x20				/*	commandByte		*/,
//						true				/*	sendPayloadByte		*/,
//						0x00				/*	payloadByte		*/);
//	if (status != kWarpStatusOK)
//	{
//		SEGGER_RTT_printf(0, "\r\tPowerdown command failed, code=%d, for L3GD20H @ 0x%02x.\n", status, deviceL3GD20HState.i2cAddress);
//	}




	/*
	 *	BME680: TODO
	 */



	/*
	 *	TCS34725: By default, is in the "start" state (see page 9).
	 *
	 *	Make it go to sleep state. See page 17, 18, and 19.
	 */
//	status = writeByteToI2cDeviceRegister(	deviceTCS34725State.i2cAddress	/*	i2cAddress		*/,
//						true				/*	sendCommandByte		*/,
//						0x00				/*	commandByte		*/,
//						true				/*	sendPayloadByte		*/,
//						0x00				/*	payloadByte		*/);
//	if (status != kWarpStatusOK)
//	{
//		SEGGER_RTT_printf(0, "\r\tPowerdown command failed, code=%d, for TCS34725 @ 0x%02x.\n", status, deviceTCS34725State.i2cAddress);
//	}




	/*
	 *	SI4705: Send a POWER_DOWN command (byte 0x17). See AN332 page 124 and page 132.
	 *
	 *	For now, simply hold its reset line low.
	 */
//	GPIO_DRV_ClearPinOutput(kWarpPinSI4705_nRST);



//#ifdef WARP_FRDMKL03
	/*
	 *	PAN1323.
	 *
	 *	For now, simply hold its reset line low.
	 */
//	GPIO_DRV_ClearPinOutput(kWarpPinPAN1323_nSHUTD);
//#else
	/*
	 *	PAN1326.
	 *
	 *	For now, simply hold its reset line low.
	 */
//	GPIO_DRV_ClearPinOutput(kWarpPinPAN1326_nSHUTD);
//#endif
//}