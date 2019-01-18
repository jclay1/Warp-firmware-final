void initINA219current(const uint8_t i2cAddress, WarpI2CDeviceState volatile * deviceStatePointer);
WarpStatus readSensorRegisterINA219(uint8_t deviceRegister);
WarpStatus      readSensorSignalINA219(WarpTypeMask signal,
                                        WarpSignalPrecision precision,
                                        WarpSignalAccuracy accuracy,
                                        WarpSignalReliability reliability,
                                        WarpSignalNoise noise);

