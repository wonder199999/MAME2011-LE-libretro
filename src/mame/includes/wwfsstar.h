/****************************************/
/*	Technos - WWF Super Stars	*/
/****************************************/

class wwfsstar_state : public driver_device
{
public:
	wwfsstar_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	/* devices */
	running_device	*maincpu;
	running_device	*audiocpu;

	/* memory pointers */
	UINT16		*spriteram;
	UINT16		*fg0_videoram;
	UINT16		*bg0_videoram;

	/* video-related */
	tilemap_t	*fg0_tilemap;
	tilemap_t	*bg0_tilemap;
	INT32		vblank;
	INT32		scrollx;
	INT32		scrolly;
};


/*----------- defined in video/wwfsstar.c -----------*/

WRITE16_HANDLER( wwfsstar_fg0_videoram_w );
WRITE16_HANDLER( wwfsstar_bg0_videoram_w );

VIDEO_START( wwfsstar );
VIDEO_UPDATE( wwfsstar );
