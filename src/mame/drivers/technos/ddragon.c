/***************************************************************************

Double Dragon     (c) 1987 Technos Japan
Double Dragon II  (c) 1988 Technos Japan

Driver by Carlos A. Lozano, Rob Rosenbrock, Phil Stroffolino, Ernesto Corvi
Toffy / Super Toffy added by David Haywood
Thanks to Bryan McPhail for spotting the Toffy program rom encryption
Toffy / Super Toffy sound hooked up by R. Belmont.

BM, 8/1/2006:

Double Dragon has a crash which sometimes occurs at the very end of the game
(right before the final animation sequence).  It occurs because of a jump look up
table:

    BAD3: LDY   #$BADD
    BAD7: JSR   [A,Y]

At the point of the crash A is 0x3e which causes a jump to 0x3401 (background tile
ram) which obviously doesn't contain proper code and causes a crash.  The jump
table has 32 entries, and only the last contains an invalid jump vector.  A is set
to 0x3e as a result of code at 0x625f - it reads from the shared spriteram (0x2049
in main cpu memory space), copies the value to 0x523 (main ram) where it is later
fetched and shifted to make 0x3e.

So..  it's not clear where the error is - the 0x1f value is actually written to
shared RAM by the main CPU - perhaps the MCU should modify it before the main CPU
reads it back?  Perhaps 0x1f should never be written at all?  If you want to trace
this further please submit a proper fix!  In the meantime I have patched the error
by making sure the invalid jump is never taken - this fixes the crash (see
ddragon_spriteram_r).



Modifications by Bryan McPhail, June-November 2003:

Correct video & interrupt timing derived from Xain schematics and confirmed on real DD board.
Corrected interrupt handling, epecially to MCU (but one semi-hack remains).
TStrike now boots but sprites don't appear (I had them working at one point, can't remember what broke them again).
Dangerous Dungeons fixed.
World version of Double Dragon added (actually same roms as the bootleg, but confirmed from real board)
Removed stereo audio flag (still on Toffy - does it have it?)

todo:

banking in Toffy / Super toffy

-- Read Me --

Super Toffy - Unico 1994

Main cpu:   MC6809EP
Sound cpu:  MC6809P
Sound:      YM2151
Clocks:     12 MHz, 3.579MHz

Graphics custom: MDE-2001

-- --

Does this make Super Toffy the sequel to a rip-off / bootleg of a
conversion kit which could be applied to a bootleg double dragon :-p?


2008-08
Dip locations verified with manual for ddragon & ddragon2

***************************************************************************/

#include "emu.h"

#include "cpu/hd6309/hd6309.h"
#include "cpu/m6800/m6800.h"
#include "cpu/m6805/m6805.h"
#include "cpu/m6809/m6809.h"
#include "cpu/z80/z80.h"

#include "sound/2151intf.h"
#include "sound/okim6295.h"
#include "sound/msm5205.h"

#include "includes/ddragon.h"


#define MAIN_CLOCK		XTAL_12MHz
#define SOUND_CLOCK		XTAL_3_579545MHz
#define MCU_CLOCK		MAIN_CLOCK / 3		// 4MHz
#define PIXEL_CLOCK		MAIN_CLOCK / 2		// 6MHz


/*************************************
 *
 *  Video timing
 *
 *************************************/

/*
    Based on the Solar Warrior schematics, vertical timing counts as follows:

        08,09,0A,0B,...,FC,FD,FE,FF,E8,E9,EA,EB,...,FC,FD,FE,FF,
        08,09,....

    Thus, it counts from 08 to FF, then resets to E8 and counts to FF again.
    This gives (256 - 8) + (256 - 232) = 248 + 24 = 272 total scanlines.

    VBLK is signalled starting when the counter hits F8, and continues through
    the reset to E8 and through until the next reset to 08 again.

    Since MAME's video timing is 0-based, we need to convert this.
*/

INLINE int scanline_to_vcount(int scanline)
{
	int vcount = scanline + 8;

	if (vcount < 0x100)
		return vcount;
	else
		return (vcount - 0x18) | 0x100;
}

static TIMER_DEVICE_CALLBACK( ddragon_scanline )
{
	ddragon_state *state = timer.machine->driver_data<ddragon_state>();

	int scanline = param;
	int screen_height = timer.machine->primary_screen->height();
	int vcount_old = scanline_to_vcount((scanline == 0) ? screen_height - 1 : scanline - 1);
	int vcount = scanline_to_vcount(scanline);

	/* update to the current point */
	if (scanline > 0)
		timer.machine->primary_screen->update_partial(scanline - 1);

	/* on the rising edge of VBLK (vcount == 0xF8), signal an NMI */
	if (vcount == 248)
		cpu_set_input_line(state->maincpu, INPUT_LINE_NMI, ASSERT_LINE);

	/* set 1ms signal on rising edge of vcount & 8 */
	if (!(vcount_old & 0x08) && (vcount & 0x08))
		cpu_set_input_line(state->maincpu, M6809_FIRQ_LINE, ASSERT_LINE);
}


/*************************************
 *
 *  System setup and intialization
 *
 *************************************/

static MACHINE_START( ddragon )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	/* configure banks */
	memory_configure_bank(machine, "bank1", 0, 8, memory_region(machine, "maincpu") + 0x10000, 0x04000);

	state->maincpu = machine->device("maincpu");
	state->sub_cpu = machine->device("sub");
	state->snd_cpu = machine->device("soundcpu");
	state->adpcm_1 = machine->device("adpcm1");
	state->adpcm_2 = machine->device("adpcm2");

	/* register for save states */
	state_save_register_global(machine, state->scrollx_hi);
	state_save_register_global(machine, state->scrolly_hi);
	state_save_register_global(machine, state->ddragon_sub_port);
	state_save_register_global_array(machine, state->adpcm_pos);
	state_save_register_global_array(machine, state->adpcm_end);
	state_save_register_global_array(machine, state->adpcm_idle);
	state_save_register_global_array(machine, state->adpcm_data);
}


static MACHINE_RESET( ddragon )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	state->scrollx_hi = 0;
	state->scrolly_hi = 0;
	state->ddragon_sub_port = 0;
	state->adpcm_pos[0] = state->adpcm_pos[1] = 0;
	state->adpcm_end[0] = state->adpcm_end[1] = 0;
	state->adpcm_idle[0] = state->adpcm_idle[1] = 1;
	state->adpcm_data[0] = state->adpcm_data[1] = -1;
}


/*************************************
 *
 *  Bank switching
 *
 *************************************/

static WRITE8_HANDLER( ddragon_bankswitch_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();
	/*
		7 6 5 4 3 2 1 0
		. . . . . . . x		// X-scroll D9 (H9BT)
		. . . . . . x .		// Y-scroll D9 (V9BT)
		. . . . . x . .		// Screen flip (*1P/2P)
		. . . . x . . .		// Sub CPU reset (*RESET)
		. . . x . . . .		// Sub CPU halt (*HALT)
		x x x . . . . .		// ROM bank (*BANK)
	*/

	state->scrollx_hi = data & 0x01;
	state->scrolly_hi = (data & 0x02) >> 1;
	flip_screen_set(space->machine, ~data & 0x04);

	cpu_set_input_line(state->sub_cpu, INPUT_LINE_RESET, (data & 0x08) ? CLEAR_LINE : ASSERT_LINE);
	cpu_set_input_line(state->sub_cpu, INPUT_LINE_HALT,  (data & 0x10) ? ASSERT_LINE : CLEAR_LINE);

	memory_set_bank(space->machine, "bank1", (data & 0xe0) >> 5);
}

static READ8_HANDLER( darktowr_mcu_bank_r )
{
	/*
	Horrible hack - the alternate TStrike set is mismatched against the MCU,
	so just hack around the protection here.  (The hacks are 'right' as I have
	the original source code & notes to this version of TStrike to examine).
	*/

	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	if (offset == 0x1401 || offset == 0x01)
		return state->darktowr_mcu_ports[0];

	logerror("Unmapped mcu bank read %04x\n", offset);

	return 0xff;
}


static WRITE8_HANDLER( darktowr_mcu_bank_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	logerror("BankWrite %05x %08x %08x\n", cpu_get_pc(space->cpu), offset, data);

	if (offset == 0x1400 || offset == 0)
	{
		state->darktowr_mcu_ports[1] = BITSWAP8(data, 0, 1, 2, 3, 4, 5, 6, 7);
		logerror("MCU PORT 1 -> %04x (from %04x)\n", BITSWAP8(data, 0, 1, 2, 3, 4, 5, 6, 7), data);
	}
}


static WRITE8_HANDLER( darktowr_bankswitch_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	state->scrollx_hi = data & 0x01;
	state->scrolly_hi = (data & 0x02) >> 1;

	cpu_set_input_line(state->sub_cpu, INPUT_LINE_RESET, (data & 0x08) ? CLEAR_LINE : ASSERT_LINE);
	cpu_set_input_line(state->sub_cpu, INPUT_LINE_HALT,  (data & 0x10) ? ASSERT_LINE : CLEAR_LINE);

	int oldbank = memory_get_bank(space->machine, "bank1");
	int newbank = (data & 0xe0) >> 5;
	memory_set_bank(space->machine, "bank1", newbank);

	if (newbank == 4 && oldbank != 4)
		memory_install_readwrite8_handler(space, 0x4000, 0x7fff, 0, 0, darktowr_mcu_bank_r, darktowr_mcu_bank_w);
	else if (newbank != 4 && oldbank == 4)
		memory_install_readwrite_bank(space, 0x4000, 0x7fff, 0, 0, "bank1");
}


/*************************************
 *
 *  Interrupt control
 *
 *************************************/

