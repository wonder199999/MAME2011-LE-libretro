/*************************************************************************

    Irem M72 hardware

*************************************************************************/

struct _sample_data
{
	const INT32 bchopper[6];
	const INT32 nspirit[9];
	const INT32 loht[7];
	const INT32 xmultiplm72[3];
	const INT32 imgfight[7];
	const INT32 dbreedm72[9];
	const INT32 airduelm72[16];
	const INT32 dkgenm72[28];
	const INT32 gallop[31];
};

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
	running_device	*sndcpu;
	running_device	*mcu;
	running_device	*m72audio;

	/* memory pointers */
	UINT16		*spriteram;
	UINT16		*spriteram2;
	UINT16		*buffered_spriteram;
	UINT16		*videoram1;
	UINT16		*videoram2;
	UINT16		*majtitle_rowscrollram;
	UINT8		*soundram;
	UINT16		*protection_ram;
	const UINT8	*protection_code;
	const UINT8	*protection_crc;
	tilemap_t	*fg_tilemap;
	tilemap_t	*bg_tilemap;

	/* video-related */
	emu_timer	*scanline_timer;
	size_t		spriteram_size;
	UINT32		raster_irq_position;
	INT32		scrollx1;
	INT32		scrollx2;
	INT32		scrolly1;
	INT32		scrolly2;
	INT32		video_off;
	INT32		majtitle_rowscroll;

	/* misc */
	INT32		prev[4];
	INT32		diff[4];
	UINT32		sample_addr;
	UINT8		mcu_snd_cmd_latch;
	UINT8		mcu_sample_latch;
	UINT8		mcu_sample_addr;
	UINT8		irqvector;
	UINT8		irq_base;
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
