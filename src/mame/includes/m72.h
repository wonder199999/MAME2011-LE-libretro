/*************************************************************************

    Irem M72 hardware

*************************************************************************/

class m72_state : public driver_data_t
{
public:
	static driver_data_t *alloc(running_machine &machine)
	{
		return auto_alloc_clear(&machine, m72_state(machine));
	}
	m72_state(running_machine &machine)
			: driver_data_t(machine) { }

	/* devices */
	running_device	*maincpu;

	/* memory pointers */
	UINT16		*protection_ram;
	const UINT8	*protection_code;
	const UINT8	*protection_crc;
	UINT16		*spriteram;
	UINT16		*videoram1;
	UINT16		*videoram2;
	UINT16		*majtitle_rowscrollram;
	UINT8		*soundram;
	tilemap_t	*fg_tilemap;
	tilemap_t	*bg_tilemap;

	/* video-related */
	emu_timer	*scanline_timer;
	UINT32		raster_irq_position;
	INT32		scrollx1;
	INT32		scrollx2;
	INT32		scrolly1;
	INT32		scrolly2;
	INT32		video_off;
	INT32		majtitle_rowscroll;

	/* misc */
	UINT8		mcu_snd_cmd_latch;
	UINT8		mcu_sample_latch;
	UINT8		mcu_sample_addr;
	UINT8		irqvector;
	UINT8		irq_base;
	UINT32		sample_addr;
	INT32		prev[4];
	INT32		diff[4];
};


/*----------- defined in video/m72.c -----------*/

VIDEO_START( m72 );
VIDEO_START( rtype2 );
VIDEO_START( majtitle );
VIDEO_START( hharry );
VIDEO_START( poundfor );

READ16_HANDLER( m72_palette1_r );
READ16_HANDLER( m72_palette2_r );

WRITE16_HANDLER( m72_palette1_w );
WRITE16_HANDLER( m72_palette2_w );
WRITE16_HANDLER( m72_videoram1_w );
WRITE16_HANDLER( m72_videoram2_w );
WRITE16_HANDLER( m72_irq_line_w );
WRITE16_HANDLER( m72_scrollx1_w );
WRITE16_HANDLER( m72_scrollx2_w );
WRITE16_HANDLER( m72_scrolly1_w );
WRITE16_HANDLER( m72_scrolly2_w );
WRITE16_HANDLER( m72_dmaon_w );
WRITE16_HANDLER( m72_port02_w );
WRITE16_HANDLER( rtype2_port02_w );
WRITE16_HANDLER( majtitle_gfx_ctrl_w );

VIDEO_UPDATE( m72 );
VIDEO_UPDATE( majtitle );