static void ddragon_interrupt_ack(address_space *space, offs_t offset, UINT8 data)
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	switch (offset)
	{
		case 0: /* 380b - NMI ack */
			cpu_set_input_line(state->maincpu, INPUT_LINE_NMI, CLEAR_LINE);
			break;

		case 1: /* 380c - FIRQ ack */
			cpu_set_input_line(state->maincpu, M6809_FIRQ_LINE, CLEAR_LINE);
			break;

		case 2: /* 380d - IRQ ack */
			cpu_set_input_line(state->maincpu, M6809_IRQ_LINE, CLEAR_LINE);
			break;

		case 3: /* 380e - SND irq */
			soundlatch_w(space, 0, data);
			cpu_set_input_line(state->snd_cpu, state->sound_irq, (state->sound_irq == INPUT_LINE_NMI) ? PULSE_LINE : HOLD_LINE);
			break;

		case 4: /* 380f - MCU IRQ */
			if (state->sub_cpu != NULL)
				cpu_set_input_line(state->sub_cpu, state->sprite_irq, ASSERT_LINE);
			break;
	}
}

static READ8_HANDLER(ddragon_interrupt_r)
{
	ddragon_interrupt_ack(space, offset, 0xff);

	return 0xff;
}

static WRITE8_HANDLER(ddragon_interrupt_w)
{
	ddragon_interrupt_ack(space, offset, data);
}

static WRITE8_HANDLER( ddragon2_sub_irq_ack_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();
	cpu_set_input_line(state->sub_cpu, state->sprite_irq, CLEAR_LINE );
}


static WRITE8_HANDLER( ddragon2_sub_irq_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();
	cpu_set_input_line(state->maincpu, M6809_IRQ_LINE, ASSERT_LINE);
}


static void irq_handler( running_device *device, int irq )
{
	ddragon_state *state = device->machine->driver_data<ddragon_state>();
	cpu_set_input_line(state->snd_cpu, state->ym_irq , irq ? ASSERT_LINE : CLEAR_LINE);
}

static READ8_HANDLER(soundlatch_ack_r)
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	cpu_set_input_line(state->snd_cpu, state->sound_irq, CLEAR_LINE);

	return soundlatch_r(space, 0);
}

static WRITE8_HANDLER(ddragonba_port_w)
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	if ((data & 0x08) == 0)
		cpu_set_input_line(state->sub_cpu, state->sprite_irq, CLEAR_LINE);

	if (!(state->ddragon_sub_port & 0x10) && (data & 0x10))
		cpu_set_input_line(state->maincpu, M6809_IRQ_LINE, ASSERT_LINE);

	state->ddragon_sub_port = data;
}


/*************************************
 *
 *  MCU handlers
 *
 *************************************/

static CUSTOM_INPUT( subcpu_bus_free )
{
	ddragon_state *state = field->port->machine->driver_data<ddragon_state>();

	if (state->sub_cpu != NULL)	// Corresponds to BA (Bus Available) on the HD63701
		return field->port->machine->device<cpu_device>("sub")->suspended(SUSPEND_REASON_RESET | SUSPEND_REASON_HALT);
	else
		return 0;
}


static WRITE8_HANDLER( darktowr_mcu_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();
	logerror("McuWrite %05x %08x %08x\n",cpu_get_pc(space->cpu), offset, data);
	state->darktowr_mcu_ports[offset] = data;
}


static READ8_HANDLER( ddragon_hd63701_internal_registers_r )
{
	logerror("%04x: read %d\n", cpu_get_pc(space->cpu), offset);

	return 0;
}


static WRITE8_HANDLER( ddragon_hd63701_internal_registers_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	// Port 6
	if (offset == 0x17)
	{
		if (data & 0x01)
			cpu_set_input_line(state->sub_cpu, state->sprite_irq, CLEAR_LINE);

		if (!(state->ddragon_sub_port & 0x02) && (data & 0x02))
			cpu_set_input_line(state->maincpu, M6809_IRQ_LINE, ASSERT_LINE);

		state->ddragon_sub_port = data;
	}
}


/*************************************
 *
 *  Sprite RAM hacks
 *
 *************************************/

static READ8_HANDLER( ddragon_comram_r )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	/* Access to shared RAM is prevented when the sub CPU is active */
	if (!space->machine->device<cpu_device>("sub")->suspended(SUSPEND_REASON_RESET | SUSPEND_REASON_HALT))
		return 0xff;

	return state->comram[offset];
}


static WRITE8_HANDLER( ddragon_comram_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	if (!space->machine->device<cpu_device>("sub")->suspended(SUSPEND_REASON_RESET | SUSPEND_REASON_HALT))
		return;

	state->comram[offset] = data;
}


/*************************************
 *
 *  ADPCM sound
 *
 *************************************/

static WRITE8_HANDLER( dd_adpcm_w )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	int chip = offset & 0x01;
	running_device *adpcm = chip ? state->adpcm_2 : state->adpcm_1;

	switch (offset / 2)
	{
		case 3:
			state->adpcm_idle[chip] = 1;
			msm5205_reset_w(adpcm, 1);
			break;
		case 2:
			state->adpcm_pos[chip] = (data & 0x7f) * 0x200;
			break;
		case 1:
			state->adpcm_end[chip] = (data & 0x7f) * 0x200;
			break;
		case 0:
			state->adpcm_idle[chip] = 0;
			msm5205_reset_w(adpcm, 0);
			break;
	}
}


static void dd_adpcm_int( running_device *device, int chip )
{
	ddragon_state *state = device->machine->driver_data<ddragon_state>();

	if (state->adpcm_pos[chip] >= state->adpcm_end[chip] || state->adpcm_pos[chip] >= 0x10000)
	{
		state->adpcm_idle[chip] = 1;
		msm5205_reset_w(device, 1);
	}
	else if (state->adpcm_data[chip] != -1)
	{
		msm5205_data_w(device, state->adpcm_data[chip] & 0x0f);
		state->adpcm_data[chip] = -1;
	}
	else
	{
		UINT8 *ROM = memory_region(device->machine, "adpcm") + 0x10000 * chip;

		state->adpcm_data[chip] = ROM[state->adpcm_pos[chip]++];
		msm5205_data_w(device, state->adpcm_data[chip] >> 4);
	}
}

static void dd_adpcm_int_1(running_device *device)
{
	dd_adpcm_int(device->machine->device("adpcm1"), 0x00);
}

static void dd_adpcm_int_2(running_device *device)
{
	dd_adpcm_int(device->machine->device("adpcm2"), 0x01);
}

static READ8_HANDLER( dd_adpcm_status_r )
{
	ddragon_state *state = space->machine->driver_data<ddragon_state>();

	return state->adpcm_idle[0] | (state->adpcm_idle[1] << 1);
}


/*************************************
 *
 *  Main CPU memory maps
 *
 *************************************/

static ADDRESS_MAP_START( ddragon_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x0fff) AM_RAM AM_BASE_MEMBER(ddragon_state, rambase)
	AM_RANGE(0x1000, 0x11ff) AM_RAM_WRITE(paletteram_xxxxBBBBGGGGRRRR_split1_w) AM_BASE_GENERIC(paletteram)
	AM_RANGE(0x1200, 0x13ff) AM_RAM_WRITE(paletteram_xxxxBBBBGGGGRRRR_split2_w) AM_BASE_GENERIC(paletteram2)
	AM_RANGE(0x1800, 0x1fff) AM_RAM_WRITE(ddragon_fgvideoram_w) AM_BASE_MEMBER(ddragon_state, fgvideoram)
	AM_RANGE(0x2000, 0x21ff) AM_MIRROR(0x0600) AM_READWRITE(ddragon_comram_r, ddragon_comram_w) AM_BASE_MEMBER(ddragon_state, comram)
	AM_RANGE(0x2800, 0x2fff) AM_RAM AM_BASE_SIZE_MEMBER(ddragon_state, spriteram, spriteram_size)
	AM_RANGE(0x3000, 0x37ff) AM_RAM_WRITE(ddragon_bgvideoram_w) AM_BASE_MEMBER(ddragon_state, bgvideoram)
	AM_RANGE(0x3800, 0x3800) AM_READ_PORT("P1")
	AM_RANGE(0x3801, 0x3801) AM_READ_PORT("P2")
	AM_RANGE(0x3802, 0x3802) AM_READ_PORT("EXTRA")
	AM_RANGE(0x3803, 0x3803) AM_READ_PORT("DSW0")
	AM_RANGE(0x3804, 0x3804) AM_READ_PORT("DSW1")
	AM_RANGE(0x3808, 0x3808) AM_WRITE(ddragon_bankswitch_w)
	AM_RANGE(0x3809, 0x3809) AM_WRITEONLY AM_BASE_MEMBER(ddragon_state, scrollx_lo)
	AM_RANGE(0x380a, 0x380a) AM_WRITEONLY AM_BASE_MEMBER(ddragon_state, scrolly_lo)
	AM_RANGE(0x380b, 0x380f) AM_READWRITE(ddragon_interrupt_r, ddragon_interrupt_w)
	AM_RANGE(0x4000, 0x7fff) AM_ROMBANK("bank1")
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( dd2_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x17ff) AM_RAM
	AM_RANGE(0x1800, 0x1fff) AM_RAM_WRITE(ddragon_fgvideoram_w) AM_BASE_MEMBER(ddragon_state, fgvideoram)
	AM_RANGE(0x2000, 0x21ff) AM_MIRROR(0x0600) AM_READWRITE(ddragon_comram_r, ddragon_comram_w) AM_BASE_MEMBER(ddragon_state, comram)
	AM_RANGE(0x2800, 0x2fff) AM_RAM AM_BASE_SIZE_MEMBER(ddragon_state, spriteram, spriteram_size)
	AM_RANGE(0x3000, 0x37ff) AM_RAM_WRITE(ddragon_bgvideoram_w) AM_BASE_MEMBER(ddragon_state, bgvideoram)
	AM_RANGE(0x3800, 0x3800) AM_READ_PORT("P1")
	AM_RANGE(0x3801, 0x3801) AM_READ_PORT("P2")
	AM_RANGE(0x3802, 0x3802) AM_READ_PORT("EXTRA")
	AM_RANGE(0x3803, 0x3803) AM_READ_PORT("DSW0")
	AM_RANGE(0x3804, 0x3804) AM_READ_PORT("DSW1")
	AM_RANGE(0x3808, 0x3808) AM_WRITE(ddragon_bankswitch_w)
	AM_RANGE(0x3809, 0x3809) AM_WRITEONLY AM_BASE_MEMBER(ddragon_state, scrollx_lo)
	AM_RANGE(0x380a, 0x380a) AM_WRITEONLY AM_BASE_MEMBER(ddragon_state, scrolly_lo)
	AM_RANGE(0x380b, 0x380f) AM_READWRITE(ddragon_interrupt_r, ddragon_interrupt_w)
	AM_RANGE(0x3c00, 0x3dff) AM_RAM_WRITE(paletteram_xxxxBBBBGGGGRRRR_split1_w) AM_BASE_GENERIC(paletteram)
	AM_RANGE(0x3e00, 0x3fff) AM_RAM_WRITE(paletteram_xxxxBBBBGGGGRRRR_split2_w) AM_BASE_GENERIC(paletteram2)
	AM_RANGE(0x4000, 0x7fff) AM_ROMBANK("bank1")
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END


