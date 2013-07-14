#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
#include "model_delay_us.h"
#include "recording.h"

// Neuron data structures
uint num_populations;
population_t *population;
psp_buffer_t *psp_buffer;
spike_ring_t *spike_ring;
uint delay_us;

void send_spike_callback(uint time_ms, uint time_us) {
  uint time = time_ms * 1000 + time_us;

#ifdef DEBUG
    io_printf(IO_STD, "Event sender callback at time %u ms\n", time);
    io_printf(IO_STD, "Ring state :\n");
    io_printf(IO_STD, "\tstart : %u\n", spike_ring->ring_start);
    io_printf(IO_STD, "\tend : %u\n", spike_ring->ring_end);
    io_printf(IO_STD, "\tempty : %d\n", spike_ring->ring_empty);
    io_printf(IO_STD, "\tring addr : 0x%x\n", spike_ring->ring);
#endif

  // Process all events that should already have been sent
  while (spike_ring->ring_empty != 1 &&
         spike_ring->ring[spike_ring->ring_start].ts <= time) {
    spin1_send_mc_packet(spike_ring->ring[spike_ring->ring_start].key, NULL, NO_PAYLOAD);
    spike_ring->ring_start = (spike_ring->ring_start + 1) % SPIKE_QUEUE_SIZE;
    if (spike_ring->ring_start == spike_ring->ring_end) {
      spike_ring->ring_empty = 1;
    }
  }
  // Queue the next event in the ring if it exists
  if (!spike_ring->ring_empty) {
    schedule_trigger_timer2(send_spike_callback,
                            spike_ring->ring[spike_ring->ring_start].ts - time);
  }
}

void buffer_post_synaptic_potentials(void *dma_copy, uint row_size)
{
    uint time = spin1_get_simulation_time()*1000 + spin1_get_us_since_last_tick();

#ifdef DEBUG
    io_printf(IO_STD, "Inside buffer_post_synaptic_potentials\n");
#endif

    synaptic_row_t *synaptic_row = (synaptic_row_t *) dma_copy;
    neuron_t *neuron = (neuron_t *) population[0].neuron;

#ifdef DEBUG
    io_printf(IO_STD, "Received event at time %u ms\n", time);
    io_printf(IO_STD, "Ring state :\n");
    io_printf(IO_STD, "\tstart : %u\n", spike_ring->ring_start);
    io_printf(IO_STD, "\tend : %u\n", spike_ring->ring_end);
    io_printf(IO_STD, "\tempty : %d\n", spike_ring->ring_empty);
    io_printf(IO_STD, "\tring addr : 0x%x\n", spike_ring->ring);
#endif

    for(uint i = 0; i < row_size; i++)
    {
        synaptic_word_t decoded_word;

        decode_synaptic_word (synaptic_row->synapses[i], &decoded_word);

#ifdef DEBUG
        io_printf(IO_STD, "Type %d, weight %d, index %d, arrival %d, synapses: %08x, synaptic word: %08x\n", decoded_word.synapse_type, decoded_word.weight, decoded_word.index, time, (int) &synaptic_row->synapses[i], synaptic_row->synapses[i]);
#endif

        // Process only if the weight is non-zero
        if (decoded_word.weight == 0) {
#ifdef DEBUG
          io_printf(IO_STD, "Non connected pre-neuron, dropping\n", time);
#endif
          continue;
        }

        // Drop the event if the queue is full
        if (!spike_ring->ring_empty && spike_ring->ring_start == spike_ring->ring_end) {
          io_printf(IO_STD, "Queue full, dropped event at time %u us\n", time);
        } else {
          spike_ring->ring[spike_ring->ring_end].ts = time + delay_us;
          spike_ring->ring[spike_ring->ring_end].key =
            spin1_get_chip_id() << 16 |
            app_data.virtual_core_id << 11 |
            population[0].id | decoded_word.index;
          spike_ring->ring_end = (spike_ring->ring_end+1) % SPIKE_QUEUE_SIZE;
          if (spike_ring->ring_empty) {
#ifdef DEBUG
          io_printf(IO_STD, "Scheduling an event.\n");
#endif
            schedule_trigger_timer2(send_spike_callback, delay_us);
          }
          spike_ring->ring_empty = 0;
        }
    }
}


void set_population_flag(int operation, int flag_position, int population_id)
{
    if (operation == 0) population[population_id].flags = population[population_id].flags & (~(1<<flag_position));   // Clear
    if (operation == 1) population[population_id].flags = population[population_id].flags | (1<<flag_position);   // Set
}


void timer_callback(uint ticks, uint null) { }

// Functions specific for every neural model, implementing prototypes in model_general.h

void handle_sdp_msg(sdp_msg_t * sdp_msg)
{
    switch(sdp_msg->cmd_rc)
    {
        case 0x102:
            set_population_flag(sdp_msg->arg1, sdp_msg->arg2, sdp_msg->arg3);
            break;
        default:
            io_printf(IO_STD, "SDP ERROR: Instruction code %x not handled\n", sdp_msg->cmd_rc);
            break;
    }
}


void configure_recording_space()
{
    record_v = (short *) 0x72136400 + 0x200000 * (app_data.virtual_core_id - 1);
    record_i = (short *) 0x7309a400 + 0x200000 * (app_data.virtual_core_id - 1);

    // cleaning
    for (uint i = 0; i < 0x100000; i++)    record_v[i] = 0;
    for (uint i = 0; i < 0x100000; i++)    record_i[i] = 0;

    record_spikes = (uint *) 0x72040000;
    for(uint i = 0; i < 1000; i++) record_spikes[i] = 0; //TODO improve

    // spike count

    int *spike_count_dest = NULL;
    spike_count_dest = (int *) spin1_malloc(num_populations);
    spike_count =  (int *) spike_count_dest;
    for(uint i = 0; i < num_populations; i++) spike_count[i] = 0; //TODO improve

    // Initialize spike queue
    delay_us = ((neuron_t *)population[0].neuron)[0].delay;
    spike_ring = (spike_ring_t*)spin1_malloc((3+SPIKE_QUEUE_SIZE)*sizeof(uint));
    spike_ring->ring_start = 0;
    spike_ring->ring_end = 0;
    spike_ring->ring_empty = 1;
}

void decode_synaptic_word (unsigned int word, synaptic_word_t *decoded_word)
{
    decoded_word -> stdp_on = (word >> 27) & 0x01;
    decoded_word -> synapse_type = (word >>13) & 0x7;
    decoded_word -> weight = word & 0x1FFF;
    decoded_word -> index = (word >> 16) & 0x7ff;
    decoded_word -> delay = 1 + ((word >> 28) & 0xf);
    decoded_word -> weight_scale = 8;
}

unsigned int encode_synaptic_word (synaptic_word_t *decoded_word)
{
    uint encoded_word = 0;

    encoded_word = (((decoded_word -> stdp_on) & 0x01) << 27);
    encoded_word |= ((decoded_word -> synapse_type & 0x07) << 13);
    encoded_word |= ((decoded_word -> weight) & 0x1FFF);
    encoded_word |= (((decoded_word -> index) & 0x7ff) << 16);
    encoded_word |= (((decoded_word -> delay - 1) & 0x0f) << 28);

    return encoded_word;
}
