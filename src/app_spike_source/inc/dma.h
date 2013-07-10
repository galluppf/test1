#ifndef __DMA_H__
#define __DMA_H__



typedef struct
{
    uint busy;
    uint flip;
    uint row_size;
    uint *cache;
} dma_pipeline_t;



extern dma_pipeline_t dma_pipeline;


void buffer_mc_packet(uint, uint);
void dma_callback(uint, uint);
void feed_dma_pipeline(uint, uint);
void lookup_synapses(uint key);
uint mc_packet_buffer_empty(void);



extern int overflows;



#endif
