#!/usr/bin/python
"""
Neuron Creator: creates the neural binary data structures.

Parameters are taken from the Network DB. Parameters can be values or lists, as specified by pynn
Translations and positions are taken from the Model Library

Each data file has the following structure::

 Header:
 --------------------------
 - uint runtime
 - unit synaptic row length
 - unit max_delay
 - uint num_pops
 - uint total_neurons
 - uint size_neuron_data
 - uint reserved2 (NULL)
 - uint reserved3 (NULL)
 --------------------------
 population metadata (for each population in the core, if grouping is activated)
 - uint pop_id 
 - uint flags
 - uint pop_size (number of neurons in the population)
 - uint size_of_neuron (size of a single neuron)
 - uint reserved1 (NULL)
 - uint reserved2 (NULL)
 - uint reserved3 (NULL)
 --------------------------
 neural structures
 ....
 

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester, January 2013. francesco.galluppi@cs.man.ac.uk

"""


import struct 
import sys

from numpy import array

import math
from scipy.stats import poisson     # needed by spikesource Poisson


import pyNN.random as r
import pacman

DEBUG = pacman.pacman_configuration.getboolean('neural_data_structure_creator', 'debug')
PARALLEL = pacman.pacman_configuration.getboolean('neural_data_structure_creator', 'parallel')


p1 = 256
p2 = 65536


def write_neural_data_structures(db):
    # gets and builds the random generators
    # insert parallelization logic

    processors = db.get_used_processors()    
    
    
    if PARALLEL:        
        from multiprocessing import Process, Queue
        num_processes = 2
        
        def compute_neural_structures_parallel_callback(db, processors):
            import time
            time.sleep(0.1)
            for p in processors: compute_neural_structures(db, p)
            
        
        print '[ neuron writer ] : Spawning %d parallel threads' % num_processes
        # parallelizing http://stackoverflow.com/questions/8521883/multiprocessing-pool-map-and-function-with-two-arguments
        # 16.6.1.5. Using a pool of workers: http://docs.python.org/library/multiprocessing.html                
        # not with SQLITE apparently
        
        # http://stackoverflow.com/questions/312443/how-do-you-split-a-list-into-evenly-sized-chunks-in-python/312644    
        if len(processors)%num_processes==0:    chunk_size = len(processors)/num_processes
        else:                                   chunk_size = len(processors)/num_processes+1
        
        processors = [ processors[i:i+chunk_size] for i in range(0, len(processors), chunk_size)]    



        processes = list()
        for p in range(num_processes):
            if DEBUG:   print "[ neuron writer ] : [ Process %d ] : chunk: %s" % (p, processors[p])
            if len(processors) > p:
                processes.append(Process(target=compute_neural_structures_parallel_callback, args=(db, processors[p])))
                processes[p].start()
                

        for p in range(num_processes):  
            processes[p].join()     # wait for all processes to finish
        

    # not parallel
    else:    
        for processor_id in processors:   # cycling all mapped processor
            compute_neural_structures(db, processor_id)
    

def compute_neural_structures(db, processor_id):
    """
    compute_neural_structures gets the db and the processor coordinate to calculate and write the translations of neural parameters in spinnaker neural_data files.
    
      -  writes the processor headers
      -  writes the part_population metadata for each part_population
      -  computes the translation of each parameter for each part_population
      -  writes the neural_data_x_y_p.dat file 
      
    """

    part_populations = db.get_part_populations_from_processor(processor_id)        
    core_address = db.get_part_population_loc(part_populations[0]['id'])    # get the core coordinates
    if DEBUG:   print "\n\n------------- Processing core %s ------------------" % core_address
    out_file_name = "%s/neural_data_%d_%d_%d.dat" % (pacman.BINARIES_DIRECTORY, core_address['x'], core_address['y'], core_address['p'])
    out_file_content = ""                               # start writing the neural file

    # start calculating headers
    runtime = eval(str(db.get_runtime()))                                   # get the total runtime
    synaptic_row_length = db.get_synaptic_row_length(processor_id)          # get the synaptic row length
    max_delay = db.get_max_delay()                                          # max_delay
    num_part_populations = len(part_populations)                            # num_populations    
    total_neurons = sum([i['size'] for i in part_populations])              # total neurons in the core
    # Retrieves the translation for that method, needed to calculate the size of the structure
    translations = db.get_parameters_translations(part_populations[0]['method_type'])   
    # calculates the size of the neural structure data by getting the size of every parameter and multiplies it by the number of neurons in the population
    size_neuron_data = struct.calcsize(str(''.join([i['type'] for i in translations]))) * total_neurons 

    # writing the header
    # TODO make mask a definition
    out_file_content += struct.pack("<IIIIIIII", runtime, synaptic_row_length, max_delay, num_part_populations, total_neurons, size_neuron_data, 0, 0) 
                
    # writing population metadata
    if DEBUG:   print "Getting part_populations from proc %d, core_address %d %d %d" % (processor_id, core_address['x'], core_address['y'], core_address['p'])
    for p in part_populations:
        out_file_content += struct.pack("<IIIIIII", p['start_id'], p['flags'], p['size'], struct.calcsize(str(''.join([i['type'] for i in translations]))), 0, 0, 0)
    
    # starts cycling part_populations and converts all parameters to lists of floats
    for p in part_populations:        
        params = eval(p['parameters'])  # gets parameters for the population                        

        # cycles all the parameters, and check if it's a list or float (int); converts them to numpy arrays param x neurons
        for param in params:
            if isinstance(params[param], int):      params[param] = [ float(params[param]) for i in range(p['size']) ]
            elif isinstance(params[param], float):    params[param] = [ params[param] for i in range(p['size']) ]                       
            # it's a list already, split
            else:   params[param] = params[param][p['offset']:p['offset']+p['size']]        
            
        
        #if DEBUG:   for t in translations: print t['param_name'], t['translation']

        # starts cycling neurons
        for i in range(p['size']):      # starts cycling neurons in the population
            for parameter_translation in translations:  # starts cycling parameters translations            
#               horrible hack for parameters with the same name # FIXME to be deprecated
                value = params[parameter_translation['param_name']][i]                  # value is the value of the correspondent param_name
#               end of horrible hack
                try:
                    translated_parameter = eval(parameter_translation['translation'])      # evaluate the expression            
                    if DEBUG:   print "n:", i, parameter_translation['param_name'], ", value ", params[parameter_translation['param_name']][i], "is translated with", parameter_translation['translation'], "as ", translated_parameter, "and will be packed as <%s" % parameter_translation['type']                    
                    out_file_content += struct.pack("<%s" % str(parameter_translation['type']), translated_parameter)
                except KeyError:
                        print "error while evaluating parameter", parameter_translation['param_name'], "for neuron", i, "in part population", p['id']
                        quit(1)
                    
                except struct.error:
                        print "\n\n !!! WARNING !!!  while evaluating parameter\n\n", parameter_translation['param_name'], "for neuron", i, "in part population", p['id'], 'value', value
                        if translated_parameter<0: translated_parameter = -(pow(2,32)-1)/2
                        else:                            translated_parameter = (pow(2,32)-1)/2
                        out_file_content += struct.pack("<%s" % str(parameter_translation['type']), translated_parameter)                            
                        continue
                        
                        
    if DEBUG:   print "writing file %s" % out_file_name
    out_file = open(out_file_name,'wb')
    out_file.write(out_file_content)
    out_file.close()
        

if __name__ == '__main__':
    print "\n----- creating neural data structures"
    from pacman import *
    db = load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)
    write_neural_data_structures(db)
    

