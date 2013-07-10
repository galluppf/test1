#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_spike_source_array.h"


dma_pipeline_t dma_pipeline;


void dma_callback(uint null0, uint tag)
{
    if (tag == 0xFFFFFFFD)
    {
        process_spikes(dma_pipeline.cache); //this is a fake routine name to allow the model spike source array to use the dma transfer to retrive the spikes form the SDRAM
    }
}

