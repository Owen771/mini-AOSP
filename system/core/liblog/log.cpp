#include "log.h"
#include <iostream>
#include <string>

namespace miniaosp {

void log(const std::string& tag, const std::string& message) {
    // Pad tag to 16 chars for aligned output
    std::string padded_tag = tag;
    if (padded_tag.size() < 16) {
        padded_tag.resize(16, ' ');
    }
    std::cout << "[" << padded_tag << "] " << message << std::endl;
}

} // namespace miniaosp
