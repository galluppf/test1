#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
//#include "nef.h"
#include "model_nef_out_multidimensional.h"
#include "recording.h"

# define DECAY_VALUE 64884         // math.exp(-.001/.1) * 65536 = 64883.90590458548


// Neuron data structures
uint num_populations;
population_t *population;
psp_buffer_t *psp_buffer;

long long filtered_value[DIMENSIONS] = {0,0};

void handle_sdp_msg(sdp_msg_t * sdp_msg)
{
// void function
}


void buffer_post_synaptic_potentials(void *dma_copy, uint row_size)
{    
    uint time = spin1_get_simulation_time();
    
    synaptic_row_t *synaptic_row = (synaptic_row_t *) dma_copy;
    

    for(uint i = 0; i < app_data.synaptic_row_length; i++)
    {
/*        uint type = synaptic_row->synapses[0] & 0x7;*/
/*        uint index = synaptic_row->synapses[0] >> 16 & 0x7ff;*/
/*        uint arrival = 1 + time + (synaptic_row->synapses[0] >> 28 & 0xf);*/



/*        uint weight = synaptic_row->synapses[i] & 0xfffff;*/
        uint type = (synaptic_row->synapses[i] >> 20) & 0x1;
        uint index = (synaptic_row->synapses[i] >> 21) & 0x7ff;
        uint arrival = 1 + time;

/*        io_printf(IO_STD, "Type %d, index %d\n", type, index);*/
        
        switch(type)
        {
            case 0: psp_buffer[index].exci[arrival % PSP_BUFFER_SIZE] = 1; break; // TODO OMG that's a hack! it will write 1 every time it received a packet
/*            case 0: psp_buffer[index].exci[arrival % PSP_BUFFER_SIZE] += weight; break;*/
            case 1: psp_buffer[index].inhi[arrival % PSP_BUFFER_SIZE] = 1; break;
            default: break;
        }
    }
}


void timer_callback(uint ticks, uint null)
{
/*    io_printf(IO_STD, "Time: %d\n", ticks);*/

    if(ticks == 1)  io_printf(IO_STD, "nef_out d: %d.\n", DIMENSIONS);

    int out_value[DIMENSIONS];

    for (int d = 0; d < DIMENSIONS; d++)    out_value[d] = 0;    
    
/*    if(ticks % 128 == 0)  io_printf(IO_STD, "Time: %d\n", ticks);*/

/*    if(ticks == 1 && app_data.virtual_core_id == 1 && spin1_get_chip_id() == 0)*/
/*    {*/
/*        io_printf(IO_STD, "Sending sync spike.\n");*/
/*        send_sync_sdp();*/
/*    }*/

    
    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Simulation complete");
        spin1_stop();
    }
    

    for(uint i = 0; i < num_populations; i++)
    {

        neuron_t *neuron = (neuron_t *) population[i].neuron;
        psp_buffer_t *psp_buffer = population[i].psp_buffer;
        
/*        out_value = 0;*/
        
        for(uint j = 0; j < population[i].num_neurons; j++)
        {
        
/*            if (ticks == 1)  io_printf(IO_STD, "decoder: %d\n", neuron[j].decoder);        */
            if(ticks <= 16)    psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE] = 0;        // FIXME Superhack: why are buffer not clean on startup? Got -1s
                                    
            // Get excitatory and inhibitory currents from synaptic inputs                    
            short exci_current = psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE];
            short spike_received=exci_current;

/*            if (spike_received>0)   io_printf(IO_STD, "n %d\n", j);*/

/*            out_value += (int)(spike_received)*neuron[j].decoder;            */
            
            for (int d = 0; d < DIMENSIONS; d++)    out_value[d] += spike_received*neuron[j].decoder[d];
            
            // Clear PSP buffers for this timestep
            psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE] = 0;
            psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE] = 0;
        }

    }
/*    io_printf(IO_STD, "T:%d;x=%d,%d\n", ticks, out_value[0] >> 8, out_value[1] >> 8);*/


    send_out_2d_value(1, 0, 0, out_value[0] >> 8, out_value[1] >> 8);        

    filtered_value[0] = (out_value[0] * (65536 - DECAY_VALUE) + filtered_value[0]*DECAY_VALUE) >> 16;
    filtered_value[1] = (out_value[1] * (65536 - DECAY_VALUE) + filtered_value[1]*DECAY_VALUE) >> 16;
    
    
/*    self.data[0]*(1-DECAY)+self.last_value[0]*DECAY*/


/*    if (ticks==1 && app_data.virtual_core_id==5)*/
    if (ticks%4096==1 && (population[0].flags & VALUE_ROBOT_OUTPUT))
    {   
        uint mgmt_key = 0;
        uint mgmt_payload = 0;
        

        // FWD / BWD neuron
        mgmt_key = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | 0x400 + 0x1;
        mgmt_payload = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | 0;        
        spin1_send_mc_packet(mgmt_key, mgmt_payload, 1);
        
        
        spin1_delay_us(100);

//        io_printf(IO_STD, "0x%x-0x%x\n", mgmt_key, mgmt_payload);
        
        // TURN CW / CCW neuron
        mgmt_key = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | 0x400 + 0x5;
        mgmt_payload = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | 1;
        spin1_send_mc_packet(mgmt_key, mgmt_payload, 1);        
        
        // LEFT / RIGHT neuron
//        mgmt_key += 2;        
//        mgmt_payload += 4;
        // spin1_send_mc_packet(mgmt_key, mgmt_payload, 1);        

        
    }
    
        
    if(ticks%256==0 && (population[0].flags & VALUE_ROBOT_OUTPUT))
    {   
    
        uint cpsr = spin1_irq_disable();            //!MUTEX!                    
        
        // send a command every 64 msec
        uint key = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | 0;
        spin1_send_mc_packet(key, ((int)filtered_value[0] >> 10), 1);     // filtered version

        spin1_delay_us(40);

        key += 1;
        spin1_send_mc_packet(key, ((int)(filtered_value[1]) >> 12), 1);       // filtered version

//        io_printf(IO_STD, "%d, %d\n", ((int)(filtered_value[0]) >> 10), ((int)(filtered_value[1]) >> 12));

        spin1_mode_restore(cpsr);


    }
    
}


void send_out_2d_value(int arg1, int arg2, int arg3, int out_value_0, int out_value_1)
{

    sdp_msg_t sdp_value_packet;
    int * ptr = (int *)&(sdp_value_packet.data[0]);
    //io_printf (IO_STD, "sending packet\n");
    sdp_value_packet.tag = 1;
    sdp_value_packet.dest_port = PORT_ETH;
    sdp_value_packet.dest_addr = 0;
    sdp_value_packet.flags = 0x07;
    sdp_value_packet.srce_port = 4 << 5 | app_data.virtual_core_id;
    sdp_value_packet.srce_addr = 0;
    ptr[0] = out_value_0;
    ptr[1] = out_value_1;
    sdp_value_packet.length = 32;
    sdp_value_packet.cmd_rc = 0x102;
    sdp_value_packet.arg1 = arg1;
    sdp_value_packet.arg2 = arg2;
    sdp_value_packet.arg3 = arg3;

    spin1_send_sdp_msg (&sdp_value_packet, 5);
/*    io_printf(IO_STD, "sending value %d\n", out_value);*/
    sdp_value_packet.arg1 = 0;
}