/*************************************
 *
 *  Sub CPU memory maps
 *
 *************************************/

static ADDRESS_MAP_START( sub_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x001f) AM_READWRITE(ddragon_hd63701_internal_registers_r, ddragon_hd63701_internal_registers_w)
	AM_RANGE(0x001f, 0x0fff) AM_RAM
	AM_RANGE(0x8000, 0x8fff) AM_RAM AM_BASE_MEMBER(ddragon_state, comram)
	AM_RANGE(0xc000, 0xffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( ddragonba_sub_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x0fff) AM_RAM
	AM_RANGE(0x8000, 0x8fff) AM_RAM AM_BASE_MEMBER(ddragon_state, comram)
	AM_RANGE(0xc000, 0xffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( dd2_sub_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0xbfff) AM_ROM
	AM_RANGE(0xc000, 0xc3ff) AM_RAM AM_BASE_MEMBER(ddragon_state, comram)
	AM_RANGE(0xd000, 0xd000) AM_WRITE(ddragon2_sub_irq_ack_w)
	AM_RANGE(0xe000, 0xe000) AM_WRITE(ddragon2_sub_irq_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( ddragonba_sub_portmap, ADDRESS_SPACE_IO, 8 )
	AM_RANGE(0x0000, 0xffff) AM_WRITE(ddragonba_port_w)
ADDRESS_MAP_END


/*************************************
 *
 *  Sound CPU memory maps
 *
 *************************************/

static ADDRESS_MAP_START( sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x0fff) AM_RAM
	AM_RANGE(0x1000, 0x1000) AM_READ(soundlatch_ack_r)
	AM_RANGE(0x1800, 0x1800) AM_READ(dd_adpcm_status_r)
	AM_RANGE(0x2800, 0x2801) AM_DEVREADWRITE("fmsnd", ym2151_r, ym2151_w)
	AM_RANGE(0x3800, 0x3807) AM_WRITE(dd_adpcm_w)
	AM_RANGE(0x8000, 0xffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( dd2_sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0x87ff) AM_RAM
	AM_RANGE(0x8800, 0x8801) AM_DEVREADWRITE("fmsnd", ym2151_r, ym2151_w)
	AM_RANGE(0x9800, 0x9800) AM_DEVREADWRITE_MODERN("oki", okim6295_device, read, write)
	AM_RANGE(0xa000, 0xa000) AM_READ(soundlatch_ack_r)
ADDRESS_MAP_END


/*************************************
 *
 *  MCU memory maps
 *
 *************************************/

static ADDRESS_MAP_START( mcu_map, ADDRESS_SPACE_PROGRAM, 8 )
	ADDRESS_MAP_GLOBAL_MASK(0x7ff)
	AM_RANGE(0x0000, 0x0007) AM_RAM_WRITE(darktowr_mcu_w) AM_BASE_MEMBER(ddragon_state, darktowr_mcu_ports)
	AM_RANGE(0x0008, 0x007f) AM_RAM
	AM_RANGE(0x0080, 0x07ff) AM_ROM
ADDRESS_MAP_END


/*************************************
 *
 *  Input ports
 *
 *************************************/

static INPUT_PORTS_START( ddragon )
	PORT_START("P1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START("P2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN2 )

	PORT_START("DSW0")
	PORT_DIPNAME( 0x07, 0x07, DEF_STR( Coin_A ) ) PORT_DIPLOCATION("SW1:1,2,3")
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x38, 0x38, DEF_STR( Coin_B ) ) PORT_DIPLOCATION("SW1:4,5,6")
	PORT_DIPSETTING(    0x00, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x38, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x28, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x18, DEF_STR( 1C_5C ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Cabinet ) ) PORT_DIPLOCATION("SW1:7")
	PORT_DIPSETTING(    0x40, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Flip_Screen ) ) PORT_DIPLOCATION("SW1:8")
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START("DSW1")
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Difficulty ) ) PORT_DIPLOCATION("SW2:1,2")
	PORT_DIPSETTING(    0x01, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Medium ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Demo_Sounds ) ) PORT_DIPLOCATION("SW2:3")
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x04, DEF_STR( On ) )
	PORT_DIPUNUSED_DIPLOC( 0x08, IP_ACTIVE_LOW, "SW2:4" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) ) PORT_DIPLOCATION("SW2:5,6")
	PORT_DIPSETTING(    0x10, "20k" )
	PORT_DIPSETTING(    0x00, "40k" )
	PORT_DIPSETTING(    0x30, "30k and every 60k" )
	PORT_DIPSETTING(    0x20, "40k and every 80k" )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:7,8")
	PORT_DIPSETTING(    0xc0, "2" )
	PORT_DIPSETTING(    0x80, "3" )
	PORT_DIPSETTING(    0x40, "4" )
	PORT_DIPSETTING(    0x00, "Infinite (Cheat)")

	PORT_START("EXTRA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SPECIAL ) PORT_CUSTOM(subcpu_bus_free, NULL)
	PORT_BIT( 0xe0, IP_ACTIVE_HIGH, IPT_UNUSED )
INPUT_PORTS_END


static INPUT_PORTS_START( ddragon2 )
	PORT_INCLUDE(ddragon)

	PORT_MODIFY("DSW1")
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Difficulty ) ) PORT_DIPLOCATION("SW2:1,2")
	PORT_DIPSETTING(    0x01, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x03, DEF_STR( Medium ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x08, 0x08, "Hurricane Kick" ) PORT_DIPLOCATION("SW2:4")
	PORT_DIPSETTING(    0x00, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Normal ) )
	PORT_DIPNAME( 0x30, 0x10, "Timer" ) PORT_DIPLOCATION("SW2:5,6")
	PORT_DIPSETTING(    0x00, "60" )
	PORT_DIPSETTING(    0x10, "65" )
	PORT_DIPSETTING(    0x30, "70" )
	PORT_DIPSETTING(    0x20, "80" )
	PORT_DIPNAME( 0xc0, 0x80, DEF_STR( Lives ) ) PORT_DIPLOCATION("SW2:7,8")
	PORT_DIPSETTING(    0xc0, "1" )
	PORT_DIPSETTING(    0x80, "2" )
	PORT_DIPSETTING(    0x40, "3" )
	PORT_DIPSETTING(    0x00, "4" )
INPUT_PORTS_END


