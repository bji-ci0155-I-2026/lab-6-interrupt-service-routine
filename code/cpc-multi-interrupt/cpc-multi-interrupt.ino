/*
Universidad de Costa Rica - Sistemas Empotrados de Tiempo Real (CI-0155)
Parte 2: Adafruit Circuit Playground Classic (ATmega32u4)

Two timer interrupts drive the board:
  - Timer1 ISR (~5 ms): debounces Button A and flags a press.
  - Timer3 ISR (~120 ms): flags the periodic tick that advances the animation.

Button A cycles four light effects, each with its own melody:
  SPIN -> TWINKLE -> ALT -> CHASE -> SPIN ...

The CPC buttons (D4, D19) are not interrupt-capable on the ATmega32u4, so we
sample the button from a fast timer ISR instead of attachInterrupt(). See the
README for the full hardware rationale and the Timer4/playTone() interaction.

ISR rules followed: shared variables are volatile, ISRs only set flags, and all
heavy work (NeoPixel rendering, Serial, sound) happens in loop().
*/

#include <Adafruit_CircuitPlayground.h>
#include <util/atomic.h>

// Christmas palette
#define RED    0xFF0000
#define GREEN  0x008000
#define WHITE  0xFFFFFF
#define GOLD   0xFFD700
#define OFF    0x000000

// Light effects cycled by Button A
#define MODE_SPIN     0   // christmas colors rotating around the ring
#define MODE_TWINKLE  1   // random pixels flash like snow
#define MODE_ALT      2   // alternating red/green
#define MODE_CHASE    3   // a lit pixel chases around the ring
#define MODE_COUNT    4

const uint8_t BUTTON_A   = 4;     // CPC left button (high when pressed)
const uint8_t NUM_PIXELS = 10;
const uint8_t BRIGHTNESS = 25;

#define PERIODIC_MS 120           // animation tick period (Timer3)
#define NOTE_GAP_MS 30            // brief silence so repeated notes stay distinct
#define DEBOUNCE_SAMPLES 4        // consecutive 5 ms HIGH samples to confirm a press

const uint32_t navidenos[NUM_PIXELS] = {
  RED, GREEN, WHITE, GOLD, RED, GREEN, WHITE, GOLD, RED, GREEN
};

#include "pitches.h"

// One note { frequency, duration_ms } advances per animation step; REST is silence.
struct Note {
  uint16_t freq;
  uint16_t dur;
};

#define REST 0
#define WHL  640   // whole-ish
#define HLF  320   // half
#define QTR  220   // quarter
#define DOT  330   // dotted quarter
#define ETH  150   // eighth

// SPIN: "Jingle Bells" (chorus)
const Note spinMelody[] = {
  {NOTE_E5,QTR},{NOTE_E5,QTR},{NOTE_E5,HLF},
  {NOTE_E5,QTR},{NOTE_E5,QTR},{NOTE_E5,HLF},
  {NOTE_E5,QTR},{NOTE_G5,QTR},{NOTE_C5,DOT},{NOTE_D5,ETH},{NOTE_E5,WHL},
  {REST,QTR}
};

// TWINKLE: "Twinkle, Twinkle, Little Star"
const Note twinkleMelody[] = {
  {NOTE_C5,QTR},{NOTE_C5,QTR},{NOTE_G5,QTR},{NOTE_G5,QTR},
  {NOTE_A5,QTR},{NOTE_A5,QTR},{NOTE_G5,HLF},
  {NOTE_F5,QTR},{NOTE_F5,QTR},{NOTE_E5,QTR},{NOTE_E5,QTR},
  {NOTE_D5,QTR},{NOTE_D5,QTR},{NOTE_C5,HLF},
  {REST,QTR}
};

// ALT: "Mary Had a Little Lamb"
const Note altMelody[] = {
  {NOTE_E5,QTR},{NOTE_D5,QTR},{NOTE_C5,QTR},{NOTE_D5,QTR},
  {NOTE_E5,QTR},{NOTE_E5,QTR},{NOTE_E5,HLF},
  {NOTE_D5,QTR},{NOTE_D5,QTR},{NOTE_D5,HLF},
  {NOTE_E5,QTR},{NOTE_G5,QTR},{NOTE_G5,HLF},
  {REST,QTR}
};

