#[cfg(target_os = "macos")]
mod sensor_spu;

#[cfg(target_os = "macos")]
pub use sensor_spu::{start_spu_sensor, SpuSample, SpuSensorRing};

#[cfg(not(target_os = "macos"))]
#[derive(Debug, Clone, Copy)]
pub struct SpuSample {
  pub x: f64,
  pub y: f64,
  pub z: f64,
}

#[cfg(not(target_os = "macos"))]
pub struct SpuSensorRing;

#[cfg(not(target_os = "macos"))]
pub fn start_spu_sensor() -> Result<SpuSensorRing, String> {
  Err("SPU sensor is only supported on macOS".to_string())
}