static INPUT_PORTS_START( ddungeon )
	PORT_INCLUDE(ddragon)

	PORT_MODIFY("DSW0")
	PORT_DIPNAME( 0x0f, 0x00, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 4C_2C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 4C_4C ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 3C_3C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0xf0, 0x00, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x30, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x70, DEF_STR( 4C_2C ) )
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x60, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0xb0, DEF_STR( 4C_3C ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( 4C_4C ) )
	PORT_DIPSETTING(    0xa0, DEF_STR( 3C_3C ) )
	PORT_DIPSETTING(    0x50, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0xe0, DEF_STR( 3C_4C ) )
	PORT_DIPSETTING(    0x90, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0xd0, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(    0x40, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x80, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0xc0, DEF_STR( 1C_4C ) )

	PORT_MODIFY("DSW1")
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x03, "4" )
	PORT_DIPUNUSED( 0x04, IP_ACTIVE_LOW )
	PORT_DIPUNUSED( 0x08, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0xf0, 0x90, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0xf0, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x90, DEF_STR( Medium ) )
	PORT_DIPSETTING(    0x70, DEF_STR( Hard ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
INPUT_PORTS_END


static INPUT_PORTS_START( darktowr )
	PORT_INCLUDE(ddungeon)

	PORT_MODIFY("DSW1")
	PORT_DIPNAME( 0x01, 0x01, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x04, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x04, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x08, 0x08, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END


/*************************************
 *
 *  Graphics layouts
 *
 *************************************/


static const gfx_layout char_layout =
{
	8, 8,
	RGN_FRAC(1, 1),	4,
	{ STEP4(0, 2) },
	{ 1, 0, 8*8+1, 8*8+0, 16*8+1, 16*8+0, 24*8+1, 24*8+0 },
	{ STEP8(0, 8) },
	32*8
};

static const gfx_layout tile_layout =
{
	16,16,
	RGN_FRAC(1,2),	4,
	{ RGN_FRAC(1,2)+0, RGN_FRAC(1,2)+4, 0, 4 },
	{ 3, 2, 1, 0, 16*8+3, 16*8+2, 16*8+1, 16*8+0, 32*8+3, 32*8+2, 32*8+1, 32*8+0, 48*8+3, 48*8+2, 48*8+1, 48*8+0 },
	{ STEP16(0, 8) },
	64*8
};


static GFXDECODE_START( ddragon )
	GFXDECODE_ENTRY( "gfx1", 0, char_layout,   0, 8 )	/* colors   0-127 */
	GFXDECODE_ENTRY( "gfx2", 0, tile_layout, 128, 8 )	/* colors 128-255 */
	GFXDECODE_ENTRY( "gfx3", 0, tile_layout, 256, 8 )	/* colors 256-383 */
GFXDECODE_END


/*************************************
 *
 *  Sound definitions
 *
 *************************************/

static const ym2151_interface  ym2151_config = { irq_handler };
static const msm5205_interface msm5205_config_1 = { dd_adpcm_int_1, MSM5205_S48_4B /* 8kHz */ };
static const msm5205_interface msm5205_config_2 = { dd_adpcm_int_2, MSM5205_S48_4B /* 8kHz */ };


/*************************************
 *
 *  Machine drivers
 *
 *************************************/

static MACHINE_CONFIG_START( ddragon, ddragon_state )
	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", HD6309, MAIN_CLOCK)		/* 12 MHz / 4 internally */
	MDRV_CPU_PROGRAM_MAP(ddragon_map)
	MDRV_TIMER_ADD_SCANLINE("scantimer", ddragon_scanline, "screen", 0, 1)
	MDRV_CPU_ADD("sub", HD63701, MAIN_CLOCK / 2)		/* 6 MHz / 4 internally */
	MDRV_CPU_PROGRAM_MAP(sub_map)
	MDRV_CPU_ADD("soundcpu", M6809, MAIN_CLOCK / 8)		/* 1.5 MHz */
	MDRV_CPU_PROGRAM_MAP(sound_map)
	MDRV_QUANTUM_TIME(HZ(60000))	/* heavy interleaving to sync up sprite<->main CPU's */
	MDRV_MACHINE_START(ddragon)
	MDRV_MACHINE_RESET(ddragon)

	/* video hardware */
	MDRV_GFXDECODE(ddragon)
	MDRV_PALETTE_LENGTH(384)
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_RAW_PARAMS(PIXEL_CLOCK, 384, 0, 256, 272, 0, 240)
	MDRV_VIDEO_START(ddragon)
	MDRV_VIDEO_UPDATE(ddragon)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD("fmsnd", YM2151, SOUND_CLOCK)
	MDRV_SOUND_CONFIG(ym2151_config)
	MDRV_SOUND_ROUTE(0, "mono", 0.60)
	MDRV_SOUND_ROUTE(1, "mono", 0.60)

	MDRV_SOUND_ADD("adpcm1", MSM5205, MAIN_CLOCK / 32)
	MDRV_SOUND_CONFIG(msm5205_config_1)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
	MDRV_SOUND_ADD("adpcm2", MSM5205, MAIN_CLOCK / 32)
	MDRV_SOUND_CONFIG(msm5205_config_2)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
MACHINE_CONFIG_END


static MACHINE_CONFIG_DERIVED( ddragonb, ddragon )
	/* basic machine hardware */
	MDRV_CPU_REPLACE("sub", M6809, MAIN_CLOCK / 8)	/* 1.5MHz */
	MDRV_CPU_PROGRAM_MAP(sub_map)
MACHINE_CONFIG_END


static MACHINE_CONFIG_DERIVED( ddragonba, ddragon )
	/* basic machine hardware */
	MDRV_CPU_REPLACE("sub", M6803, MAIN_CLOCK / 2)	/* 6MHz / 4 internally */
	MDRV_CPU_PROGRAM_MAP(ddragonba_sub_map)
	MDRV_CPU_IO_MAP(ddragonba_sub_portmap)
MACHINE_CONFIG_END


static MACHINE_CONFIG_START( ddragon2, ddragon_state )
	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", HD6309, MAIN_CLOCK)		/* 12 MHz / 4 internally */
	MDRV_CPU_PROGRAM_MAP(dd2_map)
	MDRV_TIMER_ADD_SCANLINE("scantimer", ddragon_scanline, "screen", 0, 1)
	MDRV_CPU_ADD("sub", Z80, MAIN_CLOCK / 3)		/* 4 MHz */
	MDRV_CPU_PROGRAM_MAP(dd2_sub_map)
	MDRV_CPU_ADD("soundcpu", Z80, 3579545)
	MDRV_CPU_PROGRAM_MAP(dd2_sound_map)
	MDRV_QUANTUM_TIME(HZ(60000))		/* heavy interleaving to sync up sprite<->main CPU's */
	MDRV_MACHINE_START(ddragon)
	MDRV_MACHINE_RESET(ddragon)

	/* video hardware */
	MDRV_GFXDECODE(ddragon)
	MDRV_PALETTE_LENGTH(384)
	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_RAW_PARAMS(PIXEL_CLOCK, 384, 0, 256, 272, 0, 240)
	MDRV_VIDEO_START(ddragon)
	MDRV_VIDEO_UPDATE(ddragon)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD("fmsnd", YM2151, SOUND_CLOCK)
	MDRV_SOUND_CONFIG(ym2151_config)
	MDRV_SOUND_ROUTE(0, "mono", 0.60)
	MDRV_SOUND_ROUTE(1, "mono", 0.60)
	MDRV_OKIM6295_ADD("oki", 1056000, OKIM6295_PIN7_HIGH) // clock frequency & pin 7 not verified
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.20)
MACHINE_CONFIG_END


static MACHINE_CONFIG_DERIVED( darktowr, ddragon )
	/* basic machine hardware */
	MDRV_CPU_ADD("mcu", M68705, XTAL_4MHz)
	MDRV_CPU_PROGRAM_MAP(mcu_map)
MACHINE_CONFIG_END


/*************************************
 *
 *  ROM definitions
 *
 *************************************/

ROM_START( ddragon )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "21j-1-5.26",   0x08000, 0x08000, CRC(42045dfd) SHA1(0983705ea3bb87c4c239692f400e02f15c243479) )
	ROM_LOAD( "21j-2-3.25",   0x10000, 0x08000, CRC(5779705e) SHA1(4b8f22225d10f5414253ce0383bbebd6f720f3af) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21j-3.24",     0x18000, 0x08000, CRC(3bdea613) SHA1(d9038c80646a6ce3ea61da222873237b0383680e) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21j-4-1.23",   0x20000, 0x08000, CRC(728f87b9) SHA1(d7442be24d41bb9fc021587ef44ae5b830e4503d) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "21jm-0.ic55",    0xc000, 0x4000, CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, CRC(bb0bc76f) SHA1(37b2225e0593335f636c1e5fded9b21fdeab2f5a) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, CRC(5effb0a0) SHA1(1f21acb15dad824e831ed9a42b3fde096bb31141) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0",      0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* Layer priority */
ROM_END

ROM_START( ddragonw )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "21j-1.26",     0x08000, 0x08000, CRC(ae714964) SHA1(072522b97ca4edd099c6b48d7634354dc7088c53) )
	ROM_LOAD( "21j-2-3.25",   0x10000, 0x08000, CRC(5779705e) SHA1(4b8f22225d10f5414253ce0383bbebd6f720f3af) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-3.24",     0x18000, 0x08000, CRC(dbf24897) SHA1(1504faaf07c541330cd43b72dc6846911dfd85a3) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21j-4.23",     0x20000, 0x08000, CRC(6c9f46fa) SHA1(df251a4aea69b2328f7a543bf085b9c35933e2c1) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "21jm-0.ic55",    0xc000, 0x4000, CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, CRC(bb0bc76f) SHA1(37b2225e0593335f636c1e5fded9b21fdeab2f5a) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, CRC(5effb0a0) SHA1(1f21acb15dad824e831ed9a42b3fde096bb31141) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",   0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0.16",    0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */
ROM_END

ROM_START( ddragonw1 )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "e1-1.26",       0x08000, 0x08000, CRC(4b951643) SHA1(efb1f9ef2e46597d76123c9770854c1d83639eb2) )
	ROM_LOAD( "21a-2-4.25",    0x10000, 0x08000, CRC(5cd67657) SHA1(96bc7a5354a76524bd43a4d7eb8b0053a89e39c4) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-3.24",      0x18000, 0x08000, CRC(dbf24897) SHA1(1504faaf07c541330cd43b72dc6846911dfd85a3) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "e4-1.23",       0x20000, 0x08000, CRC(b1e26935) SHA1(dfff666fd5e9dc4dfb2a1d891eced88730cbaf30) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "21jm-0.ic55",    0xc000, 0x4000, CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) ) /* Labeled as 21JM-0 */

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, CRC(bb0bc76f) SHA1(37b2225e0593335f636c1e5fded9b21fdeab2f5a) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, CRC(5effb0a0) SHA1(1f21acb15dad824e831ed9a42b3fde096bb31141) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",     0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0.16",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */
ROM_END

ROM_START( ddragonu )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "21a-1-5.26",   0x08000, 0x08000, CRC(e24a6e11) SHA1(9dd97dd712d5c896f91fd80df58be9b8a2b198ee) )
	ROM_LOAD( "21j-2-3.25",   0x10000, 0x08000, CRC(5779705e) SHA1(4b8f22225d10f5414253ce0383bbebd6f720f3af) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-3.24",     0x18000, 0x08000, CRC(dbf24897) SHA1(1504faaf07c541330cd43b72dc6846911dfd85a3) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-4.23",     0x20000, 0x08000, CRC(6ea16072) SHA1(0b3b84a0d54f7a3aba411586009babbfee653f9a) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "21jm-0.ic55",    0xc000, 0x4000, CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, CRC(bb0bc76f) SHA1(37b2225e0593335f636c1e5fded9b21fdeab2f5a) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, CRC(5effb0a0) SHA1(1f21acb15dad824e831ed9a42b3fde096bb31141) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 ) /* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",     0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0.16",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */
ROM_END

ROM_START( ddragonua )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "21a-1",     0x08000, 0x08000, CRC(1d625008) SHA1(84cc19a55e7c91fca1943d9624d93e0347ed4150) )
	ROM_LOAD( "21a-2_4",   0x10000, 0x08000, CRC(5cd67657) SHA1(96bc7a5354a76524bd43a4d7eb8b0053a89e39c4) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-3",     0x18000, 0x08000, CRC(dbf24897) SHA1(1504faaf07c541330cd43b72dc6846911dfd85a3) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-4_2",   0x20000, 0x08000, CRC(9b019598) SHA1(59f3aa15389f53c4646d21a39634cb1502e66ff6) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "21jm-0.ic55",    0xc000, 0x4000, CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, CRC(bb0bc76f) SHA1(37b2225e0593335f636c1e5fded9b21fdeab2f5a) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, CRC(5effb0a0) SHA1(1f21acb15dad824e831ed9a42b3fde096bb31141) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",     0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0.16",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */
ROM_END

ROM_START( ddragonub )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "21a-1_6.bin",0x08000,0x08000, CRC(f354b0e1) SHA1(f2fe5d6102564691a0054d2b8dd98673fdc8a348) )
	ROM_LOAD( "21a-2_4",   0x10000, 0x08000, CRC(5cd67657) SHA1(96bc7a5354a76524bd43a4d7eb8b0053a89e39c4) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-3",     0x18000, 0x08000, CRC(dbf24897) SHA1(1504faaf07c541330cd43b72dc6846911dfd85a3) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-4_2",   0x20000, 0x08000, CRC(9b019598) SHA1(59f3aa15389f53c4646d21a39634cb1502e66ff6) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "21jm-0.ic55",    0xc000, 0x4000, CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, CRC(bb0bc76f) SHA1(37b2225e0593335f636c1e5fded9b21fdeab2f5a) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, CRC(5effb0a0) SHA1(1f21acb15dad824e831ed9a42b3fde096bb31141) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",     0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0.16",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */
ROM_END

ROM_START( ddragonb ) /* Same program roms as the World set */
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "21j-1.26",     0x08000, 0x08000, CRC(ae714964) SHA1(072522b97ca4edd099c6b48d7634354dc7088c53) )
	ROM_LOAD( "21j-2-3.25",   0x10000, 0x08000, CRC(5779705e) SHA1(4b8f22225d10f5414253ce0383bbebd6f720f3af) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21a-3.24",     0x18000, 0x08000, CRC(dbf24897) SHA1(1504faaf07c541330cd43b72dc6846911dfd85a3) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "21j-4.23",     0x20000, 0x08000, CRC(6c9f46fa) SHA1(df251a4aea69b2328f7a543bf085b9c35933e2c1) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "ic38",         0x0c000, 0x04000, CRC(6a6a0325) SHA1(98a940a9f23ce9154ff94f7f2ce29efe9a92f71b) ) /* HD6903 instead of HD63701 */

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "21j-5",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, CRC(bb0bc76f) SHA1(37b2225e0593335f636c1e5fded9b21fdeab2f5a) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, CRC(5effb0a0) SHA1(1f21acb15dad824e831ed9a42b3fde096bb31141) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "21j-7",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",     0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0.16",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */
ROM_END

ROM_START( ddragonba )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "5.bin",     0x08000, 0x08000, CRC(ae714964) SHA1(072522b97ca4edd099c6b48d7634354dc7088c53) )
	ROM_LOAD( "4.bin",     0x10000, 0x08000, CRC(48045762) SHA1(ca39eea71ca76627a98210ce9cc61457a58f16b9) ) /* banked at 0x4000-0x8000 */
	ROM_CONTINUE(0x20000, 0x8000) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "3.bin",     0x18000, 0x08000, CRC(dbf24897) SHA1(1504faaf07c541330cd43b72dc6846911dfd85a3) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "2_32.bin",         0x0c000, 0x04000, CRC(67875473) SHA1(66405cb22d41d353335f037ce5aee69e4c6f05c4) ) /* 6803 instead of HD63701 */

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "6.bin",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "1.bin",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "21j-a",        0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "21j-b",        0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "21j-c",        0x20000, 0x10000, CRC(bb0bc76f) SHA1(37b2225e0593335f636c1e5fded9b21fdeab2f5a) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "21j-e",        0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "21j-f",        0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "21j-g",        0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "21j-8",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "21j-i",        0x20000, 0x10000, CRC(5effb0a0) SHA1(1f21acb15dad824e831ed9a42b3fde096bb31141) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "8.bin",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "7.bin",        0x10000, 0x10000, CRC(f9311f72) SHA1(aa554ef020e04dc896e5495bcddc64e489d0ffff) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",     0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0.16",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */
ROM_END

