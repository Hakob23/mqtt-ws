#include "IntegrationTestController.h"
#include "ComponentManagers.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>

// Constructor
IntegrationTestController::IntegrationTestController() {
    InitializeComponentManagers();
    InitializeDefaultTests();
}

// Destructor
IntegrationTestController::~IntegrationTestController() {
    StopAllComponents();
    StopRealTimeMonitoring();
}

// Initialize component managers
void IntegrationTestController::InitializeComponentManagers() {
    stm32_manager_ = std::make_unique<STM32SimulatorManager>();
    rpi4_manager_ = std::make_unique<RPi4GatewayManager>();
    bridge_manager_ = std::make_unique<MQTTBridgeManager>();
    thermal_manager_ = std::make_unique<ThermalMonitorManager>();
}

// Initialize default test cases
void IntegrationTestController::InitializeDefaultTests() {
    // End-to-End Data Flow Test
    TestCase e2e_test;
    e2e_test.name = "EndToEndDataFlow";
    e2e_test.description = "Test complete data flow from STM32 sensors through RPi4 gateway to MQTT bridge and thermal monitoring";
    e2e_test.test_function = [this](const TestConfiguration& config, TestMetrics& metrics) {
        return TestEndToEndDataFlow(config, metrics);
    };
    e2e_test.config.test_duration = std::chrono::seconds(30);
    e2e_test.config.num_stm32_sensors = 3;
    RegisterTestCase(e2e_test);

    // Performance Benchmark Test
    TestCase perf_test;
    perf_test.name = "PerformanceBenchmark";
    perf_test.description = "Measure latency, throughput, and resource usage across the system";
    perf_test.test_function = [this](const TestConfiguration& config, TestMetrics& metrics) {
        return TestPerformanceBenchmark(config, metrics);
    };
    perf_test.config.test_duration = std::chrono::seconds(60);
    perf_test.config.num_stm32_sensors = 10;
    perf_test.dependencies = {"EndToEndDataFlow"};
    RegisterTestCase(perf_test);

    // Stress Load Test
    TestCase stress_test;
    stress_test.name = "StressLoad";
    stress_test.description = "Test system behavior under high sensor loads";
    stress_test.test_function = [this](const TestConfiguration& config, TestMetrics& metrics) {
        return TestStressLoad(config, metrics);
    };
    stress_test.config.test_duration = std::chrono::seconds(120);
    stress_test.config.max_sensors_for_stress = 50;
    stress_test.config.message_rate_multiplier = 5.0;
    stress_test.dependencies = {"PerformanceBenchmark"};
    RegisterTestCase(stress_test);

    // Fault Tolerance Test
    TestCase fault_test;
    fault_test.name = "FaultTolerance";
    fault_test.description = "Test recovery from sensor failures, network drops, and component restarts";
    fault_test.test_function = [this](const TestConfiguration& config, TestMetrics& metrics) {
        return TestFaultTolerance(config, metrics);
    };
    fault_test.config.test_duration = std::chrono::seconds(90);
    fault_test.config.enable_sensor_failures = true;
    fault_test.config.enable_network_failures = true;
    fault_test.config.failure_probability = 0.1;
    fault_test.dependencies = {"EndToEndDataFlow"};
    RegisterTestCase(fault_test);

    // Thermal Integration Test
    TestCase thermal_test;
    thermal_test.name = "ThermalIntegration";
    thermal_test.description = "Test thermal monitoring system integration and alert generation";
    thermal_test.test_function = [this](const TestConfiguration& config, TestMetrics& metrics) {
        return TestThermalIntegration(config, metrics);
    };
    thermal_test.config.test_duration = std::chrono::seconds(45);
    thermal_test.config.temp_high_threshold = 30.0;
    thermal_test.config.temp_low_threshold = 20.0;
    thermal_test.dependencies = {"EndToEndDataFlow"};
    RegisterTestCase(thermal_test);

    // MQTT Bridge Reliability Test
    TestCase mqtt_test;
    mqtt_test.name = "MQTTBridgeReliability";
    mqtt_test.description = "Test MQTT-WebSocket bridge reliability and message delivery";
    mqtt_test.test_function = [this](const TestConfiguration& config, TestMetrics& metrics) {
        return TestMQTTBridgeReliability(config, metrics);
    };
    mqtt_test.config.test_duration = std::chrono::seconds(60);
    mqtt_test.dependencies = {"EndToEndDataFlow"};
    RegisterTestCase(mqtt_test);

    // Multi-Gateway Scaling Test
    TestCase scaling_test;
    scaling_test.name = "MultiGatewayScaling";
    scaling_test.description = "Test scaling with multiple RPi4 gateways";
    scaling_test.test_function = [this](const TestConfiguration& config, TestMetrics& metrics) {
        return TestMultiGatewayScaling(config, metrics);
    };
    scaling_test.config.test_duration = std::chrono::seconds(90);
    scaling_test.config.num_rpi4_gateways = 3;
    scaling_test.config.num_stm32_sensors = 15;
    scaling_test.dependencies = {"PerformanceBenchmark"};
    RegisterTestCase(scaling_test);

    // Recovery Scenarios Test
    TestCase recovery_test;
    recovery_test.name = "RecoveryScenarios";
    recovery_test.description = "Test system recovery from various failure scenarios";
    recovery_test.test_function = [this](const TestConfiguration& config, TestMetrics& metrics) {
        return TestRecoveryScenarios(config, metrics);
    };
    recovery_test.config.test_duration = std::chrono::seconds(120);
    recovery_test.dependencies = {"FaultTolerance"};
    RegisterTestCase(recovery_test);

    // Register test suites
    RegisterTestSuite("Basic", {"EndToEndDataFlow", "ThermalIntegration"});
    RegisterTestSuite("Performance", {"PerformanceBenchmark", "StressLoad", "MultiGatewayScaling"});
    RegisterTestSuite("Reliability", {"FaultTolerance", "MQTTBridgeReliability", "RecoveryScenarios"});
    RegisterTestSuite("Complete", {"EndToEndDataFlow", "PerformanceBenchmark", "StressLoad", 
                                  "FaultTolerance", "ThermalIntegration", "MQTTBridgeReliability", 
                                  "MultiGatewayScaling", "RecoveryScenarios"});
}

