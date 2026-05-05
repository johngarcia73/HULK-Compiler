#include "../common/string_builder.hpp"
#include <iostream>
#include <iomanip>

int main() {
    // Test 1: Complex JSON structure
    std::cout << "=== Test 1: Complex JSON Structure ===" << std::endl;
    StringBuilder sb1;
    std::string name = "John Doe";
    int age = 30;
    double salary = 75000.50;
    std::string department = "Engineering";
    
    sb1.addLine("{")
      .indent()
        .addLine("\"employee\": {{")
        .indent()
            .addLine("\"name\": \"{}\",", name)
            .addLine("\"age\": {},", age)
            .addLine("\"salary\": {},", salary)
            .addLine("\"department\": \"{}\"", department)
        .unindent()
        .addLine("}},")
        .addLine("\"active\": true")
      .unindent()
      .addLine("}");
    
    std::cout << sb1.toString() << std::endl;

    // Test 2: Nested arrays and objects
    std::cout << "\n=== Test 2: Nested Arrays ===" << std::endl;
    StringBuilder sb2;
    
    sb2.addLine("{")
      .indent()
        .addLine("\"users\": [")
        .indent()
            .addLine("{{")
            .indent()
                .addLine("\"id\": 1,")
                .addLine("\"name\": \"Alice\"")
            .unindent()
            .addLine("}},")
            .addLine("{{")
            .indent()
                .addLine("\"id\": 2,")
                .addLine("\"name\": \"Bob\"")
            .unindent()
            .addLine("}},")
            .addLine("{{")
            .indent()
                .addLine("\"id\": 3,")
                .addLine("\"name\": \"Charlie\"")
            .unindent()
            .addLine("}}")
        .unindent()
        .addLine("]")
      .unindent()
      .addLine("}");
    
    std::cout << sb2.toString() << std::endl;

    // Test 3: Code-like structure with multiple arguments
    std::cout << "\n=== Test 3: Code Structure ===" << std::endl;
    StringBuilder sb3;
    std::string className = "Calculator";
    std::string methodName = "add";
    int param1 = 10;
    int param2 = 20;
    int result = param1 + param2;
    
    sb3.addLine("class {} {{", className)
      .indent()
        .addLine("public:")
        .indent()
            .addLine("int {}(int a, int b) {{", methodName)
            .indent()
                .addLine("return a + b;")
            .unindent()
            .addLine("}}")
        .unindent()
      .unindent()
      .addLine("};")
      .addLine("")
      .addLine("// Example: {}({}, {}) = {}", methodName, param1, param2, result);
    
    std::cout << sb3.toString() << std::endl;

    // Test 4: Mixed content with deep nesting
    std::cout << "\n=== Test 4: Deep Nesting ===" << std::endl;
    StringBuilder sb4;
    
    sb4.addLine("{")
      .indent()
        .addLine("\"level1\": {{")
        .indent()
            .addLine("\"level2\": {{")
            .indent()
                .addLine("\"level3\": {{")
                .indent()
                    .addLine("\"level4\": {{")
                    .indent()
                        .addLine("\"value\": \"deep nested\"")
                    .unindent()
                    .addLine("}}")
                .unindent()
                .addLine("}}")
            .unindent()
            .addLine("}}")
        .unindent()
        .addLine("}}")
      .unindent()
      .addLine("}");
    
    std::cout << sb4.toString() << std::endl;

    // Test 5: Configuration file style
    std::cout << "\n=== Test 5: Configuration Style ===" << std::endl;
    StringBuilder sb5;
    std::string dbHost = "localhost";
    int dbPort = 5432;
    std::string dbName = "myapp";
    std::string dbUser = "admin";
    
    sb5.addLine("[database]")
      .addLine("host = {}", dbHost)
      .addLine("port = {}", dbPort)
      .addLine("name = {}", dbName)
      .addLine("user = {}", dbUser)
      .addLine("")
      .addLine("[features]")
      .addLine("caching = true")
      .addLine("logging = true");
    
    std::cout << sb5.toString() << std::endl;


    //Test 6
    StringBuilder sb6;
    std::string user = "Alice";
    int score = 1500;

    sb6.addLine("{")
      .indent()
        .addLine("\"user\": \"{}\",", user)
        .addLine("\"stats\": {{")  // Escape braces for format
        .indent()
            .addLine("\"score\": {},", score)
            .addLine("\"rank\": \"Master\"")
        .unindent()
        .addLine("}}")
      .unindent()
      .addLine("}");

    std::cout << sb6.toString() << std::endl;
    
    
    return 0;
}
