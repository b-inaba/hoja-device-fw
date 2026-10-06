#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_build_dir=$(mktemp -d)
trap 'rm -rf "$test_build_dir"' EXIT
hoja_lib=library/HOJA-LIB-RP2040
"${CC:-cc}" -std=c11 -Wall -Wextra \
  -I rp2040/padbox_gs_c \
  -I "$hoja_lib/include" -I "$hoja_lib/include/hal/rp2040" \
  -I "$hoja_lib/external/NS-LIB-HID/include" \
  -I "$hoja_lib/external/SINPUT-LIB-HID/include" \
  -I "$hoja_lib/external/HOJA-LIB-DONGLE/include" \
  tests/padbox_boot_test.c "$hoja_lib/src/utilities/boot.c" \
  -o "$test_build_dir/padbox-boot-test"
"$test_build_dir/padbox-boot-test"

"${CC:-cc}" -std=c11 -Wall -Wextra \
  -I tests/stubs -I rp2040/padbox_gs_c \
  -I "$hoja_lib/include" -I "$hoja_lib/include/hal/rp2040" \
  -I "$hoja_lib/external/NS-LIB-HID/include" \
  -I "$hoja_lib/external/SINPUT-LIB-HID/include" \
  -I "$hoja_lib/external/HOJA-LIB-DONGLE/include" \
  -I "$hoja_lib/external/HHL-TINYUSB-DRIVERS/include" \
  tests/padbox_transport_test.c -o "$test_build_dir/padbox-transport-test"
"$test_build_dir/padbox-transport-test"
