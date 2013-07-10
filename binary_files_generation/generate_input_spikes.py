import struct
import sys
import math
import pacman
#from math import *
#from struct import *

DEBUG = False

def SpikeSourceArrayDummy(spike_times,total_simulation_runtime, outputFileName, nNeurons):
    """
          Input 1: Input spike times in the following format spike_times=([ (1,3),(0,2) ])
          where spike_times[0] is Input neuron[0] and spike_times[1] is Input neuron[1]
          Input 2: Total simulation time in ms
          Output: A binary file named as the string saved in outputFileName variable where the LSB is the first neuron (Unless more that 32 input neurons are used)
    """                                   
#    outputFileName="InputSpikesBinary" # Filename of the binary file to be loaded to the SDRAM
    saveBinarySpikes=open(outputFileName,"wb")
#    numberOfInputNeurons=len(spike_times)
    if DEBUG: print "Number of Neurons are %d" % int(nNeurons)
    numberOfInputNeurons=nNeurons
    binarySpikeList=[]
    bebugList=[] # Used for Debugging reasons
    binarySpikeForSDRAM=""
    WORD=32 # Total size of a word of the SDRAM
    if((numberOfInputNeurons%WORD)==0): # The modulus is checked in order not to create additional memory offset were not needed. E.G. when 32 neurons / 32 (size of WORD) we need 1 memory fetch per ms not 2
        checkMod=0
    else:
        checkMod=1    
    memoryLine=numberOfInputNeurons/(WORD) # Calculate how many words needed for each ms for the input spikes
    if DEBUG: print "For each ms %d WORD fetch is needed in the SDRAM"%(memoryLine+checkMod)
    if DEBUG: print "Total Bytes Needed: %d"%((4*(memoryLine+checkMod))*total_simulation_runtime)
#    for eachMsOfSimTime in range(total_simulation_runtime): # For each ms of sim time and for each neuron calculate the binary format (0 no spike, 1 spike), from 0 to simtime-1
    for eachMsOfSimTime in range(1,(total_simulation_runtime+1)): # For each ms of sim time and for each neuron calculate the binary format (0 no spike, 1 spike), from 0 to simtime-1
        for eachInputNeuron in range(numberOfInputNeurons):
            try: # Check if that particular Neuron is Iterable (has >1 spikes)
                if (eachMsOfSimTime in spike_times[eachInputNeuron]):
                    binarySpikeList.append('1')
                else:
                    binarySpikeList.append('0')
            except TypeError: # If the Neuron has only 1 spike (Integer instead of Iterable)
                if (spike_times[eachInputNeuron]==eachMsOfSimTime):
                    binarySpikeList.append('1')
                else:
                    binarySpikeList.append('0')
            except IndexError: # If more input neurons than spike times were given
                binarySpikeList.append('0')

        tempa=[] #Temp list to break the input spikes of the neurons into words
        for x in range(memoryLine+checkMod):
            tempa.append(binarySpikeList[(x*WORD):((x+1)*WORD)])
        for x in range(memoryLine+checkMod):
            tempa[x].reverse()
            binarySpikeStr=''.join(tempa[x])     # The list is converted to string
            if DEBUG: print "BINARY STRING  IS %s \t ms is %d \t Length is %d \t Number is %d "% (binarySpikeStr,eachMsOfSimTime,len(tempa[x]),int(binarySpikeStr,2))
            binarySpikeList=[]
            binarySpikeForSDRAM+=struct.pack("<I",int(binarySpikeStr,2))
    saveBinarySpikes.write(binarySpikeForSDRAM)
    saveBinarySpikes.close()
    
        
def compile_spike_source_array_old(db):
    """
    Split the lists for the SpikeSourceArrays and compile the binary files sot each SpikeSourceArray core
    Recomputes the indexes for the list of split SpikeSourceArray
    """

    # get the method associated with the SpikeSourceArray
    method_id = [ t['method_type'] for t in db.get_distinct_cell_method_types() if t['name'] == 'SpikeSourceArray']
    if method_id == []:
        print '[ generate_input_spikes ] : SpikeSourceArray: nothing to do'
        return
        
    method_id = method_id[0]        # there will be only one
    
    # get the runtime
    runtime = eval(str(db.get_runtime()))                                              

    # get the processors in the system
    processors = db.get_processors()

    # cycling all mapped processor
    for processor in processors:                                        
        # getting the part populations for that processor
        part_populations = db.get_part_populations_from_processor(processor['id'])       
        for p in part_populations:  # cycling the part_populations in the processor
            if p['method_type'] == method_id:       # searching for a SpikeSourceArray
                if DEBUG:   print 'found SpikeSourceArray', p, processor
                spike_times = eval(p['parameters'])['spike_times']              # getting spike times for the whole population                

                # TODO check format and convert if necessary
                
                if DEBUG:   print spike_times   
                spike_times = spike_times[p['offset']:p['offset']+p['size']]  # slicing out the part_population portion
                if DEBUG:   print spike_times   
                out_file_name = "%s/spike_source_data_%d_%d_%d.dat" % (pacman.BINARIES_DIRECTORY, processor['x'], processor['y'], processor['p'])    # output file will be in the format spike_source_data_xchip_ychip_procid.dat
		if DEBUG:   print p['size']
                SpikeSourceArrayDummy(spike_times, runtime, out_file_name, p['size'])      # compiling the relative portion of the SpikeSourceArray
    

