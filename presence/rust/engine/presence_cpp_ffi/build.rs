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
use std::env;
use std::fs::create_dir;
use std::path::PathBuf;

fn main() {
    // println!("cargo:rerun-if-changed=src/lib.rs");
    // println!("cargo:rerun-if-changed=../presence_core/src/lib.rs");

    let crate_dir = env::var("CARGO_MANIFEST_DIR").unwrap();

    // Generates bindings for Rust Engine to access C++ system APIs.
    let lib_dir_path = PathBuf::from(crate_dir.as_str())
        .join("cpp")
        .join("ffi.hpp")
        .canonicalize()
        .expect(&*format!(
            "cannot canonicalize path: {}/cpp/ffi.hpp",
            crate_dir.as_str()
        ));
    let headers_path_str = lib_dir_path.to_str().expect("Path not valid");

    let bindings = bindgen::Builder::default()
        .header(headers_path_str)
        .generate()
        .expect("Unable to generate bindings");

    // Write the bindings to the $OUT_DIR/bindings.rs file.
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap()).join("cpp_ffi.rs");
    bindings
        .write_to_file(out_path)
        .expect("Couldn't write bindings!");

    let link_lib_dir = env::var("CARGO_TARGET_DIR").unwrap();
    println!("cargo:rustc-link-search={}/cpp", link_lib_dir);
    println!("cargo:rustc-link-lib=static=cpp_ffi");

    // Generates C header to access the Rust Engine.
    cbindgen::generate(&crate_dir)
        .unwrap()
        .write_to_file("presence.h");

}
