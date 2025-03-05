# GPU Monitor
GPU VRAM, Core, Junction temperatures reader (GDDR6/GDDR6X) for NVIDIA GPUs

## Prerequisites
```bash
sudo apt update
sudo apt install build-essential gcc libpci-dev
```

## Build and Install
```bash
make
```

```bash
sudo make install
```

## Usage examples

#### Python
```bash
cd examples/python
sudo python3 main.py
```

#### Go
```bash
cd examples/go
go mod init gpu_monitor
go mod tidy
sudo ./gpu_monitor
```