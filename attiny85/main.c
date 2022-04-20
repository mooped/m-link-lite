// M-Link PPM offload
// For board v1.1
// Steve Barnett 2022, Beeston
//
// PORTB
//  PB0 - SDA (I2C)
//  PB1 - x
//  PB2 - SCK (I2C)
//  PB3 - INT (out)
//  PB4 - PPM - PCINT4 (in)
//

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include <stddef.h>

void timer_init(void)
{
  // Set timer 0 to normal mode
  // Counting up with no clear
  // TOV0 bit will be set upon overflow
  // Set timer to CTC mode (reset at OCRA), divide by 1024 with prescaler
  TCCR0A = 0;
  // Divide by 8
  TCCR0B = (0<<CS02) | (1<<CS01) | (1<<CS00);
  // Timer/Counter Interrupt Mask Register
  // Enable Timer/Counter 0 Overflow Interrupt
  TIMSK = (1<<TOIE0);
}

#define PPM_NUM_CHANNELS	9

// PPM state structs
typedef struct
{
  uint8_t jittered;
  uint16_t ppm_data[PPM_NUM_CHANNELS];
} ppm_state_t;

// Initial state
ppm_state_t g_state = {
  .jittered = 0,
  .ppm_data = {
    0, 0, 0, 0, 0, 0, 0, 0, 0,
  }
  // state is implicitly zeroed
};

void init_hardware(void)
{
  // Disable watchdog
  MCUSR &= ~(1<<WDRF);
  wdt_disable();

  // Disable prescaler
  clock_prescale_set(clock_div_1);
}

int main(void)
{
  init_hardware();

  // Flash LEDs as a test
  while (1)
  {

  }
}

