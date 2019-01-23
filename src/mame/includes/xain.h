/********************************************************/
/*	     Solar Warrior / Xain'd Sleena		*/
/********************************************************/

#define MASTER_CLOCK		(XTAL_12MHz)
#define CPU_CLOCK		(MASTER_CLOCK / 8)
#define MCU_CLOCK		(MASTER_CLOCK / 4)
#define PIXEL_CLOCK		(MASTER_CLOCK / 2)

class xain_state : public driver_data_t
{
public:
	static driver_data_t *alloc(running_machine &machine) { return auto_alloc_clear(&machine, xain_state(machine)); }

	xain_state(running_machine &machine)
			: driver_data_t(machine) { }

	/* pointers */
	UINT8		*xain_charram;
	UINT8		*xain_bgram0;
	UINT8		*xain_bgram1;
	tilemap_t	*char_tilemap;
	tilemap_t	*bgram0_tilemap;
	tilemap_t	*bgram1_tilemap;

	/* devices */
	running_device	*maincpu;
	running_device	*audiocpu;
	running_device	*subcpu;
	running_device	*mcu;

	/* 68705 mcu */
	INT32		from_main;
	INT32		from_mcu;
	INT32		_mcu_ready;
	INT32		_mcu_accept;
	UINT8		ddr_a;
	UINT8		ddr_b;
	UINT8		ddr_c;
	UINT8		port_a_in;
	UINT8		port_b_in;
	UINT8		port_c_in;
	UINT8		port_a_out;
	UINT8		port_b_out;
	UINT8		port_c_out;

	/* video-related */
	UINT8		xain_pri;
	INT32		vblank;
};


/*----------- defined in video/xain.c -----------*/

VIDEO_UPDATE( xain );
VIDEO_START( xain );

WRITE8_HANDLER( xain_scrollxP0_w );
WRITE8_HANDLER( xain_scrollyP0_w );
WRITE8_HANDLER( xain_scrollxP1_w );
WRITE8_HANDLER( xain_scrollyP1_w );
WRITE8_HANDLER( xain_charram_w );
WRITE8_HANDLER( xain_bgram0_w );
WRITE8_HANDLER( xain_bgram1_w );
WRITE8_HANDLER( xain_flipscreen_w );
