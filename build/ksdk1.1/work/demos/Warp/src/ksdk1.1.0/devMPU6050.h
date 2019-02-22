
void initMPU6050(const uint8_t i2cAddress, WarpI2CDeviceState volatile * deviceStatePointer);

WarpStatus readSensorRegisterMPU6050(uint8_t deviceRegister);

WarpStatus writeSensorRegisterMPU6050(uint8_t deviceRegister, uint8_t register_data);
