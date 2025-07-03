#!/bin/bash

# 🚀 IoT Thermal Monitoring: Automated Setup Script
# This script automates the setup process for the entire project

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command_exists apt; then
            echo "ubuntu"
        elif command_exists yum; then
            echo "centos"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

# Function to install system dependencies
install_system_deps() {
    local os=$(detect_os)
    
    print_status "Detected OS: $os"
    print_status "Installing system dependencies..."
    
    case $os in
        "ubuntu")
            sudo apt update
            sudo apt install -y build-essential cmake git curl wget
            sudo apt install -y libssl-dev libwebsockets-dev libmosquitto-dev
            sudo apt install -y python3 python3-pip nodejs npm
            ;;
        "centos")
            sudo yum groupinstall -y "Development Tools"
            sudo yum install -y cmake git curl wget
            sudo yum install -y openssl-devel libwebsockets-devel mosquitto-devel
            sudo yum install -y python3 python3-pip nodejs npm
            ;;
        "macos")
            if ! command_exists brew; then
                print_error "Homebrew not found. Please install Homebrew first:"
                print_error "https://brew.sh/"
                exit 1
            fi
            brew install cmake git curl wget
            brew install openssl libwebsockets mosquitto
            brew install python3 node
            ;;
        *)
            print_error "Unsupported OS: $os"
            print_error "Please install dependencies manually. See SETUP_GUIDE.md"
            exit 1
            ;;
    esac
    
    print_success "System dependencies installed"
}

# Function to check and install Node.js
setup_nodejs() {
    print_status "Setting up Node.js dependencies..."
    
    if ! command_exists node; then
        print_error "Node.js not found. Please install Node.js first."
        exit 1
    fi
    
    if ! command_exists npm; then
        print_error "npm not found. Please install npm first."
        exit 1
    fi
    
    # Install Node.js dependencies
    cd communication-backends/js-bridge
    npm install
    cd ../..
    
    print_success "Node.js dependencies installed"
}

# Function to setup Python dependencies
setup_python() {
    print_status "Setting up Python dependencies..."
    
    if ! command_exists python3; then
        print_error "Python3 not found. Please install Python3 first."
        exit 1
    fi
    
    # Install Python dependencies
    pip3 install pytest requests websocket-client paho-mqtt
    
    print_success "Python dependencies installed"
}

# Function to build the project
build_project() {
    print_status "Building the project..."
    
    # Create necessary directories
    mkdir -p logs
    mkdir -p results
    
    # Build all components
    make -j$(nproc)
    
    print_success "Project built successfully"
}

# Function to create configuration files
create_configs() {
    print_status "Creating configuration files..."
    
    # Create MQTT config
    cat > communication-backends/mqtt-only/config.json << EOF
{
  "broker": "localhost",
  "port": 1883,
  "client_id": "thermal_monitor",
  "topics": {
    "sensor_data": "sensors/+/data",
    "alerts": "alerts/thermal"
  }
}
EOF
    
    # Create WebSocket config
    cat > communication-backends/websocket-only/config.json << EOF
{
  "port": 8080,
  "max_clients": 100,
  "heartbeat_interval": 30
}
EOF
    
    # Create C++ Bridge config
    cat > communication-backends/cpp-bridge/config.json << EOF
{
  "mqtt": {
    "broker": "localhost",
    "port": 1883
  },
  "websocket": {
    "port": 8081,
    "max_clients": 50
  }
}
EOF
    
    print_success "Configuration files created"
}

# Function to run basic tests
run_basic_tests() {
    print_status "Running basic tests..."
    
    # Test if binaries were created
    local binaries=(
        "communication-backends/mqtt-only/simple_mqtt_client"
        "communication-backends/websocket-only/simple_ws_server"
        "communication-backends/cpp-bridge/bin/mqtt_ws_bridge"
        "hardware-emulation/stm32-sensors/test_stm32_simulators"
        "hardware-emulation/rpi4-gateways/bin/test_rpi4_gateway"
        "performance-testing/benchmarks/bin/integration_tests"
    )
    
    for binary in "${binaries[@]}"; do
        if [[ -f "$binary" ]]; then
            print_success "✓ $binary exists"
        else
            print_warning "⚠ $binary not found"
        fi
    done
    
    print_success "Basic tests completed"
}

# Function to display next steps
show_next_steps() {
    echo
    print_success "🎉 Setup completed successfully!"
    echo
    echo "Next steps:"
    echo "1. Start MQTT broker: mosquitto -p 1883"
    echo "2. Run a quick test:"
    echo "   cd communication-backends/websocket-only"
    echo "   ./simple_ws_server"
    echo "3. In another terminal:"
    echo "   cd hardware-emulation/stm32-sensors"
    echo "   ./test_stm32_simulators --sensors 5 --duration 60"
    echo
    echo "For complete instructions, see SETUP_GUIDE.md"
    echo
}

# Main setup function
main() {
    echo "🚀 IoT Thermal Monitoring: Automated Setup"
    echo "=========================================="
    echo
    
    # Check if running as root
    if [[ $EUID -eq 0 ]]; then
        print_error "Please do not run this script as root"
        exit 1
    fi
    
    # Check if we're in the project directory
    if [[ ! -f "README.md" ]]; then
        print_error "Please run this script from the project root directory"
        exit 1
    fi
    
    # Install system dependencies
    install_system_deps
    
    # Setup Node.js
    setup_nodejs
    
    # Setup Python
    setup_python
    
    # Build project
    build_project
    
    # Create configs
    create_configs
    
    # Run basic tests
    run_basic_tests
    
    # Show next steps
    show_next_steps
}

# Run main function
main "$@" 