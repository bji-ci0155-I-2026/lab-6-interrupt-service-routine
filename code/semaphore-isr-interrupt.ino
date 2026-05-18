/*
Developed in the Universidad de Costa Rica for the course 
Sistemas Empotrados de Tiempo Real - CI-0155
Fixed time semaphore with ISR interrupt.

Components required:
- 1x Arduino UNO R3
- 3x LEDs (Red, Yellow, Green)
- 3x Resistors for LEDs 220 Ohm
- 1x Push Button
- 1x Pulldown Resistor 470k Ohm
- Jumper wires
*/

// Define the possible states
#define NORMAL_STATE       0
#define INTERRUPTED_STATE  1

// Global variable that changes with the interrupt (volatile is a best practice)
volatile uint8_t system_state = NORMAL_STATE;

// Define constants for the Arduino Uno R3 semaphore
const uint8_t RED = 2;            // define the red led pin
const uint8_t YELLOW = 4;         // define the yellow led pin
const uint8_t GREEN = 7;          // define the green led pin

const uint8_t BUTTON_PIN = 3;     // define the hardware interrupt pin

// Define stop and pass tempos
const uint16_t STOP_TEMPO = 4000;      // define the stop tempo
const uint16_t WARNING_TEMPO = 2000;   // define the warning tempo
const uint16_t PASS_TEMPO = 4000;      // define the pass tempo

// Interrupt Service Routine (ISR)
// Kept short, only sets the flag
void on_button_press() {
    system_state = INTERRUPTED_STATE;
}

void setup() {
  Serial.begin(9600);

  // Declare output pins for LEDs
  pinMode(GREEN, OUTPUT);
  pinMode(YELLOW, OUTPUT);
  pinMode(RED, OUTPUT);   

  // Configure the pin with an external pulldown resistor connected to GND.
  // The push button will connect 5V to the pin when pressed.
  pinMode(BUTTON_PIN, INPUT);

  // Register the Handler
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), on_button_press, RISING);
}

void loop() {
  if (system_state == INTERRUPTED_STATE) {
    Serial.println("Pedestrian button pressed! Changing to Yellow then Red.");
    Serial.println("System state:");
    Serial.println(system_state);
    
    // Ensure Green is off
    digitalWrite(GREEN, LOW);

    // Yellow light
    digitalWrite(YELLOW, HIGH);
    delay(WARNING_TEMPO);
    digitalWrite(YELLOW, LOW);

    // Red light (Pedestrians pass)
    digitalWrite(RED, HIGH);
    delay(STOP_TEMPO);
    digitalWrite(RED, LOW);

    // Restore state
    system_state = NORMAL_STATE;
    Serial.println("System state:");
    Serial.println(system_state);
    Serial.println("Returning to normal fixed-time cycle.");
  } else {
    // Normal Fixed-Time Sequence
    Serial.println("System state:");
    Serial.println(system_state);
    
    // Red light
    digitalWrite(RED, HIGH);
    delay(STOP_TEMPO);
    digitalWrite(RED, LOW);

    // Clear flag just in case the button was pressed during red
    system_state = NORMAL_STATE;

    // Green light - This is the only state we allow to be interrupted early
    digitalWrite(GREEN, HIGH);
    
    // Break the delay into small chunks to check the volatile flag
    for (uint16_t i = 0; i < PASS_TEMPO; i += 50) {
      if (system_state == INTERRUPTED_STATE) {
        break; // Exit the loop early if button is pressed
      }
      delay(50);
    }
    digitalWrite(GREEN, LOW);

    // If interrupted during green, return to start of loop to handle the interrupt block
    if (system_state == INTERRUPTED_STATE) {
      return;
    }

    // Yellow light
    digitalWrite(YELLOW, HIGH);
    delay(WARNING_TEMPO);
    digitalWrite(YELLOW, LOW);

    // Clear flag just in case the button was pressed during yellow,
    // so we don't inappropriately trigger the interrupt sequence right after normal yellow
    system_state = NORMAL_STATE;
  }
}