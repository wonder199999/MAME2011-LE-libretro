/* */

class renegade_state : public driver_data_t
{
public:
	static driver_data_t *alloc(running_machine &machine)
	{
		return auto_alloc_clear(&machine, renegade_state(machine));
	}
	renegade_state(running_machine &machine)
			: driver_data_t(machine) { }

	UINT8 *videoram;
};


/*----------- defined in video/renegade.c -----------*/

extern UINT8 *renegade_videoram2;

VIDEO_UPDATE( renegade );
VIDEO_START( renegade );
WRITE8_HANDLER( renegade_scroll0_w );
WRITE8_HANDLER( renegade_scroll1_w );
WRITE8_HANDLER( renegade_videoram_w );
WRITE8_HANDLER( renegade_videoram2_w );
WRITE8_HANDLER( renegade_flipscreen_w );
