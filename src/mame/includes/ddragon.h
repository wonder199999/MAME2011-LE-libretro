/*************************************************************************

    Double Dragon & Double Dragon II (but also China Gate)

*************************************************************************/


class ddragon_state : public driver_data_t
{
public:
	static driver_data_t *alloc(running_machine &machine) { return auto_alloc_clear(&machine, ddragon_state(machine)); }

	ddragon_state(running_machine &machine)
		: driver_data_t(machine) { }

	/* devices */
	running_device	*maincpu;
	running_device	*snd_cpu;
	running_device	*sub_cpu;
	running_device	*adpcm_1;
	running_device	*adpcm_2;

	/* memory pointers */
	UINT8		*rambase;
	UINT8		*bgvideoram;
	UINT8		*fgvideoram;
	UINT8		*spriteram;
	UINT8		*scrollx_lo;
	UINT8		*scrolly_lo;
	UINT8		*darktowr_mcu_ports;
	UINT8		*comram;

	/* video-related */
	tilemap_t	*fg_tilemap;
	tilemap_t	*bg_tilemap;
	size_t		spriteram_size;		// FIXME: this appears in chinagat.c, but is it really used?
	UINT8		technos_video_hw;
	UINT8		scrollx_hi;
	UINT8		scrolly_hi;

	/* misc */
	UINT8		ddragon_sub_port;
	UINT8		sprite_irq;
	UINT8		sound_irq;
	UINT8		ym_irq;
	UINT8		adpcm_sound_irq;
	UINT32		adpcm_pos[2];
	UINT32		adpcm_end[2];
	INT32		adpcm_data[2];
	UINT8		adpcm_idle[2];

	/* for Sai Yu Gou Ma Roku */
	INT32		adpcm_addr;
	INT32		i8748_P1;
	INT32		i8748_P2;
	INT32		pcm_shift;
	INT32		pcm_nibble;
	INT32		mcu_command;
};


/*----------- defined in video/ddragon.c -----------*/

WRITE8_HANDLER( ddragon_bgvideoram_w );
WRITE8_HANDLER( ddragon_fgvideoram_w );

VIDEO_START( chinagat );
VIDEO_START( ddragon );

VIDEO_UPDATE( ddragon );