// CHASE: "Ode to Joy" (opening)
const Note chaseMelody[] = {
  {NOTE_E5,QTR},{NOTE_E5,QTR},{NOTE_F5,QTR},{NOTE_G5,QTR},
  {NOTE_G5,QTR},{NOTE_F5,QTR},{NOTE_E5,QTR},{NOTE_D5,QTR},
  {NOTE_C5,QTR},{NOTE_C5,QTR},{NOTE_D5,QTR},{NOTE_E5,QTR},
  {NOTE_E5,DOT},{NOTE_D5,ETH},{NOTE_D5,HLF},
  {REST,QTR}
};

struct Melody {
  const Note* notes;
  uint8_t     length;
};

const Melody melodies[MODE_COUNT] = {
  { spinMelody,    sizeof(spinMelody)    / sizeof(spinMelody[0])    },
  { twinkleMelody, sizeof(twinkleMelody) / sizeof(twinkleMelody[0]) },
  { altMelody,     sizeof(altMelody)     / sizeof(altMelody[0])     },
  { chaseMelody,   sizeof(chaseMelody)   / sizeof(chaseMelody[0])   }
};

// Shared between the ISRs and loop(), so every one is volatile.
volatile uint8_t system_state  = MODE_SPIN;  // current effect (1-byte = atomic on AVR)
volatile bool    button_event  = false;      // set by Timer1 ISR on a press
volatile bool    periodic_tick = false;      // set by Timer3 ISR each interval

uint16_t frame = 0;              // animation frame, only touched by loop()

/*
setupTimer1: fire an interrupt every 5 ms to sample the button.

A hardware timer is just a counter that ticks up from the 16 MHz CPU clock.
"Clear Timer on Compare" (CTC) mode makes it count up to OCR1A, fire the
TIMER1_COMPA interrupt, reset to 0, and repeat giving us a steady heartbeat.

  - Prescaler /64 slows the clock to 16 MHz / 64 = 250 kHz (one tick = 4 us).
  - OCR1A = 1249 means it counts 1250 ticks (0..1249) before firing.
  - 1250 ticks * 4 us = 5 ms per interrupt.
*/
void setupTimer1() {
  TCCR1A = 0;                             // clear control regs to a known state
  TCCR1B = 0;
  TCNT1  = 0;                             // start the counter at 0
  OCR1A  = 1249;                          // count to 1249 -> 5 ms
  TCCR1B |= (1 << WGM12);                 // CTC mode (reset on compare match)
  TCCR1B |= (1 << CS11) | (1 << CS10);    // prescaler /64 -> 250 kHz
  TIMSK1 |= (1 << OCIE1A);                // enable the compare-match interrupt
}

// Runs every 5 ms. Counts consecutive HIGH samples to reject button bounce,
// and flags a single event on the edge into a confirmed press. Flag only —
// loop() does the actual mode change.
ISR(TIMER1_COMPA_vect) {
  static uint8_t stable_count = 0;
  static bool    pressed = false;

  if (digitalRead(BUTTON_A) == HIGH) {
    if (stable_count < DEBOUNCE_SAMPLES) {
      stable_count++;
      if (stable_count == DEBOUNCE_SAMPLES && !pressed) {
        pressed = true;          // confirmed new press
        button_event = true;
      }
    }
  } else {
    stable_count = 0;
    pressed = false;             // released, ready for the next press
  }
}

/*
setupTimer3: same idea as Timer1, but a slower /1024 prescaler for the ~120 ms
animation tick. OCR3A is computed from F_CPU so the timing stays correct if the
clock changes:  ticks = (16 MHz / 1024) * 120 ms / 1000 - 1.
*/
void setupTimer3() {
  TCCR3A = 0;
  TCCR3B = 0;
  TCNT3  = 0;
  OCR3A  = (uint16_t)((F_CPU / 1024UL) * PERIODIC_MS / 1000UL) - 1;
  TCCR3B |= (1 << WGM32);                 // CTC mode
  TCCR3B |= (1 << CS32) | (1 << CS30);    // prescaler /1024
  TIMSK3 |= (1 << OCIE3A);                // enable the compare-match interrupt
}

