#include <SPI.h>

// Pins for address shift register
#define ADDR_DATA 11    // Handled by SPI
#define ADDR_CLK 13     // Handled by SPI
#define ADDR_STORE 9

// Pins for DRAM chip
#define DRAM_DATA_WRITE 4 
#define DRAM_WRITE_MODE 5             // Active LOW
#define DRAM_CAS 6   // Active LOW
#define DRAM_RAS 7      // Active LOW
#define DRAM_DATA_READ 8

#define READ HIGH
#define WRITE LOW

#define PULSE_MICROS 1

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  pinMode(ADDR_DATA, OUTPUT);
  pinMode(ADDR_CLK, OUTPUT);
  pinMode(ADDR_STORE, OUTPUT);

  pinMode(DRAM_DATA_WRITE, OUTPUT);
  pinMode(DRAM_DATA_READ, INPUT);
  pinMode(DRAM_WRITE_MODE, OUTPUT);
  pinMode(DRAM_CAS, OUTPUT);
  pinMode(DRAM_RAS, OUTPUT);

  // Starting configuration
  digitalWrite(DRAM_WRITE_MODE, HIGH);
  digitalWrite(DRAM_RAS, HIGH);
  digitalWrite(DRAM_CAS, HIGH);

  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2); // Faster transmission
  SPI.setBitOrder(MSBFIRST);
  
  Serial.println("Ram tester initialized");
}

// Set a byte into the shift register
void setByte(uint8_t b) {
  // Shift out
//  digitalWrite(ADDR_CLK, LOW);
//  shiftOut(ADDR_DATA, ADDR_CLK, MSBFIRST, b);
  SPI.transfer(b);
    
  // Latch the byte
  digitalWrite(ADDR_STORE, LOW);
  delayMicroseconds(PULSE_MICROS);
  digitalWrite(ADDR_STORE, HIGH);
}

// Lock a 16-bit address into the DRAM chip
void latch16BitAddr(uint16_t address) {
  digitalWrite(DRAM_RAS, HIGH);
  digitalWrite(DRAM_CAS, HIGH);
  // Set row (lsb)
  setByte((byte)(address & 0xFF));
  digitalWrite(DRAM_RAS, LOW);
  // Set column (msb)
  setByte((byte)((address>>8) & 0xFF));
  digitalWrite(DRAM_CAS, LOW);
}

// No address, keep the chip in this state normally to reduce current
void endMemoryCycle() {
  digitalWrite(DRAM_RAS, HIGH);
  digitalWrite(DRAM_CAS, HIGH);
}

void refresh() {
  digitalWrite(DRAM_CAS, HIGH);
  for (uint8_t row = 0; row < 255; ++row) {
    // Select the row address
    setByte(row);
    digitalWrite(DRAM_RAS, HIGH);
    delayMicroseconds(PULSE_MICROS);
    digitalWrite(DRAM_RAS, LOW);
  }
}

// Write a bit, but return the previous (overwritten) state
// Negligibly slower than just a plain write
bool DRAMReadWriteBit(uint16_t address, bool dat) {
  digitalWrite(DRAM_WRITE_MODE, READ);
  latch16BitAddr(address);
  // Set data to write
  digitalWrite(DRAM_DATA_WRITE, dat);
  // Save current state
  bool prev = digitalRead(DRAM_DATA_READ);
  // Write (on falling edge)
  digitalWrite(DRAM_WRITE_MODE, WRITE);
  delayMicroseconds(PULSE_MICROS);
  digitalWrite(DRAM_WRITE_MODE, READ);
  // Return state before write
  return prev;
}

// Read a single bit
bool DRAMReadBit(uint16_t address) {
  digitalWrite(DRAM_WRITE_MODE, READ);
  return digitalRead(DRAM_DATA_READ);
}

bool bitAt(uint8_t row, uint8_t col, uint8_t mode) {
  if (mode == 0) 
    return 0;
  else if (mode == 1)
    return 1;
  else if (mode == 2)
    return ((col&1)^(row&1)) ? HIGH : LOW;
}

// Test the DRAM chip
// NOTE: Values only held for 4ms without refresh() apparently
void test() {
  // Init for process
  digitalWrite(DRAM_WRITE_MODE, READ);
  Serial.println("RAM test initialized");
  uint8_t successes = 0;

  // TODO: Make sure the RAM stays "alive"
  // Find a way to consistently write and then read when its all done
  // Also... figure out why reset fixes it (endMemoryCycle()?)
  for (uint8_t mode = 0; mode <= 2; ++mode) {
    bool ok = true;
    Serial.print("Running test ");
    Serial.print(mode);
    switch (mode) {
      case 0:
        Serial.println(" (Zeros)");
        break;
      case 1:
        Serial.println(" (Ones)");
        break;
      case 2:
        Serial.println(" (Checkerboard)");
        break;
    }
    
    uint32_t start_t = micros();
  
    for (uint8_t row = 0; row < 255; ++row) {
      setByte(row);
      digitalWrite(DRAM_RAS, LOW);
      for (uint8_t col = 0; col < 255; ++col) {
        // Write
        digitalWrite(DRAM_WRITE_MODE, LOW);
        digitalWrite(DRAM_DATA_WRITE, bitAt(row, col, mode));
        
        setByte(col);
        digitalWrite(DRAM_CAS, LOW);
  
        // Wait a little
        delayMicroseconds(PULSE_MICROS);
  
        digitalWrite(DRAM_CAS, HIGH);
        //digitalWrite(DRAM_WRITE_MODE, HIGH);
      }
      digitalWrite(DRAM_RAS, HIGH);
    }
    
    uint32_t end_t = micros();
  
    // Read
    digitalWrite(DRAM_WRITE_MODE, READ);
    for (uint8_t row = 0; row < 255; ++row) {
      setByte(row);
      digitalWrite(DRAM_RAS, LOW);
      for (uint8_t col = 0; col < 255; ++col) {
        setByte(col);
        digitalWrite(DRAM_CAS, LOW);
  
        // Read
        bool state = digitalRead(DRAM_DATA_READ)==HIGH;
        digitalWrite(DRAM_CAS, HIGH);
        ok &= (state == bitAt(row, col, mode));
  
        digitalWrite(DRAM_CAS, HIGH);
      }
      digitalWrite(DRAM_RAS, HIGH);
    }
  
    Serial.print("Took ");
    Serial.print(end_t - start_t);
    Serial.println(" microseconds to write 65536 bits.");

    Serial.println("Wrote to DRAM and checked.");
    if (ok) {
      Serial.println("Read/Write OK.");
      ++successes;
    } else {
      Serial.println("Read/Write FAIL.");
    }
  }

  Serial.print(successes);
  Serial.println("/3 tests completed successfully");
  Serial.println("\n------------------------------------\n");
  Serial.print("Conclusion: DRAM is ");
  Serial.println((successes == 3) ? "GOOD" : (successes == 2 ? "QUESTIONABLE, please test again" : "BAD"));

  endMemoryCycle();
}

void loop() {
  Serial.println("Type any key to run test...");
  while (!Serial.available()) {}
  while(Serial.available()) Serial.read();
  refresh(); // "Prepare" chip for testing
  test();
}