ROM_START( ddragonb2 )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "b2_4.bin",        0x08000, 0x08000, CRC(668dfa19) SHA1(9b2ff1b66eeba0989e4ed850b7df1f5719ba5572) )
	ROM_LOAD( "b2_5.bin",        0x10000, 0x08000, CRC(5779705e) SHA1(4b8f22225d10f5414253ce0383bbebd6f720f3af) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "b2_6.bin",        0x18000, 0x08000, CRC(3bdea613) SHA1(d9038c80646a6ce3ea61da222873237b0383680e) ) /* banked at 0x4000-0x8000 */
	ROM_LOAD( "b2_7.bin",        0x20000, 0x08000, CRC(728f87b9) SHA1(d7442be24d41bb9fc021587ef44ae5b830e4503d) ) /* banked at 0x4000-0x8000 */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "63701.bin",    0xc000, 0x4000, CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "b2_3.bin",     0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) )

	ROM_REGION( 0x08000, "gfx1", 0 )
	ROM_LOAD( "b2_8.bin",        0x00000, 0x08000, CRC(7a8b8db4) SHA1(8368182234f9d4d763d4714fd7567a9e31b7ebeb) )	/* chars */

	ROM_REGION( 0x80000, "gfx2", 0 )
	ROM_LOAD( "11.bin",       0x00000, 0x10000, CRC(574face3) SHA1(481fe574cb79d0159a65ff7486cbc945d50538c5) )	/* sprites */
	ROM_LOAD( "12.bin",       0x10000, 0x10000, CRC(40507a76) SHA1(74581a4b6f48100bddf20f319903af2fe36f39fa) )
	ROM_LOAD( "13.bin",       0x20000, 0x10000, CRC(c8b91e17) SHA1(0ce6f6ef68ecc7309a2923f7e756d5e2bf5c7a4a) )
	ROM_LOAD( "14.bin",       0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) )
	ROM_LOAD( "15.bin",       0x40000, 0x10000, CRC(a0a0c261) SHA1(25c534d82bd237386d447d72feee8d9541a5ded4) )
	ROM_LOAD( "16.bin",       0x50000, 0x10000, CRC(6ba152f6) SHA1(a301ff809be0e1471f4ff8305b30c2fa4aa57fae) )
	ROM_LOAD( "17.bin",       0x60000, 0x10000, CRC(3220a0b6) SHA1(24a16ea509e9aff82b9ddd14935d61bb71acff84) )
	ROM_LOAD( "18.bin",       0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "9.bin",        0x00000, 0x10000, CRC(7c435887) SHA1(ecb76f2148fa9773426f05aac208eb3ac02747db) )	/* tiles */
	ROM_LOAD( "10.bin",       0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) )
	ROM_LOAD( "19.bin",       0x20000, 0x10000, CRC(22d65df2) SHA1(2f286a24ea7af438b39126a4ed0c515745981416) )
	ROM_LOAD( "20.bin",       0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "b2_1.bin",     0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) )
	ROM_LOAD( "2.bin",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) )

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",     0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */
	ROM_LOAD( "21j-l-0.16",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */
ROM_END

ROM_START( ddragon2 )
	ROM_REGION( 0x30000, "maincpu", 0 )
	ROM_LOAD( "26a9-04.bin",  0x08000, 0x8000, CRC(f2cfc649) SHA1(d3f1e0bae02472914a940222e4f600170a91736d) )
	ROM_LOAD( "26aa-03.bin",  0x10000, 0x8000, CRC(44dd5d4b) SHA1(427c4e419668b41545928cfc96435c010ecdc88b) )
	ROM_LOAD( "26ab-0.bin",   0x18000, 0x8000, CRC(49ddddcd) SHA1(91dc53718d04718b313f23d86e241027c89d1a03) )
	ROM_LOAD( "26ac-0e.63",   0x20000, 0x8000, CRC(57acad2c) SHA1(938e2a78af38ecd7e9e08fb10acc1940f7585f5e) )

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite CPU 64kb (Upper 16kb = 0) */
	ROM_LOAD( "26ae-0.bin",   0x00000, 0x10000, CRC(ea437867) SHA1(cd910203af0565f981b9bdef51ea6e9c33ee82d3) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* music CPU, 64kb */
	ROM_LOAD( "26ad-0.bin",   0x00000, 0x8000, CRC(75e36cd6) SHA1(f24805f4f6925b3ac508e66a6fc25c275b05f3b9) )

	ROM_REGION( 0x10000, "gfx1", 0 )
	ROM_LOAD( "26a8-0e.19",   0x00000, 0x10000, CRC(4e80cd36) SHA1(dcae0709f27f32effb359f6b943f61b102749f2a) )	/* chars */

	ROM_REGION( 0xc0000, "gfx2", 0 )
	ROM_LOAD( "26j0-0.bin",   0x00000, 0x20000, CRC(db309c84) SHA1(ee095e4a3bc86737539784945decb1f63da47b9b) )	/* sprites */
	ROM_LOAD( "26j1-0.bin",   0x20000, 0x20000, CRC(c3081e0c) SHA1(c4a9ae151aae21073a2c79c5ac088c72d4f3d9db) )
	ROM_LOAD( "26af-0.bin",   0x40000, 0x20000, CRC(3a615aad) SHA1(ec90a35224a177d00327de6fd1a299df38abd790) )
	ROM_LOAD( "26j2-0.bin",   0x60000, 0x20000, CRC(589564ae) SHA1(1e6e0ef623545615e8409b6d3ba586a71e2612b6) )
	ROM_LOAD( "26j3-0.bin",   0x80000, 0x20000, CRC(daf040d6) SHA1(ab0fd5482625dbe64f0f0b0baff5dcde05309b81) )
	ROM_LOAD( "26a10-0.bin",  0xa0000, 0x20000, CRC(6d16d889) SHA1(3bc62b3e7f4ddc3200a9cf8469239662da80c854) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "26j4-0.bin",   0x00000, 0x20000, CRC(a8c93e76) SHA1(54d64f052971e7fa0d21c5ce12f87b0fa2b648d6) )	/* tiles */
	ROM_LOAD( "26j5-0.bin",   0x20000, 0x20000, CRC(ee555237) SHA1(f9698f3e57f933a43e508f60667c860dee034d05) )

	ROM_REGION( 0x40000, "oki", 0 )		/* adpcm samples */
	ROM_LOAD( "26j6-0.bin",   0x00000, 0x20000, CRC(a84b2a29) SHA1(9cb529e4939c16a0a42f45dd5547c76c2f86f07b) )
	ROM_LOAD( "26j7-0.bin",   0x20000, 0x20000, CRC(bc6a48d5) SHA1(04c434f8cd42a8f82a263548183569396f9b684d) )

	ROM_REGION( 0x0200, "proms", 0 )
	ROM_LOAD( "prom.16",      0x0000, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown (same as ddragon) */
ROM_END

ROM_START( ddragon2u )
	ROM_REGION( 0x30000, "maincpu", 0 )
	ROM_LOAD( "26a9-04.bin",  0x08000, 0x8000, CRC(f2cfc649) SHA1(d3f1e0bae02472914a940222e4f600170a91736d) )
	ROM_LOAD( "26aa-03.bin",  0x10000, 0x8000, CRC(44dd5d4b) SHA1(427c4e419668b41545928cfc96435c010ecdc88b) )
	ROM_LOAD( "26ab-0.bin",   0x18000, 0x8000, CRC(49ddddcd) SHA1(91dc53718d04718b313f23d86e241027c89d1a03) )
	ROM_LOAD( "26ac-02.bin",  0x20000, 0x8000, CRC(097eaf26) SHA1(60504abd30fec44c45197cdf3832c87d05ef577d) )

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite CPU 64kb (Upper 16kb = 0) */
	ROM_LOAD( "26ae-0.bin",   0x00000, 0x10000, CRC(ea437867) SHA1(cd910203af0565f981b9bdef51ea6e9c33ee82d3) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* music CPU, 64kb */
	ROM_LOAD( "26ad-0.bin",   0x00000, 0x8000, CRC(75e36cd6) SHA1(f24805f4f6925b3ac508e66a6fc25c275b05f3b9) )

	ROM_REGION( 0x10000, "gfx1", 0 )
	ROM_LOAD( "26a8-0.bin",   0x00000, 0x10000, CRC(3ad1049c) SHA1(11d9544a56f8e6a84beb307a5c8a9ff8afc55c66) )	/* chars */

	ROM_REGION( 0xc0000, "gfx2", 0 )
	ROM_LOAD( "26j0-0.bin",   0x00000, 0x20000, CRC(db309c84) SHA1(ee095e4a3bc86737539784945decb1f63da47b9b) )	/* sprites */
	ROM_LOAD( "26j1-0.bin",   0x20000, 0x20000, CRC(c3081e0c) SHA1(c4a9ae151aae21073a2c79c5ac088c72d4f3d9db) )
	ROM_LOAD( "26af-0.bin",   0x40000, 0x20000, CRC(3a615aad) SHA1(ec90a35224a177d00327de6fd1a299df38abd790) )
	ROM_LOAD( "26j2-0.bin",   0x60000, 0x20000, CRC(589564ae) SHA1(1e6e0ef623545615e8409b6d3ba586a71e2612b6) )
	ROM_LOAD( "26j3-0.bin",   0x80000, 0x20000, CRC(daf040d6) SHA1(ab0fd5482625dbe64f0f0b0baff5dcde05309b81) )
	ROM_LOAD( "26a10-0.bin",  0xa0000, 0x20000, CRC(6d16d889) SHA1(3bc62b3e7f4ddc3200a9cf8469239662da80c854) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "26j4-0.bin",   0x00000, 0x20000, CRC(a8c93e76) SHA1(54d64f052971e7fa0d21c5ce12f87b0fa2b648d6) )	/* tiles */
	ROM_LOAD( "26j5-0.bin",   0x20000, 0x20000, CRC(ee555237) SHA1(f9698f3e57f933a43e508f60667c860dee034d05) )

	ROM_REGION( 0x40000, "oki", 0 )		/* adpcm samples */
	ROM_LOAD( "26j6-0.bin",   0x00000, 0x20000, CRC(a84b2a29) SHA1(9cb529e4939c16a0a42f45dd5547c76c2f86f07b) )
	ROM_LOAD( "26j7-0.bin",   0x20000, 0x20000, CRC(bc6a48d5) SHA1(04c434f8cd42a8f82a263548183569396f9b684d) )

	ROM_REGION( 0x0200, "proms", 0 )
	ROM_LOAD( "prom.16",      0x0000, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown (same as ddragon) */
ROM_END

ROM_START( ddragon2b )
	ROM_REGION( 0x30000, "maincpu", 0 )
	ROM_LOAD( "3",   0x08000, 0x8000, CRC(5cc38bad) SHA1(8ebbb998cce48b5baa4a738c2d4c2e481e2637fb) )
	ROM_LOAD( "4",   0x10000, 0x8000, CRC(78750947) SHA1(6b8349c3cd27c37a4329cea213b6ff0167c4edee) )
	ROM_LOAD( "5",   0x18000, 0x8000, CRC(49ddddcd) SHA1(91dc53718d04718b313f23d86e241027c89d1a03) )
	ROM_LOAD( "6",   0x20000, 0x8000, CRC(097eaf26) SHA1(60504abd30fec44c45197cdf3832c87d05ef577d) )

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite CPU 64kb (Upper 16kb = 0) */
	ROM_LOAD( "2",   0x00000, 0x10000, CRC(ea437867) SHA1(cd910203af0565f981b9bdef51ea6e9c33ee82d3) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* music CPU, 64kb */
	ROM_LOAD( "11",   0x00000, 0x8000, CRC(75e36cd6) SHA1(f24805f4f6925b3ac508e66a6fc25c275b05f3b9) )

	ROM_REGION( 0x10000, "gfx1", 0 )
	ROM_LOAD( "1",   0x00000, 0x10000, CRC(3ad1049c) SHA1(11d9544a56f8e6a84beb307a5c8a9ff8afc55c66) )  /* chars */

	ROM_REGION( 0xc0000, "gfx2", 0 )
	ROM_LOAD( "27",   0x00000, 0x10000, CRC(fe42df5d) SHA1(aab801346c2db04263cb61c97c6e086387675586) )  /* sprites */
	ROM_LOAD( "26",   0x10000, 0x10000, CRC(42f582c6) SHA1(bb269f677321f706043aa33a12bd3ddda4c32e55) )
	ROM_LOAD( "23",   0x20000, 0x10000, CRC(e157319f) SHA1(8b898fa20329b12293e7cb7ffc2e1b17304f826f) )
	ROM_LOAD( "22",   0x30000, 0x10000, CRC(82e952c9) SHA1(d340262c11f3c0ef3640c487e6a78745a2fe97d4) )
	ROM_LOAD( "25",   0x40000, 0x10000, CRC(4a4a085d) SHA1(80786c6fda135af1f9e9d8191879ab27baf36167) )
	ROM_LOAD( "24",   0x50000, 0x10000, CRC(c9d52536) SHA1(54f9236c4d22e3fd79d66c3f45b134f1fc9a1d32) )
	ROM_LOAD( "21",   0x60000, 0x10000, CRC(32ab0897) SHA1(f992dc3876621896b6e1fd6518f576b48d54a631) )
	ROM_LOAD( "20",   0x70000, 0x10000, CRC(a68e168f) SHA1(6ae596c097d7d435b767207012de1d23316d86d4) )
	ROM_LOAD( "17",   0x80000, 0x10000, CRC(882f99b1) SHA1(2fbb9171a2c9ddab177efe1e89e96426643d382b) )
	ROM_LOAD( "16",   0x90000, 0x10000, CRC(e2fe3eca) SHA1(bfd2e91261b9a002a99998486a2b606d4ee2e59b) )
	ROM_LOAD( "18",   0xa0000, 0x10000, CRC(0e1c6c63) SHA1(506e43161992c41d9b77c1df11228117f0587cbd) )
	ROM_LOAD( "19",   0xb0000, 0x10000, CRC(0e21eae0) SHA1(0cde9cdc6dbe2015e7f38b391c78cf3f16658e5c) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "15",   0x00000, 0x10000, CRC(3c3f16f6) SHA1(2fccbf1dd072c59b5923631fc1c6d40f7ea63996))
	ROM_LOAD( "13",   0x10000, 0x10000, CRC(7c21be72) SHA1(9935c983d0f7613ee192758ddcd8d8592e8bf78a) )
	ROM_LOAD( "14",   0x20000, 0x10000, CRC(e92f91f4) SHA1(4351b2b117c1104dcdb6f48531ddad385691c945) )
	ROM_LOAD( "12",   0x30000, 0x10000, CRC(6896e2f7) SHA1(d230d2406ae451f59d1d0783b1d670a0d3e28d8c) )

	ROM_REGION( 0x40000, "oki", 0 )		/* adpcm samples */
	ROM_LOAD( "7",   0x00000, 0x10000, CRC(6d9e3f0f) SHA1(5c3e7fb2e46939dd3c540b9e1af9591dbfd15b19) )
	ROM_LOAD( "9",   0x10000, 0x10000, CRC(0c15dec9) SHA1(b0a6bb13216f4321b5fc01a649ea84d2d1d51088) )
	ROM_LOAD( "8",   0x20000, 0x10000, CRC(151b22b4) SHA1(3b0470df9b719dd76115d8c549010ec92e28d0d0) )
	ROM_LOAD( "10",  0x30000, 0x10000, CRC(ae2fc028) SHA1(94fea9088b7b412706b6aaf7aac856709649fb63) )

	ROM_REGION( 0x0200, "proms", 0 )	// wasn't in this set, is it still present?
	ROM_LOAD( "prom.16",      0x0000, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )    /* sprite timing (same as ddragon) */
ROM_END

ROM_START( ddragon2b2 )
	ROM_REGION( 0x30000, "maincpu", 0 )
	ROM_LOAD( "dd2ub-3.3g",   0x08000, 0x8000, CRC(f5bd19d2) SHA1(a3005fb42fb0500c2f8ad0cd8dcd146dde51fefb) )
	ROM_LOAD( "dd2ub-4.4g",   0x10000, 0x8000, CRC(78750947) SHA1(6b8349c3cd27c37a4329cea213b6ff0167c4edee) )
	ROM_LOAD( "26ab-0.bin",   0x18000, 0x8000, CRC(49ddddcd) SHA1(91dc53718d04718b313f23d86e241027c89d1a03) )
	ROM_LOAD( "dd2ub-6.5g",   0x20000, 0x8000, CRC(097eaf26) SHA1(60504abd30fec44c45197cdf3832c87d05ef577d) )

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite CPU 64kb (Upper 16kb = 0) */
	ROM_LOAD( "26ae-0.bin",   0x00000, 0x10000, CRC(ea437867) SHA1(cd910203af0565f981b9bdef51ea6e9c33ee82d3) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* music CPU, 64kb */
	ROM_LOAD( "26ad-0.bin",   0x00000, 0x8000, CRC(75e36cd6) SHA1(f24805f4f6925b3ac508e66a6fc25c275b05f3b9) )

	ROM_REGION( 0x10000, "gfx1", 0 )
	ROM_LOAD( "26a8-0.bin",   0x00000, 0x10000, CRC(3ad1049c) SHA1(11d9544a56f8e6a84beb307a5c8a9ff8afc55c66) )	/* chars */

	ROM_REGION( 0xc0000, "gfx2", 0 )
	ROM_LOAD( "dd2ub-27.8n",   0x00000, 0x10000, CRC(fe42df5d) SHA1(aab801346c2db04263cb61c97c6e086387675586) )	/* sprites */
	ROM_LOAD( "dd2ub-26.7n",   0x10000, 0x10000, CRC(d2d9a400) SHA1(420abcb9c1de1be9bff36a572c0e100bb9a74e07) )
	ROM_LOAD( "dd2ub-23.8k",   0x20000, 0x10000, CRC(e157319f) SHA1(8b898fa20329b12293e7cb7ffc2e1b17304f826f) )
	ROM_LOAD( "dd2ub-22.7k",   0x30000, 0x10000, CRC(9f10018c) SHA1(a3da267312c01b3da192e745ed47ab3bed1fc3de) )
	ROM_LOAD( "dd2ub-25.6n",   0x40000, 0x10000, CRC(4a4a085d) SHA1(80786c6fda135af1f9e9d8191879ab27baf36167) )
	ROM_LOAD( "dd2ub-24.5n",   0x50000, 0x10000, CRC(c9d52536) SHA1(54f9236c4d22e3fd79d66c3f45b134f1fc9a1d32) )
	ROM_LOAD( "dd2ub-21.8g",   0x60000, 0x10000, CRC(32ab0897) SHA1(f992dc3876621896b6e1fd6518f576b48d54a631) )
	ROM_LOAD( "dd2ub-20.7g",   0x70000, 0x10000, CRC(f564bd18) SHA1(1981e63b7d564c2575f784a7ab372bfba27172db) )
	ROM_LOAD( "dd2ub-17.8d",   0x80000, 0x10000, CRC(882f99b1) SHA1(2fbb9171a2c9ddab177efe1e89e96426643d382b) )
	ROM_LOAD( "dd2ub-16.7d",   0x90000, 0x10000, CRC(cf3c34d5) SHA1(e28bac1d31bd791af8dc5227c3d2f69026a9d80e) )
	ROM_LOAD( "dd2ub-18.9d",   0xa0000, 0x10000, CRC(0e1c6c63) SHA1(506e43161992c41d9b77c1df11228117f0587cbd) )
	ROM_LOAD( "dd2ub-19.10d",  0xb0000, 0x10000, CRC(0e21eae0) SHA1(0cde9cdc6dbe2015e7f38b391c78cf3f16658e5c) )

	ROM_REGION( 0x40000, "gfx3", 0 )
	ROM_LOAD( "dd2ub-15.5d",   0x00000, 0x10000, CRC(3c3f16f6) SHA1(2fccbf1dd072c59b5923631fc1c6d40f7ea63996))
	ROM_LOAD( "dd2ub-13.4d",   0x10000, 0x10000, CRC(7c21be72) SHA1(9935c983d0f7613ee192758ddcd8d8592e8bf78a) )
	ROM_LOAD( "dd2ub-14.5b",   0x20000, 0x10000, CRC(e92f91f4) SHA1(4351b2b117c1104dcdb6f48531ddad385691c945) )
	ROM_LOAD( "dd2ub-12.4b",   0x30000, 0x10000, CRC(6896e2f7) SHA1(d230d2406ae451f59d1d0783b1d670a0d3e28d8c) )

	ROM_REGION( 0x40000, "oki", 0 )		/* adpcm samples */
	ROM_LOAD( "dd2ub-7.3f",   0x00000, 0x10000, CRC(6d9e3f0f) SHA1(5c3e7fb2e46939dd3c540b9e1af9591dbfd15b19) )
	ROM_LOAD( "dd2ub-9.5c",   0x10000, 0x10000, CRC(0c15dec9) SHA1(b0a6bb13216f4321b5fc01a649ea84d2d1d51088) )
	ROM_LOAD( "dd2ub-8.3d",   0x20000, 0x10000, CRC(151b22b4) SHA1(3b0470df9b719dd76115d8c549010ec92e28d0d0) )
	ROM_LOAD( "dd2ub-10.5b",  0x30000, 0x10000, CRC(95885e12) SHA1(518186d329d2ed148d732ad56173ba1cc0406246) )
ROM_END

ROM_START( ddungeon )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* Main CPU? */
	ROM_LOAD( "dd25.25",    0x10000, 0x8000,  CRC(922e719c) SHA1(d1c73f56913cd368158abc613d7bbab669509742) )
	ROM_LOAD( "dd26.26",    0x08000, 0x8000,  CRC(a6e7f608) SHA1(83b9301c39bfdc1e50a37f2bdc4d4f65a1111bee) )
	/* IC23 is replaced with a daughterboard containing a 68705 MCU */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "63701.bin",  0xc000,  0x4000,  CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "dd30.30",    0x08000, 0x08000, CRC(ef1af99a) SHA1(7ced695b81ca9efbb7b28b78013e112edac85672) )

	ROM_REGION( 0x0800, "mcu", 0 )		/* 8k for the microcontroller */
	ROM_LOAD( "dd_mcu.bin", 0x00000, 0x0800,  CRC(34cbb2d3) SHA1(8e0c3b13c636012d88753d547c639b1a8af85680) )

	ROM_REGION( 0x10000, "gfx1", 0 )	/* GFX? */
	ROM_LOAD( "dd20.20",    0x00000, 0x08000, CRC(d976b78d) SHA1(e1cd47032a0f91d812c3925d1f1267a9972bf48e) )

	ROM_REGION( 0x20000, "gfx2", 0 )	/* GFX */
	ROM_LOAD( "dd117.117",  0x00000, 0x08000, CRC(e912ca81) SHA1(8c274400170f46f84042f4f9cffba8d2fe9fbc10) )
	ROM_LOAD( "dd113.113",  0x10000, 0x08000, CRC(43264ad8) SHA1(74f031d6179390bc4fa99f4929a6886db8c2b510) )

	ROM_REGION( 0x20000, "gfx3", 0 )	/* GFX */
	ROM_LOAD( "dd78.78",    0x00000, 0x08000, CRC(3deacae9) SHA1(6663f054ed3eed50c5cacfa5d22d465dfb179964) )
	ROM_LOAD( "dd109.109",  0x10000, 0x08000, CRC(5a2f31eb) SHA1(1b85533443e148adb2a9c2c09c43cbf2c35c86bc) )

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "21j-6",      0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) ) /* at IC95 */
	ROM_LOAD( "21j-7",      0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) ) /* at IC94 */

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0",    0x0000,  0x0100,  CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) ) /* at IC101 */
	ROM_LOAD( "21j-l-0",    0x0100,  0x0200,  CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) ) /* at IC16 */
ROM_END

ROM_START( darktowr )
	ROM_REGION( 0x30000, "maincpu", 0 )	/* 64k for code + bankswitched memory */
	ROM_LOAD( "dt.26",         0x08000, 0x08000, CRC(8134a472) SHA1(7d42d2ed8d09855241d98ed94bce140a314c2f66) )
	ROM_LOAD( "21j-2-3.25",    0x10000, 0x08000, CRC(5779705e) SHA1(4b8f22225d10f5414253ce0383bbebd6f720f3af) ) /* from ddragon */
	ROM_LOAD( "dt.24",         0x18000, 0x08000, CRC(523a5413) SHA1(71c04287e4f2e792c98abdeb97fe70abd0d5e918) ) /* banked at 0x4000-0x8000 */
	/* IC23 is replaced with a daughterboard containing a 68705 MCU */

	ROM_REGION( 0x10000, "sub", 0 )		/* sprite cpu */
	ROM_LOAD( "63701.bin",    0xc000, 0x4000, CRC(f5232d03) SHA1(e2a194e38633592fd6587690b3cb2669d93985c7) )

	ROM_REGION( 0x10000, "soundcpu", 0 )	/* audio cpu */
	ROM_LOAD( "21j-0-1",      0x08000, 0x08000, CRC(9efa95bb) SHA1(da997d9cc7b9e7b2c70a4b6d30db693086a6f7d8) ) /* from ddragon */

	ROM_REGION( 0x0800, "mcu", 0 )		/* 8k for the microcontroller */
	ROM_LOAD( "68705prt.mcu",   0x00000, 0x0800, CRC(34cbb2d3) SHA1(8e0c3b13c636012d88753d547c639b1a8af85680) )

	ROM_REGION( 0x08000, "gfx1", 0 )	/* chars */
	ROM_LOAD( "dt.20",        0x00000, 0x08000, CRC(860b0298) SHA1(087e4e6511c5bed74ffbfd077ece55a756b13253) )

	ROM_REGION( 0x80000, "gfx2", 0 )	/* sprites */
	ROM_LOAD( "dt.117",       0x00000, 0x10000, CRC(750dd0fa) SHA1(d95b95a54c7ed87a27edb8660810dd89efa10c9f) )
	ROM_LOAD( "dt.116",       0x10000, 0x10000, CRC(22cfa87b) SHA1(0008a41f307be96be91f491bdeaa1fa450dd0fdf) )
	ROM_LOAD( "dt.115",       0x20000, 0x10000, CRC(8a9f1c34) SHA1(1f07f424b2ab14a051f2c84b3d89fc5d35c5f20b) )
	ROM_LOAD( "21j-d",        0x30000, 0x10000, CRC(cb4f231b) SHA1(9f2270f9ceedfe51c5e9a9bbb00d6f43dbc4a3ea) ) /* from ddragon */
	ROM_LOAD( "dt.113",       0x40000, 0x10000, CRC(7b4bbf9c) SHA1(d0caa3c38e059d3ee48e3e801da36f67457ed542) )
	ROM_LOAD( "dt.112",       0x50000, 0x10000, CRC(df3709d4) SHA1(9cca44be97260e730786db8244a0d655c86537aa) )
	ROM_LOAD( "dt.111",       0x60000, 0x10000, CRC(59032154) SHA1(637372e4619472a958f4971b50a6fe0985bffc8b) )
	ROM_LOAD( "21j-h",        0x70000, 0x10000, CRC(65c7517d) SHA1(f177ba9c1c7cc75ff04d5591b9865ee364788f94) ) /* from ddragon */

	ROM_REGION( 0x40000, "gfx3", 0 )	/* tiles */
	ROM_LOAD( "dt.78",        0x00000, 0x10000, CRC(72c15604) SHA1(202b46a2445eea5877e986a871bb0a6b76b88a6f) )
	ROM_LOAD( "21j-9",        0x10000, 0x10000, CRC(c6640aed) SHA1(f156c337f48dfe4f7e9caee9a72c7ea3d53e3098) ) /* from ddragon */
	ROM_LOAD( "dt.109",       0x20000, 0x10000, CRC(15bdcb62) SHA1(75382a3805dc333b196e119d28b5c3f320bd9f2a) )
	ROM_LOAD( "21j-j",        0x30000, 0x10000, CRC(5fb42e7c) SHA1(7953316712c56c6f8ca6bba127319e24b618b646) ) /* from ddragon */

	ROM_REGION( 0x20000, "adpcm", 0 )	/* adpcm samples */
	ROM_LOAD( "21j-6",        0x00000, 0x10000, CRC(34755de3) SHA1(57c06d6ce9497901072fa50a92b6ed0d2d4d6528) ) /* from ddragon */
	ROM_LOAD( "21j-7",        0x10000, 0x10000, CRC(904de6f8) SHA1(3623e5ea05fd7c455992b7ed87e605b87c3850aa) ) /* from ddragon */

	ROM_REGION( 0x0300, "proms", 0 )
	ROM_LOAD( "21j-k-0.101",     0x0000, 0x0100, CRC(fdb130a9) SHA1(4c4f214229b9fab2b5d69c745ec5428787b89e1f) )	/* unknown */ /* from ddragon */
	ROM_LOAD( "21j-l-0.16",      0x0100, 0x0200, CRC(46339529) SHA1(64f4c42a826d67b7cbaa8a23a45ebc4eb6248891) )	/* unknown */ /* from ddragon */
ROM_END


/*************************************
 *
 *  Driver-specific initialization
 *
 *************************************/

static DRIVER_INIT( ddragon )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	state->sprite_irq = INPUT_LINE_NMI;
	state->sound_irq = M6809_IRQ_LINE;
	state->ym_irq = M6809_FIRQ_LINE;
	state->technos_video_hw = 0;
}

static DRIVER_INIT( ddragon2 )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	state->sprite_irq = INPUT_LINE_NMI;
	state->sound_irq = INPUT_LINE_NMI;
	state->ym_irq = 0;
	state->technos_video_hw = 2;
}

