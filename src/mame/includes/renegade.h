/************************/
/*	RENEGADE	*/
/************************/

class renegade_state : public driver_device
{
public:
	renegade_state(running_machine &machine, const driver_device_config_base &config)
		: driver_device(machine, config) { }

	/* devices */
	running_device	*maincpu;
	running_device	*audiocpu;
	running_device	*mcu;
	running_device	*msm5205;

	/* memory pointers */
	UINT8		*bg_videoram;
	UINT8		*fg_videoram;

	/* video-related */
	tilemap_t	*bg_tilemap;
	tilemap_t	*fg_tilemap;
	INT32		scrollx;

	/* mcu */
	INT32		from_main;
	INT32		from_mcu;
	INT32		main_sent;
	INT32		mcu_sent;
	UINT8		ddr_a;
	UINT8		ddr_b;
	UINT8		ddr_c;
	UINT8		port_a_out;
	UINT8		port_b_out;
	UINT8		port_c_out;
	UINT8		port_a_in;
	UINT8		port_b_in;
	UINT8		port_c_in;

	/* mcu simulation (Kunio Kun) */
	UINT8		mcu_buffer[6];
	UINT8		mcu_input_size;
	UINT8		mcu_output_byte;
	INT8		mcu_key;

	/* misc */
	const UINT8	*mcu_encrypt_table;
	INT32		mcu_sim;
	INT32		mcu_checksum;
	INT32		mcu_encrypt_table_len;

	/* adpcm */
	UINT8		*adpcm_rom;
	UINT32		adpcm_pos;
	UINT32		adpcm_end;
	UINT8		adpcm_playing;
};


/*----------- defined in video/renegade.c -----------*/

VIDEO_UPDATE( renegade );
VIDEO_START( renegade );

WRITE8_HANDLER( scroll_lsb_w );
WRITE8_HANDLER( scroll_msb_w );
WRITE8_HANDLER( bg_videoram_w );
WRITE8_HANDLER( fg_videoram_w );
WRITE8_HANDLER( renegade_flipscreen_w );

/*----------- defined in driver/renegade.c -----------*/

struct _renegade_arrays
{
	const UINT16	timer_table[4];
	const UINT8	sound_command_table[256];
	const UINT8	difficulty_table[4];
	const UINT8	joy_table[16];
	const UINT8	kuniokun_xor_table[42];
	const UINT8	enemy_type_table[39];
};

WRITE8_HANDLER( renegade_68705_port_a_w );
WRITE8_HANDLER( renegade_68705_port_b_w );
WRITE8_HANDLER( renegade_68705_port_c_w );
WRITE8_HANDLER( renegade_68705_ddr_a_w );
WRITE8_HANDLER( renegade_68705_ddr_b_w );
WRITE8_HANDLER( renegade_68705_ddr_c_w );
READ8_HANDLER( renegade_68705_port_a_r );
READ8_HANDLER( renegade_68705_port_b_r );
READ8_HANDLER( renegade_68705_port_c_r );
