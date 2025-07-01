#!/usr/bin/env python3
"""
System Resource Monitor for MQTT Performance Testing
Monitors CPU and memory usage during performance tests
"""

import psutil
import time
import json
import argparse
import sys
from datetime import datetime
import threading
import signal

class SystemMonitor:
    def __init__(self, output_file="system_stats.json", interval=1.0):
        self.output_file = output_file
        self.interval = interval
        self.running = False
        self.stats = []
        self.process_name = "mqtt_performance_test"
        
    def start_monitoring(self):
        """Start monitoring system resources"""
        self.running = True
        print(f"🔍 Starting system monitoring (interval: {self.interval}s)")
        print(f"📊 Output file: {self.output_file}")
        
        # Set up signal handler for graceful shutdown
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
        
        try:
            while self.running:
                stat = self.collect_stats()
                self.stats.append(stat)
                
                # Print current stats
                print(f"📈 CPU: {stat['cpu_percent']:.1f}% | "
                      f"Memory: {stat['memory_mb']:.1f}MB | "
                      f"Process CPU: {stat['process_cpu_percent']:.1f}% | "
                      f"Process Memory: {stat['process_memory_mb']:.1f}MB")
                
                time.sleep(self.interval)
                
        except KeyboardInterrupt:
            print("\n🛑 Monitoring stopped by user")
        finally:
            self.save_results()
    
    def collect_stats(self):
        """Collect current system statistics"""
        timestamp = datetime.now().isoformat()
        
        # System-wide stats
        cpu_percent = psutil.cpu_percent(interval=0.1)
        memory = psutil.virtual_memory()
        memory_mb = memory.used / 1024 / 1024
        
        # Process-specific stats
        process_cpu_percent = 0.0
        process_memory_mb = 0.0
        
        # Find the MQTT performance test process
        for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
            try:
                if proc.info['name'] and self.process_name in proc.info['name']:
                    process = psutil.Process(proc.info['pid'])
                    process_cpu_percent = process.cpu_percent()
                    process_memory_mb = process.memory_info().rss / 1024 / 1024
                    break
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                continue
        
        return {
            'timestamp': timestamp,
            'cpu_percent': cpu_percent,
            'memory_mb': memory_mb,
            'memory_percent': memory.percent,
            'process_cpu_percent': process_cpu_percent,
            'process_memory_mb': process_memory_mb,
            'disk_io': self.get_disk_io(),
            'network_io': self.get_network_io()
        }
    
    def get_disk_io(self):
        """Get disk I/O statistics"""
        try:
            disk_io = psutil.disk_io_counters()
            return {
                'read_bytes': disk_io.read_bytes,
                'write_bytes': disk_io.write_bytes,
                'read_count': disk_io.read_count,
                'write_count': disk_io.write_count
            }
        except:
            return {}
    
    def get_network_io(self):
        """Get network I/O statistics"""
        try:
            net_io = psutil.net_io_counters()
            return {
                'bytes_sent': net_io.bytes_sent,
                'bytes_recv': net_io.bytes_recv,
                'packets_sent': net_io.packets_sent,
                'packets_recv': net_io.packets_recv
            }
        except:
            return {}
    
    def signal_handler(self, signum, frame):
        """Handle shutdown signals"""
        print(f"\n🛑 Received signal {signum}, stopping monitoring...")
        self.running = False
    
    def save_results(self):
        """Save monitoring results to file"""
        if not self.stats:
            print("⚠️ No statistics collected")
            return
        
        # Calculate summary statistics
        cpu_values = [s['cpu_percent'] for s in self.stats]
        memory_values = [s['memory_mb'] for s in self.stats]
        process_cpu_values = [s['process_cpu_percent'] for s in self.stats]
        process_memory_values = [s['process_memory_mb'] for s in self.stats]
        
        summary = {
            'monitoring_duration_seconds': len(self.stats) * self.interval,
            'samples_collected': len(self.stats),
            'cpu_summary': {
                'average': sum(cpu_values) / len(cpu_values),
                'maximum': max(cpu_values),
                'minimum': min(cpu_values)
            },
            'memory_summary': {
                'average_mb': sum(memory_values) / len(memory_values),
                'maximum_mb': max(memory_values),
                'minimum_mb': min(memory_values)
            },
            'process_cpu_summary': {
                'average': sum(process_cpu_values) / len(process_cpu_values),
                'maximum': max(process_cpu_values),
                'minimum': min(process_cpu_values)
            },
            'process_memory_summary': {
                'average_mb': sum(process_memory_values) / len(process_memory_values),
                'maximum_mb': max(process_memory_values),
                'minimum_mb': min(process_memory_values)
            }
        }
        
        # Save detailed data
        output_data = {
            'summary': summary,
            'detailed_stats': self.stats
        }
        
        with open(self.output_file, 'w') as f:
            json.dump(output_data, f, indent=2)
        
        print(f"💾 Results saved to {self.output_file}")
        print("\n📊 Summary:")
        print(f"   Monitoring duration: {summary['monitoring_duration_seconds']:.1f}s")
        print(f"   Samples collected: {summary['samples_collected']}")
        print(f"   CPU usage - Avg: {summary['cpu_summary']['average']:.1f}%, Max: {summary['cpu_summary']['maximum']:.1f}%")
        print(f"   Memory usage - Avg: {summary['memory_summary']['average_mb']:.1f}MB, Max: {summary['memory_summary']['maximum_mb']:.1f}MB")
        print(f"   Process CPU - Avg: {summary['process_cpu_summary']['average']:.1f}%, Max: {summary['process_cpu_summary']['maximum']:.1f}%")
        print(f"   Process Memory - Avg: {summary['process_memory_summary']['average_mb']:.1f}MB, Max: {summary['process_memory_summary']['maximum_mb']:.1f}MB")

def main():
    parser = argparse.ArgumentParser(description='System Resource Monitor for MQTT Performance Testing')
    parser.add_argument('--output', '-o', default='system_stats.json', 
                       help='Output file for statistics (default: system_stats.json)')
    parser.add_argument('--interval', '-i', type=float, default=1.0,
                       help='Monitoring interval in seconds (default: 1.0)')
    parser.add_argument('--process', '-p', default='mqtt_performance_test',
                       help='Process name to monitor (default: mqtt_performance_test)')
    
    args = parser.parse_args()
    
    # Check if psutil is available
    try:
        import psutil
    except ImportError:
        print("❌ psutil library not found. Install with: pip install psutil")
        sys.exit(1)
    
    monitor = SystemMonitor(args.output, args.interval)
    monitor.process_name = args.process
    
    print("🚀 MQTT Performance Test - System Monitor")
    print("==========================================")
    print(f"Process to monitor: {monitor.process_name}")
    print(f"Monitoring interval: {args.interval}s")
    print(f"Output file: {args.output}")
    print("Press Ctrl+C to stop monitoring")
    print()
    
    monitor.start_monitoring()

if __name__ == "__main__":
    main() 