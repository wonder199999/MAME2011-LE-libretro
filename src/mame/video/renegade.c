/***************************************************************************

    Renegade Video Hardware

***************************************************************************/

#include "emu.h"
#include "includes/renegade.h"

WRITE8_HANDLER( bg_videoram_w )
{
	renegade_state *state = space->machine->driver_data<renegade_state>();

	state->bg_videoram[offset] = data;
	tilemap_mark_tile_dirty(state->bg_tilemap, offset & 0x03ff);
}

WRITE8_HANDLER( fg_videoram_w )
{
	renegade_state *state = space->machine->driver_data<renegade_state>();

	state->fg_videoram[offset] = data;
	tilemap_mark_tile_dirty(state->fg_tilemap, offset & 0x03ff);
}

WRITE8_HANDLER( renegade_flipscreen_w )
{
	flip_screen_set(space->machine, ~data & 0x01);
}

WRITE8_HANDLER( scroll_lsb_w )
{
	renegade_state *state = space->machine->driver_data<renegade_state>();

	state->scrollx = (state->scrollx & 0xff00) | data;
}

WRITE8_HANDLER( scroll_msb_w )
{
	renegade_state *state = space->machine->driver_data<renegade_state>();

	state->scrollx = (state->scrollx & 0xff) | (data << 8);
}

static TILE_GET_INFO( get_bg_tilemap_info )
{
	renegade_state *state = machine->driver_data<renegade_state>();

	const UINT8 *source = &state->bg_videoram[tile_index];
	UINT8 attributes = source[0x0400];	/* CCC??BBB */
	SET_TILE_INFO( (attributes & 0x07) + 1, source[0], attributes >> 5, 0 );
}

static TILE_GET_INFO( get_fg_tilemap_info )
{
	renegade_state *state = machine->driver_data<renegade_state>();

	const UINT8 *source = &state->fg_videoram[tile_index];
	UINT8 attributes = source[0x0400];
	SET_TILE_INFO( 0, (attributes & 0x03) << 8 | source[0], attributes >> 6, 0 );
}

VIDEO_START( renegade )
{
	renegade_state *state = machine->driver_data<renegade_state>();

	state->bg_tilemap = tilemap_create(machine, get_bg_tilemap_info, tilemap_scan_rows, 16, 16, 64, 16);
	state->fg_tilemap = tilemap_create(machine, get_fg_tilemap_info, tilemap_scan_rows, 8, 8, 32, 32);

	tilemap_set_transparent_pen(state->fg_tilemap, 0);
	tilemap_set_scrolldx(state->bg_tilemap, 256, 0);

	state_save_register_global(machine, state->scrollx);
}

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	UINT8 *source = machine->generic.spriteram.u8;
	UINT8 *finish = source + 0x0180;
	INT32 sx, sy;
	UINT32 attributes, sprite_number, sprite_bank, color, xflip;
	bool flip_screen_set;

	for ( ; source < finish; source += 4)
	{
		sy = 240 - source[0];

		if (sy >= 16)
		{
			attributes = source[1];		/* SFCCBBBB */
			sx = source[3];
			sprite_number = source[2];
			sprite_bank = 9 + (attributes & 0x0f);
			color = (attributes >> 4) & 0x03;
			xflip = attributes & 0x40;
			flip_screen_set = flip_screen_get(machine) ? TRUE : FALSE;

			if (sx > 248)
				sx -= 256;

			if (flip_screen_set)
			{
				sx = 240 - sx;
				sy = 240 - sy;
				xflip = !xflip;
			}

			if (attributes & 0x80)		/* big sprite */
			{
				drawgfx_transpen(bitmap, cliprect, machine->gfx[sprite_bank],
					sprite_number | 0x01, color, xflip,
					flip_screen_set,
					sx, sy + (flip_screen_set ? -16 : 16), 0);
			}
			else
				sy += flip_screen_set ? -16 : 16;

			drawgfx_transpen(bitmap, cliprect, machine->gfx[sprite_bank],
				sprite_number, color, xflip,
				flip_screen_set,
				sx, sy, 0);
		}
	}
}

VIDEO_UPDATE( renegade )
{
	renegade_state *state = screen->machine->driver_data<renegade_state>();

	tilemap_set_scrollx(state->bg_tilemap, 0, state->scrollx);
	tilemap_draw(bitmap, cliprect, state->bg_tilemap, 0 , 0);
	draw_sprites(screen->machine, bitmap, cliprect);
	tilemap_draw(bitmap, cliprect, state->fg_tilemap, 0 , 0);

	return 0;
}
