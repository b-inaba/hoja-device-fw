#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

// Test the actual USB HAL policy, mocking only the USB/PIO hardware boundary.
#include "../library/HOJA-LIB-RP2040/src/hal/rp2040/usb_hal.c"

static boot_info_s boot_info;
static gamepadConfig_s gamepad;
gamepadConfig_s *gamepad_config = &gamepad;
static bool usb_connected, gc_detected, start_ok, probe_ok;
static unsigned probe_starts, probe_stops, usb_stops;
static boot_memory_s next_boot;
static jmp_buf reboot_jump;
static const hoja_config_s config = {0};
static const core_hid_device_t hid = {0};

const hoja_config_s *hoja_config_get(void) { return &config; }
const boot_info_s *boot_get_info(void) { return &boot_info; }
void boot_set_memory(boot_memory_s *in) { next_boot = *in; }
void sys_hal_reboot(void) { longjmp(reboot_jump, 1); }
void sys_hal_sleep_ms(uint32_t ms) { (void)ms; }
bool tud_connected(void) { return usb_connected; }
bool joybus_gc_hal_probe_start(void) { ++probe_starts; return probe_ok; }
bool joybus_gc_hal_probe_detected(void) { return gc_detected; }
void joybus_gc_hal_probe_stop(void) { ++probe_stops; }
void hhl_tusb_init(const hhl_tusb_config_s *cfg) { (void)cfg; }
bool hhl_tusb_start(void) { return start_ok; }
void hhl_tusb_stop(void) { ++usb_stops; }
void hhl_tusb_task(void) {}
bool hhl_tusb_report_ready(void) { return false; }
bool hhl_tusb_report_send(uint8_t id, const void *data, uint16_t len)
{ (void)id; (void)data; (void)len; return false; }
void tasks_mark_sent_isr(void) {}
void tasks_mark_sent(void) {}
bool tasks_get_required_done(void) { return true; }
bool core_get_generated_report(core_report_s *out) { (void)out; return false; }
bool webusb_outputting_check(void) { return false; }
void webusb_send_rawinput(uint64_t timestamp) { (void)timestamp; }
void webusb_command_handler(uint8_t *data, uint32_t size) { (void)data; (void)size; }

static core_params_s params;
static void reset(bool automatic, core_reportformat_t format)
{
    boot_info = (boot_info_s){.auto_gamecube_usb = automatic};
    params = (core_params_s){.core_report_format = format, .hid_device = &hid};
    usb_connected = gc_detected = false;
    start_ok = probe_ok = true;
    probe_starts = probe_stops = usb_stops = 0;
    next_boot = (boot_memory_s){0};
    _usb_gc_probe_active = false;
}

int main(void)
{
    // A passive USB power source must not close the detection window, even
    // after the console has been idle for longer than a boot-time timeout.
    reset(true, CORE_REPORTFORMAT_SLIPPI);
    assert(transport_usb_init(&params));
    assert(probe_starts == 1);
    for (unsigned i = 0; i < 10000; ++i)
        transport_usb_task((uint64_t)i * 1000);
    assert(probe_stops == 0);
    gc_detected = true;
    if (setjmp(reboot_jump) == 0)
    {
        transport_usb_task(10000001);
        assert(!"valid console traffic must reboot into native GameCube");
    }
    assert(next_boot.report_format == CORE_REPORTFORMAT_GAMECUBE);
    assert(next_boot.gamepad_method == GAMEPAD_METHOD_WIRED);
    assert(probe_stops == 1 && usb_stops == 1);

    // Receiving a USB setup request locks in USB, without waiting for a class
    // driver/mount. Later noise cannot change a running PC session.
    reset(true, CORE_REPORTFORMAT_SLIPPI);
    assert(transport_usb_init(&params));
    usb_connected = true;
    transport_usb_task(0);
    assert(probe_stops == 1 && !_usb_gc_probe_active);
    gc_detected = true;
    transport_usb_task(1);
    assert(next_boot.val == 0 && usb_stops == 0);
    transport_usb_stop();
    assert(probe_stops == 1);

    // Every explicit USB hold bypasses probing, including forced WUP-028.
    const core_reportformat_t formats[] = {
        CORE_REPORTFORMAT_SWPRO, CORE_REPORTFORMAT_XINPUT,
        CORE_REPORTFORMAT_SLIPPI, CORE_REPORTFORMAT_SINPUT,
    };
    for (unsigned i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i)
    {
        reset(false, formats[i]);
        assert(transport_usb_init(&params));
        assert(probe_starts == 0);
    }

    // Failed hardware init must not leave a phantom active listener.
    reset(true, CORE_REPORTFORMAT_SLIPPI);
    start_ok = false;
    assert(!transport_usb_init(&params));
    assert(probe_starts == 0);
    reset(true, CORE_REPORTFORMAT_SLIPPI);
    probe_ok = false;
    assert(transport_usb_init(&params));
    assert(!_usb_gc_probe_active);
    transport_usb_stop();
    assert(probe_stops == 0);

    puts("Padbox transport tests passed");
    return 0;
}
