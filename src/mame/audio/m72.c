/***************************************************************************

IREM "M72" sound hardware

All games have a YM2151 for music, and most of them also samples. Samples
are not handled consistently by all the games, some use a high frequency NMI
handler to push them through a DAC, others use external hardware.
In the following table, the NMI column indicates with a No the games whose
NMI handler only consists of RETN. R-Type is an exception, it doesn't have
a valid NMI handler at all.

Game                                    Year  ID string     NMI
--------------------------------------  ----  ------------  ---
R-Type                                  1987  - (earlier version, no samples)
Battle Chopper / Mr. Heli               1987  Rev 2.20      Yes
Vigilante                               1988  Rev 2.20      Yes
Ninja Spirit                            1988  Rev 2.20      Yes
Image Fight                             1988  Rev 2.20      Yes
Legend of Hero Tonma                    1989  Rev 2.20      Yes
X Multiply                              1989  Rev 2.20      Yes
Dragon Breed                            1989  Rev 2.20      Yes
Kickle Cubicle                          1988  Rev 2.21      Yes
Shisensho                               1989  Rev 2.21      Yes
R-Type II                               1989  Rev 2.21      Yes
Major Title                             1990  Rev 2.21      Yes
Air Duel                                1990  Rev 3.14 M72   No
Daiku no Gensan                         1990  Rev 3.14 M81  Yes
Daiku no Gensan (M72)                   1990  Rev 3.15 M72   No
Hammerin' Harry                         1990  Rev 3.15 M81  Yes
Ken-Go                                  1991  Rev 3.15 M81  Yes
Pound for Pound                         1990  Rev 3.15 M83   No
Cosmic Cop                              1991  Rev 3.15 M81  Yes
Gallop - Armed Police Unit              1991  Rev 3.15 M72   No
Hasamu                                  1991  Rev 3.15 M81  Yes
Bomber Man                              1991  Rev 3.15 M81  Yes
Bomber Man World (Japan)                1992  Rev 3.31 M81  Yes
Bomber Man World (World) / Atomic Punk  1992  Rev 3.31 M99   No
Quiz F-1 1,2finish                      1992  Rev 3.33 M81  Yes
Risky Challenge                         1993  Rev 3.34 M81  Yes
Shisensho II                            1993  Rev 3.34 M81  Yes

***************************************************************************/

#include "emu.h"
#include "sound/dac.h"
#include "m72.h"


/*

  The sound CPU runs in interrup mode 0. IRQ is shared by two sources: the
  YM2151 (bit 4 of the vector), and the main CPU (bit 5).
  Since the vector can be changed from different contexts (the YM2151 timer
  callback, the main CPU context, and the sound CPU context), it's important
  to accurately arbitrate the changes to avoid out-of-order execution. We do
  that by handling all vector changes in a single timer callback.

*/

static struct m72audio
{
	UINT8	       *samples;
	UINT32		sample_addr;
	UINT32		samples_size;
	UINT8		irq_vector;
} audio;

enum
{
	VECTOR_INIT,
	YM2151_ASSERT,
	YM2151_CLEAR,
	Z80_ASSERT,
	Z80_CLEAR
};

static TIMER_CALLBACK( setvector_callback )
{
	switch (param)
	{
		case VECTOR_INIT:
			audio.irq_vector  = 0xff;
		break;
		case YM2151_ASSERT:
			audio.irq_vector &= 0xef;
		break;
		case YM2151_CLEAR:
			audio.irq_vector |= 0x10;
		break;
		case Z80_ASSERT:
			audio.irq_vector &= 0xdf;
		break;
		case Z80_CLEAR:
			audio.irq_vector |= 0x20;
		break;
	}

	if (audio.irq_vector == 0)
		logerror("You didn't call m72_init_sound()\n");

	/* no IRQs pending */
	if (audio.irq_vector == 0xff)
		cputag_set_input_line_and_vector(machine, "soundcpu", 0, CLEAR_LINE, audio.irq_vector);
	/* IRQ pending */
	else
		cputag_set_input_line_and_vector(machine, "soundcpu", 0, ASSERT_LINE, audio.irq_vector);
}

SOUND_START( m72 )
{
	audio.samples = memory_region(machine, "samples");
	audio.samples_size = memory_region_length(machine, "samples");

	state_save_register_global(machine, audio.irq_vector);
	state_save_register_global(machine, audio.sample_addr);
}

SOUND_RESET( m72 )
{
	setvector_callback(machine, NULL, VECTOR_INIT);
}

void m72_ym2151_irq_handler(running_device *device, int irq)
{
	timer_call_after_resynch(device->machine, NULL, irq ? YM2151_ASSERT : YM2151_CLEAR, setvector_callback);
}

WRITE16_HANDLER( m72_sound_command_w )
{
	if (ACCESSING_BITS_0_7)
	{
		soundlatch_w(space, offset, data);
		timer_call_after_resynch(space->machine, NULL, Z80_ASSERT, setvector_callback);
	}
}

WRITE8_HANDLER( m72_sound_command_byte_w )
{
	soundlatch_w(space, offset, data);
	timer_call_after_resynch(space->machine, NULL, Z80_ASSERT, setvector_callback);
}

WRITE8_HANDLER( m72_sound_irq_ack_w )
{
	timer_call_after_resynch(space->machine, NULL, Z80_CLEAR, setvector_callback);
}

void m72_set_sample_start(int start)
{
	audio.sample_addr = start;
}

WRITE8_HANDLER( vigilant_sample_addr_w )
{
	if (offset == 1)
		audio.sample_addr = (audio.sample_addr & 0x00ff) | ((data << 8) & 0xff00);
	else
		audio.sample_addr = (audio.sample_addr & 0xff00) | ((data << 0) & 0x00ff);
}

WRITE8_HANDLER( shisen_sample_addr_w )
{
	audio.sample_addr >>= 2;

	if (offset == 1)
		audio.sample_addr = (audio.sample_addr & 0x00ff) | ((data << 8) & 0xff00);
	else
		audio.sample_addr = (audio.sample_addr & 0xff00) | ((data << 0) & 0x00ff);

	audio.sample_addr <<= 2;
}

WRITE8_HANDLER( rtype2_sample_addr_w )
{
	audio.sample_addr >>= 5;

	if (offset == 1)
		audio.sample_addr = (audio.sample_addr & 0x00ff) | ((data << 8) & 0xff00);
	else
		audio.sample_addr = (audio.sample_addr & 0xff00) | ((data << 0) & 0x00ff);

	audio.sample_addr <<= 5;
}

WRITE8_HANDLER( poundfor_sample_addr_w )
{
	/* poundfor writes both sample start and sample END - a first for Irem...
	   we don't handle the end written here, 00 marks the sample end as usual. */
	if (offset > 1) return;

	audio.sample_addr >>= 4;

	if (offset == 1)
		audio.sample_addr = (audio.sample_addr & 0x00ff) | ((data << 8) & 0xff00);
	else
		audio.sample_addr = (audio.sample_addr & 0xff00) | ((data << 0) & 0x00ff);

	audio.sample_addr <<= 4;
}

READ8_HANDLER( m72_sample_r )
{
	return audio.samples[audio.sample_addr];
}

WRITE8_DEVICE_HANDLER( m72_sample_w )
{
	dac_signed_data_w(device, data);
	audio.sample_addr = (audio.sample_addr + 1) & (audio.samples_size - 1);
}
