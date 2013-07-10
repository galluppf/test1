/*#include "spinn_api.h"*/
#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "model_general.h"
#include "model_spike_source_poisson.h"
#include "config.h"
#include "recording.h"

#include "lfsr.h"

uint num_populations;
population_t *population;

unsigned int seed = 283645;

void process_spikes(void *dma_copy)
{
}

void handle_sdp_msg(sdp_msg_t * sdp_msg)
{
/*
    switch(sdp_msg->cmd_rc)
    {
        case 0x102: // SDP input spikes packet
            spin1_memcoy(&(neuron[sdp_msg->arg1]), (uint *) sdp_msg->data, sizeof(neuron_t));
            break;
        default:
            break;
    }
    spin1_msg_free(sdp_msg);
*/
}

unsigned int ISI_calculator(unsigned long long rate_conv)
{
//    unsigned long long p = ~((long long)0);
    uint_64_t p;
    unsigned int k = 1;
    unsigned int temp;

    p.value = 0x7FFFFFFFFFFFFFFF;

    while (p.value > rate_conv)
    {
        seed = shift_lfsr(seed);

        //multiplication of 0.31 (with the top bit being always zero) term with a 0.64 term. The outcome is 0.95 and it is stored as 0.64 truncating the lowest bits.
        temp = (unsigned int)((((unsigned long long) seed) * (unsigned long long) p.words.lo) >> 32);
        p.value = (((unsigned long long) seed) * (unsigned long long) p.words.hi) + temp;

        k++;
    }

//    io_printf(IO_STD, "%d\n", k-1);

    return k-1;
}

void timer_callback(uint ticks, uint null)
{

    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Simulation complete.\n");
        spin1_stop();
    }

    for(uint i = 0; i < num_populations; i++)
    {
        neuron_t *neuron = (neuron_t *) population[i].neuron;

        for(uint j = 0; j < population[i].num_neurons; j++)
        {
//            io_printf(IO_STD, "Time: %d, neuron %d, start: %d, end: %d, tts: %d\n", ticks, j, neuron[j].start, neuron[j].end, neuron[j].time_to_next_spike);

            if (ticks >= neuron[j].start && ticks < neuron[j].end)
            {
                neuron[j].time_to_next_spike--;

                if (neuron[j].time_to_next_spike <= 0)
                {//fire
                    uint key = spin1_get_chip_id() << 16 |
                        app_data.virtual_core_id << 11 |
                        population[i].id |
                        j;

                    spin1_send_mc_packet(key, NULL, NO_PAYLOAD);

//                    io_printf(IO_STD, "key sent: %08x\n", key);

                //recharge counter
                    neuron[j].time_to_next_spike = ISI_calculator(neuron[j].rate_conv);
//                    io_printf(IO_STD, "next TTS: %d\n", neuron[j].time_to_next_spike);

                // Optionally record spikes FROM A SINGLE POPULATION ONLY
                    if(population[i].flags & RECORD_SPIKE_BIT)
                    {
                        uint size = population[i].num_neurons >> 5;
                        size += (population[i].num_neurons & 0x1F) ? 1 : 0;

                        record_spikes[(size * ticks) + (j >> 5)] |= 1 << (j & 0x1F);
                    }
                }
            }
        }
    }
}

void configure_recording_space()
{
    record_spikes = (uint *) 0x72040000;
    for(uint i = 0; i < 1000; i++) record_spikes[i] = 0; //TODO improve 

}

