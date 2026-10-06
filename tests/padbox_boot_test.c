#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "hoja.h"
#include "utilities/boot.h"
#include "utilities/settings.h"
#include "input/hover.h"
#include "devices/battery.h"

static mapper_input_s buttons;
static uint32_t memory;
static jmp_buf bootloader_jump;
static gamepadConfig_s gamepad;
static hoverConfig_s hover;
gamepadConfig_s *gamepad_config = &gamepad;
hoverConfig_s *hover_config = &hover;
inputInfoStatic_s input_static;

static const hoja_config_s config = {
    .sewn_layout = SEWN_LAYOUT_ABXY,
    .usb_bootloader_code = {INPUT_CODE_START, INPUT_CODE_SELECT},
    .sync_on_boot_code = INPUT_CODE_UNUSED,
    .wlan_force_code = INPUT_CODE_UNUSED,
};

const hoja_config_s *hoja_config_get(void) { return &config; }
mapper_input_s hover_access_boot(void) { return buttons; }
bool cb_hoja_boot_custom_face_mode(const mapper_input_s *in, core_reportformat_t *out)
{ (void)in; (void)out; return false; }
uint32_t sys_hal_get_bootmemory(void) { return memory; }
void sys_hal_set_bootmemory(uint32_t value) { memory = value; }
void sys_hal_bootloader(void) { longjmp(bootloader_jump, 1); }
battery_result_t battery_init(void) { return (battery_result_t)0; }
void battery_get_status(battery_status_s *out) { memset(out, 0, sizeof(*out)); }

static void reset(void)
{
    memset(&buttons, 0, sizeof(buttons));
    memset(&gamepad, 0, sizeof(gamepad));
    memset(&hover, 0, sizeof(hover));
    memset(&input_static, 0, sizeof(input_static));
    for (int i = 0; i < INPUT_CODE_MAX; ++i)
        input_static.input_info[i].input_type = MAPPER_INPUT_TYPE_DIGITAL;
    memory = 0;
}

static void expect(core_reportformat_t format, gamepad_transport_t transport, bool automatic)
{
    boot_init();
    const boot_info_s *info = boot_get_info();
    assert(info->reportformat == format);
    assert(info->transport == transport);
    assert(info->auto_gamecube_usb == automatic);
}

int main(void)
{
    // No hold must ignore every saved mode, including an invalid saved value.
    for (int mode = 0; mode <= 255; ++mode)
    {
        reset();
        gamepad.gamepad_default_mode = mode;
        expect(CORE_REPORTFORMAT_SLIPPI, GAMEPAD_TRANSPORT_USB, true);
    }

    const mapper_input_code_t holds[] = {
        INPUT_CODE_SOUTH, INPUT_CODE_EAST, INPUT_CODE_WEST, INPUT_CODE_NORTH,
        INPUT_CODE_LEFT, INPUT_CODE_DOWN, INPUT_CODE_RIGHT,
    };
    const core_reportformat_t formats[] = {
        CORE_REPORTFORMAT_SWPRO, CORE_REPORTFORMAT_SINPUT, CORE_REPORTFORMAT_XINPUT,
        CORE_REPORTFORMAT_SLIPPI, CORE_REPORTFORMAT_SNES, CORE_REPORTFORMAT_N64,
        CORE_REPORTFORMAT_GAMECUBE,
    };
    const gamepad_transport_t transports[] = {
        GAMEPAD_TRANSPORT_USB, GAMEPAD_TRANSPORT_USB, GAMEPAD_TRANSPORT_USB,
        GAMEPAD_TRANSPORT_USB, GAMEPAD_TRANSPORT_NESBUS, GAMEPAD_TRANSPORT_JOYBUS64,
        GAMEPAD_TRANSPORT_JOYBUSGC,
    };
    for (unsigned i = 0; i < sizeof(holds) / sizeof(holds[0]); ++i)
    {
        reset();
        gamepad.gamepad_default_mode = CORE_REPORTFORMAT_GAMECUBE;
        buttons.presses[holds[i]] = true;
        expect(formats[i], transports[i], false);
    }

    // LB alone must not route the automatic wired mode to nonexistent Bluetooth.
    reset();
    buttons.presses[INPUT_CODE_LB] = true;
    expect(CORE_REPORTFORMAT_SLIPPI, GAMEPAD_TRANSPORT_USB, true);

    // Ambiguous face holds retain stock fallback policy.
    reset();
    gamepad.gamepad_default_mode = CORE_REPORTFORMAT_GAMECUBE;
    buttons.presses[INPUT_CODE_SOUTH] = buttons.presses[INPUT_CODE_WEST] = true;
    expect(CORE_REPORTFORMAT_GAMECUBE, GAMEPAD_TRANSPORT_JOYBUSGC, false);

    // Detected-console reboot takes precedence, resolves the wired bus, and
    // consumes its one-shot memory. A later cold boot must be automatic again.
    reset();
    buttons.presses[INPUT_CODE_WEST] = true;
    boot_memory_s next = {
        .report_format = CORE_REPORTFORMAT_GAMECUBE,
        .gamepad_method = GAMEPAD_METHOD_WIRED,
    };
    boot_set_memory(&next);
    expect(CORE_REPORTFORMAT_GAMECUBE, GAMEPAD_TRANSPORT_JOYBUSGC, false);
    assert(memory == 0);
    memset(&buttons, 0, sizeof(buttons));
    expect(CORE_REPORTFORMAT_SLIPPI, GAMEPAD_TRANSPORT_USB, true);

    // A configurator-requested USB mode must also override a held console mode.
    reset();
    buttons.presses[INPUT_CODE_RIGHT] = true;
    next = (boot_memory_s){
        .report_format = CORE_REPORTFORMAT_XINPUT,
        .gamepad_method = GAMEPAD_METHOD_USB,
    };
    boot_set_memory(&next);
    expect(CORE_REPORTFORMAT_XINPUT, GAMEPAD_TRANSPORT_USB, false);

    // Invalid watchdog memory cannot disable automatic boot.
    reset();
    memory = 0x12345678;
    expect(CORE_REPORTFORMAT_SLIPPI, GAMEPAD_TRANSPORT_USB, true);

    // Recovery combo still wins over every mode selection.
    reset();
    buttons.presses[INPUT_CODE_START] = buttons.presses[INPUT_CODE_SELECT] = true;
    if (setjmp(bootloader_jump) == 0)
    {
        boot_init();
        assert(!"bootloader combo must not return");
    }
    puts("Padbox boot tests passed");
    return 0;
}
