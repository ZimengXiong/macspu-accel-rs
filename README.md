# macspu-accel-rs

Rust crate exposing private macOS SPU accelerometer access through
`AppleSPUHIDDevice`.

## Warning

This uses private Apple interfaces and is not App Store safe.

## Build & Package

```bash
make check
make package
# cargo publish
```

Packaged crate output:

- `dist/macspu-accel-rs-<version>.crate`

Standalone sensor probe:

```bash
make probe
# same as: cargo run --example probe
```

## Usage

```rust
use macspu_accel_rs::start_spu_sensor;

let mut ring = start_spu_sensor()?;
if let Some(sample) = ring.read_new().last() {
    println!("{} {} {}", sample.x, sample.y, sample.z);
}
```
