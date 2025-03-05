#!/usr/bin/env python3
"""
Example of using the GPU Temperature Monitor library from Python using ctypes.
This script needs to be run with sudo privileges.
"""

import ctypes
import sys
import time
from ctypes import c_int, c_uint, c_uint32, c_char, POINTER, Structure

# Check for root privileges
if os.geteuid() != 0:
    print("This script requires root privileges to access GPU registers.")
    print("Please run with sudo.")
    sys.exit(1)

# Define the structures to match the C library
class GpuTemperatures(Structure):
    _fields_ = [
        ("gpu_temp", c_uint32),
        ("junction_temp", c_uint32),
        ("vram_temp", c_uint32),
        ("error_message", c_char * 256)
    ]

# Load the shared library
try:
    gpu_lib = ctypes.CDLL("libgpumonitor.so")
except OSError as e:
    print(f"Error loading the library: {e}")
    print("Make sure you've installed the library correctly.")
    sys.exit(1)

# Define function prototypes
gpu_lib.gpu_monitor_init.restype = ctypes.c_void_p
gpu_lib.gpu_monitor_get_device_count.argtypes = [ctypes.c_void_p]
gpu_lib.gpu_monitor_get_device_count.restype = c_int
gpu_lib.gpu_monitor_get_temperatures.argtypes = [ctypes.c_void_p, c_int, POINTER(GpuTemperatures)]
gpu_lib.gpu_monitor_get_temperatures.restype = c_int
gpu_lib.gpu_monitor_cleanup.argtypes = [ctypes.c_void_p]

# Main function
def main():
    # Initialize the GPU monitoring
    ctx = gpu_lib.gpu_monitor_init()
    if not ctx:
        print("Failed to initialize GPU monitoring")
        return 1
    
    try:
        # Get the number of GPUs
        gpu_count = gpu_lib.gpu_monitor_get_device_count(ctx)
        print(f"Found {gpu_count} GPU(s)")
        
        if gpu_count <= 0:
            print("No GPUs detected")
            return 1
        
        # Monitor temperatures every second
        try:
            while True:
                for i in range(gpu_count):
                    temps = GpuTemperatures()
                    result = gpu_lib.gpu_monitor_get_temperatures(ctx, i, ctypes.byref(temps))
                    
                    if result == 0:
                        print(f"\nGPU {i}:")
                        print(f"  Core Temperature:     {temps.gpu_temp}°C")
                        print(f"  Junction Temperature: {temps.junction_temp}°C")
                        print(f"  VRAM Temperature:     {temps.vram_temp}°C")
                    else:
                        print(f"Failed to get temperatures for GPU {i}")
                
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nMonitoring stopped by user")
    
    finally:
        # Clean up
        gpu_lib.gpu_monitor_cleanup(ctx)
    
    return 0

if __name__ == "__main__":
    import os
    sys.exit(main())