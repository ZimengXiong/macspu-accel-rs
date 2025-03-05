# macspu-accel-rs

Small Rust crate for the private macOS SPU accelerometer.

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
```

## Notes

This uses private Apple interfaces and may break across macOS updates.
