/****************************************/
/*	Technos - WWF WrestleFest	*/
/****************************************/

class wwfwfest_state : public driver_data_t
{
public:
	static driver_data_t *alloc(running_machine &machine)
	{
		return auto_alloc_clear(&machine, wwfwfest_state(machine));
	}

	wwfwfest_state(running_machine &machine)
		: driver_data_t(machine) { }

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
	UINT16		wwfwfest_pri;
	UINT16		wwfwfest_bg0_scrollx;
	UINT16		wwfwfest_bg0_scrolly;
	UINT16		wwfwfest_bg1_scrollx;
	UINT16		wwfwfest_bg1_scrolly;
	UINT16		sprite_xoff;
};

/*----------- defined in video/wwfwfest.c -----------*/

WRITE16_HANDLER( wwfwfest_fg0_videoram_w );
WRITE16_HANDLER( wwfwfest_bg0_videoram_w );
WRITE16_HANDLER( wwfwfest_bg1_videoram_w );

VIDEO_START( wwfwfest );
VIDEO_START( wwfwfstb );
VIDEO_UPDATE( wwfwfest );
