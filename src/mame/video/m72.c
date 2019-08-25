#include "emu.h"
#include "audio/m72.h"
#include "includes/m72.h"


/***************************************************************************

  Callbacks for the TileMap code

***************************************************************************/

INLINE void m72_get_tile_info(running_machine *machine, tile_data *tileinfo, int tile_index, const UINT16 *vram, int gfxnum)
{
	tile_index <<= 1;

	INT32 code = vram[tile_index] & 0xff;
	INT32 attr = vram[tile_index] >> 8;
	INT32 color = vram[tile_index + 1] & 0xff;
	INT32 pri = 0;

	if (color & 0x80)
		pri = 2;
	else if (color & 0x40)
		pri = 1;

	/* color & 0x10 is used in bchopper and hharry, more priority? */
	SET_TILE_INFO(	gfxnum,
			code + ((attr & 0x3f) << 8),
			color & 0x0f,
			TILE_FLIPYX((attr & 0xc0) >> 6));
	tileinfo->group = pri;
}

INLINE void rtype2_get_tile_info(running_machine *machine, tile_data *tileinfo, int tile_index, const UINT16 *vram, int gfxnum)
{
	tile_index <<= 1;

	INT32 code = vram[tile_index];
	INT32 attr = vram[tile_index + 1] >> 8;
	INT32 color = vram[tile_index + 1] & 0xff;
	INT32 pri = 0;

	if (attr & 0x01)
		pri = 2;
	else if (color & 0x80)
		pri = 1;

	/* (vram[tile_index+2] & 0x10) is used by majtitle on the green, but it's not clear for what */
	/* (vram[tile_index+3] & 0xfe) are used as well */
	SET_TILE_INFO(	gfxnum,
			code,
			color & 0x0f,
			TILE_FLIPYX((color & 0x60) >> 5));
	tileinfo->group = pri;
}


static TILE_GET_INFO( m72_get_bg_tile_info )
{
	m72_state *state = machine->driver_data<m72_state>();
	m72_get_tile_info(machine, tileinfo, tile_index, state->videoram2, 2);
}

static TILE_GET_INFO( m72_get_fg_tile_info )
{
	m72_state *state = machine->driver_data<m72_state>();
	m72_get_tile_info(machine, tileinfo, tile_index, state->videoram1, 1);
}

static TILE_GET_INFO( hharry_get_bg_tile_info )
{
	m72_state *state = machine->driver_data<m72_state>();
	m72_get_tile_info(machine, tileinfo, tile_index, state->videoram2, 1);
}

static TILE_GET_INFO( rtype2_get_bg_tile_info )
{
	m72_state *state = machine->driver_data<m72_state>();
	rtype2_get_tile_info(machine, tileinfo, tile_index, state->videoram2, 1);
}

static TILE_GET_INFO( rtype2_get_fg_tile_info )
{
	m72_state *state = machine->driver_data<m72_state>();
	rtype2_get_tile_info(machine, tileinfo, tile_index, state->videoram1, 1);
}


