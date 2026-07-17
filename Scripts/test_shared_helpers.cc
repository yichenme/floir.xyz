#include <Shared/AccountValidation.hh>
#include <Shared/StackFormat.hh>
#include <Shared/RarityScale.hh>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

int main() {
    assert(account_username_valid("alice"));
    assert(account_username_valid("A_1"));
    assert(!account_username_valid("ab"));          // too short
    assert(!account_username_valid("has-dash"));
    assert(!account_username_valid("has space"));
    assert(account_password_valid("abcd"));
    assert(!account_password_valid("abc"));         // too short
    assert(!account_password_valid("bad!pass"));

    assert(format_stack_count(1) == "");
    assert(format_stack_count(99) == "x99");
    assert(format_stack_count(1000) == "x1k");
    assert(format_stack_count(1001) == "x1k");
    assert(format_stack_count(1500) == "x1.5k");
    assert(format_stack_count(1000000) == "x1m");
    assert(format_stack_count(1000001) == "x1m");
    assert(format_stack_count(1200000) == "x1.2m");

    assert(std::fabs(mob_size_mult(0) - 1.0f) < 1e-4f);       // Common: no change
    assert(std::fabs(mob_size_mult(1) - 1.2f) < 1e-4f);       // 1.2x per tier
    assert(std::fabs(mob_size_mult(4) - 2.0736f) < 1e-3f);    // 1.2^4
    assert(std::fabs(mob_size_mult(8) - 4.29981696f) < 1e-3f); // 1.2^8 (Unique)
    assert(mob_size_mult(4) > mob_size_mult(0));               // strictly increasing
    assert(mob_size_mult(8) > mob_size_mult(4));

    std::cout << "ok\n";
    return 0;
}
