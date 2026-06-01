/*
Developed in the Universidad de Costa Rica for the course 
Sistemas Empotrados de Tiempo Real - CI-0155
Simple ISR interrupt.

Components required:
- 1x Arduino UNO R3
- 1x Push Button
- 1x Pulldown Resistor 470k O
- Jumper wires (Connections to 5V and GND)
*/
// Define the possible states  
#define NORMAL_STATE       0  
#define INTERRUPTED_STATE  1

// The Arduino UNO R3 supports hardware interrupts on digital pins 2 and 3.  
const int buttonPin = 2;

// 1. Use of 'volatile' (Best Practice)
// Global variable that changes with the interrupt
volatile int system_state = NORMAL_STATE;

// Debouncing: time-window guard inside the ISR.
// Since this is an edge-triggered ISR (RISING), we discard spurious edges
// that arrive within DEBOUNCE_MS of the last accepted press.
const unsigned long DEBOUNCE_MS = 200;        // anti-bounce window
volatile unsigned long last_isr_time = 0;     // timestamp of last valid edge

// 2. Interrupt Service Routine (ISR)
// Kept as short as possible (Best Practice)
void on_button_press() {
    // millis() is readable inside an ISR; its counter does not advance during
    // the ISR, but the captured value is valid to compare against the last edge.
    unsigned long now = millis();
    if (now - last_isr_time >= DEBOUNCE_MS) {
        system_state = INTERRUPTED_STATE;
        last_isr_time = now;
    }
}

void setup() {  
    Serial.begin(9600);  
    // Configure the pin with an external pulldown resistor connected to GND.
    // The push button will connect 5V to the pin when pressed.
    pinMode(buttonPin, INPUT);

    // 3. Decoupling and registering the Handler  
    // Here the Arduino framework handles the ATmega328P microcontroller registers for us.  
    // We only pass the pin, the callback function, and the event (RISING = when pressed, due to pulldown).  
    attachInterrupt(digitalPinToInterrupt(buttonPin), on_button_press, RISING);  
}

void loop() {  
    // Main Routine  
    if (system_state == INTERRUPTED_STATE) {

        // Here heavy processing occurs (e.g. trigger alarm, stop motors)  
        Serial.print("Interrupt detected! system_state: ");
        Serial.println(system_state);

        delay(1000);

        // Return the flag to normal after finishing  
        system_state = NORMAL_STATE;  
        Serial.print("State restored. system_state: ");
        Serial.println(system_state);
    } else {  
        // The robot or system continues with its standard behavior  
        Serial.print("System in normal state. system_state: ");
        Serial.println(system_state);
    }  
}