static TILEMAP_MAPPER( majtitle_scan_rows )
{
	/* logical (col,row) -> memory offset */
	return ((row << 8) + col);
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/

static void register_savestate(running_machine *machine)
{
	m72_state *state = machine->driver_data<m72_state>();

	state_save_register_global(machine, state->raster_irq_position);
	state_save_register_global(machine, state->video_off);
	state_save_register_global(machine, state->scrollx1);
	state_save_register_global(machine, state->scrolly1);
	state_save_register_global(machine, state->scrollx2);
	state_save_register_global(machine, state->scrolly2);
	state_save_register_global_pointer(machine, state->buffered_spriteram, state->spriteram_size / 2);
}


VIDEO_START( m72 )
{
	m72_state *state = machine->driver_data<m72_state>();

	state->bg_tilemap = tilemap_create(machine, m72_get_bg_tile_info, tilemap_scan_rows, 8, 8, 64, 64);
	state->fg_tilemap = tilemap_create(machine, m72_get_fg_tile_info, tilemap_scan_rows, 8, 8, 64, 64);

	state->buffered_spriteram = auto_alloc_array(machine, UINT16, state->spriteram_size / 2);

	tilemap_set_transmask(state->fg_tilemap, 0, 0xffff, 0x0001);
	tilemap_set_transmask(state->fg_tilemap, 1, 0x00ff, 0xff01);
	tilemap_set_transmask(state->fg_tilemap, 2, 0x0001, 0xffff);

	tilemap_set_transmask(state->bg_tilemap, 0, 0xffff, 0x0000);
	tilemap_set_transmask(state->bg_tilemap, 1, 0x00ff, 0xff00);
	tilemap_set_transmask(state->bg_tilemap, 2, 0x0007, 0xfff8);

	memset(state->buffered_spriteram, 0, state->spriteram_size);

	tilemap_set_scrolldx(state->fg_tilemap, 0, 0);
	tilemap_set_scrolldy(state->fg_tilemap, -128, 16);

	tilemap_set_scrolldx(state->bg_tilemap, 0, 0);
	tilemap_set_scrolldy(state->bg_tilemap, -128, 16);

	register_savestate(machine);
}

VIDEO_START( rtype2 )
{
	m72_state *state = machine->driver_data<m72_state>();

	state->bg_tilemap = tilemap_create(machine, rtype2_get_bg_tile_info, tilemap_scan_rows, 8, 8, 64, 64);
	state->fg_tilemap = tilemap_create(machine, rtype2_get_fg_tile_info, tilemap_scan_rows, 8, 8, 64, 64);

	state->buffered_spriteram = auto_alloc_array(machine, UINT16, state->spriteram_size / 2);

	tilemap_set_transmask(state->fg_tilemap, 0, 0xffff, 0x0001);
	tilemap_set_transmask(state->fg_tilemap, 1, 0x00ff, 0xff01);
	tilemap_set_transmask(state->fg_tilemap, 2, 0x0001, 0xffff);

	tilemap_set_transmask(state->bg_tilemap, 0, 0xffff, 0x0000);
	tilemap_set_transmask(state->bg_tilemap, 1, 0x00ff, 0xff00);
	tilemap_set_transmask(state->bg_tilemap, 2, 0x0001, 0xfffe);

	memset(state->buffered_spriteram, 0, state->spriteram_size);

	tilemap_set_scrolldx(state->fg_tilemap, 4, 0);
	tilemap_set_scrolldy(state->fg_tilemap, -128, 16);

	tilemap_set_scrolldx(state->bg_tilemap, 4, 0);
	tilemap_set_scrolldy(state->bg_tilemap, -128, 16);

	register_savestate(machine);
}

VIDEO_START( poundfor )
{
	VIDEO_START_CALL(rtype2);

	m72_state *state = machine->driver_data<m72_state>();

	tilemap_set_scrolldx(state->fg_tilemap, 6, 0);
	tilemap_set_scrolldx(state->bg_tilemap, 6, 0);
}


/* Major Title has a larger background RAM, and rowscroll */
VIDEO_START( majtitle )
{
	m72_state *state = machine->driver_data<m72_state>();

	state->bg_tilemap = tilemap_create(machine, rtype2_get_bg_tile_info, majtitle_scan_rows, 8, 8, 128, 64);
	state->fg_tilemap = tilemap_create(machine, rtype2_get_fg_tile_info, tilemap_scan_rows, 8, 8, 64, 64);

	state->buffered_spriteram = auto_alloc_array(machine, UINT16, state->spriteram_size / 2);

	tilemap_set_transmask(state->fg_tilemap, 0, 0xffff, 0x0001);
	tilemap_set_transmask(state->fg_tilemap, 1, 0x00ff, 0xff01);
	tilemap_set_transmask(state->fg_tilemap, 2, 0x0001, 0xffff);

	tilemap_set_transmask(state->bg_tilemap, 0, 0xffff, 0x0000);
	tilemap_set_transmask(state->bg_tilemap, 1, 0x00ff, 0xff00);
	tilemap_set_transmask(state->bg_tilemap, 2, 0x0001, 0xfffe);

	memset(state->buffered_spriteram, 0, state->spriteram_size);

	tilemap_set_scrolldx(state->fg_tilemap, 4, 0);
	tilemap_set_scrolldy(state->fg_tilemap, -128, 16);

	tilemap_set_scrolldx(state->bg_tilemap, 4, 0);
	tilemap_set_scrolldy(state->bg_tilemap, -128, 16);

	register_savestate(machine);
}

VIDEO_START( hharry )
{
	m72_state *state = machine->driver_data<m72_state>();

	state->bg_tilemap = tilemap_create(machine, hharry_get_bg_tile_info, tilemap_scan_rows, 8, 8, 64, 64);
	state->fg_tilemap = tilemap_create(machine, m72_get_fg_tile_info, tilemap_scan_rows, 8, 8, 64, 64);

	state->buffered_spriteram = auto_alloc_array(machine, UINT16, state->spriteram_size / 2);

	tilemap_set_transmask(state->fg_tilemap, 0, 0xffff, 0x0001);
	tilemap_set_transmask(state->fg_tilemap, 1, 0x00ff, 0xff01);
	tilemap_set_transmask(state->fg_tilemap, 2, 0x0001, 0xffff);

	tilemap_set_transmask(state->bg_tilemap, 0, 0xffff, 0x0000);
	tilemap_set_transmask(state->bg_tilemap, 1, 0x00ff, 0xff00);
	tilemap_set_transmask(state->bg_tilemap, 2, 0x0001, 0xfffe);

	memset(state->buffered_spriteram, 0, state->spriteram_size);

	tilemap_set_scrolldx(state->fg_tilemap, 4, 0);
	tilemap_set_scrolldy(state->fg_tilemap, -128, 16);

	tilemap_set_scrolldx(state->bg_tilemap, 6, 0);
	tilemap_set_scrolldy(state->bg_tilemap, -128, 16);

	register_savestate(machine);
}


/***************************************************************************

  Memory handlers

***************************************************************************/

READ16_HANDLER( m72_palette1_r )
{
	/* A9 isn't connected, so 0x200-0x3ff mirrors 0x000-0x1ff etc. */
	offset &= ~0x0100;

	return space->machine->generic.paletteram.u16[offset] | 0xffe0;		/* only D0-D4 are connected */
}

READ16_HANDLER( m72_palette2_r )
{
	/* A9 isn't connected, so 0x200-0x3ff mirrors 0x000-0x1ff etc. */
	offset &= ~0x0100;

	return space->machine->generic.paletteram2.u16[offset] | 0xffe0;	/* only D0-D4 are connected */
}

INLINE void changecolor(running_machine *machine, int color, int r, int g, int b)
{
	palette_set_color_rgb(machine, color, pal5bit(r), pal5bit(g), pal5bit(b));
}

WRITE16_HANDLER( m72_palette1_w )
{
	/* A9 isn't connected, so 0x200-0x3ff mirrors 0x000-0x1ff etc. */
	offset &= ~0x0100;
	COMBINE_DATA(&space->machine->generic.paletteram.u16[offset]);
	offset &= 0xff;

	changecolor(space->machine, offset,
			space->machine->generic.paletteram.u16[offset + 0x000],
			space->machine->generic.paletteram.u16[offset + 0x200],
			space->machine->generic.paletteram.u16[offset + 0x400]
	);
}

WRITE16_HANDLER( m72_palette2_w )
{
	/* A9 isn't connected, so 0x200-0x3ff mirrors 0x000-0x1ff etc. */
	offset &= ~0x0100;
	COMBINE_DATA(&space->machine->generic.paletteram2.u16[offset]);
	offset &= 0xff;

	changecolor(space->machine, offset + 256,
			space->machine->generic.paletteram2.u16[offset + 0x000],
			space->machine->generic.paletteram2.u16[offset + 0x200],
			space->machine->generic.paletteram2.u16[offset + 0x400]
	);
}

WRITE16_HANDLER( m72_videoram1_w )
{
	m72_state *state = space->machine->driver_data<m72_state>();

	COMBINE_DATA(&state->videoram1[offset]);
	tilemap_mark_tile_dirty(state->fg_tilemap, offset / 2);
}

WRITE16_HANDLER( m72_videoram2_w )
{
	m72_state *state = space->machine->driver_data<m72_state>();

	COMBINE_DATA(&state->videoram2[offset]);
	tilemap_mark_tile_dirty(state->bg_tilemap, offset / 2);
}

WRITE16_HANDLER( m72_irq_line_w )
{
	m72_state *state = space->machine->driver_data<m72_state>();
	COMBINE_DATA(&state->raster_irq_position);
}

WRITE16_HANDLER( m72_scrollx1_w )
{
	m72_state *state = space->machine->driver_data<m72_state>();
	COMBINE_DATA(&state->scrollx1);
}

WRITE16_HANDLER( m72_scrollx2_w )
{
	m72_state *state = space->machine->driver_data<m72_state>();
	COMBINE_DATA(&state->scrollx2);
}

WRITE16_HANDLER( m72_scrolly1_w )
{
	m72_state *state = space->machine->driver_data<m72_state>();
	COMBINE_DATA(&state->scrolly1);
}

WRITE16_HANDLER( m72_scrolly2_w )
{
	m72_state *state = space->machine->driver_data<m72_state>();
	COMBINE_DATA(&state->scrolly2);
}

WRITE16_HANDLER( m72_dmaon_w )
{
	if (ACCESSING_BITS_0_7)
	{
		m72_state *state = space->machine->driver_data<m72_state>();
		memcpy(state->buffered_spriteram, state->spriteram, state->spriteram_size);
	}
}

WRITE16_HANDLER( m72_port02_w )
{
	if (ACCESSING_BITS_0_7)
	{
		m72_state *state = space->machine->driver_data<m72_state>();

		/* bits 0/1 are coin counters */
		coin_counter_w(space->machine, 0, data & 0x01);
		coin_counter_w(space->machine, 1, data & 0x02);

		/* bit 2 is flip screen (handled both by software and hardware) */
		flip_screen_set(space->machine, ((data & 0x04) >> 2) ^ ((~input_port_read(space->machine, "DSW") >> 8) & 0x01));

		/* bit 3 is display disable */
		state->video_off = data & 0x08;

		/* bit 4 resets sound CPU (active low) */
		cpu_set_input_line(state->sndcpu, INPUT_LINE_RESET, (data & 0x10) ? CLEAR_LINE : ASSERT_LINE);

		/* bit 5 = "bank"? */
	}
}

WRITE16_HANDLER( rtype2_port02_w )
{
	if (ACCESSING_BITS_0_7)
	{
		m72_state *state = space->machine->driver_data<m72_state>();

		/* bits 0/1 are coin counters */
		coin_counter_w(space->machine, 0,data & 0x01);
		coin_counter_w(space->machine, 1,data & 0x02);

		/* bit 2 is flip screen (handled both by software and hardware) */
		flip_screen_set(space->machine, ((data & 0x04) >> 2) ^ ((~input_port_read(space->machine, "DSW") >> 8) & 0x01));

		/* bit 3 is display disable */
		state->video_off = data & 0x08;

		/* other bits unknown */
	}
}

/* the following is mostly a kludge. This register seems to be used for something else */
WRITE16_HANDLER( majtitle_gfx_ctrl_w )
{
	if (ACCESSING_BITS_8_15)
	{
		m72_state *state = space->machine->driver_data<m72_state>();
		state->majtitle_rowscroll = (data & 0xff00) ? 1 : 0;
	}
}


/***************************************************************************

  Display refresh

***************************************************************************/

static void m72_draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	m72_state *state = machine->driver_data<m72_state>();

	UINT16 *sprite = state->buffered_spriteram;
	INT32 code, color, sx, sy, flipx, flipy, c, w, h;
	const UINT32 size = state->spriteram_size / 2;

	for (UINT32 offs = 0; offs < size; offs += w << 2)
	{
		code = sprite[offs + 1];
		color = sprite[offs + 2] & 0x0f;
		sx = -256 + (sprite[offs + 3] & 0x3ff);
		sy = 384 - (sprite[offs + 0] & 0x1ff);
		flipx = sprite[offs + 2] & 0x0800;
		flipy = sprite[offs + 2] & 0x0400;

		w = 1 << ((sprite[offs + 2] & 0xc000) >> 14);
		h = 1 << ((sprite[offs + 2] & 0x3000) >> 12);
		sy -= 16 * h;

		if (flip_screen_get(machine))
		{
			sx = 512 - 16 * w - sx;
			sy = 284 - 16 * h - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		for (UINT32 x = 0; x < w; x++)
		{
			for (UINT32 y = 0; y < h; y++)
			{
				c = code;

				if (flipx)
					c += 8 * (w - 1 - x);
				else
					c += 8 * x;
				if (flipy)
					c += h - 1 - y;
				else
					c += y;

				drawgfx_transpen(bitmap, cliprect, machine->gfx[0],
						 c, color, flipx, flipy,
						 sx + (x << 4), sy + (y << 4), 0
						);
			}
		}
	}
}

static void majtitle_draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	m72_state *state = machine->driver_data<m72_state>();

	UINT16 *sprite2 = state->spriteram2;
	INT32 code, color, sx, sy, flipx, flipy, c, w, h;

	for (UINT32 offs = 0; offs < state->spriteram_size; offs += 4)
	{
		code = sprite2[offs + 1];
		color = sprite2[offs + 2] & 0x0f;
		sx = -256 + (sprite2[offs + 3] & 0x03ff);
		sy = 384 - (sprite2[offs + 0] & 0x01ff);
		flipx = sprite2[offs + 2] & 0x0800;
		flipy = sprite2[offs + 2] & 0x0400;

		w = 1;
		h = 1 << ((sprite2[offs + 2] & 0x3000) >> 12);
		sy -= 16 * h;

		if (flip_screen_get(machine))
		{
			sx = 512 - 16 * w - sx;
			sy = 256 - 16 * h - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		for (UINT32 x = 0; x < w; x++)
		{
			for (UINT32 y = 0; y < h; y++)
			{
				c = code;

				if (flipx)
					c += 8 * (w - 1 - x);
				else
					c += 8 * x;
				if (flipy)
					c += h - 1 - y;
				else
					c += y;

				drawgfx_transpen(bitmap, cliprect, machine->gfx[2],
						 c, color,
						 flipx, flipy,
						 sx + (x << 4), sy + (y << 4), 0
						);
			}
		}
	}
}

VIDEO_UPDATE( m72 )
{
	m72_state *state = screen->machine->driver_data<m72_state>();

	if (state->video_off)
	{
		bitmap_fill(bitmap, cliprect, get_black_pen(screen->machine));
		return 0;
	}

	tilemap_set_scrollx(state->fg_tilemap, 0, state->scrollx1);
	tilemap_set_scrolly(state->fg_tilemap, 0, state->scrolly1);
	tilemap_set_scrollx(state->bg_tilemap, 0, state->scrollx2);
	tilemap_set_scrolly(state->bg_tilemap, 0, state->scrolly2);

	tilemap_draw(bitmap, cliprect, state->bg_tilemap, TILEMAP_DRAW_LAYER1, 0);
	tilemap_draw(bitmap, cliprect, state->fg_tilemap, TILEMAP_DRAW_LAYER1, 0);

	m72_draw_sprites(screen->machine, bitmap, cliprect);

	tilemap_draw(bitmap, cliprect, state->bg_tilemap, TILEMAP_DRAW_LAYER0, 0);
	tilemap_draw(bitmap, cliprect, state->fg_tilemap, TILEMAP_DRAW_LAYER0, 0);

	return 0;
}

VIDEO_UPDATE( majtitle )
{
	m72_state *state = screen->machine->driver_data<m72_state>();

	if (state->video_off)
	{
		bitmap_fill(bitmap, cliprect, get_black_pen(screen->machine));
		return 0;
	}

	tilemap_set_scrollx(state->fg_tilemap, 0, state->scrollx1);
	tilemap_set_scrolly(state->fg_tilemap, 0, state->scrolly1);
	if (state->majtitle_rowscroll)
	{
		tilemap_set_scroll_rows(state->bg_tilemap, 512);
		for (int i = 0; i < 512; i++)
			tilemap_set_scrollx(state->bg_tilemap, (i + state->scrolly2) & 0x01ff, 256 + state->majtitle_rowscrollram[i]);
	}
	else
	{
		tilemap_set_scroll_rows(state->bg_tilemap, 1);
		tilemap_set_scrollx(state->bg_tilemap, 0, 256 + state->scrollx2);
	}
	tilemap_set_scrolly(state->bg_tilemap, 0, state->scrolly2);

	tilemap_draw(bitmap, cliprect, state->bg_tilemap, TILEMAP_DRAW_LAYER1, 0);
	tilemap_draw(bitmap, cliprect, state->fg_tilemap, TILEMAP_DRAW_LAYER1, 0);

	majtitle_draw_sprites(screen->machine, bitmap, cliprect);
	m72_draw_sprites(screen->machine, bitmap, cliprect);

	tilemap_draw(bitmap, cliprect, state->bg_tilemap, TILEMAP_DRAW_LAYER0, 0);
	tilemap_draw(bitmap, cliprect, state->fg_tilemap, TILEMAP_DRAW_LAYER0, 0);

	return 0;
}
