#!/usr/bin/env bash
set -o errexit  # Exit the script with error if any of the commands fail

# Supported/used environment variables:
#   LINK_STATIC              Whether to statically link to libbson
#   BUILD_SAMPLE_WITH_CMAKE  Link program w/ CMake. Default: use pkg-config.
#   BUILD_SAMPLE_WITH_CMAKE_DEPRECATED  If BUILD_SAMPLE_WITH_CMAKE is set, then use deprecated CMake scripts instead.


echo "LINK_STATIC=$LINK_STATIC BUILD_SAMPLE_WITH_CMAKE=$BUILD_SAMPLE_WITH_CMAKE BUILD_SAMPLE_WITH_CMAKE_DEPRECATED=$BUILD_SAMPLE_WITH_CMAKE_DEPRECATED"

. "$(dirname "${BASH_SOURCE[0]}")/use-tools.sh" base paths platform

build_sample_with_cmake=$(get-bool BUILD_SAMPLE_WITH_CMAKE)
build_sample_with_cmake_deprecated=$(get-bool BUILD_SAMPLE_WITH_CMAKE_DEPRECATED)
link_static=$(get-bool LINK_STATIC)

. "$EVG_SCRIPTS/find-cmake-latest.sh"
CMAKE=$(find_cmake_latest)
. "$EVG_SCRIPTS/check-symlink.sh"


if $IS_WINDOWS; then
  fail "This script doesn't work on Windows"
elif $IS_DARWIN; then
  lib_so=libbson-1.0.0.dylib
  ldd="otool -L"
else
  lib_so=libbson-1.0.so.0
  ldd=ldd
fi

scratch_root=$MONGOC_DIR/_build/scratch/link-sample-program
build_dir=$scratch_root/build
install_dir=$scratch_root/install
example_build_dir=$scratch_root/example-build
rm -rf -- "$example_build_dir" "$install_dir"

configure_options=(
  -S "$MONGOC_DIR"
  -B "$build_dir"
  -D CMAKE_INSTALL_PREFIX="$install_dir"
  -D ENABLE_TESTS=OFF
)

if $link_static; then
  configure_options+=(-D ENABLE_STATIC=ON)
else
  configure_options+=(-D ENABLE_STATIC=OFF)
fi

$CMAKE "${configure_options[@]}"
$CMAKE --build "$build_dir" --parallel
$CMAKE --build "$build_dir" --parallel --target install

expect_present=(
  lib/pkgconfig/libbson-1.0.pc
  lib/cmake/bson-1.0/bson-1.0-config.cmake
  lib/cmake/bson-1.0/bson-1.0-config-version.cmake
  lib/cmake/bson-1.0/bson-targets.cmake
)
expect_absent=()
install_ok=true

if ! $IS_DARWIN; then
  # Check on Linux that libbson is installed into lib/ like:
  # libbson-1.0.so -> libbson-1.0.so.0
  # libbson-1.0.so.0 -> libbson-1.0.so.0.0.0
  # libbson-1.0.so.0.0.0
  # (From check-symlink.sh)
  check-symlink-eq "$install_dir/lib/libbson-1.0.so" libbson-1.0.so.0
  check-symlink-eq "$install_dir/lib/libbson-1.0.so.0" libbson-1.0.so.0.0.0
  got_soname=$(objdump -p "$install_dir/lib/$lib_so" | grep SONAME |awk '{print $2}')
  EXPECTED_SONAME="libbson-1.0.so.0"
  if [ "$got_soname" != "$EXPECTED_SONAME" ]; then
    echo "SONAME should be $EXPECTED_SONAME, but got ‘$got_soname’"
    exit 1
  else
    echo "library name check ok, SONAME=$got_soname"
  fi
else
  # Just test that the shared lib was installed.
  expect_present+=("lib/$lib_so")
fi

static_files=(
  lib/libbson-static-1.0.a
  lib/pkgconfig/libbson-static-1.0.pc
  lib/libbson-static-1.0.a
)
if $link_static; then
  expect_present+=("${static_files[@]}")
else
  expect_absent+=("${static_files[@]}")
fi

# Check files that we want to exist:
for exp in "${expect_present[@]}"; do
  debug "Check for file present: ‘$exp’"
  abs=$install_dir/$exp
  if ! is-file "$abs"; then
    log " ⛔ File ‘$exp’ was not installed (Expected [$abs])"
    install_ok=false
  fi
done

# Check files that we want to *not* exist:
for absent in "${expect_absent[@]+${expect_absent[@]}}"; do
  debug "Check for file absent: ‘$exp’"
  abs=$install_dir/$absent
  if exists "$abs"; then
    log " ⛔ File ‘$exp’ is present, but should not be (Found [$abs])"
    install_ok=false
  fi
done

$install_ok || fail "The installation is invalid (See above)"

if $build_sample_with_cmake; then
  # Test our CMake package config file with CMake's find_package command.
  if $build_sample_with_cmake_deprecated; then
    example_dir=$MONGOC_DIR/src/libbson/examples/cmake-deprecated/find_package
  else
    example_dir=$MONGOC_DIR/src/libbson/examples/cmake/find_package
  fi

  if $link_static; then
    example_dir="${example_dir}_static"
  fi

  $CMAKE "-DCMAKE_PREFIX_PATH=$install_dir/lib/cmake" -S "$example_dir" -B "$example_build_dir"
  $CMAKE --build "$example_build_dir"
else
  # Test our pkg-config file.
  export PKG_CONFIG_PATH=$install_dir/lib/pkgconfig
  cd $MONGOC_DIR/src/libbson/examples
  example_build_dir=$PWD

  if $link_static; then
    echo "pkg-config output:"
    pkg-config --libs --cflags libbson-static-1.0
    sh compile-with-pkg-config-static.sh
  else
    echo "pkg-config output:"
    pkg-config --libs --cflags libbson-1.0
    sh compile-with-pkg-config.sh
  fi
fi

if ! $link_static; then
  export DYLD_LIBRARY_PATH=$install_dir/lib
  export LD_LIBRARY_PATH=$install_dir/lib
fi

echo "ldd hello_bson:"
$ldd "$example_build_dir/hello_bson"

"$example_build_dir/hello_bson"
