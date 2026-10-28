#include "Pokitto.h"
#include "USBSerial.h"

#include "Shell.h"

using Core = Pokitto::Core;
using Display = Pokitto::Display;

bool serial_changed = false;

void on_serial_changed(int baud, int bits, int parity, int stop) {
    serial_changed = true;
}

int main() {
    Core::begin();
    
    // Print info for user before calling USBSerial constructor, which blocks until connection is established
    Core::update();
    Display::setCursor(0, 0);
    Display::write("\n\nConnecting USB...");
    Core::update();
    
    USBSerial serial;
    serial.attach(on_serial_changed);
    
    Shell* shell = nullptr;
    
    static constexpr uint8_t dots[] = "...";
    uint8_t dot_count = 3;
    
    uint32_t next = Core::getTime();
    while(Core::isRunning()) {
        if(!Core::update()) continue;
        
        if(serial_changed) {
            if(!shell) {
                // Drain any pending garbage data
                while(serial.available() > 0) serial._getc();
                
                shell = new Shell(&serial);
                serial.attach(shell, &Shell::process);
            }
            
            shell->reset();
            serial_changed = false;
        }
        
        const uint32_t now = Core::getTime();
        if(now >= next) {
            next += 500;
            dot_count = dot_count < 3 ? ++dot_count : 0;
        }
        
        Display::setCursor(0, 0);
        Display::write("Minimal Python-like REPL for Pokitto\n\n");
        Display::write(shell ? "Running" : "Waiting");
        Display::write(dots, dot_count);
    }
    
    return 0;
}
