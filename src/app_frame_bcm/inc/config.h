#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__


/****s* config.h/app_data_t
*
* SUMMARY
*  This structure holds all the data relevant for the simulation
*
* FIELDS
*  run: time (in msec) of the simulation
*  synaptic_row_length: maximum synaptic row length
*  max_axonal_delay: maximum axonal delay
*  num_populations: number of neural population
*  total_neurons: total number of neurons simulated in the core
*  neuron_data_size: size (in bytes) of the neural data (including all neurons)
*  virtual_core_id: id of the core (filled at run time)
*  reserved_2: for future use
*
* SOURCE
*/

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

/*
*******/

/****s* config.h/mc_table_entry_t
*
* SUMMARY
*  This structure holds a single routing entry
*  (see SpiNNaker datasheet for details)
*
* FIELDS
*  key: routing key to match
*  mask: routing mask for selection
*  route: direction to which propagate the packet
*
* SOURCE
*/

typedef struct
{
    uint key;
    uint mask;
    uint route;
} mc_table_entry_t;

/*
*******/

extern app_data_t app_data;

void configure_app_frame(void);
void load_application_data(void);
void load_mc_routing_tables(void);


#endif

