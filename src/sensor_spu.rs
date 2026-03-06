#[cfg(target_os = "macos")]
use std::{
  sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
  },
  thread::{self, JoinHandle},
};

const RING_CAP: usize = 8000;
const RING_ENTRY: usize = 12;
const RING_HEADER: usize = 16;
const ACCEL_SCALE: f64 = 65536.0;

#[cfg(target_os = "macos")]
unsafe extern "C" {
  fn iokit_sensor_init() -> i32;
  fn iokit_sensor_run();
  fn iokit_sensor_stop();
  fn iokit_ring_ptr() -> *const u8;
  fn iokit_gyro_ring_ptr() -> *const u8;
}

#[derive(Debug, Clone, Copy)]
pub struct SpuSample {
  pub x: f64,
  pub y: f64,
  pub z: f64,
}

pub struct SpuSensorRing {
  accel_ring_ptr: *const u8,
  gyro_ring_ptr: *const u8,
  last_accel_total: u64,
  last_gyro_total: u64,
  running: Arc<AtomicBool>,
  worker: Option<JoinHandle<()>>,
}

unsafe impl Send for SpuSensorRing {}
unsafe impl Sync for SpuSensorRing {}

impl SpuSensorRing {
  pub fn read_new(&mut self) -> Vec<SpuSample> {
    Self::read_ring(self.accel_ring_ptr, &mut self.last_accel_total)
  }

  pub fn read_new_gyro(&mut self) -> Vec<SpuSample> {
    Self::read_ring(self.gyro_ring_ptr, &mut self.last_gyro_total)
  }

  fn read_ring(ring: *const u8, last_total: &mut u64) -> Vec<SpuSample> {
    if ring.is_null() {
      return Vec::new();
    }
    unsafe {
      for _ in 0..3 {
        let total_before = u64::from_le_bytes(std::slice::from_raw_parts(ring.add(4), 8).try_into().unwrap());
        let n_new = (total_before as i64 - *last_total as i64).max(0) as usize;
        if n_new == 0 {
          return Vec::new();
        }
        let n_new = n_new.min(RING_CAP);
        let idx = u32::from_le_bytes(std::slice::from_raw_parts(ring, 4).try_into().unwrap()) as usize;
        let start = (idx as isize - n_new as isize).rem_euclid(RING_CAP as isize) as usize;
        let mut samples = Vec::with_capacity(n_new);

        for i in 0..n_new {
          let pos = (start + i) % RING_CAP;
          let off = RING_HEADER + pos * RING_ENTRY;
          let x = i32::from_le_bytes(std::slice::from_raw_parts(ring.add(off), 4).try_into().unwrap());
          let y = i32::from_le_bytes(std::slice::from_raw_parts(ring.add(off + 4), 4).try_into().unwrap());
          let z = i32::from_le_bytes(std::slice::from_raw_parts(ring.add(off + 8), 4).try_into().unwrap());
          samples.push(SpuSample {
            x: x as f64 / ACCEL_SCALE,
            y: y as f64 / ACCEL_SCALE,
            z: z as f64 / ACCEL_SCALE,
          });
        }

        let total_after = u64::from_le_bytes(std::slice::from_raw_parts(ring.add(4), 8).try_into().unwrap());
        if total_after == total_before {
          *last_total = total_before;
          return samples;
        }
      }
    }

    Vec::new()
  }

  pub fn is_running(&self) -> bool {
    self.running.load(Ordering::Relaxed)
  }

  pub fn stop(&mut self) {
    unsafe {
      iokit_sensor_stop();
    }
    if let Some(worker) = self.worker.take() {
      let _ = worker.join();
    }
    self.running.store(false, Ordering::Relaxed);
  }
}

impl Drop for SpuSensorRing {
  fn drop(&mut self) {
    self.stop();
  }
}

pub fn start_spu_sensor() -> Result<SpuSensorRing, String> {
  unsafe {
    let ret = iokit_sensor_init();
    if ret != 0 {
      return Err("failed to initialize SPU IOKit sensor".to_string());
    }
  }

  let running = Arc::new(AtomicBool::new(true));
  let running_clone = Arc::clone(&running);

  let worker = thread::Builder::new()
    .name("tiltball-spu-sensor".to_string())
    .spawn(move || {
      unsafe {
        iokit_sensor_run();
      }
      running_clone.store(false, Ordering::Relaxed);
    })
    .map_err(|err| format!("failed to spawn SPU sensor thread: {err}"))?;

  let ring_ptr = unsafe { iokit_ring_ptr() };
  let gyro_ring_ptr = unsafe { iokit_gyro_ring_ptr() };
  if ring_ptr.is_null() || gyro_ring_ptr.is_null() {
    return Err("SPU sensor ring pointer unavailable".to_string());
  }

  Ok(SpuSensorRing {
    accel_ring_ptr: ring_ptr,
    gyro_ring_ptr,
    last_accel_total: 0,
    last_gyro_total: 0,
    running,
    worker: Some(worker),
  })
}
