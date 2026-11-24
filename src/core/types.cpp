#include "chronogit/core/types.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace chronogit {

Signature Signature::now(std::string_view name, std::string_view email) {
    Signature sig;
    sig.name = std::string(name);
    sig.email = std::string(email);
    auto tp = std::chrono::system_clock::now();
    sig.timestamp = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();

    std::time_t t = static_cast<std::time_t>(sig.timestamp);
    std::tm local_tm{};
#if defined(_WIN32)
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif
    std::tm gm_tm{};
#if defined(_WIN32)
    gmtime_s(&gm_tm, &t);
#else
    gmtime_r(&t, &gm_tm);
#endif
    long offset_sec = static_cast<long>(std::difftime(std::mktime(&local_tm), std::mktime(&gm_tm)));
    char sign = offset_sec >= 0 ? '+' : '-';
    if (offset_sec < 0) offset_sec = -offset_sec;
    long hours = offset_sec / 3600;
    long minutes = (offset_sec % 3600) / 60;
    char tz_buf[8];
    std::snprintf(tz_buf, sizeof(tz_buf), "%c%02ld%02ld", sign, hours, minutes);
    sig.tz_offset = tz_buf;
    return sig;
}

std::string Signature::to_string() const {
    std::ostringstream oss;
    oss << name << " <" << email << "> " << timestamp << " " << tz_offset;
    return oss.str();
}

Signature Signature::parse(std::string_view line) {
    Signature sig;
    auto email_start = line.find('<');
    auto email_end = line.find('>', email_start);
    if (email_start != std::string_view::npos && email_end != std::string_view::npos) {
        sig.name = std::string(line.substr(0, email_start));
        while (!sig.name.empty() && sig.name.back() == ' ') sig.name.pop_back();
        sig.email = std::string(line.substr(email_start + 1, email_end - email_start - 1));

        std::string rest(line.substr(email_end + 1));
        std::istringstream iss(rest);
        iss >> sig.timestamp >> sig.tz_offset;
    } else {
        sig.name = std::string(line);
    }
    return sig;
}

} // namespace chronogit
