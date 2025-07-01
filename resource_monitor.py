#!/usr/bin/env python3
"""
Resource Monitor for IoT Thermal Monitoring Performance Tests
Monitors CPU, Memory, Network, and Process statistics during tests
"""

import psutil
import time
import json
import sys
import os
from datetime import datetime
import threading
import subprocess

class ResourceMonitor:
    def __init__(self, test_name="unknown", duration=20):
        self.test_name = test_name
        self.duration = duration
        self.monitoring = False
        self.stats = {
            'test_name': test_name,
            'start_time': None,
            'end_time': None,
            'duration': duration,
            'cpu_samples': [],
            'memory_samples': [],
            'network_samples': [],
            'process_samples': []
        }
        
        # Get initial network stats
        self.initial_network = psutil.net_io_counters()
        
    def start_monitoring(self):
        """Start resource monitoring"""
        self.monitoring = True
        self.stats['start_time'] = datetime.now().isoformat()
        
        # Start monitoring threads
        cpu_thread = threading.Thread(target=self._monitor_cpu)
        memory_thread = threading.Thread(target=self._monitor_memory)
        network_thread = threading.Thread(target=self._monitor_network)
        process_thread = threading.Thread(target=self._monitor_process)
        
        cpu_thread.daemon = True
        memory_thread.daemon = True
        network_thread.daemon = True
        process_thread.daemon = True
        
        cpu_thread.start()
        memory_thread.start()
        network_thread.start()
        process_thread.start()
        
        print(f"🔍 Resource monitoring started for {self.test_name}")
        
    def stop_monitoring(self):
        """Stop resource monitoring"""
        self.monitoring = False
        self.stats['end_time'] = datetime.now().isoformat()
        
    def _monitor_cpu(self):
        """Monitor CPU usage"""
        while self.monitoring:
            try:
                cpu_percent = psutil.cpu_percent(interval=1)
                self.stats['cpu_samples'].append({
                    'timestamp': time.time(),
                    'cpu_percent': cpu_percent
                })
            except Exception as e:
                print(f"CPU monitoring error: {e}")
                
    def _monitor_memory(self):
        """Monitor memory usage"""
        while self.monitoring:
            try:
                memory = psutil.virtual_memory()
                self.stats['memory_samples'].append({
                    'timestamp': time.time(),
                    'total_mb': memory.total / (1024 * 1024),
                    'available_mb': memory.available / (1024 * 1024),
                    'used_mb': memory.used / (1024 * 1024),
                    'percent': memory.percent
                })
            except Exception as e:
                print(f"Memory monitoring error: {e}")
            time.sleep(1)
                
    def _monitor_network(self):
        """Monitor network I/O"""
        while self.monitoring:
            try:
                network = psutil.net_io_counters()
                self.stats['network_samples'].append({
                    'timestamp': time.time(),
                    'bytes_sent': network.bytes_sent,
                    'bytes_recv': network.bytes_recv,
                    'packets_sent': network.packets_sent,
                    'packets_recv': network.packets_recv
                })
            except Exception as e:
                print(f"Network monitoring error: {e}")
            time.sleep(1)
                
    def _monitor_process(self):
        """Monitor current process statistics"""
        current_pid = os.getpid()
        while self.monitoring:
            try:
                process = psutil.Process(current_pid)
                self.stats['process_samples'].append({
                    'timestamp': time.time(),
                    'cpu_percent': process.cpu_percent(),
                    'memory_mb': process.memory_info().rss / (1024 * 1024),
                    'num_threads': process.num_threads(),
                    'num_fds': process.num_fds() if hasattr(process, 'num_fds') else 0
                })
            except Exception as e:
                print(f"Process monitoring error: {e}")
            time.sleep(1)
    
    def get_summary(self):
        """Get resource usage summary"""
        if not self.stats['cpu_samples']:
            return None
            
        # CPU statistics
        cpu_values = [s['cpu_percent'] for s in self.stats['cpu_samples']]
        cpu_avg = sum(cpu_values) / len(cpu_values)
        cpu_max = max(cpu_values)
        cpu_min = min(cpu_values)
        
        # Memory statistics
        memory_values = [s['used_mb'] for s in self.stats['memory_samples']]
        memory_avg = sum(memory_values) / len(memory_values)
        memory_max = max(memory_values)
        memory_min = min(memory_values)
        
        # Network statistics
        if len(self.stats['network_samples']) >= 2:
            initial = self.stats['network_samples'][0]
            final = self.stats['network_samples'][-1]
            bytes_sent = final['bytes_sent'] - initial['bytes_sent']
            bytes_recv = final['bytes_recv'] - initial['bytes_recv']
        else:
            bytes_sent = bytes_recv = 0
            
        # Process statistics
        process_values = [s['memory_mb'] for s in self.stats['process_samples']]
        process_memory_avg = sum(process_values) / len(process_values) if process_values else 0
        
        return {
            'test_name': self.test_name,
            'duration_seconds': self.duration,
            'cpu': {
                'average_percent': round(cpu_avg, 2),
                'max_percent': round(cpu_max, 2),
                'min_percent': round(cpu_min, 2)
            },
            'memory': {
                'average_mb': round(memory_avg, 2),
                'max_mb': round(memory_max, 2),
                'min_mb': round(memory_min, 2)
            },
            'network': {
                'bytes_sent': bytes_sent,
                'bytes_received': bytes_recv,
                'bytes_sent_mb': round(bytes_sent / (1024 * 1024), 2),
                'bytes_received_mb': round(bytes_recv / (1024 * 1024), 2)
            },
            'process': {
                'average_memory_mb': round(process_memory_avg, 2)
            }
        }
    
    def print_summary(self):
        """Print resource usage summary"""
        summary = self.get_summary()
        if not summary:
            print("❌ No resource data collected")
            return
            
        print(f"\n📊 Resource Usage Summary: {summary['test_name']}")
        print("=" * 50)
        print(f"Duration: {summary['duration_seconds']} seconds")
        print(f"\n🖥️  CPU Usage:")
        print(f"  Average: {summary['cpu']['average_percent']}%")
        print(f"  Maximum: {summary['cpu']['max_percent']}%")
        print(f"  Minimum: {summary['cpu']['min_percent']}%")
        
        print(f"\n💾 Memory Usage:")
        print(f"  Average: {summary['memory']['average_mb']} MB")
        print(f"  Maximum: {summary['memory']['max_mb']} MB")
        print(f"  Minimum: {summary['memory']['min_mb']} MB")
        
        print(f"\n🌐 Network I/O:")
        print(f"  Sent: {summary['network']['bytes_sent_mb']} MB")
        print(f"  Received: {summary['network']['bytes_received_mb']} MB")
        
        print(f"\n⚙️  Process Memory:")
        print(f"  Average: {summary['process']['average_memory_mb']} MB")
        
    def save_results(self, filename=None):
        """Save detailed results to JSON file"""
        if not filename:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"resource_usage_{self.test_name}_{timestamp}.json"
            
        with open(filename, 'w') as f:
            json.dump(self.stats, f, indent=2)
        print(f"💾 Resource data saved to: {filename}")

def monitor_test(test_name, command, duration=20):
    """Monitor a specific test command"""
    print(f"🚀 Starting resource monitoring for: {test_name}")
    print(f"Command: {command}")
    print(f"Duration: {duration} seconds")
    
    # Start resource monitor
    monitor = ResourceMonitor(test_name, duration)
    monitor.start_monitoring()
    
    # Start the test command
    try:
        process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # Wait for the specified duration
        time.sleep(duration)
        
        # Stop monitoring
        monitor.stop_monitoring()
        
        # Terminate the process if still running
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=5)
            
        # Get and print results
        monitor.print_summary()
        monitor.save_results()
        
        return monitor.get_summary()
        
    except Exception as e:
        print(f"❌ Error running test: {e}")
        monitor.stop_monitoring()
        return None

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 resource_monitor.py <test_name> <command> [duration]")
        print("Example: python3 resource_monitor.py 'JS Bridge' 'node performance_test.js' 20")
        sys.exit(1)
        
    test_name = sys.argv[1]
    command = sys.argv[2]
    duration = int(sys.argv[3]) if len(sys.argv) > 3 else 20
    
    monitor_test(test_name, command, duration) 