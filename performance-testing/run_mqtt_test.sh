#!/bin/bash

# MQTT Performance Test Runner
# Runs comprehensive performance testing with system monitoring

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SENSORS=${1:-10}
DURATION=${2:-60}
INTERVAL=${3:-100}
OUTPUT_DIR="test_results_$(date +%Y%m%d_%H%M%S)"

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check dependencies
check_dependencies() {
    print_status "Checking dependencies..."
    
    # Check for required commands
    local missing_deps=()
    
    if ! command -v g++ &> /dev/null; then
        missing_deps+=("g++")
    fi
    
    if ! command -v mosquitto &> /dev/null; then
        missing_deps+=("mosquitto")
    fi
    
    if ! command -v python3 &> /dev/null; then
        missing_deps+=("python3")
    fi
    
    if ! python3 -c "import psutil" &> /dev/null; then
        missing_deps+=("python3-psutil")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "Missing dependencies: ${missing_deps[*]}"
        print_status "Install with: sudo apt install build-essential mosquitto mosquitto-clients python3 python3-pip && pip3 install psutil"
        exit 1
    fi
    
    print_success "All dependencies found"
}

# Function to check MQTT broker
check_mqtt_broker() {
    print_status "Checking MQTT broker..."
    
    if ! pgrep mosquitto > /dev/null; then
        print_warning "Mosquitto broker not running, attempting to start..."
        if sudo systemctl start mosquitto; then
            print_success "Mosquitto broker started"
        else
            print_error "Failed to start Mosquitto broker"
            print_status "Please start manually: sudo systemctl start mosquitto"
            exit 1
        fi
    else
        print_success "Mosquitto broker is running"
    fi
}

# Function to build the performance test
build_test() {
    print_status "Building performance test..."
    
    if make clean && make all; then
        print_success "Performance test built successfully"
    else
        print_error "Failed to build performance test"
        exit 1
    fi
}

# Function to run the performance test with monitoring
run_test() {
    print_status "Creating output directory: $OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"
    
    print_status "Starting system monitoring..."
    python3 system_monitor.py --output "$OUTPUT_DIR/system_stats.json" --interval 0.5 &
    MONITOR_PID=$!
    
    # Wait a moment for monitor to start
    sleep 2
    
    print_status "Starting MQTT performance test..."
    print_status "Configuration: $SENSORS sensors, ${DURATION}s duration, ${INTERVAL}ms interval"
    
    # Run the performance test
    ./mqtt_performance_test "$SENSORS" "$DURATION" "$INTERVAL" > "$OUTPUT_DIR/mqtt_test.log" 2>&1
    TEST_EXIT_CODE=$?
    
    # Stop monitoring
    print_status "Stopping system monitoring..."
    kill $MONITOR_PID 2>/dev/null || true
    wait $MONITOR_PID 2>/dev/null || true
    
    if [ $TEST_EXIT_CODE -eq 0 ]; then
        print_success "Performance test completed successfully"
    else
        print_error "Performance test failed with exit code $TEST_EXIT_CODE"
    fi
}

