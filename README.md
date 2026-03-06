# macspu-accel-rs

Small Rust crate for the private macOS SPU accelerometer and gyroscope.

## Requirements

- macOS
- Rust 1.77+

## Build

```bash
make check
make package
```

## Probe

```bash
make probe
```

## Usage

```rust
use macspu_accel_rs::start_spu_sensor;

let mut ring = start_spu_sensor()?;
if let Some(sample) = ring.read_new().last() {
    println!("{} {} {}", sample.x, sample.y, sample.z);
}
if let Some(sample) = ring.read_new_gyro().last() {
    println!("gyro {} {} {}", sample.x, sample.y, sample.z);
}
```

`read_new()` remains the accelerometer API for compatibility.
Use `read_new_gyro()` to consume the gyroscope stream.

## Notes

This uses private Apple interfaces and may break across macOS updates.
