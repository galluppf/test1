/****** config.c/config
*
* SUMMARY
*  This file contains the functions used to load the simulation data from SDRAM in
*  the designated locations.
*  The data is loaded in SDRAM using this memory map:
*
*  SDRAM memory at 0x70000000
*  +-----------------------------------------------------------------+
*  |0x70000000                                                       |
*  |         Synaptic information (for all the cores)                |
*  |0x73FFFFFF                                                       |
*  +-----------------------------------------------------------------+
*  |0x74000000                                                       |
*  |          Neural population data for core 1                      |
*  |0x7400FFFF                                                       |
*  +-----------------------------------------------------------------+
*  |0x74010000                                                       |
*  |          Neural population data for core 2                      |
*  |0x7401FFFF                                                       |
*  +-----------------------------------------------------------------+
*  |0x74020000                                                       |
*  |          Neural population data for core 3                      |
*  |0x7402FFFF                                                       |
*  +-----------------------------------------------------------------+
*  |                    (repeat for 16 cores)                        |
*  +-----------------------------------------------------------------+
*  |0x74200000            (size of lookup tree as number of entries) |
*  |          Synaptic lookup data for core 1                        |
*  |0x74200FFF                                                       |
*  +-----------------------------------------------------------------+
*  |0x74201000            (size of lookup tree as number of entries) |
*  |          Synaptic lookup data for core 2                        |
*  |0x74201FFF                                                       |
*  +-----------------------------------------------------------------+
*  |0x74202000            (size of lookup tree as number of entries) |
*  |          Synaptic lookup data for core 3                        |
*  |0x74202FFF                                                       |
*  +-----------------------------------------------------------------+
*  |                    (repeat for 16 cores)                        |
*  +-----------------------------------------------------------------+
*  |0x74210000          (size of routing table as number of entries) |
*  |          Routing table for the chip organized as:               |
*  | routing key 0                                                   |
*  | mask 0                                                          |
*  | direction 0                                                     |
*  | routing key 1                                                   |
*  | mask 1                                                          |
*  | direction 1                                                     |
*  |  (...)                                                          |
*  +-----------------------------------------------------------------+
*
*  Once the data is loaded the space is considered free fr future use.
*  In particular each neuon sets up its recording space once the function
*  configure_recording_space is called
* 
* AUTHOR
*  Thomas Sharp - thomas.sharp@cs.man.ac.uk
*
* DETAILS
*  Created on       : 03 May 2011
*  Version          : $Revision: 2054 $
*  Last modified on : $Date: 2012-11-11 22:17:23 +0000 (Sun, 11 Nov 2012) $
*  Last modified by : $Author: daviess $
*  $Id: config.c 2054 2012-11-11 22:17:23Z daviess $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/branches/app_frame_48/src/config.c $
* 
* COPYRIGHT
*  SpiNNaker Project, The University of Manchester
*  Copyright (C) SpiNNaker Project, 2010. All rights reserved.
*
*******/

#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "config.h"
#include "comms.h"
#include "dma.h"
#include "model_general.h"
#include "recording.h"



app_data_t app_data;

/****f* config.c/configure_app_frame
*
* SUMMARY
*  This function is called to allocate memory for the buffers in DTCM that
*  hold the synaptic data while processing incoming spikes.
*  In addition, the function allocates DTCM memory for the input buffers
*  for each of the neurons.
*  One network packet is allocated to forward spikes to the ethernet.
*
* SYNOPSIS
*  void configure_app_frame()
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
* 
* SOURCE
*/

