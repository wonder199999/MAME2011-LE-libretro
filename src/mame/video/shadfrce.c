#include "emu.h"
#include "includes/shadfrce.h"

static TILE_GET_INFO( get_shadfrce_fgtile_info )
{
	/* ---- ----  tttt tttt  ---- ----  pppp TTTT */
	shadfrce_state *state = machine->driver_data<shadfrce_state>();

	INT32 colour = (state->fgvideoram[(tile_index << 1) + 1] & 0x00f0) >> 4;
	INT32 tileno = (state->fgvideoram[tile_index << 1] & 0x00ff) | ((state->fgvideoram[(tile_index << 1) + 1] & 0x000f) << 8);
	SET_TILE_INFO(0, tileno, colour << 2, 0);
}

WRITE16_HANDLER( shadfrce_fgvideoram_w )
{
	shadfrce_state *state = space->machine->driver_data<shadfrce_state>();

	state->fgvideoram[offset] = data;
	tilemap_mark_tile_dirty(state->fgtilemap, offset / 2);
}

static TILE_GET_INFO( get_shadfrce_bg0tile_info )
{
	/* ---- ----  ---- cccc  --TT TTTT TTTT TTTT */
	shadfrce_state *state = machine->driver_data<shadfrce_state>();

	INT32 colour = state->bg0videoram[tile_index << 1] & 0x001f;
	if (colour & 0x10)
		colour ^= 0x30;	/* skip hole */
	INT32 tileno = (state->bg0videoram[(tile_index << 1) + 1] & 0x3fff);
	INT32 fyx = (state->bg0videoram[tile_index << 1] & 0x00c0) >> 6;

	SET_TILE_INFO(2, tileno, colour, TILE_FLIPYX(fyx));
}

WRITE16_HANDLER( shadfrce_bg0videoram_w )
{
	shadfrce_state *state = space->machine->driver_data<shadfrce_state>();

	state->bg0videoram[offset] = data;
	tilemap_mark_tile_dirty(state->bg0tilemap, offset / 2);
}

static TILE_GET_INFO( get_shadfrce_bg1tile_info )
{
	shadfrce_state *state = machine->driver_data<shadfrce_state>();

	INT32 colour = (state->bg1videoram[tile_index] & 0xf000) >> 12;
	INT32 tileno = (state->bg1videoram[tile_index] & 0x0fff);
	SET_TILE_INFO(2, tileno, colour + 64, 0);
}

WRITE16_HANDLER( shadfrce_bg1videoram_w )
{
	shadfrce_state *state = space->machine->driver_data<shadfrce_state>();

	state->bg1videoram[offset] = data;
	tilemap_mark_tile_dirty(state->bg1tilemap, offset);
}

WRITE16_HANDLER ( shadfrce_bg0scrollx_w )
{
	shadfrce_state *state = space->machine->driver_data<shadfrce_state>();
	tilemap_set_scrollx(state->bg0tilemap, 0, data & 0x01ff);
}

WRITE16_HANDLER ( shadfrce_bg0scrolly_w )
{
	shadfrce_state *state = space->machine->driver_data<shadfrce_state>();
	tilemap_set_scrolly(state->bg0tilemap, 0, data & 0x01ff );
}

WRITE16_HANDLER ( shadfrce_bg1scrollx_w )
{
	shadfrce_state *state = space->machine->driver_data<shadfrce_state>();
	tilemap_set_scrollx(state->bg1tilemap, 0, data & 0x01ff);
}

WRITE16_HANDLER ( shadfrce_bg1scrolly_w )
{
	shadfrce_state *state = space->machine->driver_data<shadfrce_state>();
	tilemap_set_scrolly(state->bg1tilemap, 0, data & 0x01ff);
}

