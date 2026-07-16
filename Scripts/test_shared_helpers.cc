#include <Shared/AccountValidation.hh>
#include <Shared/StackFormat.hh>
#include <cassert>
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
    assert(format_stack_count(1500) == "x1.5k");
    assert(format_stack_count(1000000) == "x1m");
    assert(format_stack_count(1200000) == "x1.2m");
    std::cout << "ok\n";
    return 0;
}
