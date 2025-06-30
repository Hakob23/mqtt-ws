#include "IntegrationTestController.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <cstring>

// Command line argument parsing
struct CommandLineArgs {
    std::string suite_name;
    std::string test_name;
    bool verbose = false;
    bool health_check = false;
    std::string config_file;
    std::string output_file;
    std::chrono::seconds duration{0};
    int num_sensors = 0;
    int num_gateways = 0;
    double rate_multiplier = 0.0;
    bool show_help = false;
    std::vector<std::string> remaining_args;
};

void PrintUsage(const char* program_name) {
    std::cout << "MQTT-WebSocket IoT System Integration Test Suite\n";
    std::cout << "Usage: " << program_name << " [options]\n\n";
    
    std::cout << "Test Selection:\n";
    std::cout << "  --suite <name>       Run specific test suite (Basic, Performance, Reliability, Complete)\n";
    std::cout << "  --test <name>        Run specific test case\n";
    std::cout << "  --list-tests         List all available tests and suites\n";
    std::cout << "  --list-suites        List all available test suites\n\n";
    
    std::cout << "Configuration:\n";
    std::cout << "  --config <file>      Load configuration from file\n";
    std::cout << "  --duration <time>    Override test duration (e.g., 30s, 2m)\n";
    std::cout << "  --sensors <num>      Number of STM32 sensors to simulate\n";
    std::cout << "  --gateways <num>     Number of RPi4 gateways to use\n";
    std::cout << "  --rate <multiplier>  Message rate multiplier for stress tests\n\n";
    
    std::cout << "Output and Logging:\n";
    std::cout << "  --verbose, -v        Enable verbose output\n";
    std::cout << "  --output <file>      Save results to JSON file\n";
    std::cout << "  --quiet, -q          Suppress non-essential output\n\n";
    
    std::cout << "Utilities:\n";
    std::cout << "  --health-check       Check system component health\n";
    std::cout << "  --setup-env          Setup test environment\n";
    std::cout << "  --clean-logs         Clean previous test logs\n";
    std::cout << "  --help, -h           Show this help message\n\n";
    
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " --suite Basic --verbose\n";
    std::cout << "  " << program_name << " --test EndToEndDataFlow --duration 60s\n";
    std::cout << "  " << program_name << " --suite Performance --sensors 50 --output results.json\n";
    std::cout << "  " << program_name << " --test StressLoad --rate 5x --duration 2m\n";
    std::cout << "  " << program_name << " --health-check\n\n";
    
    std::cout << "Available Test Suites:\n";
    std::cout << "  Basic        - EndToEndDataFlow, ThermalIntegration\n";
    std::cout << "  Performance  - PerformanceBenchmark, StressLoad, MultiGatewayScaling\n";
    std::cout << "  Reliability  - FaultTolerance, MQTTBridgeReliability, RecoveryScenarios\n";
    std::cout << "  Complete     - All tests in sequence\n\n";
    
    std::cout << "Available Test Cases:\n";
    std::cout << "  EndToEndDataFlow      - Test complete data pipeline\n";
    std::cout << "  PerformanceBenchmark  - Measure system performance metrics\n";
    std::cout << "  StressLoad           - High-load stress testing\n";
    std::cout << "  FaultTolerance       - Fault injection and recovery\n";
    std::cout << "  ThermalIntegration   - Thermal monitoring system tests\n";
    std::cout << "  MQTTBridgeReliability - MQTT-WebSocket bridge reliability\n";
    std::cout << "  MultiGatewayScaling  - Multi-gateway scaling tests\n";
    std::cout << "  RecoveryScenarios    - System recovery testing\n";
}

std::chrono::seconds ParseDuration(const std::string& duration_str) {
    if (duration_str.empty()) return std::chrono::seconds(0);
    
    size_t pos = 0;
    int value = std::stoi(duration_str, &pos);
    
    if (pos >= duration_str.length()) {
        return std::chrono::seconds(value); // Default to seconds
    }
    
    char unit = duration_str[pos];
    switch (unit) {
        case 's': case 'S':
            return std::chrono::seconds(value);
        case 'm': case 'M':
            return std::chrono::seconds(value * 60);
        case 'h': case 'H':
            return std::chrono::seconds(value * 3600);
        default:
            std::cerr << "Invalid duration unit: " << unit << " (use s, m, or h)" << std::endl;
            return std::chrono::seconds(value);
    }
}

double ParseRateMultiplier(const std::string& rate_str) {
    if (rate_str.empty()) return 1.0;
    
    std::string str = rate_str;
    if (str.back() == 'x' || str.back() == 'X') {
        str.pop_back();
    }
    
    try {
        return std::stod(str);
    } catch (const std::exception&) {
        std::cerr << "Invalid rate multiplier: " << rate_str << std::endl;
        return 1.0;
    }
}

