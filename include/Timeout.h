#pragma once

#include "Error.h"
#include <stdexcept>

namespace Chili {

class Timeout : public Error, public std::runtime_error {
public:
    Timeout() : runtime_error("Operation timed out") {}
};

} // namespace Chili

