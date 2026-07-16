#include <Shared/StackFormat.hh>

#include <string>

static std::string format_compact(uint64_t count, uint64_t divisor, char suffix) {
    uint64_t const whole = count / divisor;
    uint64_t const rem = count % divisor;
    if (rem == 0) {
        return "x" + std::to_string(whole) + suffix;
    }
    uint64_t tenths = (rem * 10 + divisor / 2) / divisor;
    if (tenths >= 10) {
        return "x" + std::to_string(whole + 1) + suffix;
    }
    return "x" + std::to_string(whole) + "." + std::to_string(tenths) + suffix;
}

std::string format_stack_count(uint64_t count) {
    if (count <= 1) {
        return "";
    }
    if (count < 1000) {
        return "x" + std::to_string(count);
    }
    if (count < 1000000) {
        return format_compact(count, 1000, 'k');
    }
    return format_compact(count, 1000000, 'm');
}
