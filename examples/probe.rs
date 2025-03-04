use std::{thread, time::Duration};

use macspu_accel_rs::start_spu_sensor;

fn main() -> Result<(), Box<dyn std::error::Error>> {
  let mut ring = start_spu_sensor().map_err(|err| format!("failed to start SPU sensor: {err}"))?;
  println!("provider=spu-hid samples=10");

  for idx in 0..10 {
    let mut printed = false;
    for _ in 0..20 {
      if let Some(sample) = ring.read_new().last() {
        println!(
          "sample_{} x={:.6} y={:.6} z={:.6}",
          idx + 1,
          sample.x,
          sample.y,
          sample.z
        );
        printed = true;
        break;
      }
      thread::sleep(Duration::from_millis(25));
    }
    if !printed {
      println!("sample_{} unavailable", idx + 1);
    }
    thread::sleep(Duration::from_millis(100));
  }

  ring.stop();
  Ok(())
}