# Function to generate test report
generate_report() {
    print_status "Generating test report..."
    
    local report_file="$OUTPUT_DIR/test_report.md"
    
    cat > "$report_file" << EOF
# MQTT Performance Test Report

**Test Date:** $(date)
**Configuration:**
- Sensors: $SENSORS
- Duration: ${DURATION}s
- Message Interval: ${INTERVAL}ms

## Test Results

### MQTT Performance
EOF
    
    # Extract MQTT test results
    if [ -f "$OUTPUT_DIR/mqtt_test.log" ]; then
        echo "```" >> "$report_file"
        cat "$OUTPUT_DIR/mqtt_test.log" >> "$report_file"
        echo "```" >> "$report_file"
    fi
    
    # Extract system monitoring results
    if [ -f "$OUTPUT_DIR/system_stats.json" ]; then
        echo "" >> "$report_file"
        echo "### System Resource Usage" >> "$report_file"
        echo "" >> "$report_file"
        
        # Parse JSON and extract summary
        python3 -c "
import json
try:
    with open('$OUTPUT_DIR/system_stats.json', 'r') as f:
        data = json.load(f)
    summary = data.get('summary', {})
    print(f'- **Monitoring Duration:** {summary.get(\"monitoring_duration_seconds\", 0):.1f}s')
    print(f'- **Samples Collected:** {summary.get(\"samples_collected\", 0)}')
    
    cpu = summary.get('cpu_summary', {})
    print(f'- **CPU Usage:** Avg {cpu.get(\"average\", 0):.1f}%, Max {cpu.get(\"maximum\", 0):.1f}%')
    
    mem = summary.get('memory_summary', {})
    print(f'- **Memory Usage:** Avg {mem.get(\"average_mb\", 0):.1f}MB, Max {mem.get(\"maximum_mb\", 0):.1f}MB')
    
    proc_cpu = summary.get('process_cpu_summary', {})
    print(f'- **Process CPU:** Avg {proc_cpu.get(\"average\", 0):.1f}%, Max {proc_cpu.get(\"maximum\", 0):.1f}%')
    
    proc_mem = summary.get('process_memory_summary', {})
    print(f'- **Process Memory:** Avg {proc_mem.get(\"average_mb\", 0):.1f}MB, Max {proc_mem.get(\"maximum_mb\", 0):.1f}MB')
except Exception as e:
    print(f'Error parsing system stats: {e}')
" >> "$report_file"
    fi
    
    echo "" >> "$report_file"
    echo "## Files Generated" >> "$report_file"
    echo "" >> "$report_file"
    echo "- \`mqtt_test.log\` - MQTT performance test output" >> "$report_file"
    echo "- \`system_stats.json\` - Detailed system monitoring data" >> "$report_file"
    echo "- \`test_report.md\` - This report" >> "$report_file"
    
    print_success "Test report generated: $report_file"
}

# Function to display summary
display_summary() {
    print_status "Test Summary:"
    echo "  Output Directory: $OUTPUT_DIR"
    echo "  Sensors: $SENSORS"
    echo "  Duration: ${DURATION}s"
    echo "  Interval: ${INTERVAL}ms"
    
    if [ -f "$OUTPUT_DIR/mqtt_test.log" ]; then
        echo ""
        print_status "MQTT Test Output (last 10 lines):"
        tail -10 "$OUTPUT_DIR/mqtt_test.log"
    fi
    
    if [ -f "$OUTPUT_DIR/system_stats.json" ]; then
        echo ""
        print_status "System Monitoring Summary:"
        python3 -c "
import json
try:
    with open('$OUTPUT_DIR/system_stats.json', 'r') as f:
        data = json.load(f)
    summary = data.get('summary', {})
    print(f'  CPU Usage: Avg {summary.get(\"cpu_summary\", {}).get(\"average\", 0):.1f}%')
    print(f'  Memory Usage: Avg {summary.get(\"memory_summary\", {}).get(\"average_mb\", 0):.1f}MB')
    print(f'  Process CPU: Avg {summary.get(\"process_cpu_summary\", {}).get(\"average\", 0):.1f}%')
    print(f'  Process Memory: Avg {summary.get(\"process_memory_summary\", {}).get(\"average_mb\", 0):.1f}MB')
except:
    print('  Error reading system stats')
"
    fi
}

# Main execution
main() {
    echo "🚀 MQTT Performance Test Runner"
    echo "==============================="
    echo "Sensors: $SENSORS"
    echo "Duration: ${DURATION}s"
    echo "Interval: ${INTERVAL}ms"
    echo ""
    
    # Change to script directory
    cd "$(dirname "$0")"
    
    # Run test steps
    check_dependencies
    check_mqtt_broker
    build_test
    run_test
    generate_report
    display_summary
    
    print_success "Test completed! Results saved in: $OUTPUT_DIR"
}

# Handle script arguments
if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "MQTT Performance Test Runner"
    echo ""
    echo "Usage: $0 [sensors] [duration] [interval]"
    echo ""
    echo "Arguments:"
    echo "  sensors   Number of sensors to simulate (default: 10)"
    echo "  duration  Test duration in seconds (default: 60)"
    echo "  interval  Message interval in milliseconds (default: 100)"
    echo ""
    echo "Examples:"
    echo "  $0                    # Run with defaults (10 sensors, 60s, 100ms)"
    echo "  $0 20                 # 20 sensors, 60s, 100ms"
    echo "  $0 10 120 50          # 10 sensors, 120s, 50ms"
    exit 0
fi

# Run main function
main "$@" 