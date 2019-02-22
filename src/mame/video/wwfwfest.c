/*******************************************************************************
	WWF Wrestlefest (C) 1991 Technos Japan  (video/wwfwfest.c)
********************************************************************************
		driver by David Haywood

	see (drivers/wwfwfest.c) for more notes
*******************************************************************************/

#include "emu.h"
#include "includes/wwfwfest.h"

/*******************************************************************************
	Write Handlers
********************************************************************************
	for writes to Video Ram
*******************************************************************************/

WRITE16_HANDLER( wwfwfest_fg0_videoram_w )
{
	wwfwfest_state *state = space->machine->driver_data<wwfwfest_state>();

	/* Videoram is 8 bit, upper & lower byte writes end up in the same place due to m68k byte smearing */
	state->wwfwfest_fg0_videoram[offset] = data & 0xff;
	tilemap_mark_tile_dirty(state->fg0_tilemap, offset / 2);
}

WRITE16_HANDLER( wwfwfest_bg0_videoram_w )
{
	wwfwfest_state *state = space->machine->driver_data<wwfwfest_state>();

	COMBINE_DATA(&state->wwfwfest_bg0_videoram[offset]);
	tilemap_mark_tile_dirty(state->bg0_tilemap, offset / 2);
}

WRITE16_HANDLER( wwfwfest_bg1_videoram_w )
{
	wwfwfest_state *state = space->machine->driver_data<wwfwfest_state>();

	COMBINE_DATA(&state->wwfwfest_bg1_videoram[offset]);
	tilemap_mark_tile_dirty(state->bg1_tilemap, offset);
}

/*******************************************************************************
	Tilemap Related Functions
*******************************************************************************/
static TILE_GET_INFO( get_fg0_tile_info )
{
	wwfwfest_state *state = machine->driver_data<wwfwfest_state>();

	/*- FG0 RAM Format -**

	4 bytes per tile

	---- ----  tttt tttt  ---- ----  ???? TTTT

	C = Colour Bank (0-15)
	T = Tile Number (0 - 4095)

	other bits unknown / unused

	basically the same as WWF Superstar's FG0 Ram but
	more of it and the used bytes the other way around

	**- End of Comments -*/

	UINT16 *tilebase = &state->wwfwfest_fg0_videoram[tile_index * 2];
	int tileno =  (tilebase[0] & 0x00ff) | ((tilebase[1] & 0x000f) << 8);
	int colbank = (tilebase[1] & 0x00f0) >> 4;

	SET_TILE_INFO(	0,
			tileno,
			colbank,
			0);
}

static TILE_GET_INFO( get_bg0_tile_info )
{
	wwfwfest_state *state = machine->driver_data<wwfwfest_state>();

	/*- BG0 RAM Format -**

	4 bytes per tile

	---- ----  fF-- CCCC  ---- TTTT tttt tttt

	C = Colour Bank (0-15)
	T = Tile Number (0 - 4095)
	f = Flip Y
	F = Flip X

	other bits unknown / unused

	**- End of Comments -*/

	UINT16 *tilebase = &state->wwfwfest_bg0_videoram[tile_index * 2];
	int tileno = tilebase[1] & 0x0fff;
	int colbank = tilebase[0] & 0x000f;

	SET_TILE_INFO(	2,
			tileno,
			colbank,
			TILE_FLIPYX((tilebase[0] & 0x00c0) >> 6)
			);
}

static TILE_GET_INFO( get_bg1_tile_info )
{
	wwfwfest_state *state = machine->driver_data<wwfwfest_state>();

	/*- BG1 RAM Format -**

	2 bytes per tile

	CCCC TTTT tttt tttt

	C = Colour Bank (0-15)
	T = Tile Number (0 - 4095)

	**- End of Comments -*/

	UINT16 *tilebase = &state->wwfwfest_bg1_videoram[tile_index];
	int tileno =  (tilebase[0] & 0x0fff);
	int colbank = (tilebase[0] & 0xf000) >> 12;

	SET_TILE_INFO(	3,
			tileno,
			colbank,
			0);
}

/*******************************************************************************
	Sprite Related Functions
********************************************************************************
	sprite drawing could probably be improved a bit
*******************************************************************************/

