#include <Shared/AccountValidation.hh>

static bool charset_valid(std::string_view s) {
    for (char c : s) {
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

bool account_username_valid(std::string_view username) {
    return username.size() >= 3 && username.size() <= 16 && charset_valid(username);
}

bool account_password_valid(std::string_view password) {
    return password.size() >= 4 && password.size() <= 32 && charset_valid(password);
}
