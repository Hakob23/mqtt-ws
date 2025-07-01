#!/usr/bin/env python3
"""
Comprehensive Resource Usage Testing for IoT Thermal Monitoring
Tests all 4 communication approaches with resource monitoring
"""

import subprocess
import time
import json
import os
from datetime import datetime
from resource_monitor import monitor_test

def run_resource_tests():
    """Run resource monitoring tests for all 4 approaches"""
    
    print("🚀 **IoT Thermal Monitoring - Resource Usage Tests**")
    print("=" * 60)
    print("Testing all 4 communication approaches with resource monitoring")
    print("Standardized test parameters: 20 seconds, 5 sensors")
    print()
    
    # Test configurations
    tests = [
        {
            'name': 'MQTT-Only',
            'command': './communication-backends/mqtt-only/simple_mqtt_client',
            'description': 'Direct MQTT client implementation'
        },
        {
            'name': 'WebSocket-Only', 
            'command': './communication-backends/websocket-only/simple_ws_server',
            'description': 'Direct WebSocket server implementation'
        },
        {
            'name': 'C++-Bridge',
            'command': './communication-backends/cpp-bridge/test_thermal_system',
            'description': 'C++ MQTT-to-WebSocket bridge'
        },
        {
            'name': 'JS-Bridge',
            'command': 'cd communication-backends/js-bridge && node performance_test.js',
            'description': 'JavaScript MQTT-to-WebSocket bridge'
        }
    ]
    
    results = {}
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    for i, test in enumerate(tests, 1):
        print(f"\n{'='*60}")
        print(f"🧪 TEST {i}/4: {test['name']}")
        print(f"📝 Description: {test['description']}")
        print(f"⏱️  Duration: 20 seconds")
        print(f"{'='*60}")
        
        # Check if executable exists
        if not os.path.exists(test['command'].split()[0]):
            print(f"❌ Executable not found: {test['command']}")
            print("   Skipping this test...")
            continue
            
        try:
            # Run resource monitoring test
            result = monitor_test(
                test_name=test['name'],
                command=test['command'],
                duration=20
            )
            
            if result:
                results[test['name']] = result
                print(f"✅ {test['name']} test completed successfully")
            else:
                print(f"❌ {test['name']} test failed")
                
        except Exception as e:
            print(f"❌ Error running {test['name']}: {e}")
            
        # Wait between tests
        if i < len(tests):
            print("\n⏳ Waiting 5 seconds before next test...")
            time.sleep(5)
    
    # Generate comparison report
    generate_comparison_report(results, timestamp)
    
