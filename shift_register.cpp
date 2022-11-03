#include <Arduino.h>

void setup()
{
    // put your setup code here, to ru once:
    Serial.begin(9600);
    pinMode(21, INPUT);
    pinMode(23, OUTPUT);//clk
    pinMode(22, OUTPUT);//latch
    pinMode(14, OUTPUT);//data
}


uint8_t shiftBit(uint8_t latch, uint8_t data, uint8_t clock, uint8_t state, uint8_t max) {
    digitalWrite(latch, LOW);
    
    if (state == 0)
        digitalWrite(data, HIGH);
    else
        digitalWrite(data, LOW);
    
    digitalWrite(clock, HIGH);
    digitalWrite(clock, LOW);
    
    digitalWrite(latch, HIGH);

    return (state + 1) % max;
}

uint8_t reg_state = 0;
void loop()
{
    if (digitalRead(21)) {
        Serial.println("Read Detected");
    }

    delay(1000);

    reg_state = shiftBit(22, 14, 23, reg_state, 8);


    // delay(1000);
    // digitalWrite(22, LOW);
    // shiftOut(14, 23, LSBFIRST, 0x0F);
    // digitalWrite(22, HIGH);


    // delay(1000);
    // digitalWrite(22, LOW);
    shiftOut(14, 23, LSBFIRST, 0xF0);
    // digitalWrite(22, HIGH);

    // digitalWrite(22, LOW);
    // digitalWrite(14, HIGH);
    // digitalWrite(23, HIGH);
    // digitalWrite(23, LOW);
    // digitalWrite(22, HIGH);
    
    // delay(1000);

    // digitalWrite(22, LOW);
    // digitalWrite(14, LOW);
    // digitalWrite(23, HIGH);
    // digitalWrite(23, LOW);
    // digitalWrite(22, HIGH);
    
    // delay(1000);
}