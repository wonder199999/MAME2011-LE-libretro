/*************************************************************************

    Double Dragon 3 & The Combatribes

*************************************************************************/


class ddragon3_state : public driver_device
{
public:
	ddragon3_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	/* devices */
	running_device	*maincpu;
	running_device	*audiocpu;

	/* memory pointers */
	UINT16		*bg_videoram;
	UINT16		*fg_videoram;
	UINT16		*spriteram;

	/* misc */
	UINT16          io_reg[8];

	/* video-related */
	tilemap_t	*fg_tilemap;
	tilemap_t	*bg_tilemap;
	UINT16          vreg;
	UINT16          bg_scrollx;
	UINT16          bg_scrolly;
	UINT16          fg_scrollx;
	UINT16          fg_scrolly;
	UINT16          bg_tilebase;
};


/*----------- defined in video/ddragon3.c -----------*/

extern WRITE16_HANDLER( ddragon3_bg_videoram_w );
extern WRITE16_HANDLER( ddragon3_fg_videoram_w );
extern WRITE16_HANDLER( ddragon3_scroll_w );
extern READ16_HANDLER( ddragon3_scroll_r );

extern VIDEO_START( ddragon3 );
extern VIDEO_UPDATE( ddragon3 );
extern VIDEO_UPDATE( ctribe );
