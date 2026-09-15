// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
use cmake;
use std::env;
use std::path::PathBuf;

fn main() {
    // println!("cargo:rerun-if-changed=src/lib.rs");
    // println!("cargo:rerun-if-changed=../presence_core/src/lib.rs");
    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    // Directory contains C library source codes.
    let c_input_dir_path = PathBuf::from(crate_dir.as_str())
        .join("cpp")
        .canonicalize()
        .expect(&*format!(
            "cannot canonicalize path: {}/cpp",
            crate_dir.as_str()
        ));

    // ========== FFI C to Rust APIs for Rust codes to construct C structures. ==========
    // The cbindgen below reads rust_to_c.hpp and generates rust_to_c_autogen.rs.
    let c_input_header_path = c_input_dir_path
        .join("rust_to_c.hpp")
        .canonicalize()
        .expect(&*format!(
            "cannot canonicalize path: {}/cpp/rust_to_c.hpp",
            crate_dir.as_str()
        ));

    let c_input_header_path_str = c_input_header_path
        .to_str()
        .expect("C header Path not valid.");
    let rust_output_path = PathBuf::from(env::var("OUT_DIR").unwrap()).join("rust_to_c_autogen.rs");

    let bindings = bindgen::Builder::default()
        .header(c_input_header_path_str)
        .generate()
        .expect("Unable to generate rust_to_c.rs");

    bindings
        .write_to_file(rust_output_path.clone())
        .expect(&*format!(
            "Failed to write to {}",
            rust_output_path.to_str().unwrap()
        ));

    // ========== FFI Rust to C APIs for Presence Engine C/C++ clients. ==========
    cbindgen::generate(&crate_dir)
        .unwrap()
        .write_to_file("presence.h");

    // ============ Build C library cpp_ffi.a =================
    // The library will be installed under OUT_DIR.
    let c_input_dir_path_str = c_input_dir_path.to_str().expect("C input directory not valid.");

    let rust_to_c = cmake::build(c_input_dir_path_str);

    // Search and link cpp_ffi.a
    println!("cargo:rustc-link-search=native={}/lib", rust_to_c.display());
    println!("cargo:rustc-link-lib=static=rust_to_c");
}