def compile_spike_source_array(db):

    method_id = [ t['method_type'] for t in db.get_distinct_cell_method_types() if t['name'] == 'SpikeSourceArray']
    if method_id == []:
        print '[ generate_input_spikes ] : SpikeSourceArray: nothing to do'
        return

    method_id = method_id[0]        # there will be only one    

    # get the runtime
    runtime = eval(str(db.get_runtime()))                                              

    # get the processors in the system
    processors = db.get_processors()

    # cycling all mapped processor
    for processor in processors:                                        
        # getting the part populations for that processor
        part_populations = db.get_part_populations_from_processor(processor['id'])  
        #print part_populations
        #print len(part_populations)
        if len(part_populations) > 0:
            #print part_populations[0]['method_type']
            #print method_id
            if part_populations[0]['method_type'] == method_id:       # searching for a SpikeSourceArray
                spike_list_population = []
                spike_output_list = []
                total_size = 0
                neuron_id = 0
                for p in part_populations:  # cycling the part_populations in the processor
                    #print p
                    size = p['size']
                    total_size += size
                    spike_times = eval(p['parameters'])['spike_times']
                    if size > len(spike_times):
#                        print "Error in the definition of the spikeing input: population size -", size, "input defined for", len(spike_times), "neurons"
                        raise StandardError('Error in the definition of the spiking input: population size - %d, input defined for %d neurons' % (size, len(spike_times)))
#                    print "size of spike times", len(spike_times)
                    #print spike_times
                    for n in range(0,size): #iteration for each of the spike times neuron's list
                        for s in range(0,len(spike_times[n])): #iteration for each of the spike times
                            spike_list_population.append([int(spike_times[n][s]), neuron_id + n])
                    neuron_id += size
                spike_list_population.sort()
                #print spike_list_population
                #print "total_size: ", total_size
                words_per_msec = int(math.ceil(float(total_size) / 32))
                #print "words_per_msec: ", words_per_msec
                spikes = [0 for w in range (0,words_per_msec)]
                current_msec = 1

                #reshaping the spike time lists into a list of couples [[time, neuron], [time, neuron], [time, neuron], ... ]
                #then sorting, so that the time is the first element sorted and spikes are then stored followinf the usual bitmask
                for couples in range(0,len(spike_list_population)):
                    if spike_list_population[couples][0] != current_msec:
                        for time in range(current_msec, spike_list_population[couples][0]):
                            spike_output_list.append(spikes)
                            current_msec += 1
                            spikes = [0 for w in range (0,words_per_msec)]
                    word_affected = int(math.floor(spike_list_population[couples][1] / 32))
                    bit_affected = spike_list_population[couples][1] % 32
                    #print "neuron: ",spike_list_population[couples][1] ,", word_affected: ",word_affected,", bit_affected: ", bit_affected
                    spikes[word_affected] |= 1 << bit_affected
                spike_output_list.append(spikes)

                #completing the input file after the last spike: the last spike may not coincide with the simulation run time
                runtime = eval(str(db.get_runtime()))
                spikes = [0 for w in range (0,words_per_msec)]
                for null_spikes in range(current_msec, runtime):
                    spike_output_list.append(spikes)
                    current_msec += 1

                out_file_name = "%s/spike_source_data_%d_%d_%d.dat" % (pacman.BINARIES_DIRECTORY, processor['x'], processor['y'], processor['p'])    # output file will be in the format spike_source_data_xchip_ychip_procid.dat
                fspike = open(out_file_name, "wb")
                
                for a in range (0,len(spike_output_list)):
                    for b in range (0,words_per_msec):
                        data = struct.pack("<I", spike_output_list[a][b])
                        fspike.write(data)