void configure_app_frame()
{
    //set_number_of_cores(2);
    for (uint i = 0; synapse_lookup[i].core_id != 0xffffffff; i++)
    {
        if (synapse_lookup[i].row_size > app_data.synaptic_row_length)
        {
            app_data.synaptic_row_length = synapse_lookup[i].row_size;
        }
    }

    // Configure DMA pipeline
    io_printf(IO_STD, "Configure DMA pipeline\n");
    dma_pipeline.busy = FALSE;
    dma_pipeline.flip = 0;
/*    dma_pipeline.row_size_max = sizeof(synaptic_row_t) + sizeof(uint) * app_data.synaptic_row_length;*/
    dma_pipeline.row_size_max = sizeof(synaptic_row_t) + sizeof(uint) * app_data.total_neurons;
    dma_pipeline.cache[0] = (synaptic_row_t *) spin1_malloc(dma_pipeline.row_size_max);
    dma_pipeline.cache[1] = (synaptic_row_t *) spin1_malloc(dma_pipeline.row_size_max);
    dma_pipeline.synapse_lookup_address[0] = dma_pipeline.synapse_lookup_address[1] = NULL;
    dma_pipeline.row_size[0] = dma_pipeline.row_size[1] = 0;

    // Configure comms buffers
    //io_printf(IO_STD, "Configure comms buffers\n");    
    mc_packet_buffer.start = mc_packet_buffer.end = 0;
    mc_packet_buffer.buffer = spin1_malloc(MC_PACKET_BUFFER_SIZE * sizeof(int));
    spike_output = (sdp_msg_t *) spin1_malloc(sizeof(sdp_msg_t));
    spike_output->flags = 7;
    spike_output->tag = 1;
    spike_output->dest_port = 255;
    spike_output->srce_port = 1 << 5 | app_data.virtual_core_id;
    spike_output->dest_addr = 0;
    spike_output->srce_addr = spin1_get_chip_id();
    spike_output->cmd_rc = 0x100;
    spike_output->arg1 = 0;

    // Allocate global PSP buffers and point each population to its chunk of the PSP buffers
    psp_buffer = (psp_buffer_t *) spin1_malloc(sizeof(psp_buffer_t) * app_data.total_neurons);
    char *clear = (char *) psp_buffer;
    for(uint i = 0; i < sizeof(psp_buffer_t) * app_data.total_neurons; i++) clear[i] = 0;
    population[0].psp_buffer = psp_buffer;
    population[0].neuron = (void *) &population[app_data.num_populations];
    for(uint i = 1; i < num_populations; i++)
    {
        population[i].psp_buffer = population[i - 1].psp_buffer + population[i - 1].num_neurons;
        //REALLY ugly hack to get population pointers correct TODO improve
        uchar *offset = population[i - 1].neuron;
        offset += population[i - 1].num_neurons * population[i - 1].neuron_size;
        population[i].neuron = (void *) offset;
    }

    // Configure state recording space: this is specific for every neural model. 
    // model_general.h contains the prototype, bodies can be found in every neural model
    
    // Configure state recording space
    record_v = (short *) 0x72136400 + 0x200000 * (app_data.virtual_core_id - 1);
    record_i = (short *) 0x7309a400 + 0x200000 * (app_data.virtual_core_id - 1);
    record_spikes = (uint *) 0x72040000;
    for(uint i = 0; i < 1000; i++) record_spikes[i] = 0; //TODO improve
}

/*
*******/

/****f* config.c/load_application_data
*
* SUMMARY
*  This function is called to allocate memory in DTCM for the neural data
*  structures and the lookup table.
*  In addition, the memory allocated is filled with the data loaded from
*  SDRAM according to the map described earlier.
*
* SYNOPSIS
*  void load_application_data()
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
* 
* SOURCE
*/

void load_application_data()
{ //TODO clean up this horific mess of addresses and offsets
    app_data = *((app_data_t *) (0x74000000 + 0x10000 * (spin1_get_core_id() - 1)));
    app_data.virtual_core_id = spin1_get_core_id();
    
    io_printf(IO_STD, "app_data loading from...%x\n", &app_data);                
	app_data.virtual_core_id = spin1_get_core_id();
    
    uint size = 0;
    char *source = NULL;
    char *dest = NULL;
    
    size = app_data.num_populations * sizeof(population_t) + app_data.neuron_data_size;    
    //io_printf(IO_STD, "population_t...%d\n", sizeof(population_t));
    //io_printf(IO_STD, "app_data.neuron_data_size...%d\n", app_data.neuron_data_size);            
    source = (char *) 0x74000000 + sizeof(app_data_t) + 0x10000 * (app_data.virtual_core_id - 1);
    dest = (char *) spin1_malloc(size);
    for(uint i = 0; i < size; i++) dest[i] = source[i];
    num_populations = app_data.num_populations;
    //io_printf(IO_STD, "num populations...%d\n", num_populations);
    population = (population_t *) dest;
    // Load synapse lookup tables
    size = *((uint *) (0x74200000 + 0x1000 * (app_data.virtual_core_id - 1))) * sizeof(synapse_lookup_t); //TODO I want size in bytes!
    source = (char *) 0x74200004 + 0x1000 * (app_data.virtual_core_id - 1);
    dest = (char *) spin1_malloc(size);
    io_printf(IO_STD, "Load synapse lookup tables... source=%x, dest=%x\n", source, dest);       
    for(uint i = 0; i < size; i++) dest[i] = source[i];
    synapse_lookup = (synapse_lookup_t *)  dest;
}

/*
*******/

/****f* config.c/load_mc_routing_tables
*
* SUMMARY
*  This function loads the routing table from tSDRAM in the router
*
* SYNOPSIS
*  void load_mc_routing_tables()
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
* 
* SOURCE
*/

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

/*
*******/
