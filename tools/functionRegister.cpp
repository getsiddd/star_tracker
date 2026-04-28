#include <iostream>
#include <map>
#include <functional>
#include <any>

// Function Registry Class
class FunctionRegistry {
public:
    using GenericFunc = std::function<std::any()>;  // Stores functions that return values

    // Function storage
    static std::map<std::string, GenericFunc>& getRegistry() {
        static std::map<std::string, GenericFunc> registry;
        return registry;
    }

    // Constructor to register functions
    FunctionRegistry(const std::string& name, GenericFunc func) {
        getRegistry()[name] = func;
    }

    // Function to run all registered functions
    static void runAll() {
        std::cout << "Running all registered functions...\n";
        for (const auto& [name, func] : getRegistry()) {
            std::cout << "Executing: " << name << std::endl;
            std::any result = func();  // Call function and get return value
            printResult(name, result);
        }
    }

    // Function to set new arguments and rebind a function
    template <typename Func, typename... Args>
    static void setArguments(const std::string& name, Func func, Args... args) {
        auto& registry = getRegistry();
        if (registry.find(name) != registry.end()) {
            registry[name] = std::bind(func, args...);  // Rebind function with new arguments
        } else {
            std::cerr << "Function '" << name << "' not found in registry!" << std::endl;
        }
    }

    // Function to call a specific registered function and return its result
    static std::any callFunction(const std::string& name) {
        auto& registry = getRegistry();
        if (registry.find(name) != registry.end()) {
            return registry[name]();  // Execute function and return result
        } else {
            std::cerr << "Function '" << name << "' not found!" << std::endl;
            return {};
        }
    }

    // Utility function to print results
    static void printResult(const std::string& name, const std::any& result) {
        if (result.type() == typeid(std::string)) {
            std::cout << name << " result: " << std::any_cast<std::string>(result) << std::endl;
        } else if (result.type() == typeid(int)) {
            std::cout << name << " result: " << std::any_cast<int>(result) << std::endl;
        } else {
            std::cout << name << " result: [Unknown Type]" << std::endl;
        }
    }
};

// Macro to register functions with default arguments
#define REGISTER_FUNCTION(name, retType, defaultArgs, ...) \
    retType name(__VA_ARGS__); \
    static FunctionRegistry _##name##_register(#name, []() -> std::any { return name defaultArgs; }); \
    retType name(__VA_ARGS__)

// Custom Data Type
struct Point {
    double x, y;
    Point(double _x, double _y) : x(_x), y(_y) {}
    std::string toString() const { return "Point(" + std::to_string(x) + ", " + std::to_string(y) + ")"; }
};

// Example Function Using Custom Type
REGISTER_FUNCTION(getPoint, std::string, (3.5, 7.2), double a, double b) {
    Point p(a, b);
    return p.toString();
}

// Example Function With Arguments
REGISTER_FUNCTION(addValues, int, (5, 10, 15), int a, int b, int c) {
    return a + b + c;
}

int main() {
    // Run all registered functions initially
    FunctionRegistry::runAll();

    // Accessing registry
    auto& registry = FunctionRegistry::getRegistry();

    // Call registered functions and capture return values
    if (registry.find("getPoint") != registry.end()) {
        std::any result = FunctionRegistry::callFunction("getPoint");
        FunctionRegistry::printResult("getPoint", result);
    }

    if (registry.find("addValues") != registry.end()) {
        std::any result = FunctionRegistry::callFunction("addValues");
        FunctionRegistry::printResult("addValues", result);
    }

    // ✅ Dynamically update arguments and call functions in a loop
    std::cout << "\nUpdating arguments dynamically in loop...\n";
    for (int i = 1; i <= 3; ++i) {
        std::cout << "\nIteration " << i << ":\n";

        // Change getPoint arguments dynamically
        FunctionRegistry::setArguments("getPoint", getPoint, i * 1.1, i * 2.2);
        std::any result1 = FunctionRegistry::callFunction("getPoint");
        FunctionRegistry::printResult("getPoint", result1);

        // Change addValues arguments dynamically
        FunctionRegistry::setArguments("addValues", addValues, i * 2, i * 3, i * 4);
        std::any result2 = FunctionRegistry::callFunction("addValues");
        FunctionRegistry::printResult("addValues", result2);
    }

    return 0;
}