CommandLineArgs ParseCommandLine(int argc, char* argv[]) {
    CommandLineArgs args;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            args.show_help = true;
        } else if (arg == "--verbose" || arg == "-v") {
            args.verbose = true;
        } else if (arg == "--quiet" || arg == "-q") {
            args.verbose = false;
        } else if (arg == "--health-check") {
            args.health_check = true;
        } else if (arg == "--suite" && i + 1 < argc) {
            args.suite_name = argv[++i];
        } else if (arg == "--test" && i + 1 < argc) {
            args.test_name = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            args.config_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            args.output_file = argv[++i];
        } else if (arg == "--duration" && i + 1 < argc) {
            args.duration = ParseDuration(argv[++i]);
        } else if (arg == "--sensors" && i + 1 < argc) {
            args.num_sensors = std::stoi(argv[++i]);
        } else if (arg == "--gateways" && i + 1 < argc) {
            args.num_gateways = std::stoi(argv[++i]);
        } else if (arg == "--rate" && i + 1 < argc) {
            args.rate_multiplier = ParseRateMultiplier(argv[++i]);
        } else if (arg == "--list-tests") {
            // This will be handled in main
        } else if (arg == "--list-suites") {
            // This will be handled in main
        } else if (arg == "--setup-env") {
            // This will be handled in main
        } else if (arg == "--clean-logs") {
            // This will be handled in main
        } else {
            args.remaining_args.push_back(arg);
        }
    }
    
    return args;
}

void ListAvailableTests() {
    std::cout << "Available Test Cases:\n";
    std::cout << "  EndToEndDataFlow      - Test complete data flow from sensors to monitoring\n";
    std::cout << "  PerformanceBenchmark  - Measure latency, throughput, and resource usage\n";
    std::cout << "  StressLoad           - Test system under high sensor loads\n";
    std::cout << "  FaultTolerance       - Test recovery from failures and errors\n";
    std::cout << "  ThermalIntegration   - Test thermal monitoring integration\n";
    std::cout << "  MQTTBridgeReliability - Test MQTT-WebSocket bridge reliability\n";
    std::cout << "  MultiGatewayScaling  - Test scaling with multiple gateways\n";
    std::cout << "  RecoveryScenarios    - Test system recovery from various failures\n";
}

void ListAvailableTestSuites() {
    std::cout << "Available Test Suites:\n";
    std::cout << "  Basic        - Basic integration tests (EndToEndDataFlow, ThermalIntegration)\n";
    std::cout << "  Performance  - Performance testing (PerformanceBenchmark, StressLoad, MultiGatewayScaling)\n";
    std::cout << "  Reliability  - Reliability testing (FaultTolerance, MQTTBridgeReliability, RecoveryScenarios)\n";
    std::cout << "  Complete     - All available tests in recommended order\n";
}

void SetupTestEnvironment() {
    std::cout << "Setting up test environment...\n";
    
    // Create necessary directories
    system("mkdir -p logs test_data results");
    
    // Check system requirements
    std::cout << "Checking system requirements...\n";
    
    if (system("which mosquitto > /dev/null 2>&1") == 0) {
        std::cout << "✓ mosquitto broker found\n";
    } else {
        std::cout << "⚠ mosquitto broker not found - some tests may fail\n";
    }
    
    if (system("which g++ > /dev/null 2>&1") == 0) {
        std::cout << "✓ g++ compiler found\n";
    } else {
        std::cout << "✗ g++ compiler not found\n";
    }
    
    std::cout << "Test environment setup completed.\n";
}

void CleanLogs() {
    std::cout << "Cleaning previous test logs...\n";
    system("rm -f logs/*.log test_data/*.json results/*.json");
    std::cout << "Log cleanup completed.\n";
}

bool PerformHealthCheck() {
    std::cout << "=== System Component Health Check ===\n";
    
    bool all_healthy = true;
    
    // Check if ports are available
    std::vector<int> required_ports = {1883, 8080, 9001};
    for (int port : required_ports) {
        std::string cmd = "netstat -ln | grep :" + std::to_string(port) + " > /dev/null 2>&1";
        if (system(cmd.c_str()) == 0) {
            std::cout << "⚠ Port " << port << " is already in use\n";
        } else {
            std::cout << "✓ Port " << port << " is available\n";
        }
    }
    
    // Check file system permissions
    if (system("touch logs/health_check.tmp > /dev/null 2>&1") == 0) {
        std::cout << "✓ Log directory is writable\n";
        system("rm -f logs/health_check.tmp");
    } else {
        std::cout << "✗ Log directory is not writable\n";
        all_healthy = false;
    }
    
    // Check available memory
    std::cout << "System memory status:\n";
    system("free -h | head -2");
    
    // Check CPU cores
    std::cout << "CPU information:\n";
    system("nproc");
    
    std::cout << "\nHealth check " << (all_healthy ? "PASSED" : "FAILED") << "\n";
    return all_healthy;
}