static DRIVER_INIT( darktowr )
{
	ddragon_state *state = machine->driver_data<ddragon_state>();

	state->sprite_irq = INPUT_LINE_NMI;
	state->sound_irq = M6809_IRQ_LINE;
	state->ym_irq = M6809_FIRQ_LINE;
	state->technos_video_hw = 0;
	memory_install_write8_handler(cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM), 0x3808, 0x3808, 0, 0, darktowr_bankswitch_w);
}


/*************************************
 *
 *  Game drivers
 *
 *************************************/

GAME( 1987,   ddragon,	   0,         ddragon,	   ddragon,	ddragon,  ROT0, "Technos", "Double Dragon (Japan)", GAME_SUPPORTS_SAVE )
GAME( 1987,   ddragonw,    ddragon,   ddragon,	   ddragon,	ddragon,  ROT0, "Technos (Taito license)", "Double Dragon (World Set 1)", GAME_SUPPORTS_SAVE )
GAME( 1987,   ddragonw1,   ddragon,   ddragon,	   ddragon,	ddragon,  ROT0, "Technos (Taito license)", "Double Dragon (World Set 2)", GAME_SUPPORTS_SAVE )
GAME( 1987,   ddragonu,    ddragon,   ddragon,	   ddragon,	ddragon,  ROT0, "Technos (Taito America license)", "Double Dragon (US Set 1)", GAME_SUPPORTS_SAVE )
GAME( 1987,   ddragonua,   ddragon,   ddragon,	   ddragon,	ddragon,  ROT0, "Technos (Taito America license)", "Double Dragon (US Set 2)", GAME_SUPPORTS_SAVE )
GAME( 1987,   ddragonub,   ddragon,   ddragon,	   ddragon,	ddragon,  ROT0, "Technos (Taito America license)", "Double Dragon (US Set 3)", GAME_SUPPORTS_SAVE )
GAME( 1987,   ddragonb2,   ddragon,   ddragon,	   ddragon,	ddragon,  ROT0, "bootleg", "Double Dragon (bootleg)", GAME_SUPPORTS_SAVE )
GAME( 1987,   ddragonb,    ddragon,   ddragonb,	   ddragon,	ddragon,  ROT0, "bootleg", "Double Dragon (bootleg with HD6309)", GAME_SUPPORTS_SAVE ) // according to dump notes
GAME( 1987,   ddragonba,   ddragon,   ddragonba,   ddragon,	ddragon,  ROT0, "bootleg", "Double Dragon (bootleg with M6803)", GAME_SUPPORTS_SAVE )

