/****a* stdp_tts.c/stdp_tts
*
* SUMMARY
*  This file contains the functions related to the STDP learning algorithm
*  with Time-To-Spike forecast approximation.
*  The algorithm computes the synaptic depotentiation using the time
*  difference with the latest spike emitted by the post-synaptic neuron and
*  subsequently computes the potentiation using a forecast function based
*  on the spost-synaptic neuron membrane potential.
*  The forecast scheme can be described as a two-segment function:
*    * between (+30mV, 0 msec) and (-40mV, 3msec)
*    * between (-40mV, 3msec) and (L_paramater, 32 msec)
*  The L_parameter is expressed in millivolt and represents a parameter of
*  the forecast rule. The theory of this algorithm is described in the paper
*  "A forecast-based STDP rule suitable for neuromorphic implementation" by 
*  Sergio Davies, Alexander Rast, Francesco Galluppi and Steve Furber
*
* AUTHOR
*  Sergio Davies - daviess@cs.man.ac.uk
*
* DETAILS
*  Created on       : 26 May 2011
*  Version          : $Revision: 2061 $
*  Last modified on : $Date: 2012-11-13 13:51:25 +0000 (Tue, 13 Nov 2012) $
*  Last modified by : $Author: daviess $
*  $Id: stdp_tts.c 2061 2012-11-13 13:51:25Z daviess $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/testing/src/stdp_tts.c $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#ifdef STDP_TTS

#include "spin1_api.h"
#include "spinn_io.h"

#include <string.h>
#include "stdp_generic.h"
#include "stdp_tts.h"
#include "config.h"
#include "dma.h"

#include "model_general.h"
#include "model_izhikevich.h"

//#define L -65 //set the "L" parameter for the forecast rule

//#define DEBUG_PRINT_STDP_TTS
//#define DEBUG_STDP_ALLOC
//#define DEBUG_PRINT_STDP_POST_UPDATE

int *PostTimeStamp = NULL;
extern app_data_t app_data;
stdp_table_t *stdp_table = NULL;
unsigned short int *voltage_addr = NULL;

int a1, a2;

#ifdef TIMER_MEASURE
unsigned long long iteration_count=0, iterations_num=0;
#endif

/****f* stdp_tts.c/load_stdp_config_data
*
* SUMMARY
*  The function allocates memory for the STDP data structure, as defined in
*  stdp_table_t struct, and copies the data from the SDRAM location where it was
*  loaded to the allocated memory are in DTCM. For a complete memory map refer
*  to config.c file.
*  In addition, an array of pointers to neuron's membrane potential is set up
*  to facilitate forecast. The array stores only the lowest two bytes of the
*  address as the DTCM is 64K (addressable using 16 bits), starting at address
*  0x400000.
*  Finally, the two parameters a1 and a2 are computed as function of the
*  L parameter of the TTS rule, and are used in the forecast function
*
* SYNOPSIS
*  void load_stdp_config_data ()
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
*
* SOURCE
*/

void load_stdp_config_data ()
{
    unsigned int src;
    int i, j, counter;
    short int L;

    stdp_table = (stdp_table_t *) spin1_malloc (sizeof(stdp_table_t));
    if (stdp_table == NULL)
    {
         io_printf(IO_STD, "memory allocation error\n");
         while (1); //TODO: stop simulation
    }
    src = STDP_START_ADDR + STDP_BLOCK_SIZE * (spin1_get_core_id() - 1);
    spin1_memcpy ((void*) stdp_table, (void*)src, sizeof(stdp_table_t));

    io_printf(IO_STD, "resolution: %d\n", stdp_table->resolution);
    if (stdp_table->resolution != 1 && stdp_table->resolution != 2 && stdp_table->resolution != 4 && stdp_table->resolution != 8 && stdp_table->resolution != 16 && stdp_table->resolution != 32 )
    {
        io_printf(IO_STD, "resolution can only be 1, 2, 4, 8, 16 or 32 millisecond(s)\nSetting the resolution to 1\n");
        stdp_table->resolution = 1;
        stdp_table->log_resolution = 0;
    }

    voltage_addr = (unsigned short int*) spin1_malloc (app_data.total_neurons * sizeof(unsigned short int));

    counter = 0;

    for (i = 0; i < num_populations; i++)
    {
        neuron_t *ptr = (neuron_t *) population[i].neuron;
        for (j = 0; j < population[i].num_neurons; j++)
        {
            voltage_addr[counter++] = (unsigned short int)((((int)(&(ptr[j].v))) & 0xFFFF));
        }
    }

    L = stdp_table -> L_parameter;

    a1 = ((29 * 256) / ((L) + 40));
    a2 = (((1280 + 3 * (L)) * 256) / ((L) + 40));

    io_printf(IO_STD, "Forecast parameters: a1=%d a2 = %d\n", a1, a2);
}

/*
*******/

