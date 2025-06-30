#ifndef INTEGRATION_TEST_CONTROLLER_H
#define INTEGRATION_TEST_CONTROLLER_H

#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <map>
#include <functional>

// Forward declarations for component interfaces
class STM32SimulatorManager;
class RPi4GatewayManager;
class MQTTBridgeManager;
class ThermalMonitorManager;

struct TestMetrics {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    size_t messages_sent = 0;
    size_t messages_received = 0;
    size_t alerts_generated = 0;
    double avg_latency_ms = 0.0;
    double max_latency_ms = 0.0;
    double min_latency_ms = 999999.0;
    size_t memory_usage_kb = 0;
    double cpu_usage_percent = 0.0;
    std::vector<std::string> errors;
    std::map<std::string, double> custom_metrics;
};

struct TestConfiguration {
    // Component configurations
    int num_stm32_sensors = 5;
    int num_rpi4_gateways = 1;
    bool enable_mqtt_bridge = true;
    bool enable_thermal_monitoring = true;
    
    // Test parameters
    std::chrono::seconds test_duration{30};
    std::chrono::milliseconds sensor_interval{1000};
    std::chrono::milliseconds gateway_processing_interval{100};
    
    // Stress test parameters
    int max_sensors_for_stress = 100;
    double message_rate_multiplier = 1.0;
    
    // Fault injection parameters
    bool enable_sensor_failures = false;
    bool enable_network_failures = false;
    bool enable_gateway_failures = false;
    double failure_probability = 0.05;
    
    // Alert thresholds
    double temp_high_threshold = 35.0;
    double temp_low_threshold = 15.0;
    double humidity_high_threshold = 80.0;
    
    // Performance requirements
    double max_acceptable_latency_ms = 100.0;
    double min_message_success_rate = 0.95;
    size_t max_memory_usage_mb = 100;
};

enum class TestResult {
    PASSED,
    FAILED,
    SKIPPED,
    ERROR
};

struct TestCase {
    std::string name;
    std::string description;
    std::function<TestResult(const TestConfiguration&, TestMetrics&)> test_function;
    TestConfiguration config;
    bool is_enabled = true;
    std::vector<std::string> dependencies;
};

class IntegrationTestController {
public:
    IntegrationTestController();
    ~IntegrationTestController();
    
    // Main test execution
    bool RunAllTests();
    bool RunTestSuite(const std::string& suite_name);
    bool RunSingleTest(const std::string& test_name);
    
    // Test suite management
    void RegisterTestCase(const TestCase& test_case);
    void RegisterTestSuite(const std::string& suite_name, const std::vector<std::string>& test_names);
    
    // Configuration
    void SetGlobalConfiguration(const TestConfiguration& config);
    void LoadConfigurationFromFile(const std::string& config_file);
    
    // Results and reporting
    void PrintTestResults() const;
    void SaveTestResults(const std::string& output_file) const;
    TestMetrics GetAggregatedMetrics() const;
    
    // Component management
    bool StartAllComponents();
    bool StopAllComponents();
    bool RestartComponent(const std::string& component_name);
    
    // Real-time monitoring
    void StartRealTimeMonitoring();
    void StopRealTimeMonitoring();
    std::map<std::string, double> GetCurrentMetrics() const;

private:
    // Component managers
    std::unique_ptr<STM32SimulatorManager> stm32_manager_;
    std::unique_ptr<RPi4GatewayManager> rpi4_manager_;
    std::unique_ptr<MQTTBridgeManager> bridge_manager_;
    std::unique_ptr<ThermalMonitorManager> thermal_manager_;
    
    // Test management
    std::vector<TestCase> test_cases_;
    std::map<std::string, std::vector<std::string>> test_suites_;
    std::map<std::string, TestResult> test_results_;
    std::map<std::string, TestMetrics> test_metrics_;
    
    // Configuration
    TestConfiguration global_config_;
    
    // Synchronization
    std::atomic<bool> running_{false};
    std::atomic<bool> monitoring_active_{false};
    std::mutex results_mutex_;
    std::condition_variable test_completion_cv_;
    
    // Monitoring thread
    std::unique_ptr<std::thread> monitoring_thread_;
    
    // Internal methods
    void InitializeDefaultTests();
    void InitializeComponentManagers();
    bool CheckTestDependencies(const TestCase& test_case);
    TestResult ExecuteTestCase(const TestCase& test_case);
    void UpdateTestMetrics(const std::string& test_name, const TestMetrics& metrics);
    void MonitoringLoop();
    
    // Performance measurement utilities
    std::chrono::steady_clock::time_point GetCurrentTime() const;
    double CalculateLatency(const std::chrono::steady_clock::time_point& start,
                           const std::chrono::steady_clock::time_point& end) const;
    size_t GetMemoryUsage() const;
    double GetCPUUsage() const;
    
    // Test implementations
    TestResult TestEndToEndDataFlow(const TestConfiguration& config, TestMetrics& metrics);
    TestResult TestPerformanceBenchmark(const TestConfiguration& config, TestMetrics& metrics);
    TestResult TestStressLoad(const TestConfiguration& config, TestMetrics& metrics);
    TestResult TestFaultTolerance(const TestConfiguration& config, TestMetrics& metrics);
    TestResult TestThermalIntegration(const TestConfiguration& config, TestMetrics& metrics);
    TestResult TestMQTTBridgeReliability(const TestConfiguration& config, TestMetrics& metrics);
    TestResult TestMultiGatewayScaling(const TestConfiguration& config, TestMetrics& metrics);
    TestResult TestRecoveryScenarios(const TestConfiguration& config, TestMetrics& metrics);
};

// Utility functions for test implementations
namespace TestUtils {
    std::string GenerateTestSensorData(double temperature, double humidity, const std::string& location);
    bool ValidateMessageFormat(const std::string& message);
    double SimulateNetworkLatency();
    void InjectRandomFailure(double probability);
    std::vector<std::string> GenerateTestScenarios(int count);
    bool WaitForCondition(std::function<bool()> condition, std::chrono::milliseconds timeout);
}

#endif // INTEGRATION_TEST_CONTROLLER_H 