GAME( 1988,   ddragon2,	   0,	      ddragon2,	   ddragon2,	ddragon2, ROT0, "Technos", "Double Dragon II - The Revenge (World)", GAME_SUPPORTS_SAVE )
GAME( 1988,   ddragon2u,   ddragon2,  ddragon2,	   ddragon2,	ddragon2, ROT0, "Technos", "Double Dragon II - The Revenge (US)", GAME_SUPPORTS_SAVE )
GAME( 1988,   ddragon2b,   ddragon2,  ddragon2,	   ddragon2,	ddragon2, ROT0, "bootleg", "Double Dragon II - The Revenge (US bootleg, set 1)", GAME_SUPPORTS_SAVE )
GAME( 1988,   ddragon2b2,  ddragon2,  ddragon2,	   ddragon2,	ddragon2, ROT0, "bootleg", "Double Dragon II - The Revenge (US bootleg, set 2)", GAME_SUPPORTS_SAVE )

GAME( 1992,   ddungeon,	   0,	      darktowr,	   ddungeon,	darktowr, ROT0, "Game Room", "Dangerous Dungeons", GAME_SUPPORTS_SAVE )
GAME( 1992,   darktowr,	   0,	      darktowr,	   darktowr,	darktowr, ROT0, "Game Room", "Dark Tower", GAME_SUPPORTS_SAVE )