static void draw_sprites( running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect )
{
	wwfwfest_state *state = machine->driver_data<wwfwfest_state>();

	/*- SPR RAM Format -**

	16 bytes per sprite

	---- ----  yyyy yyyy  ---- ----  lllF fXYE  ---- ----  nnnn nnnn  ---- ----  NNNN NNNN
	---- ----  ---- CCCC  ---- ----  xxxx xxxx  ---- ----  ---- ----  ---- ----  ---- ----

	Yy = sprite Y Position
	Xx = sprite X Position
	C  = colour bank
	f  = flip Y
	F  = flip X
	l  = chain sprite
	E  = sprite enable
	Nn = Sprite Number

	other bits unused

	**- End of Comments - */

	const gfx_element *gfx = machine->gfx[1];
	UINT16 *source = machine->generic.buffered_spriteram.u16;
	UINT16 *finish = source + 0x2000 / 2;
	INT32 xpos, ypos, flipx, flipy;
	UINT32 colourbank, chain, number, count;

	for ( ; source < finish; source += 8)
	{
		if (source[1] & 0x0001)
		{
			xpos = +(source[5] & 0x00ff) | (source[1] & 0x0004) << 6;
			if (xpos > 512 - 16)
				xpos -= 512;
			xpos += state->sprite_xoff;

			ypos = (source[0] & 0x00ff) | (source[1] & 0x0002) << 7;
			ypos = ((256 - ypos) & 0x01ff) - 16;

			flipx = (source[1] & 0x0010) >> 4;
			flipy = (source[1] & 0x0008) >> 3;

			if (flip_screen_get(machine))
			{
				flipy = flipy ? 0 : 1;
				flipx = flipx ? 0 : 1;

				ypos = 240 - ypos - state->sprite_xoff;
				xpos = 304 - xpos;
			}

			chain = ((source[1] & 0x00e0) >> 5) + 1;
			colourbank = (source[4] & 0x000f);
			number = (source[2] & 0x00ff) | (source[3] & 0x00ff) << 8;

			for (count = 0; count < chain; count++)
			{
				if (flip_screen_get(machine))
				{
					if (!flipy)
						drawgfx_transpen(bitmap, cliprect, gfx, number + count, colourbank, flipx, flipy, xpos, ypos + (16 * (chain - 1)) - (16 * count), 0);
					else
						drawgfx_transpen(bitmap, cliprect, gfx, number + count, colourbank, flipx, flipy, xpos, ypos + 16 * count, 0);
				}
				else
				{
					if (flipy)
						drawgfx_transpen(bitmap, cliprect, gfx, number + count, colourbank, flipx, flipy, xpos, ypos - (16 * (chain - 1)) + (16 * count), 0);
					else
						drawgfx_transpen(bitmap, cliprect, gfx, number + count, colourbank, flipx, flipy, xpos, ypos - 16 * count, 0);
				}
			}
		}
	}
}

/***************************************************************************************
	Video Start and Refresh Functions
****************************************************************************************
	Draw Order / Priority seems to affect where the scroll values are used also.
***************************************************************************************/

VIDEO_START( wwfwfest )
{
	wwfwfest_state *state = machine->driver_data<wwfwfest_state>();

	state->fg0_tilemap = tilemap_create(machine, get_fg0_tile_info, tilemap_scan_rows,  8,  8, 64, 32);
	state->bg1_tilemap = tilemap_create(machine, get_bg1_tile_info, tilemap_scan_rows, 16, 16, 32, 32);
	state->bg0_tilemap = tilemap_create(machine, get_bg0_tile_info, tilemap_scan_rows, 16, 16, 32, 32);
	tilemap_set_transparent_pen(state->fg0_tilemap, 0);
	tilemap_set_transparent_pen(state->bg1_tilemap, 0);
	tilemap_set_transparent_pen(state->bg0_tilemap, 0);
}

VIDEO_START( wwfwfstb )
{
	wwfwfest_state *state = machine->driver_data<wwfwfest_state>();

	VIDEO_START_CALL(wwfwfest);

	state->sprite_xoff = 2;
	state->bg0_dx = -4;
	state->bg1_dx[0] = -4;
	state->bg1_dx[1] = -2;
}

VIDEO_UPDATE( wwfwfest )
{
	wwfwfest_state *state = screen->machine->driver_data<wwfwfest_state>();

	if (state->wwfwfest_pri == 0x78)
	{
		tilemap_set_scrolly(state->bg0_tilemap, 0, state->wwfwfest_bg0_scrolly);
		tilemap_set_scrollx(state->bg0_tilemap, 0, state->wwfwfest_bg0_scrollx + state->bg0_dx);
		tilemap_set_scrolly(state->bg1_tilemap, 0, state->wwfwfest_bg1_scrolly);
		tilemap_set_scrollx(state->bg1_tilemap, 0, state->wwfwfest_bg1_scrollx + state->bg1_dx[0]);
	}
	else
	{
		tilemap_set_scrolly(state->bg1_tilemap, 0, state->wwfwfest_bg0_scrolly);
		tilemap_set_scrollx(state->bg1_tilemap, 0, state->wwfwfest_bg0_scrollx + state->bg1_dx[1]);
		tilemap_set_scrolly(state->bg0_tilemap, 0, state->wwfwfest_bg1_scrolly);
		tilemap_set_scrollx(state->bg0_tilemap, 0, state->wwfwfest_bg1_scrollx + state->bg0_dx);
	}

	/* todo : which bits of pri are significant to the order */
	if (state->wwfwfest_pri == 0x78)
	{
		tilemap_draw(bitmap, cliprect, state->bg1_tilemap, TILEMAP_DRAW_OPAQUE, 0);
		tilemap_draw(bitmap, cliprect, state->bg0_tilemap, 0, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
		tilemap_draw(bitmap, cliprect, state->fg0_tilemap, 0, 0);
	}
	else if (state->wwfwfest_pri == 0x7b)
	{
		tilemap_draw(bitmap, cliprect, state->bg0_tilemap, TILEMAP_DRAW_OPAQUE, 0);
		tilemap_draw(bitmap, cliprect, state->bg1_tilemap, 0, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
		tilemap_draw(bitmap, cliprect, state->fg0_tilemap, 0, 0);
	}
	else if (state->wwfwfest_pri == 0x7c)
	{
		tilemap_draw(bitmap, cliprect, state->bg0_tilemap, TILEMAP_DRAW_OPAQUE, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
		tilemap_draw(bitmap, cliprect, state->bg1_tilemap, 0, 0);
		tilemap_draw(bitmap, cliprect, state->fg0_tilemap, 0, 0);
	}

	return 0;
}
