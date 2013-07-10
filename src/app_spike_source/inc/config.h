#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__



typedef struct
{
    uint run_time;
    uint synaptic_row_length;
    uint max_axonal_delay;
    uint num_populations;
    uint total_neurons;
    uint neuron_data_size;
    uint virtual_core_id;
    uint reserved_2;
} app_data_t;

typedef struct
{
    uint key;
    uint mask;
    uint route;
} mc_table_entry_t;

extern app_data_t app_data;

void configure_app_frame(void);
void load_application_data(void);
void load_mc_routing_tables(void);
uint virtual_to_physical_route(uint);



#endif

