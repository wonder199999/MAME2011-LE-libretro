/****************************************/
/*	Technos - WWF WrestleFest	*/
/****************************************/

class wwfwfest_state : public driver_device
{
public:
	wwfwfest_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	/* devices */
	running_device	*maincpu;
	running_device	*audiocpu;

	/* memory pointers */
	UINT16		*wwfwfest_fg0_videoram;
	UINT16		*wwfwfest_bg0_videoram;
	UINT16		*wwfwfest_bg1_videoram;

	/* video-related */
	tilemap_t	*fg0_tilemap;
	tilemap_t	*bg0_tilemap;
	tilemap_t	*bg1_tilemap;
	UINT16		bg0_dx;
	UINT16		bg1_dx[2];
	UINT16		sprite_xoff;
	UINT16		wwfwfest_bg0_scrollx;
	UINT16		wwfwfest_bg0_scrolly;
	UINT16		wwfwfest_bg1_scrollx;
	UINT16		wwfwfest_bg1_scrolly;
	UINT8		wwfwfest_pri;
};

/*----------- defined in video/wwfwfest.c -----------*/

WRITE16_HANDLER( wwfwfest_fg0_videoram_w );
WRITE16_HANDLER( wwfwfest_bg0_videoram_w );
WRITE16_HANDLER( wwfwfest_bg1_videoram_w );

VIDEO_START( wwfwfest );
VIDEO_START( wwfwfstb );
VIDEO_UPDATE( wwfwfest );
