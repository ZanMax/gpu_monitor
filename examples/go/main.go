package main

/*
#cgo LDFLAGS: -lgpumonitor
#include <stdlib.h>

typedef struct {
    void* ctx;
} GpuContext;

typedef struct {
    unsigned int gpu_temp;
    unsigned int junction_temp;
    unsigned int vram_temp;
    char error_message[256];
} GpuTemperatures;

// Function declarations that match the C library
extern GpuContext* gpu_monitor_init(void);
extern void gpu_monitor_cleanup(GpuContext* ctx);
extern int gpu_monitor_get_device_count(GpuContext* ctx);
extern int gpu_monitor_get_temperatures(GpuContext* ctx, int device_index, GpuTemperatures* temps);
*/
import "C"
import (
	"fmt"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func main() {
	// Check if running as root
	if os.Geteuid() != 0 {
		fmt.Println("This program requires root privileges to access GPU registers.")
		fmt.Println("Please run with sudo.")
		os.Exit(1)
	}

	// Initialize GPU monitoring
	ctx := C.gpu_monitor_init()
	if ctx == nil {
		fmt.Println("Failed to initialize GPU monitoring")
		os.Exit(1)
	}

	// Ensure cleanup on exit
	defer C.gpu_monitor_cleanup(ctx)

	// Get the number of available GPUs
	gpuCount := int(C.gpu_monitor_get_device_count(ctx))
	fmt.Printf("Found %d GPU(s)\n", gpuCount)

	if gpuCount <= 0 {
		fmt.Println("No GPUs detected")
		os.Exit(1)
	}

	// Set up signal handling for graceful exit
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Create a done channel for stopping the monitor
	done := make(chan bool)

	// Start monitoring in a goroutine
	go func() {
		for {
			select {
			case <-done:
				return
			default:
				// Create a temperatures struct for each GPU
				for i := 0; i < gpuCount; i++ {
					var temps C.GpuTemperatures
					result := C.gpu_monitor_get_temperatures(ctx, C.int(i), &temps)

					if result == 0 {
						fmt.Printf("\nGPU %d:\n", i)
						fmt.Printf("  Core Temperature:     %d°C\n", temps.gpu_temp)
						fmt.Printf("  Junction Temperature: %d°C\n", temps.junction_temp)
						fmt.Printf("  VRAM Temperature:     %d°C\n", temps.vram_temp)
					} else {
						fmt.Printf("Failed to get temperatures for GPU %d\n", i)
					}
				}
			}
			time.Sleep(1 * time.Second)
		}
	}()

	// Wait for interrupt signal
	<-sigChan
	fmt.Println("\nShutting down...")

	// Signal the monitoring goroutine to stop
	done <- true
}