int main(int argc, char* argv[]) {
    std::cout << "MQTT-WebSocket IoT System Integration Test Suite\n";
    std::cout << "Phase 2, Step 3: Comprehensive Integration Testing\n";
    std::cout << "========================================\n\n";
    
    CommandLineArgs args = ParseCommandLine(argc, argv);
    
    // Handle special commands
    if (args.show_help) {
        PrintUsage(argv[0]);
        return 0;
    }
    
    // Check for special utility commands
    for (const auto& arg : args.remaining_args) {
        if (arg == "--list-tests") {
            ListAvailableTests();
            return 0;
        } else if (arg == "--list-suites") {
            ListAvailableTestSuites();
            return 0;
        } else if (arg == "--setup-env") {
            SetupTestEnvironment();
            return 0;
        } else if (arg == "--clean-logs") {
            CleanLogs();
            return 0;
        }
    }
    
    if (args.health_check) {
        return PerformHealthCheck() ? 0 : 1;
    }
    
    // Create and configure test controller
    IntegrationTestController controller;
    
    // Configure test parameters if specified
    if (!args.config_file.empty()) {
        try {
            controller.LoadConfigurationFromFile(args.config_file);
            if (args.verbose) {
                std::cout << "Loaded configuration from: " << args.config_file << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to load configuration file: " << e.what() << "\n";
            return 1;
        }
    }
    
    // Override configuration with command line parameters
    TestConfiguration config;
    if (args.duration.count() > 0) {
        config.test_duration = args.duration;
    }
    if (args.num_sensors > 0) {
        config.num_stm32_sensors = args.num_sensors;
    }
    if (args.num_gateways > 0) {
        config.num_rpi4_gateways = args.num_gateways;
    }
    if (args.rate_multiplier > 0.0) {
        config.message_rate_multiplier = args.rate_multiplier;
    }
    
    controller.SetGlobalConfiguration(config);
    
    if (args.verbose) {
        std::cout << "Test Configuration:\n";
        std::cout << "  STM32 Sensors: " << config.num_stm32_sensors << "\n";
        std::cout << "  RPi4 Gateways: " << config.num_rpi4_gateways << "\n";
        std::cout << "  Test Duration: " << config.test_duration.count() << "s\n";
        std::cout << "  Message Rate Multiplier: " << config.message_rate_multiplier << "x\n";
        std::cout << "\n";
    }
    
    // Execute tests
    bool test_success = false;
    
    try {
        if (!args.test_name.empty()) {
            // Run single test
            std::cout << "Running single test: " << args.test_name << "\n\n";
            test_success = controller.RunSingleTest(args.test_name);
        } else if (!args.suite_name.empty()) {
            // Run test suite
            std::cout << "Running test suite: " << args.suite_name << "\n\n";
            test_success = controller.RunTestSuite(args.suite_name);
        } else {
            // Run all tests
            std::cout << "Running complete integration test suite\n\n";
            test_success = controller.RunAllTests();
        }
    } catch (const std::exception& e) {
        std::cerr << "Test execution failed with exception: " << e.what() << "\n";
        return 1;
    }
    
    // Save results if requested
    if (!args.output_file.empty()) {
        try {
            controller.SaveTestResults(args.output_file);
            if (args.verbose) {
                std::cout << "\nResults saved to: " << args.output_file << "\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to save results: " << e.what() << "\n";
        }
    }
    
    // Print final status
    std::cout << "\n========================================\n";
    if (test_success) {
        std::cout << "✓ Integration tests PASSED\n";
    } else {
        std::cout << "✗ Integration tests FAILED\n";
    }
    
    // Print aggregated metrics
    if (args.verbose) {
        std::cout << "\nAggregated Test Metrics:\n";
        TestMetrics aggregated = controller.GetAggregatedMetrics();
        std::cout << "  Total messages processed: " << aggregated.messages_sent << "\n";
        std::cout << "  Average latency: " << std::fixed << std::setprecision(2) 
                  << aggregated.avg_latency_ms << "ms\n";
        std::cout << "  Peak memory usage: " << aggregated.memory_usage_kb << "KB\n";
        std::cout << "  Alerts generated: " << aggregated.alerts_generated << "\n";
    }
    
    return test_success ? 0 : 1;
} 