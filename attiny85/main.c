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

#define PPM_NUM_CHANNELS        9

typedef enum
{
  PPM_GAP,        	// During a pulse
  PPM_PULSE,        	// Waiting for the next pulse
  PPM_WAITING,        	// Waiting for sync pulse to end
} ppm_state_t;

// PPM state
ppm_state_t ppm_state = PPM_WAITING;

// PPM last edge tracker
uint16_t ppm_last_edge = 0;

// PPM pulse widths and channel counter
uint16_t ppm_data[PPM_NUM_CHANNELS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
uint16_t ppm_syncwidth = 0;
uint8_t  ppm_channel = 0;

// Counter to measure latency between INT out and handling
uint16_t ppm_capture_time = 0;

// Timer overflow counter
uint8_t overflow_count = 0;

void init_hardware(void)
{
  // Disable watchdog
  MCUSR &= ~(1<<WDRF);
  wdt_disable();

  // Disable timer 1 and ADC
  PRR = (1<<PRTIM1) | (0<<PRTIM0) | (0<<PRUSI) | (0<<PRADC);

  // Initialise timer 0 as a plain counter at 1uS per tick
  // Normal mode
  TCCR0A = 0;
  // 8/ Prescaler
  TCCR0B = (0<<CS02) | (1<<CS01) | (0<<CS00);
  // Enable overflow interrupt
  TIMSK = (1<<TOIE0);

  // Setup PORTB pins
  // Disable pull ups on all pins, set outputs low
  PORTB = 0;
  //      PPM in     INT out    SCL i/o    unused     SDA i/o
  // TODO                        x                     x
  DDRB  = (0<<PB4) | (1<<PB3) | (0<<PB2) | (0<<PB1) | (0<<PB0);
  // Enable pin change interrupt for PPM
  PCMSK = (1<<PCINT4);
  GIMSK = (1<<PCIE);
  
  // Setup USI
  // TODO
}

void set_interrupt(uint8_t value)
{
  if (value)
  {
   PORTB |= (1<<PB3);
  }
  else
  {
    PORTB &= ~(1<<PB3);
  }
}

int main(void)
{
  // Initialise everything
  init_hardware();

  // Enable interrupts
  sei();

  set_interrupt(1);

  // TODO: Sleep
  while (1);
}

// Timer overflow interrupt
ISR(TIM0_OVF_vect)
{
  ++overflow_count;
  //set_interrupt(overflow_count % 2);
}

// Pin change interrupt for PPM
ISR(PCINT0_vect)
{
  // If we hit the end of a gap, get out ASAP
  if (ppm_state == PPM_GAP)
  {
    ppm_state = PPM_PULSE;
    return;
  }

  // This is an important event so capture the current time ASAP
  const uint8_t snapshot = TCNT0;
  const uint16_t now = ((uint16_t)overflow_count << 8) | snapshot;

  // Calculate the pulsewidth (assuming we only ever roll over once)
  const uint16_t pulsewidth = (now > ppm_last_edge) ? (now - ppm_last_edge) : (now + (0xffff - ppm_last_edge));

  // Update the last edge
  ppm_last_edge = now;

  // We got a long pulse, so treat it as a sync pulse and reset
  if (pulsewidth > 8000)
  {
    ppm_state = PPM_GAP;
    ppm_channel = 0;
    ppm_syncwidth = pulsewidth;
    // Clear interrupt - host CPU waited too long
    set_interrupt(0);
    return;
  }

  // Hit the end of a pulse
  if (ppm_state == PPM_PULSE)
  {
    // Record pulse
    ppm_data[ppm_channel++] = pulsewidth;

    // More to come, wait for the next
    if (ppm_channel < PPM_NUM_CHANNELS)
    {
      ppm_state = PPM_GAP;
    }
    // We're done - signal interrupt to host CPU and wait for next sync pulse
    else
    {
      ppm_state = PPM_WAITING;
      ppm_channel = 0;
      set_interrupt(1);
    }
  }
}

