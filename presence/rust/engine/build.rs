extern crate cbindgen;

use std::env;

fn main() {
    // println!("cargo:rerun-if-changed=src/ffi.rs");

    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

    cbindgen::generate(&crate_dir)
        .unwrap()
        .write_to_file("engine.h");
}