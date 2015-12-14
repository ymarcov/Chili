#include "Protocol.h"

namespace Yam {
namespace Http {

void CookieOptions::SetDomain(const std::string& domain) {
    _domain = std::make_pair(true, domain);
}

void CookieOptions::SetPath(const std::string& path) {
    _path = std::make_pair(true, path);
}

void CookieOptions::SetMaxAge(std::chrono::seconds maxAge) {
    _maxAge = std::make_pair(true, maxAge);
}

bool CookieOptions::GetDomain(std::string* out) const {
    if (out && _domain.first)
        *out = _domain.second;
    return _domain.first;
}

bool CookieOptions::GetPath(std::string* out) const {
    if (out && _path.first)
        *out = _path.second;
    return _path.first;
}

bool CookieOptions::GetMaxAge(std::chrono::seconds* out) const {
    if (out && _maxAge.first)
        *out = _maxAge.second;
    return _maxAge.first;
}

} // namespace Http
} // namespace Yam

