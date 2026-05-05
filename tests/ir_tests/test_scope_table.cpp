#include <iostream>
#include <string>
#include <cassert>
#include <stdexcept>
#include "../ir_generator/scope_table.hpp"

void testBasicEnvironment() {
    ScopeTable env;

    env.defineVariable("x", "%x_1");
    env.defineVariable("y", "%y_1");

    assert(env.resolveVariable("x") == "%x_1");
    assert(env.resolveVariable("y") == "%y_1");

    std::cout << "testBasicEnvironment passed.\n";
}

void testNestedEnvironmentResolvesParent() {
    ScopeTable env;
    env.defineVariable("globalVar", "%global_1");

    env.enterScope();
    env.defineVariable("localVar", "%local_1");

    // Can resolve its own variables
    assert(env.resolveVariable("localVar") == "%local_1");
    // Can resolve parent variables
    assert(env.resolveVariable("globalVar") == "%global_1");

    env.exitScope();

    std::cout << "testNestedEnvironmentResolvesParent passed.\n";
}

void testNestedEnvironmentShadowsParent() {
    ScopeTable env;
    env.defineVariable("x", "%x_parent");

    env.enterScope();
    env.defineVariable("x", "%x_child"); // Shadows parent 'x'

    // Child should see its own 'x'
    assert(env.resolveVariable("x") == "%x_child");
    
    env.exitScope();
    
    // Parent still sees its own 'x'
    assert(env.resolveVariable("x") == "%x_parent");

    std::cout << "testNestedEnvironmentShadowsParent passed.\n";
}

void testVariableNotFoundThrows() {
    ScopeTable env;
    bool threw = false;

    try {
        env.resolveVariable("nonExistentVar");
    } catch (const std::runtime_error& e) {
        threw = true;
    }

    assert(threw && "Expected a runtime_error to be thrown for an undefined variable.");

    std::cout << "testVariableNotFoundThrows passed.\n";
}

int main() {
    std::cout << "Running ScopeTable Tests...\n";

    testBasicEnvironment();
    testNestedEnvironmentResolvesParent();
    testNestedEnvironmentShadowsParent();
    testVariableNotFoundThrows();

    std::cout << "All ScopeTable tests passed successfully!\n";
    return 0;
}