// Runs every ~120 ms. Flag only.
ISR(TIMER3_COMPA_vect) {
  periodic_tick = true;
}

// Effect renderers: run from loop() only (NeoPixel show() is too slow for an ISR).
void setAllPixels(uint32_t color) {
  for (uint8_t p = 0; p < NUM_PIXELS; p++) {
    CircuitPlayground.setPixelColor(p, color);
  }
}

// Rotate the christmas palette around the ring.
void renderSpin(uint16_t f) {
  for (uint8_t p = 0; p < NUM_PIXELS; p++) {
    CircuitPlayground.setPixelColor(p, navidenos[(p + f) % NUM_PIXELS]);
  }
}

// Random twinkle, like falling snow.
void renderTwinkle() {
  setAllPixels(OFF);
  for (uint8_t i = 0; i < 4; i++) {
    CircuitPlayground.setPixelColor(random(NUM_PIXELS), navidenos[random(NUM_PIXELS)]);
  }
}

// Classic alternating red/green, swapping every frame.
void renderAlternate(uint16_t f) {
  bool swap = (f & 1);
  for (uint8_t p = 0; p < NUM_PIXELS; p++) {
    bool evenPixel = (p % 2 == 0);
    if (evenPixel != swap) {
      CircuitPlayground.setPixelColor(p, RED);
    } else {
      CircuitPlayground.setPixelColor(p, GREEN);
    }
  }
}

// A gold light chasing around the ring.
void renderChase(uint16_t f) {
  setAllPixels(OFF);
  uint8_t head = f % NUM_PIXELS;
  CircuitPlayground.setPixelColor(head, GOLD);
  CircuitPlayground.setPixelColor((head + 1) % NUM_PIXELS, GOLD);
}

void renderEffect(uint8_t mode, uint16_t f) {
  switch (mode) {
    case MODE_SPIN:    renderSpin(f);      break;
    case MODE_TWINKLE: renderTwinkle();    break;
    case MODE_ALT:     renderAlternate(f); break;
    case MODE_CHASE:   renderChase(f);     break;
  }
}

const char* modeName(uint8_t mode) {
  switch (mode) {
    case MODE_SPIN:    return "SPIN";
    case MODE_TWINKLE: return "TWINKLE";
    case MODE_ALT:     return "ALT";
    case MODE_CHASE:   return "CHASE";
    default:           return "?";
  }
}

void setup() {
  Serial.begin(9600);
  CircuitPlayground.begin();
  CircuitPlayground.setBrightness(BRIGHTNESS);

  // Configure both timers with interrupts globally off, then re-enable them.
  cli();
  setupTimer1();
  setupTimer3();
  sei();

  Serial.println(F("CPC multi-interrupt ready."));
  Serial.println(F("Timer1: button A (debounced). Timer3: periodic LED task."));
  Serial.print(F("Mode: "));
  Serial.println(modeName(system_state));
  renderEffect(system_state, frame);
}

void loop() {
  // Read and clear both flags in one atomic step so an ISR firing mid-read
  // can't corrupt the snapshot or drop an event.
  bool do_button;
  bool do_tick;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    do_button = button_event;
    do_tick   = periodic_tick;
    button_event = false;
    periodic_tick = false;
  }

  // Button wins: apply and render a pending mode change first.
  if (do_button) {
    system_state = (system_state + 1) % MODE_COUNT;
    frame = 0;                       // restart the new effect from frame 0
    Serial.print(F("Button A -> mode: "));
    Serial.println(modeName(system_state));
    renderEffect(system_state, frame);
  }

  if (do_tick) {
    uint8_t mode = system_state;     // 1-byte read, atomic on AVR
    frame++;
    renderEffect(mode, frame);

    // Play one note per tick (blocking), then a short gap. The note duration
    // sets the tempo; the Timer3 tick just triggers the next step.
    const Melody& m = melodies[mode];
    const Note& n = m.notes[frame % m.length];
    if (n.freq != REST) {
      CircuitPlayground.playTone(n.freq, n.dur);
    } else {
      delay(n.dur);
    }
    delay(NOTE_GAP_MS);

    Serial.print(F("[tick] mode="));
    Serial.print(modeName(mode));
    Serial.print(F(" frame="));
    Serial.println(frame);
  }
}
