fn main() {
  println!("cargo:rerun-if-changed=build.rs");
  println!("cargo:rerun-if-changed=src/iokit.c");

  #[cfg(target_os = "macos")]
  {
    cc::Build::new().file("src/iokit.c").compile("macspu_iokit");
    println!("cargo:rustc-link-lib=framework=IOKit");
    println!("cargo:rustc-link-lib=framework=CoreFoundation");
  }
}
