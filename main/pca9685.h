// PCA9685 PWM controller driver
#pragma once

void pca9685_set_addr(int address);

void pca9685_initialize(void);

void pca9685_set_raw(int channel, int pulsewidth);
void pca9685_set_microseconds(int channel, int pulsewidth);
void pca9685_set_on(int channel);
void pca9685_set_off(int channel);

void pca9685_set_all_raw(int pulsewidth);
void pca9685_set_all_microseconds(int pulsewidth);
void pca9685_set_all_on(void);
void pca9685_set_all_off(void);

