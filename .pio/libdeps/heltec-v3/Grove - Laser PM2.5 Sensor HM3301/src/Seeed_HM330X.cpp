/*
    Seeed_HM330X.cpp
    Driver for Seeed PM2.5 Sensor(HM300)

    Copyright (c) 2018 Seeed Technology Co., Ltd.
    Website    : www.seeed.cc
    Author     : downey
    Create Time: August 2018
    Change Log :
      17th July 2026 by Óscar González:
       - Add option to set a different I2C bus
       - Add checksum function

    The MIT License (MIT)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/

#include "Seeed_HM330X.h"

HM330X::HM330X() {}

HM330XErrorCode HM330X::select_comm() {
    return IIC_SEND_CMD(SELECT_COMM_CMD);
}

HM330XErrorCode HM330X::init(uint8_t IIC_ADDR) {
    return init(&Wire, IIC_ADDR);
}

HM330XErrorCode HM330X::init(TwoWire *bus, uint8_t IIC_ADDR) {
    _bus = bus;
    set_iic_addr(_bus, IIC_ADDR);
    _bus->begin();
    return select_comm();
}

HM330XErrorCode HM330X::read_sensor_value(uint8_t* data, uint32_t data_len) {
    uint32_t time_out_count = 0;
    HM330XErrorCode ret = NO_ERROR;
    _bus->requestFrom(0x40, 29);
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

HM330XErrorCode HM330X::checksum_calc(uint8_t* data) {
    if (NULL == data) {
        return ERROR_PARAM;
    }

    uint8_t sum = 0;
    for (int i = 0; i < 28; i++) {
        sum += data[i];
    }

    if (sum != data[28]) {
        return ERROR_COMM;
    }

    return NO_ERROR;
}
