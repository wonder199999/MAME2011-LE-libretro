/************************/
/*	RENEGADE	*/
/************************/

class renegade_state : public driver_data_t
{
public:
	static driver_data_t *alloc(running_machine &machine)
	{
		return auto_alloc_clear(&machine, renegade_state(machine));
	}
	renegade_state(running_machine &machine)
			: driver_data_t(machine) { }

	/* devices */
	running_device	*maincpu;
	running_device	*audiocpu;
	running_device	*mcu;

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
	UINT8		bank;
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
	const UINT8 kuniokun_xor_table[42];
	const UINT8 sound_command_table[256];
	const UINT8 joy_table[16];
	const UINT8 difficulty_table[4];
	const UINT16 timer_table[4];
	const UINT8 enemy_type_table[39];
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
