#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace Nitra {

class BackTrace {
public:
    BackTrace();
    ~BackTrace();

    const std::vector<std::string>& GetFrames() const;
    friend std::ostream& operator<<(std::ostream&, const BackTrace&);

private:
    mutable std::shared_ptr<void> _context;
    mutable std::vector<std::string> _frames;
};

} // namespace Nitra

