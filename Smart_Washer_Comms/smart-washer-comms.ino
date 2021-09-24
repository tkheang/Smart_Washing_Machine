#include <blynk.h>

#define BLYNK_PRINT Serial

// Blynk auth token
char auth[] = "PfR7tsZxSdyOOQowdlJyoT7CtxNJP5TN";

BlynkTimer arduinoDataTimer;

// Push notification variables
bool notifyReady = false;
bool notifyWashing = false;
bool notifyPaused = false;
bool notifyFinished = false;

// For debugging in serial monitor
#define DEBUG false

void setup()
{
    if (DEBUG) {
        Serial.begin(115200);
    }
    
    Serial1.begin(115200);
    
    delay(5000); // Allow board to settle
    Blynk.begin(auth);
    
    // Set up timers
    arduinoDataTimer.setInterval(1000L, arduinoDataTimerEvent);

    pinMode(D7, OUTPUT); // Button signal and status LED
    
    // Set up time for TOU feature
    Time.zone(-7);
}

// Main program loop
void loop()
{
    Blynk.run();
    arduinoDataTimer.run();
}

// Read and parse data every tick of arduinoDataTimer
void arduinoDataTimerEvent() {
    char * strtokIndx; // This is used by strtok() as an index
    const size_t READ_BUF_SIZE = 64;
    char readBuf[READ_BUF_SIZE];
    size_t readBufOffset = 0;
    
    int state;
    float capacity;
    float lidAngle;
    float vibration;
    
    // Read data from Serial1 port
    while(Serial1.available()) {
        if (readBufOffset < READ_BUF_SIZE) {
            char c = Serial1.read();
            if (c != '\n') {
                // Add character to buffer
                readBuf[readBufOffset++] = c;
            }
            else {
                // End of line character found, process line
                readBuf[readBufOffset] = 0;
                readBufOffset = 0;
            }
        }
        else {
            Serial.println("readBuf overflow, emptying buffer");
            readBufOffset = 0;
        }
    }
    
    // Parse data
    strtokIndx = strtok(readBuf,",");
    state = atoi(strtokIndx);
    
    strtokIndx = strtok(NULL, ",");
    capacity = atof(strtokIndx);
    
    strtokIndx = strtok(NULL, ","); 
    lidAngle = atof(strtokIndx);
    
    strtokIndx = strtok(NULL, ","); 
    vibration = atof(strtokIndx);
    
    // Display state in app
    switch (state) {
        case 0: {
            Blynk.virtualWrite(V0, "Status: Ready");
            notifyWashing = false;
            notifyPaused = false;
            notifyFinished = false;
            if (notifyReady == false) {
                notifyReady = true;
                Blynk.notify("Washing machine is ready.");
            }
            break;
        }
        case 1: {
            Blynk.virtualWrite(V0, "Status: Washing");
            notifyReady = false;
            notifyPaused = false;
            notifyFinished = false;
            if (notifyWashing == false) {
                notifyWashing = true;
                Blynk.notify("Washing machine has started.");
            }
            break;
        }
        case 2: {
            Blynk.virtualWrite(V0, "Status: Paused");
            notifyReady = false;
            notifyWashing = false;
            notifyFinished = false;
            if (notifyPaused == false) {
                notifyPaused = true;
                Blynk.notify("Washing machine has paused.");
            }
            break;
        }
        case 3: {
            Blynk.virtualWrite(V0, "Status: Finished");
            notifyReady = false;
            notifyWashing = false;
            notifyPaused = false;
            if (notifyFinished == false) {
                notifyFinished = true;
                Blynk.notify("Washing machine has finished.");
            }
            break;
        }
        default: {
            Blynk.virtualWrite(V0, "Status: Ready");
            break;
        }
    }
    
    // Display rest of data
    Blynk.virtualWrite(V1, capacity);
    Blynk.virtualWrite(V2, lidAngle);
    Blynk.virtualWrite(V3, vibration);
    
    // Detergent feature
    if (capacity >= 40.5) {
        Blynk.virtualWrite(V4, "Recommended Detergent Amount: 1 Tide Pod");
    }
    else if (capacity < 40.5 && capacity >= 27) {
        Blynk.virtualWrite(V4, "Recommended Detergent Amount: 2 Tide Pods");
    }
    else if (capacity < 27) {
        Blynk.virtualWrite(V4, "Recommended Detergent Amount: 3 Tide Pods");
    }
    
    // TOU feature
    if (Time.hourFormat12() >= 5 && Time.hourFormat12() <= 8) {
        Blynk.virtualWrite(V5, "Electricity Cost: HIGH ($0.54/kWh)");
    }
    else {
        Blynk.virtualWrite(V5, "Electricity Cost: NORMAL ($0.27/kWh)");
    }
    
    if (DEBUG) {
        Serial.print(state); Serial.print(',');
        Serial.print(capacity); Serial.print(',');
        Serial.print(lidAngle); Serial.print(',');
        Serial.println(vibration);
    }
}