def generate_comparison_report(results, timestamp):
    """Generate comprehensive comparison report"""
    
    print(f"\n{'='*80}")
    print("📊 **RESOURCE USAGE COMPARISON REPORT**")
    print(f"{'='*80}")
    
    if not results:
        print("❌ No test results available")
        return
        
    # Create comparison table
    print(f"\n🖥️  **CPU Usage Comparison**")
    print("-" * 80)
    print(f"{'Approach':<15} {'Avg CPU %':<12} {'Max CPU %':<12} {'Min CPU %':<12}")
    print("-" * 80)
    
    for name, result in results.items():
        cpu = result['cpu']
        print(f"{name:<15} {cpu['average_percent']:<12.2f} {cpu['max_percent']:<12.2f} {cpu['min_percent']:<12.2f}")
    
    print(f"\n💾 **Memory Usage Comparison**")
    print("-" * 80)
    print(f"{'Approach':<15} {'Avg Memory MB':<15} {'Max Memory MB':<15} {'Min Memory MB':<15}")
    print("-" * 80)
    
    for name, result in results.items():
        memory = result['memory']
        print(f"{name:<15} {memory['average_mb']:<15.2f} {memory['max_mb']:<15.2f} {memory['min_mb']:<15.2f}")
    
    print(f"\n🌐 **Network I/O Comparison**")
    print("-" * 80)
    print(f"{'Approach':<15} {'Sent MB':<12} {'Received MB':<15} {'Total MB':<12}")
    print("-" * 80)
    
    for name, result in results.items():
        network = result['network']
        total_mb = network['bytes_sent_mb'] + network['bytes_received_mb']
        print(f"{name:<15} {network['bytes_sent_mb']:<12.2f} {network['bytes_received_mb']:<15.2f} {total_mb:<12.2f}")
    
    print(f"\n⚙️  **Process Memory Comparison**")
    print("-" * 80)
    print(f"{'Approach':<15} {'Process Memory MB':<20}")
    print("-" * 80)
    
    for name, result in results.items():
        process = result['process']
        print(f"{name:<15} {process['average_memory_mb']:<20.2f}")
    
    # Find winners
    print(f"\n🏆 **RESOURCE EFFICIENCY RANKINGS**")
    print("-" * 80)
    
    # CPU efficiency (lower is better)
    cpu_rankings = sorted(results.items(), key=lambda x: x[1]['cpu']['average_percent'])
    print("🥇 **CPU Efficiency (Lower is Better):**")
    for i, (name, result) in enumerate(cpu_rankings, 1):
        print(f"   {i}. {name}: {result['cpu']['average_percent']:.2f}% CPU")
    
    # Memory efficiency (lower is better)
    memory_rankings = sorted(results.items(), key=lambda x: x[1]['memory']['average_mb'])
    print("\n🥇 **Memory Efficiency (Lower is Better):**")
    for i, (name, result) in enumerate(memory_rankings, 1):
        print(f"   {i}. {name}: {result['memory']['average_mb']:.2f} MB")
    
    # Network efficiency (higher throughput per resource)
    print("\n🥇 **Network Efficiency (MB per CPU %):**")
    network_efficiency = []
    for name, result in results.items():
        total_mb = result['network']['bytes_sent_mb'] + result['network']['bytes_received_mb']
        cpu_percent = result['cpu']['average_percent']
        efficiency = total_mb / cpu_percent if cpu_percent > 0 else 0
        network_efficiency.append((name, efficiency, total_mb, cpu_percent))
    
    network_efficiency.sort(key=lambda x: x[1], reverse=True)
    for i, (name, efficiency, total_mb, cpu_percent) in enumerate(network_efficiency, 1):
        print(f"   {i}. {name}: {efficiency:.2f} MB per CPU % ({total_mb:.2f} MB / {cpu_percent:.2f}%)")
    
    # Save detailed results
    filename = f"resource_comparison_{timestamp}.json"
    with open(filename, 'w') as f:
        json.dump({
            'timestamp': timestamp,
            'test_duration': 20,
            'results': results,
            'rankings': {
                'cpu_efficiency': [name for name, _ in cpu_rankings],
                'memory_efficiency': [name for name, _ in memory_rankings],
                'network_efficiency': [name for name, _, _, _ in network_efficiency]
            }
        }, f, indent=2)
    
    print(f"\n💾 Detailed results saved to: {filename}")
    
    # Summary recommendations
    print(f"\n📋 **SUMMARY & RECOMMENDATIONS**")
    print("-" * 80)
    
    best_cpu = cpu_rankings[0][0]
    best_memory = memory_rankings[0][0]
    best_network = network_efficiency[0][0]
    
    print(f"🎯 **Best CPU Efficiency**: {best_cpu}")
    print(f"🎯 **Best Memory Efficiency**: {best_memory}")
    print(f"🎯 **Best Network Efficiency**: {best_network}")
    
    if best_cpu == best_memory == best_network:
        print(f"\n🏆 **OVERALL WINNER**: {best_cpu} - Best across all metrics!")
    else:
        print(f"\n⚖️  **TRADE-OFFS**: Different approaches excel in different areas")
        print(f"   - Choose {best_cpu} for CPU-constrained environments")
        print(f"   - Choose {best_memory} for memory-constrained environments")
        print(f"   - Choose {best_network} for maximum throughput efficiency")

if __name__ == "__main__":
    run_resource_tests() 