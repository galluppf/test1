/*#include "spinn_api.h"*/
#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "model_general.h"
#include "comms.h"
#include "config.h"

uint num_populations = 0;
population_t *population = NULL;

extern uint sdp_packets_received;


void process_spikes(void *dma_copy)
{
}

void configure_recording_space() 
{
}

void handle_sdp_msg(sdp_msg_t * sdp_msg)
{
}

void timer_callback(uint ticks, uint null)
{
    if((ticks & 0x3ff) == 1)
    {
        io_printf (IO_STD, "Spike source model\n");
        send_sync_sdp();
    }

    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Simulation complete - received %d sdp packets.\n", sdp_packets_received);
        spin1_stop();
    }
    
}