/****f* stdp_tts.c/alloc_stdp_post_TS_mem
*
* SUMMARY
*  The function allocates the memory for the STDP algoritm and sets
*  this memory areas to 0x00. The memory allocated is used to store
*  the timestamp of the last post-synaptic event;
*
* SYNOPSIS
*  int alloc_stdp_post_TS_mem ()
*
* INPUTS
*  NONE
*
* OUTPUTS
*   0: function completed succesfully
*  -1: memory allocation error
*
* SOURCE
*/

int alloc_stdp_post_TS_mem ()
{
    int size = stdp_table -> ps_record_words;
    int numNeurons = app_data.total_neurons;

#ifdef DEBUG_STDP_ALLOC
    io_printf (IO_STD, "Post-synaptic record size: %d\n", size);
#endif

    if (size != 1)
    {
        io_printf (IO_STD, "Post-synaptic record size must be equal to 1 for voltage-gated STDP\n");
        size = stdp_table -> ps_record_words = 1;
    }

    if ((PostTimeStamp = (int *)spin1_malloc(sizeof(int) * app_data.total_neurons ))==NULL)          //time stamp for post-synaptic neurons
    {
        io_printf(IO_STD, "memory allocation error\n");
        return -1;
    }

#ifdef DEBUG_STDP_ALLOC
    io_printf(IO_STD, "PostTimeStamp = 0x%08x, size = %d\n", PostTimeStamp, sizeof(int) * numNeurons * size);
#endif

    spin1_memset(PostTimeStamp, 0x00, sizeof(int) * numNeurons * size);

    return 0;
}
/*
*******/

/****f* stdp_tts.c/StdpPostUpdate
*
* SUMMARY
*  The function updates the time of the last spike emitted
*  by the post-synaptic neuron. The function stores the information
*  in the PostTimeStamp array.
*
* SYNOPSIS
*  void StdpPostUpdate (unsigned int neuronID, unsigned int currentSimTime)
*
* INPUTS
*  neuronID: ID of the neuron emitting a spike. In the case of multiple
*            populations inside a single core, this is a sequentionl number
*            that starts from 0 (firt neuron of the first population) and
*            increments for all the neurons in all the populations in the core
*  currentSimTime: current simulation time
*
* OUTPUTS
*  NONE
*
* SOURCE
*/

void StdpPostUpdate (unsigned int neuronID, unsigned int currentSimTime)
{
    unsigned char resolution = stdp_table->log_resolution;
    unsigned int resolution_bitmask = (0x01 << resolution) - 1;

    PostTimeStamp[neuronID] = currentSimTime & ~resolution_bitmask;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "Inside c_STDP_post_Update. Time: %d, neuron: %d\n", currentSimTime, neuronID);
#endif
}

/*
*******/

/****f* stdp_tts.c/forecast_tts
*
* SUMMARY
*  The function forecasts the future spike time of a neuron
*  usiiig its membrane potential as key feature for the evaluation.
*
* SYNOPSIS
*  char forecast_tts (int voltage)
*
* INPUTS
*  voltage: membrane potential on which evaluate the time-to-spike.
*           The input value is represented with a fixed point structure
*           using 8 bit for the integer part and 8 for the decimal part (8.8).
*
* OUTPUTS
*  The value returned represents the forecasted time required for the neuron to
*  spike, based on its membrane potential
*
* SOURCE
*/

char forecast_tts (int voltage)
{
    char return_val;

    if (voltage >= 7680) //Izhikevich neuron spiking condition
        return_val = 0;
    else if (voltage >= 1707)
        return_val = 1;
    else if (voltage >= -4267)
        return_val = 2;
    else if (voltage >= -10240)
        return_val = 3;
    else
    {
        int linear_forecast;
        linear_forecast = (((((a1 * (int)voltage) >> 8) + a2) >> 8)); // general forecast rule dependent on the parameter L

        if (linear_forecast > 127 || linear_forecast < 0)
            return_val = 127;
        else
            return_val = linear_forecast;
    }
    
#ifdef DEBUG_PRINT_STDP_TTS
    io_printf (IO_STD, "Forecast for membrane potential %d: %d msec\n", voltage, return_val);
#endif

    return return_val;
}

/*
*******/


/****f* stdp_tts.c/retrieve_voltage
*
* SUMMARY
*  The function retrieves the membrane potential for a given neuron
*  ID inside the core using the array voltage_addr as described
*  in load_stdp_config_data
*
* SYNOPSIS
*  int retrieve_voltage (int neuron)
*
* INPUTS
*  neuron:
*
* OUTPUTS
*  The value returned represents the membrane potential of the
*  requested neuron
*
* SOURCE
*/

int retrieve_voltage (int neuron)
{

    int* memory = (int*)0x400000;

#ifdef DEBUG_PRINT_STDP_TTS
    io_printf (IO_STD, "Membrane potential for neuron %d requested\n", neuron);
#endif

    return memory[((voltage_addr[neuron])>>2)];

}