// Register test case
void IntegrationTestController::RegisterTestCase(const TestCase& test_case) {
    test_cases_.push_back(test_case);
}

// Register test suite
void IntegrationTestController::RegisterTestSuite(const std::string& suite_name, 
                                                 const std::vector<std::string>& test_names) {
    test_suites_[suite_name] = test_names;
}

// Set global configuration
void IntegrationTestController::SetGlobalConfiguration(const TestConfiguration& config) {
    global_config_ = config;
}

// Run all tests
bool IntegrationTestController::RunAllTests() {
    std::cout << "\n=== Starting Integration Test Suite ===" << std::endl;
    std::cout << "Total test cases: " << test_cases_.size() << std::endl;
    
    bool all_passed = true;
    
    // Start all components
    if (!StartAllComponents()) {
        std::cerr << "Failed to start components. Aborting tests." << std::endl;
        return false;
    }
    
    // Start real-time monitoring
    StartRealTimeMonitoring();
    
    try {
        for (const auto& test_case : test_cases_) {
            if (!test_case.is_enabled) {
                std::cout << "Skipping disabled test: " << test_case.name << std::endl;
                continue;
            }
            
            if (!CheckTestDependencies(test_case)) {
                std::cout << "Skipping test due to dependency failure: " << test_case.name << std::endl;
                test_results_[test_case.name] = TestResult::SKIPPED;
                continue;
            }
            
            std::cout << "\nRunning test: " << test_case.name << std::endl;
            std::cout << "Description: " << test_case.description << std::endl;
            
            TestResult result = ExecuteTestCase(test_case);
            test_results_[test_case.name] = result;
            
            if (result != TestResult::PASSED) {
                all_passed = false;
            }
            
            // Brief pause between tests
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during test execution: " << e.what() << std::endl;
        all_passed = false;
    }
    
    // Stop monitoring and components
    StopRealTimeMonitoring();
    StopAllComponents();
    
    // Print results
    PrintTestResults();
    
    return all_passed;
}

// Run test suite
bool IntegrationTestController::RunTestSuite(const std::string& suite_name) {
    auto it = test_suites_.find(suite_name);
    if (it == test_suites_.end()) {
        std::cerr << "Test suite not found: " << suite_name << std::endl;
        return false;
    }
    
    std::cout << "\n=== Running Test Suite: " << suite_name << " ===" << std::endl;
    
    bool all_passed = true;
    
    if (!StartAllComponents()) {
        std::cerr << "Failed to start components. Aborting test suite." << std::endl;
        return false;
    }
    
    StartRealTimeMonitoring();
    
    for (const auto& test_name : it->second) {
        auto test_it = std::find_if(test_cases_.begin(), test_cases_.end(),
                                   [&test_name](const TestCase& tc) { return tc.name == test_name; });
        
        if (test_it == test_cases_.end()) {
            std::cerr << "Test not found: " << test_name << std::endl;
            all_passed = false;
            continue;
        }
        
        if (!test_it->is_enabled) continue;
        
        if (!CheckTestDependencies(*test_it)) {
            test_results_[test_name] = TestResult::SKIPPED;
            continue;
        }
        
        std::cout << "\nRunning test: " << test_name << std::endl;
        TestResult result = ExecuteTestCase(*test_it);
        test_results_[test_name] = result;
        
        if (result != TestResult::PASSED) {
            all_passed = false;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    StopRealTimeMonitoring();
    StopAllComponents();
    PrintTestResults();
    
    return all_passed;
}

// Start all components
bool IntegrationTestController::StartAllComponents() {
    std::cout << "Starting all components..." << std::endl;
    
    // Start in order: STM32 simulators, RPi4 gateways, MQTT bridge, thermal monitoring
    if (!stm32_manager_->StartAll()) {
        std::cerr << "Failed to start STM32 simulators" << std::endl;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (!rpi4_manager_->StartAll()) {
        std::cerr << "Failed to start RPi4 gateways" << std::endl;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (!bridge_manager_->Start()) {
        std::cerr << "Failed to start MQTT bridge" << std::endl;
        return false;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    if (!thermal_manager_->Start()) {
        std::cerr << "Failed to start thermal monitoring" << std::endl;
        return false;
    }
    
    std::cout << "All components started successfully" << std::endl;
    return true;
}

// Stop all components
bool IntegrationTestController::StopAllComponents() {
    std::cout << "Stopping all components..." << std::endl;
    
    bool success = true;
    
    if (thermal_manager_ && !thermal_manager_->Stop()) {
        std::cerr << "Failed to stop thermal monitoring" << std::endl;
        success = false;
    }
    
    if (bridge_manager_ && !bridge_manager_->Stop()) {
        std::cerr << "Failed to stop MQTT bridge" << std::endl;
        success = false;
    }
    
    if (rpi4_manager_ && !rpi4_manager_->StopAll()) {
        std::cerr << "Failed to stop RPi4 gateways" << std::endl;
        success = false;
    }
    
    if (stm32_manager_ && !stm32_manager_->StopAll()) {
        std::cerr << "Failed to stop STM32 simulators" << std::endl;
        success = false;
    }
    
    std::cout << "Components stopped" << std::endl;
    return success;
}

// Execute test case
TestResult IntegrationTestController::ExecuteTestCase(const TestCase& test_case) {
    TestMetrics metrics;
    metrics.start_time = GetCurrentTime();
    
    try {
        TestResult result = test_case.test_function(test_case.config, metrics);
        
        metrics.end_time = GetCurrentTime();
        metrics.memory_usage_kb = GetMemoryUsage();
        metrics.cpu_usage_percent = GetCPUUsage();
        
        UpdateTestMetrics(test_case.name, metrics);
        
        std::cout << "Test " << test_case.name << ": ";
        switch (result) {
            case TestResult::PASSED:
                std::cout << "PASSED" << std::endl;
                break;
            case TestResult::FAILED:
                std::cout << "FAILED" << std::endl;
                break;
            case TestResult::SKIPPED:
                std::cout << "SKIPPED" << std::endl;
                break;
            case TestResult::ERROR:
                std::cout << "ERROR" << std::endl;
                break;
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Exception in test " << test_case.name << ": " << e.what() << std::endl;
        metrics.errors.push_back(e.what());
        UpdateTestMetrics(test_case.name, metrics);
        return TestResult::ERROR;
    }
}

// Check test dependencies
bool IntegrationTestController::CheckTestDependencies(const TestCase& test_case) {
    for (const auto& dependency : test_case.dependencies) {
        auto it = test_results_.find(dependency);
        if (it == test_results_.end() || it->second != TestResult::PASSED) {
            return false;
        }
    }
    return true;
}

// Update test metrics
void IntegrationTestController::UpdateTestMetrics(const std::string& test_name, const TestMetrics& metrics) {
    std::lock_guard<std::mutex> lock(results_mutex_);
    test_metrics_[test_name] = metrics;
}

// Print test results
void IntegrationTestController::PrintTestResults() const {
    std::cout << "\n=== Test Results Summary ===" << std::endl;
    
    int passed = 0, failed = 0, skipped = 0, errors = 0;
    
    for (const auto& result : test_results_) {
        std::cout << std::left << std::setw(25) << result.first << " : ";
        
        switch (result.second) {
            case TestResult::PASSED:
                std::cout << "PASSED";
                passed++;
                break;
            case TestResult::FAILED:
                std::cout << "FAILED";
                failed++;
                break;
            case TestResult::SKIPPED:
                std::cout << "SKIPPED";
                skipped++;
                break;
            case TestResult::ERROR:
                std::cout << "ERROR";
                errors++;
                break;
        }
        
        // Print metrics if available
        auto metrics_it = test_metrics_.find(result.first);
        if (metrics_it != test_metrics_.end()) {
            const auto& metrics = metrics_it->second;
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                metrics.end_time - metrics.start_time).count();
            
            std::cout << " (" << duration << "ms";
            if (metrics.messages_sent > 0) {
                std::cout << ", " << metrics.messages_sent << " msgs";
            }
            if (metrics.avg_latency_ms > 0) {
                std::cout << ", " << std::fixed << std::setprecision(1) 
                         << metrics.avg_latency_ms << "ms avg";
            }
            std::cout << ")";
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "\nSummary: " << passed << " passed, " << failed << " failed, " 
              << skipped << " skipped, " << errors << " errors" << std::endl;
    
    // Print aggregated metrics
    TestMetrics aggregated = GetAggregatedMetrics();
    std::cout << "\nAggregated Metrics:" << std::endl;
    std::cout << "  Total messages: " << aggregated.messages_sent << std::endl;
    std::cout << "  Average latency: " << std::fixed << std::setprecision(2) 
              << aggregated.avg_latency_ms << "ms" << std::endl;
    std::cout << "  Peak memory usage: " << aggregated.memory_usage_kb << "KB" << std::endl;
    std::cout << "  Average CPU usage: " << std::fixed << std::setprecision(1) 
              << aggregated.cpu_usage_percent << "%" << std::endl;
}

// Get current time
std::chrono::steady_clock::time_point IntegrationTestController::GetCurrentTime() const {
    return std::chrono::steady_clock::now();
}

// Calculate latency
double IntegrationTestController::CalculateLatency(const std::chrono::steady_clock::time_point& start,
                                                  const std::chrono::steady_clock::time_point& end) const {
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
}

// Get memory usage
size_t IntegrationTestController::GetMemoryUsage() const {
    std::ifstream status("/proc/self/status");
    std::string line;
    
    while (std::getline(status, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            std::istringstream iss(line);
            std::string key, value, unit;
            iss >> key >> value >> unit;
            return std::stoul(value);
        }
    }
    return 0;
}

// Get CPU usage (simplified)
double IntegrationTestController::GetCPUUsage() const {
    // This is a simplified implementation
    // In a real implementation, you'd measure CPU usage over time
    return 0.0;
}

// Get aggregated metrics
TestMetrics IntegrationTestController::GetAggregatedMetrics() const {
    TestMetrics aggregated;
    
    for (const auto& pair : test_metrics_) {
        const auto& metrics = pair.second;
        aggregated.messages_sent += metrics.messages_sent;
        aggregated.messages_received += metrics.messages_received;
        aggregated.alerts_generated += metrics.alerts_generated;
        aggregated.memory_usage_kb = std::max(aggregated.memory_usage_kb, metrics.memory_usage_kb);
        aggregated.cpu_usage_percent = std::max(aggregated.cpu_usage_percent, metrics.cpu_usage_percent);
        
        if (metrics.avg_latency_ms > 0) {
            if (aggregated.avg_latency_ms == 0) {
                aggregated.avg_latency_ms = metrics.avg_latency_ms;
            } else {
                aggregated.avg_latency_ms = (aggregated.avg_latency_ms + metrics.avg_latency_ms) / 2.0;
            }
        }
        
        aggregated.max_latency_ms = std::max(aggregated.max_latency_ms, metrics.max_latency_ms);
        if (metrics.min_latency_ms < aggregated.min_latency_ms) {
            aggregated.min_latency_ms = metrics.min_latency_ms;
        }
    }
    
    return aggregated;
}

// Start real-time monitoring
void IntegrationTestController::StartRealTimeMonitoring() {
    monitoring_active_ = true;
    std::cout << "Real-time monitoring started" << std::endl;
}

// Stop real-time monitoring
void IntegrationTestController::StopRealTimeMonitoring() {
    monitoring_active_ = false;
    std::cout << "Real-time monitoring stopped" << std::endl;
}

// Load configuration from file
void IntegrationTestController::LoadConfigurationFromFile(const std::string& config_file) {
    std::cout << "Loading configuration from: " << config_file << std::endl;
    // Stub implementation - would parse JSON configuration file
}

// Run single test
bool IntegrationTestController::RunSingleTest(const std::string& test_name) {
    std::cout << "Running single test: " << test_name << std::endl;
    
    auto test_it = std::find_if(test_cases_.begin(), test_cases_.end(),
                               [&test_name](const TestCase& tc) { return tc.name == test_name; });
    
    if (test_it == test_cases_.end()) {
        std::cerr << "Test not found: " << test_name << std::endl;
        return false;
    }
    
    if (!StartAllComponents()) {
        std::cerr << "Failed to start components for test: " << test_name << std::endl;
        return false;
    }
    
    TestResult result = ExecuteTestCase(*test_it);
    test_results_[test_name] = result;
    
    StopAllComponents();
    
    return result == TestResult::PASSED;
}

// Save test results
void IntegrationTestController::SaveTestResults(const std::string& output_file) const {
    std::cout << "Saving test results to: " << output_file << std::endl;
    std::ofstream file(output_file);
    if (file.is_open()) {
        file << "{\n";
        file << "  \"test_results\": {\n";
        
        bool first = true;
        for (const auto& result : test_results_) {
            if (!first) file << ",\n";
            file << "    \"" << result.first << "\": \"";
            switch (result.second) {
                case TestResult::PASSED: file << "PASSED"; break;
                case TestResult::FAILED: file << "FAILED"; break;
                case TestResult::SKIPPED: file << "SKIPPED"; break;
                case TestResult::ERROR: file << "ERROR"; break;
            }
            file << "\"";
            first = false;
        }
        
        file << "\n  }\n";
        file << "}\n";
        file.close();
    }
}

// Utility functions
namespace TestUtils {
    std::string GenerateTestSensorData(double temperature, double humidity, const std::string& location) {
        std::ostringstream oss;
        oss << "{"
            << "\"temperature\":" << std::fixed << std::setprecision(1) << temperature << ","
            << "\"humidity\":" << std::fixed << std::setprecision(1) << humidity << ","
            << "\"location\":\"" << location << "\","
            << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch()).count()
            << "}";
        return oss.str();
    }
    
    bool ValidateMessageFormat(const std::string& message) {
        // Simple validation - check for required fields
        return (message.find("temperature") != std::string::npos &&
                message.find("humidity") != std::string::npos &&
                message.find("{") != std::string::npos &&
                message.find("}") != std::string::npos);
    }
    
    double SimulateNetworkLatency() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::normal_distribution<> dis(5.0, 2.0); // 5ms ± 2ms
        
        return std::max(1.0, dis(gen));
    }
    
    void InjectRandomFailure(double probability) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> dis(0.0, 1.0);
        
        if (dis(gen) < probability) {
            throw std::runtime_error("Injected random failure");
        }
    }
    
    std::vector<std::string> GenerateTestScenarios(int count) {
        std::vector<std::string> scenarios;
        
        for (int i = 0; i < count; ++i) {
            scenarios.push_back("scenario_" + std::to_string(i));
        }
        
        return scenarios;
    }
    
    bool WaitForCondition(std::function<bool()> condition, std::chrono::milliseconds timeout) {
        auto start = std::chrono::steady_clock::now();
        
        while (std::chrono::steady_clock::now() - start < timeout) {
            if (condition()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        return false;
    }
} 