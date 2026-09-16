#include "I2COperations.h"

/**
    @brief I2C write byte
    @param reg :Register address of operation object
    @param byte :The byte to be wrote.
    @return result of operation,non-zero if failed.
*/
HM330XErrorCode I2COperations::IIC_write_byte(uint8_t reg, uint8_t byte) {
    int ret = 0;
    _bus->beginTransmission(_IIC_ADDR);
    _bus->write(reg);
    _bus->write(byte);
    ret = _bus->endTransmission();
    if (!ret) {
        return NO_ERROR;
    } else {
        return ERROR_COMM;
    }
}

/**
    @brief I2C write 16bit value
    @param reg: Register address of operation object
    @param value: The 16bit value to be wrote .
    @return result of operation,non-zero if failed.
*/
HM330XErrorCode I2COperations::IIC_write_16bit(uint8_t reg, uint16_t value) {
    int ret = 0;
    _bus->beginTransmission(_IIC_ADDR);
    _bus->write(reg);

    _bus->write((uint8_t)(value >> 8));
    _bus->write((uint8_t) value);
    ret = _bus->endTransmission();
    if (!ret) {
        return NO_ERROR;
    } else {
        return ERROR_COMM;
    }
}

/**
    @brief I2C read byte
    @param reg: Register address of operation object
    @param byte: The byte to be read in.
    @return result of operation,non-zero if failed.
*/
HM330XErrorCode I2COperations::IIC_read_byte(uint8_t reg, uint8_t* byte) {
    uint32_t time_out_count = 0;
    _bus->beginTransmission(_IIC_ADDR);
    _bus->write(reg);
    _bus->endTransmission(false);

    _bus->requestFrom(_IIC_ADDR, (uint8_t) 1);
    while (1 != _bus->available()) {
        time_out_count++;
        if (time_out_count > 10) {
            return ERROR_COMM;
        }
        delay(1);
    }
    *byte = _bus->read();
    return NO_ERROR;
}

/**
    @brief I2C read 16bit value
    @param reg: Register address of operation object
    @param byte: The 16bit value to be read in.
    @return result of operation,non-zero if failed.
*/
HM330XErrorCode I2COperations::IIC_read_16bit(uint8_t start_reg, uint16_t* value) {
    uint32_t time_out_count = 0;
    uint8_t val = 0;
    *value = 0;
    _bus->beginTransmission(_IIC_ADDR);
    _bus->write(start_reg);
    _bus->endTransmission(false);

    _bus->requestFrom(_IIC_ADDR, sizeof(uint16_t));
    while (sizeof(uint16_t) != _bus->available()) {
        time_out_count++;
        if (time_out_count > 10) {
            return ERROR_COMM;
        }
        delay(1);
    }
    val = _bus->read();
    *value |= (uint16_t) val << 8;
    val = _bus->read();
    *value |= val;
    return NO_ERROR;
}

/**
    @brief I2C read some bytes
    @param reg: Register address of operation object
    @param data: The buf  to be read in.
    @param data_len: The length of buf need to read in.
    @return result of operation,non-zero if failed.
*/
HM330XErrorCode I2COperations::IIC_read_bytes(uint8_t start_reg, uint8_t* data, uint32_t data_len) {
    HM330XErrorCode ret = NO_ERROR;
    uint32_t time_out_count = 0;
    _bus->beginTransmission(_IIC_ADDR);
    _bus->write(start_reg);
    _bus->endTransmission(false);

    _bus->requestFrom(_IIC_ADDR, data_len);
    while (data_len != _bus->available()) {
        time_out_count++;
        if (time_out_count > 10) {
            return ERROR_COMM;
        }
        delay(1);
    }

    for (int i = 0; i < data_len; i++) {
        data[i] = _bus->read();
    }
    return ret;
}

/**
    @brief change the I2C bus and address from default.
    @param bus: Wire bus to be used
    @param IIC_ADDR: I2C address to be set
*/
void I2COperations::set_iic_addr(TwoWire *bus, uint8_t IIC_ADDR) {
    _bus = bus;
    _IIC_ADDR = IIC_ADDR;
}

HM330XErrorCode I2COperations::IIC_SEND_CMD(uint8_t CMD) {
    _bus->beginTransmission(_IIC_ADDR);
    _bus->write(CMD);
    byte ret = _bus->endTransmission();
    if (ret == 0) {
        return NO_ERROR;
    } else {
        return ERROR_COMM;
    }
}
