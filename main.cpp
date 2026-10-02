#include "Pokitto.h"
#include "USBSerial.h"

#include "Shell.h"

using Core = Pokitto::Core;
using Display = Pokitto::Display;

int main() {
    static constexpr char title[] = "Minimal Python-like REPL for Pokitto";
    
    Core::begin();
    
    // Print info for user before calling USBSerial constructor, which blocks until connection is established
    Core::update();
    Display::setCursor(0, 0);
    Display::write(title);
    Display::write("\n\nConnecting USB...");
    Core::update();

    USBSerial serial;
    
    static constexpr uint8_t dots[] = ".....";
    uint8_t dot_count = 5;
    
    uint32_t next = Core::getTime();
    while(Core::isRunning()) {
        if(!Core::update()) continue;
        
        const uint32_t now = Core::getTime();
        if(now >= next) {
            next += 200;
            dot_count = dot_count < 5 ? ++dot_count : 1;
            
            // Send probe to see if host is consuming data
            uint8_t probe[] = {'\r'};
            if(serial.writeBlock(probe, 1)) break;
        }
        
        Display::setCursor(0, 0);
        Display::write(title);
        Display::write("\n\nConnecting USB");
        Display::write(dots, dot_count);
    }
    
    auto shell = new Shell(&serial);
    
    dot_count = 5;
    
    next = Core::getTime();
    while(Core::isRunning()) {
        if(!Core::update()) continue;
        
        const uint32_t now = Core::getTime();
        if(now >= next) {
            next += 200;
            dot_count = dot_count < 5 ? ++dot_count : 1;
        }
        
        Display::setCursor(0, 0);
        Display::write(title);
        Display::write("\n\nRunning");
        Display::write(dots, dot_count);
    }

    return 0;
}
