#include "spinn_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "model_general.h"
#include "comms.h"
#include "config.h"

uint num_populations = 0;
population_t *population = NULL;
psp_buffer_t *psp_buffer = NULL;

extern uint sdp_packets_received;


void buffer_post_synaptic_potentials(void *dma_copy)
{
}

void timer_callback(uint ticks, uint null)
{
    if(ticks == 1 || ticks==1000)
    {
        send_sync_sdp();
    }

    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Simulation complete - received %d sdp packets.\n", sdp_packets_received);
        stop();
    }
}