VIDEO_START( shadfrce )
{
	shadfrce_state *state = machine->driver_data<shadfrce_state>();

	state->fgtilemap = tilemap_create(machine, get_shadfrce_fgtile_info, tilemap_scan_rows, 8, 8, 64, 32);
	tilemap_set_transparent_pen(state->fgtilemap, 0);
	state->bg0tilemap = tilemap_create(machine, get_shadfrce_bg0tile_info, tilemap_scan_rows, 16, 16, 32, 32);
	tilemap_set_transparent_pen(state->bg0tilemap, 0);

	state->bg1tilemap = tilemap_create(machine, get_shadfrce_bg1tile_info, tilemap_scan_rows, 16, 16, 32, 32);
	state->spvideoram_old = auto_alloc_array(machine, UINT16, state->spvideoram_size / 2);
}

#define DRAWSPRITES(ORDER, SX, SY)							\
	pdrawgfx_transpen(bitmap, cliprect, machine->gfx[1], ORDER, pal,		\
			flipx, flipy, SX, SY, machine->priority_bitmap, pri_mask, 0);	\

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	shadfrce_state *state = machine->driver_data<shadfrce_state>();

	/* | ---- ---- hhhf Fe-Y | ---- ---- yyyy yyyy | ---- ---- TTTT TTTT | ---- ---- tttt tttt |
	   | ---- ---- -pCc cccX | ---- ---- xxxx xxxx | ---- ---- ---- ---- | ---- ---- ---- ---- |

	   h  = height
	   f  = flipx
	   F  = flipy
	   e  = enable
	   Yy = Y Position
	   Tt = Tile No.
	   Xx = X Position
	   Cc = color
	   P  = priority
	*/

	UINT16 *finish = state->spvideoram_old;
	UINT16 *source = finish + 0x2000 / 2 - 8;
	INT32 xpos, ypos, tile, height, flipx, flipy, pal, pri_mask, hcount, sx, sy, order;

	for ( ; source >= finish; source -= 8)
	{
		ypos = 0x100 - (((source[0] & 0x0003) << 8) | (source[1] & 0x00ff));
		xpos = (((source[4] & 0x0001) << 8) | (source[5] & 0x00ff)) + 1;
		tile = ((source[2] & 0x00ff) << 8) | (source[3] & 0x00ff);
		height = ((source[0] & 0x00e0) >> 5) + 1;
		flipx = (source[0] & 0x0010) >> 4;
		flipy = (source[0] & 0x0008) >> 3;
		pri_mask = (source[4] & 0x0040) ? 0x02 : 0x00;
		pal = source[4] & 0x003e;
		if (pal & 0x20)
			pal ^= 0x60;	/* skip hole */

		if (source[0] & 0x0004)
		{
			for (hcount = 0; hcount < height; hcount++)
			{
				sx = xpos - 0x0200;
				sy = ypos - (hcount + 1) * 16;
				order = tile + hcount;

				DRAWSPRITES(order, xpos, sy)
				DRAWSPRITES(order, sx, sy)
				DRAWSPRITES(order, xpos, 0x0200 + sy)
				DRAWSPRITES(order, sx, 0x0200 + sy)
			}
		}
	}
}
#undef DRAWSPRITES

VIDEO_UPDATE( shadfrce )
{
	shadfrce_state *state = screen->machine->driver_data<shadfrce_state>();

	bitmap_fill(screen->machine->priority_bitmap, cliprect, 0);

	if (state->video_enable)
	{
		tilemap_draw(bitmap, cliprect, state->bg1tilemap, 0, 0);
		tilemap_draw(bitmap, cliprect, state->bg0tilemap, 0, 1);
		draw_sprites(screen->machine, bitmap, cliprect);
		tilemap_draw(bitmap, cliprect, state->fgtilemap, 0, 0);
	}
	else
		bitmap_fill(bitmap, cliprect, get_black_pen(screen->machine));

	return 0;
}

VIDEO_EOF( shadfrce )
{
	shadfrce_state *state = machine->driver_data<shadfrce_state>();

	/* looks like sprites are *two* frames ahead */
	memcpy(state->spvideoram_old, state->spvideoram, state->spvideoram_size);
}