/*
*******/

/****f* stdp_tts.c/Stdp
*
* SUMMARY
*  The function computes the synaptic weight update (where required)
*  following the STDP_TTS rule. FInally the weights updated are transferred
*  back to SDRAM
*
* SYNOPSIS
*  void Stdp (synaptic_row_t *start, unsigned short int row_size, unsigned int currentSimTime)
*
* INPUTS
*  start: address of the synaptic weight block in DTCM
*  row_size: number of synapses to process
*  currentSimTime: simulation time
*
* OUTPUTS
*  NONE
*
* SOURCE
*/

void Stdp (synaptic_row_t *start, unsigned short int row_size, unsigned int currentSimTime)
{
    // the resolution is in bits!!! eg. 2 ms resolution -> 1 bit, 4 msec resolution -> 2 bits
    unsigned char resolution = stdp_table->log_resolution;
    unsigned int k;

    unsigned int CurrentTick = currentSimTime;
    unsigned int pre_time;
    unsigned int resolution_bitmask, delay_bitmask;

#ifdef DEBUG_PRINT_STDP_TTS
    io_printf (IO_STD, "Inside STDP. Src neuron ID: 0x%08x\n", start -> neuron_id);
#endif

    resolution_bitmask = (0x01 << resolution) - 1;
    delay_bitmask = 0x0F & (~resolution_bitmask);

    pre_time = CurrentTick;

#ifdef DEBUG_PRINT_STDP_TTS
    io_printf (IO_STD, "Number of synaptic word to process: %d\n", row_size);
#endif

    for (k = 0; k < row_size; k++)
    {
        unsigned int post_time;
        //int membrane_potential_old;
        int membrane_potential;
        char time_to_spike;

        synaptic_word_t decoded_word;

        decode_synaptic_word (start -> synapses[k], &decoded_word);

#ifdef DEBUG_PRINT_STDP_TTS
        io_printf (IO_STD, "Start synaptic word: 0x%08x\n", start -> synapses[k]);
#endif

        if (decoded_word.stdp_on)
        {

#ifdef DEBUG_PRINT_STDP_TTS
            io_printf (IO_STD, "Time: %d, Destination neuron: 0x%08x, Delay: 0x%08x, Weight: 0x%08x, Connector type: %08x\n", currentSimTime, decoded_word.index, decoded_word.delay, decoded_word.weight, decoded_word.synapse_type);
#endif

            post_time = PostTimeStamp[decoded_word.index];

            //compute LTD
#ifdef DEBUG_PRINT_STDP_TTS
            io_printf (IO_STD, "Pre-time: %d, Pre-time with delay: %d, Post-time: %d, Old weight: %04x, ", pre_time, ((pre_time + decoded_word.delay) & ~resolution_bitmask), post_time, decoded_word.weight);
#endif

            if (post_time != 0)
            {
                decoded_word.weight = STDP_update (((pre_time + decoded_word.delay) & ~resolution_bitmask), post_time, decoded_word.weight, decoded_word.weight_scale);

#ifdef DEBUG_PRINT_STDP_TTS
                io_printf (IO_STD, "New weight: 0x%04x\n", decoded_word.weight);
#endif
            }

            //compute LTP
            membrane_potential = retrieve_voltage(decoded_word.index);

            time_to_spike = forecast_tts (membrane_potential);

            if (((pre_time + decoded_word.delay) & delay_bitmask) <= (CurrentTick + time_to_spike)& ~resolution_bitmask)
            {

#ifdef DEBUG_PRINT_STDP_TTS
                io_printf (IO_STD, "Membrane Potential: %d, Time to spike forecast: %d, current tick (with resolution): %d, Post-time: %d, Old weight: %04x, ", membrane_potential, ((pre_time + decoded_word.delay) & ~resolution_bitmask), (CurrentTick + time_to_spike + 1)& ~resolution_bitmask, decoded_word.weight);
#endif

                decoded_word.weight = STDP_update (((pre_time + decoded_word.delay) & ~resolution_bitmask), (CurrentTick + time_to_spike + 1)& ~resolution_bitmask, decoded_word.weight, decoded_word.weight_scale);

#ifdef DEBUG_PRINT_STDP_TTS
                io_printf (IO_STD, "New weight: 0x%04x\n", decoded_word.weight);
#endif
            }
            //reconstruct the synaptic word and store it back
            start -> synapses[k] = encode_synaptic_word (&decoded_word);


#ifdef DEBUG_PRINT_STDP_TTS
            io_printf (IO_STD, "End synaptic word: 0x%08x\n", start -> synapses[k]);
#endif
        }
    }

#ifdef OUTPUT_WEIGHT
    send_weight (CurrentTick, start, (row_size + 3)<<2);
#endif


#ifdef DEBUG_PRINT_STDP_TTS
    io_printf (IO_STD, "STDP finished\n");
#endif
}

/*
*******/

#endif

