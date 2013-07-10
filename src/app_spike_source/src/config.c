#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
#include "recording.h"

app_data_t app_data;

void configure_app_frame()
{
    // Configure DMA pipeline
    dma_pipeline.busy = FALSE;
    dma_pipeline.flip = 0;
    dma_pipeline.row_size = sizeof(uint) * ((app_data.total_neurons - 1) >> 5);
    dma_pipeline.cache = (uint *) spin1_malloc((app_data.total_neurons - 1) >> 5);

    // Configure output sdp
    spike_output = (sdp_msg_t *) spin1_malloc(sizeof(sdp_msg_t));
    spike_output->flags = 7;
    spike_output->tag = 1;
    spike_output->dest_port = 255;
    // spike_output->srce_port = 1;
    spike_output->srce_port = 1 << 5 | app_data.virtual_core_id;
    spike_output->dest_addr = 0;
    spike_output->srce_addr = spin1_get_chip_id();
    spike_output->cmd_rc = 0x100;
    spike_output->arg1 = 0;

    population[0].neuron = (void *) &population[app_data.num_populations];
    for(uint i = 1; i < num_populations; i++)
    {
        //REALLY ugly hack to get population pointers correct TODO improve
        uchar *offset = population[i - 1].neuron;
        offset += population[i - 1].num_neurons * population[i - 1].neuron_size;
        population[i].neuron = (void *) offset;
    }
    // Configure state recording space: this is specific for every neural model. 
    // model_general.h contains the prototype, bodies can be found in every neural model
    
    configure_recording_space();            // prototype in model_general.h, body in each neural model
}


void load_application_data()
{ //TODO clean up this horific mess of addresses and offsets
    app_data = *((app_data_t *) (0x74000000 + 0x10000 * (spin1_get_core_id() - 1)));
    app_data.virtual_core_id = spin1_get_core_id();

    uint size = 0;
    char *source = NULL;
    char *dest = NULL;

    size = app_data.num_populations * sizeof(population_t) + app_data.neuron_data_size;
    source = (char *) 0x74000000 + sizeof(app_data_t) + 0x10000 * (app_data.virtual_core_id - 1);
    dest = (char *) spin1_malloc(size);
    for(uint i = 0; i < size; i++) dest[i] = source[i];
    num_populations = app_data.num_populations;
    population = (population_t *) dest;
}


void load_mc_routing_tables()
{
    // Everybody is doing it as we don't know which cores will be available to the simulation // FIXME use a smarter solution
    uint size = *((uint *) 0x74210000);
    mc_table_entry_t *mc = (mc_table_entry_t *) 0x74210004;

    for(uint i = 0; i < size; i++)
    {
        spin1_set_mc_table_entry(i, mc[i].key, mc[i].mask, mc[i].route);
    }

}
