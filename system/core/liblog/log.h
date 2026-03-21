#pragma once
#include <string>

namespace miniaosp {

// Tagged timestamped logging: [tag] message
void log(const std::string& tag, const std::string& message);

} // namespace miniaosp
