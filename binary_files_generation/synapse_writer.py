"""
Synapse writer. Computes the synaptic data structures to be loaded in SDRAM. 
Can be parallelized in the pacman.cfg file.

.. moduleauthor:: Francesco Galluppi, June 2012, francesco.galluppi@cs.man.ac.uk
"""


import pacman

import struct 
import sys
import numpy
import numpy.random as r
numpy.set_printoptions(precision=4)

from pyNN.utility import Timer

#import pickle
import cPickle as pickle
import sqlite3

timer = Timer()
timer.start()

DEBUG = pacman.pacman_configuration.getboolean('synapse_writer', 'debug')

INFO = True

PARALLEL = pacman.pacman_configuration.getboolean('synapse_writer', 'parallel')     # run the synapse_writer in parallel processes
num_processes = pacman.pacman_configuration.getint('synapse_writer', 'processes')      # number of processes

DELTA_T = pacman.pacman_configuration.getfloat('synapse_writer', 'parallel_delta_t')     # delay between 2 queries
N_QUERY=500

##### INSERTED FOR LOOKUP TABLE GENERATION
LOOKUP_WORDS_PER_ENTRY = 5  # each lookup entry has 5 words

OFFSET_CORE_ID = 11
MASK_CORE_ID = 0xF # (4 bits)

OFFSET_X_CHIP_ID = 24
MASK_X_CHIP_ID = 0xFF # 8bits

OFFSET_Y_CHIP_ID = 16
MASK_Y_CHIP_ID = 0xFF # 8bits



import threading
from multiprocessing import Queue
import time, random




def run_update(db, query):
    cur = db.get_cursor().execute(query)
    return(db.get_last_inserted_row_id())


class db_writer(threading.Thread):
    """
    Thread managing the writes to the DB
    """

    def __init__(self):
        threading.Thread.__init__(self)
        self.running = True
        self.counter = 0
        self.l = list()
        

    def run(self):
        global queue
        global db_global
                        
        while (True):
            item = queue.get()
            if item == None:    break
            db_global.get_cursor().execute(item)
            time.sleep(DELTA_T)   # DELTA_T
            self.counter += 1
            if(self.counter%N_QUERY)==0:                                   
                db_global.commit()                
                if (self.counter%(N_QUERY*10)==0): print "[ synapse_writer ] : [ DB writer ] : committing", self.counter
        self.running = False
        db_global.commit()

BASE = 0x70000000

# how to do enumerations in python (for matrix columns) # http://stackoverflow.com/questions/36932/whats-the-best-way-to-implement-an-enum-in-python
def enum(**enums):  return type('Enum', (), enums)

# The input list is composed by different entries organized as follows:
#INPUT = enum(PRE=0, POST=1, W=2, D=4, TYPE=4, TRANSLATION = 5)

PRE = 0
POST = 1
W = 2
D = 3
TYPE = 4
PLASTICITY_ON = 5
TRANSLATION = 6


#[[0, 0, 0, 0], [0, 0, 0, 0], [0, 65536, 0x7FFFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 31]]


# For the dynamic translation
# 5 components
##  order: delay, destination_neuron, weight, plasticity_on, synapse_type            


#TRANSLATION = enum(DELAY=0, DEST=1, W=2, D=4, TYPE=4, TRANSLATION = 5)
TRANS_DELAY = 0
TRANS_DEST = 1
TRANS_WEIGHT = 2
TRANS_PLAST = 3
TRANS_TYPE = 4

# for each value
#the operation done is: (((value + offset) * scale) & mask) << shift
# offset, scale, mask, shift

#OPERATION = enum(OFFSET=0, SCALE=1, MASK=2, SHIFT=4)
OFFSET=0
SCALE=1
MASK=2
SHIFT=3

# Output file
OUT_PRE = 0     # The first column stores the ID
OUT_HEADER = 3  # The header of each line in the output file is (pre_id+offset), 0, 0 (2 empty synaptic words)

# in part_out_matrix PRE and POST are the same (0 and 1) as the input list and the resulting synaptic word PART_OUT_WORD is 2
PART_OUT_WORD = 2


def compute_sdram_entries(db, c, memory_pointer_base, process_id=0):
    """
    Syntax: compute_sdram_entries(db, c, memory_pointer_base, process_id=0)
        
    Description: Computes the SDRAM entries for core c. 
       - c is the core from the image map (a dictionary that needs to contain x,y,p)
       - db is the pacman.dao instantiation of the db (model+library)
       - memory_pointer is the actual position in the memory
    
    returns out_string, a string representing the entry for the memory block
    
    """
    global n_synapses   # synaptic counter for profiing 
    r_key_map = db.get_routing_key_map()
    
    out_string = ""
    lookup_file_name = '%s/lktbl_%d_%d_%d.dat' % (pacman.BINARIES_DIRECTORY, c['x'], c['y'], c['p'])
    lookup_data = []
    
    memory_pointer= memory_pointer_base     #   the pointer for this file increments from this base and the length of out_string (cumulative)
    if DEBUG:
        if PARALLEL: print '[ synapse writer ] : [ Process %d ] : evaluating core' % process_id, c['x'], c['y'], c['p']
        else:    print '[ synapse writer ] : evaluating core', c['x'], c['y'], c['p']
    
    # extracting all projections that have this postsynaptic core
    projections = db.generic_select('pop.size as post_part_pop_size, proj.id as proj_id, proj.*, processors.*, pop.start_id, pop.end_id, pop.offset', 'part_projections proj, part_populations as pop, map, processors where postsynaptic_part_population_id = pop.id and map.processor_group_id = pop.processor_group_id and processors.id = map.processor_id and processors.x=%d and processors.y = %d and p = %d' % (c['x'], c['y'], c['p']))
    
    # Getting all the pre part_population that project to this core
    pre_part_populations = db.get_part_populations()
        
    if DEBUG:   
        for pre_part_population in pre_part_populations:    
            print 'pre_part_population: ', pre_part_population['id'], pre_part_population['size'], pre_part_population['population_core_offset']
            print "computing SDRAM entry for core %s at offset %x, length %d" % (c, memory_pointer, len(projections))
    
    # Extract a list with distinct pre part_population ids
    distinct_pre_pop = [ el['presynaptic_part_population_id'] for el in projections ]        
    distinct_pre_pop = list(set(distinct_pre_pop))  #   list(set(list)) eliminates dupicates in the list 
    
    # getting the dynamic translation from the DB
    synaptic_translations = db.get_synaptic_translation(c['x'], c['y'], c['p'])
#    if DEBUG:	print 'synaptic_translations', synaptic_translations

    #   cycling distinct pre part_populations
    #       cycling distinct projections from population (build cumulative synaptic matrix portion, not translated)
    #           cycling different synapse types and translating


    # Every distinct part_population will have a different entry, cumulative of all its contributed projections
    for p in distinct_pre_pop:  # for every source population we need to build a synaptic block. This is the main loop of the file
        # Retrie
        distinct_projections_from_pop = [ el for el in projections if el['presynaptic_part_population_id'] == p ]
        pre_proc_coord = db.generic_select('processors.x, processors.y, processors.p', 'part_populations as pop, map, processors where pop.id = %d and map.processor_group_id = pop.processor_group_id and processors.id = map.processor_id' % p)[0]
        
        # Every pre part_population as a distinct block, hence a different pointer, which is calculated based on the base pointer passed and the length of what has been done so far
        memory_pointer = memory_pointer_base + len(out_string)
                                           
        synaptic_matrix = list()        # synaptic matrix contains all the synaptic_matrix_portions for each different preojections
        target_vectors = list()         # verctor of synapses flags for type
        plasticity_vectors = list()     # vector of plasticity on or off

                
        # add every distinct projection to a cumulative list of lists synaptic_matrix made by synaptic_matrix_portions (as original lists)
        # we also compute the synapse type flag and add it to the target_vectors list (made of numpy arrays long as the number of synapses in the synaptic_matrix_portion)
        
        for d in distinct_projections_from_pop:     # d['proj_id'] is the part_projection_id
    
            # start building projections
            # calculate row length
            synaptic_flag = db.get_synaptic_flag(d['projection_id'])
            
            # evaluating if the projection is plastic
            if (d['plasticity_instantiation_id'] != 0):     plasticity_on = 1
            else:     plasticity_on = 0
            
            
            if DEBUG:   "[ synapse_writer ] : plasticity?",  plasticity_on
            
            #getting informations about the pre population in order to calculate the synaptic row length and update DB
            presynaptic_population = [ p for p in pre_part_populations if p['id']== d['presynaptic_part_population_id'] ][0]
#            print presynaptic_population, d
            if d['method'] == '1':    # AllToAll (will need pre and post size)
                # TODO random, allow_self_connections
#                print "Pre population size: %d, Post population size: %d" % (presynaptic_population['size'], d['post_part_pop_size'])
                projection_parameters = eval(d['parameters'])                
                synaptic_matrix_portion = [ (x, y, 0, 0) 
                    for x in range(presynaptic_population['size']) 
                    for y in range(d['post_part_pop_size'])] 
                    
                synaptic_matrix_portion = numpy.array(synaptic_matrix_portion, dtype='float32')
                                    
                # it's a string -> RNG! # FIXME hack, writing id to db in dictionary in __init__ in pyNN
                if isinstance(projection_parameters['weights'], dict):   
                    synaptic_matrix_portion[:,W] = return_random_weights(db, projection_parameters['weights']['r']-1, len(synaptic_matrix_portion))                                                                                
                else:   #is a scalar
                    synaptic_matrix_portion[:,W] = projection_parameters['weights']
                                
                
                if isinstance(projection_parameters['delays'], dict):   
                    synaptic_matrix_portion[:,D] = return_random_delays(db, projection_parameters['delays']['r']-1, len(synaptic_matrix_portion))                         
                    
                else:   #is a scalar
                    synaptic_matrix_portion[:,D] = projection_parameters['delays']

                    
                synaptic_matrix.append(numpy.asarray(synaptic_matrix_portion))
                target_vectors.append(numpy.ones(len(synaptic_matrix_portion)) * synaptic_flag)
                
                # evaluating plasticity
                plasticity_vectors.append(numpy.ones(len(synaptic_matrix_portion)) * plasticity_on)
                                

            if d['method'] == '3':    # FixedProbability (will need pre and post size)  # // TODO
#                print "Pre population size: %d, Post population size: %d" % (presynaptic_population['size'], d['post_part_pop_size'])
                projection_parameters = eval(d['parameters'])
                
                # synaptic matrix contains the probability of connection in the format (pre, post, weight, delay, probability)                
                synaptic_matrix_portion = numpy.zeros((presynaptic_population['size']*d['post_part_pop_size'], 5))
                PROB = 4
                
                # mesh explodes all the possible connections and puts them in the synaptic_matrix_portion
                mesh = numpy.meshgrid(range(0,presynaptic_population['size']), range(0,d['post_part_pop_size']))
                len_mesh = presynaptic_population['size']*d['post_part_pop_size']
                synaptic_matrix_portion[:,PRE] = numpy.reshape(mesh[0], len_mesh)
                synaptic_matrix_portion[:,POST] = numpy.reshape(mesh[1], len_mesh)                
                synaptic_matrix_portion[:,PROB] = numpy.random.rand(len_mesh)
                            
                # filter the connections based on probability
                idx = synaptic_matrix_portion[:,PROB] <= projection_parameters['p_connect']
                synaptic_matrix_portion = synaptic_matrix_portion[idx]
                
                if isinstance(projection_parameters['weights'], dict):   # it's a string -> RNG! # FIXME hack, writing id to db in dictionary in __init__ in pyNN
                    synaptic_matrix_portion[:,W] = return_random_weights(db, projection_parameters['weights']['r']-1, len(synaptic_matrix_portion))                                                            
                    
                else:   #is a scalar
                    synaptic_matrix_portion[:,W] = projection_parameters['weights']


                if isinstance(projection_parameters['delays'], dict):   # it's a string -> RNG! # FIXME hack, writing id to db in dictionary in __init__ in pyNN
                    synaptic_matrix_portion[:,D] = return_random_delays(db, projection_parameters['delays']['r']-1, len(synaptic_matrix_portion))                         
                    
                else:   #is a scalar
                    synaptic_matrix_portion[:,D] = projection_parameters['delays']
                                                                                
                
                if DEBUG:   print '\n synaptic_matrix_portion:', synaptic_matrix_portion[:,0:4], target_vectors
                synaptic_matrix.append(synaptic_matrix_portion[:,0:4])

                
                target_vectors.append(numpy.ones(len(synaptic_matrix_portion)) * synaptic_flag)
                plasticity_vectors.append(numpy.ones(len(synaptic_matrix_portion)) * plasticity_on)                

                                                
            if d['method'] == '4' or d['method'] == '2':    # FromListConnector/OneToOneConnector
                
                synaptic_matrix_portion = pickle.load( open( "/tmp/part_proj_%d.raw" % d['proj_id'], "rb" ))
                if DEBUG:   print "open /tmp/part_proj_%d.raw"  % d['proj_id'], synaptic_matrix_portion
                    
                    
                synaptic_matrix.append(synaptic_matrix_portion)
                target_vectors.append(numpy.ones(len(synaptic_matrix_portion)) * synaptic_flag)
                plasticity_vectors.append(numpy.ones(len(synaptic_matrix_portion)) * plasticity_on)
                
                                            
        # we are now computing the SDRAM entry for a single presynaptic population (all projections)
        # by converting the lists just extracted into numpy_synaptic_matrix    
        # numpy_synaptic_matrix is a matrix composed by list entries in the format (pre, post, weight, delay, type, translation)
#        numpy_synaptic_matrix = numpy.zeros(shape=(sum([ len(l) for l in synaptic_matrix ]), 6))


        # numpy_synaptic_matrix is a matrix composed by list entries in the format (pre, post, weight, delay, type, plasticity_on, translation)
        numpy_synaptic_matrix = numpy.zeros(shape=(sum([ len(l) for l in synaptic_matrix ]), 7))
                
        # we fill numpy_synaptic_matrix
        base_id = 0
        for s in range(len(synaptic_matrix)):   # by cycling every portion in synaptic_matrix            
            if DEBUG:   print "Synaptic matrix portion:", synaptic_matrix[s]
            if len(synaptic_matrix[s]) <= 0:    
                print '[ synapse writer ] : !! WARNING THERE IS NO SYNAPSE HERE'
                continue        # FIXME WHY DO I NEED THIS????
            
            numpy_synaptic_matrix[base_id:base_id+len(synaptic_matrix[s]), 0:4] = synaptic_matrix[s]    # adding pre, post, weight
            numpy_synaptic_matrix[base_id:base_id+len(synaptic_matrix[s]), TYPE] = target_vectors[s]       # adding synapse type
            numpy_synaptic_matrix[base_id:base_id+len(synaptic_matrix[s]), PLASTICITY_ON] = plasticity_vectors[s]       # adding plasticity_on
            
            base_id += len(synaptic_matrix[s])
            if DEBUG:   print "incrementing base_id: ", base_id, numpy_synaptic_matrix
            
            
        

        if DEBUG:   print "numpy_synaptic_matrix", numpy_synaptic_matrix                    
        
        # calculation of the synaptic_row_length
        max_out_vector = numpy.zeros(presynaptic_population['size'])
        
        for pre in range(presynaptic_population['size']):
            max_out_vector[pre] = numpy.sum((numpy_synaptic_matrix[:,PRE]==pre))
            
#        if DEBUG:	printmax_out_vector, numpy.max(max_out_vector)
        synaptic_row_length = numpy.max(max_out_vector)
        
        
        
#        we can update the database for all the projections with this relevant information: address, synaptic row_length
        if DEBUG:	print 'memory_pointer: 0x%x, row_length: %d, pre-pop %s' % (memory_pointer, synaptic_row_length, ', '.join([ str(ppp['proj_id'])  for ppp in distinct_projections_from_pop ]))


        # gets the starting r_key of the part_population
        r_key = (r for r in r_key_map if r['part_population_id'] == presynaptic_population['id']).next()            
        
        # calculates the routing key
        r_key = (r_key['x'] << OFFSET_X_CHIP_ID) | (r_key['y'] << OFFSET_Y_CHIP_ID)  | (r_key['p'] << OFFSET_CORE_ID)
        
        # appends the info needed to generate the lookup table entry
        lookup_data.append({'memory_pointer':memory_pointer, 
                            'synaptic_row_length':synaptic_row_length, 
                            'mask': eval(presynaptic_population['mask']),
                            'r_key': r_key})


        # if the synaptic row_length == 0 then there's no need to write the block (usually used for app_monitoring purpouses). go to the next iteration
        if synaptic_row_length <= 0:
            print 'no synapses for this connection'    
            continue
        
        
        # we have now the synaptic row length, the presynaptic_population size and the connectivity list bloack for this particular presynaptic core. we can start building the out_matrix
        # out_matrix is a presynaptic_population_size x (3+ row_length) matrix 
        # each row is (pre_id_offset + 0 + 0 + synapses (size: row_length))                
        # it's how the block will look like when is written (final version)
        output_matrix = numpy.zeros((presynaptic_population['size'], synaptic_row_length+OUT_HEADER), dtype='uint32')
                        
        # filling out the PRE column with consecutive ids, offsetting by chip/core id
        output_matrix[:, OUT_PRE] = numpy.arange(presynaptic_population['size']+presynaptic_population['population_core_offset'], dtype='uint32')
        
        # cycling synaptic translations and performing synaptic word translations
        for s in range(len(synaptic_translations)):
            # we create a copy of the original numpy_synaptic_matrix just for the synapses of the type interested
            idx = numpy_synaptic_matrix[:,TYPE]==s      # extracting boolean indexes                        
            # distinct_type_synapses is a n x 5 matrix of synapses with the corresponding type            
            distinct_type_synapses = numpy_synaptic_matrix[idx] 
                                    
            if DEBUG:	print "Core %s, Extracted %s distinct type %d synapses from %s" % (c, len(distinct_type_synapses), s, pre_proc_coord)
            if DEBUG:	print distinct_type_synapses
            n_synapses += len(distinct_type_synapses)
            

            # part_out_matrix is a temporary matrix which will hold the result of the or of all the components of the synaptic words once translated
            part_out_matrix = numpy.zeros(shape=(len(distinct_type_synapses), 3), dtype='uint32')
            part_out_matrix[:, PRE] = distinct_type_synapses[:, PRE]            
            part_out_matrix[:, POST] = distinct_type_synapses[:, POST]
            
            # this function is used to translate columns of synapses            
            def translate_synapses(distinct_type_synapses, value_index, translate_index):
                """
                translate_synapses translates a single component of the synaptic word into the distinct_type_synapses matrix
                it needs the index of the column of the values of translate and the value of the synaptic translation we are using (s)
                """
                # adding an offset, scaling and rounding to next int
                distinct_type_synapses[:,value_index] = numpy.round((distinct_type_synapses[:,value_index] + synaptic_translations[s][translate_index][OFFSET])*synaptic_translations[s][translate_index][SCALE])                
                # applying the mask
                distinct_type_synapses[:,value_index]= numpy.bitwise_and(distinct_type_synapses[:,value_index].astype('uint32'), synaptic_translations[s][translate_index][MASK])
                # shifting up
                distinct_type_synapses[:,value_index] = numpy.left_shift(distinct_type_synapses[:,value_index].astype('uint32'), synaptic_translations[s][translate_index][SHIFT])
            
#            # translating synapses by column
            for t in [(D, TRANS_DELAY), (W, TRANS_WEIGHT), (POST, TRANS_DEST), (TYPE, TRANS_TYPE), (PLASTICITY_ON, TRANS_PLAST)]:
                translate_synapses(distinct_type_synapses, t[0], t[1])            
            
            if DEBUG:   print "distinct_type_synapses", distinct_type_synapses.astype('uint32')
                        
            # or-ing all the components of the synaptic word into a single synaptic word
            for t in [POST, W, D, TYPE, PLASTICITY_ON]:                
                part_out_matrix[:, PART_OUT_WORD] = numpy.bitwise_or(part_out_matrix[:, PART_OUT_WORD].astype('uint32'), distinct_type_synapses[:,t].astype('uint32'))
                
            # we have now a matrix which lists pre, post, synaptic word for each connection
            if DEBUG:   print "part_out_matrix", part_out_matrix
            
                
            # part_numpy_synaptic_matrix is a copy of the portion that will get written in the numpy_synaptic_matrix
            # needed to do this because of the double index numpy_synaptic_matrix[idx][:, 0:5]
            part_numpy_synaptic_matrix = numpy.zeros((len(numpy_synaptic_matrix[idx]), 7))            
            part_numpy_synaptic_matrix[:,0:TRANSLATION] = numpy_synaptic_matrix[idx][:, 0:6]  # the first 6 columns (pre, post, w, d, type, plasticity_on) are the same
            part_numpy_synaptic_matrix[:,TRANSLATION] = part_out_matrix[:,2]                  # the 7th is the synaptic word column just calculated
            
#            if DEBUG:	print part_numpy_synaptic_matrix           
            # we can now write it in the corresponding part of the numpy_synaptic_matrix, translating all the words for a particular synapse type
            numpy_synaptic_matrix[idx] = part_numpy_synaptic_matrix
            
        # numpy_synaptic_matrix now has all the contributions for all the different synapse types translated. we can now write it out.
        if DEBUG:	print "numpy_synaptic_matrix completed for this portion:\n", numpy_synaptic_matrix.astype('uint32')

        # TODO implicit indexing for NEF -> to be deprecated
        #ind=numpy.lexsort((numpy_synaptic_matrix[:,0],numpy_synaptic_matrix[:,1]))    
        #numpy_synaptic_matrix = numpy_synaptic_matrix[ind]
        
#        for p in numpy_synaptic_matrix:
#            print p
        
        for pre in range(presynaptic_population['size']):
            # we have to extract all synapses from numpy_synaptic_matrix row 5 (translated), organize them into a vector and write them in each row
            idx_pre_numpy_matrix = numpy_synaptic_matrix[:,PRE]==pre    # all synapses which have pre as a pre
            idx_pre_output = output_matrix[:,PRE]==pre                  # the row in the block for pre
            
#            if DEBUG:	print output_matrix[idx_pre_output][0, :OUT_HEADER], len(numpy_synaptic_matrix[idx_pre_numpy_matrix][:, 5].astype('int32'))

            # idx_pre_output is the index of the row in the output_matrix for that presynaptic id. It's set to be the original output_matrix (id, 0, 0) concatenated with the 6th column of the numpy_synaptic_matrix (translated synaptic row) organized as a vector.
            # as the rows might not be homongeneous they need to be padded with 0s
                                
            padded_translation = numpy.concatenate([numpy_synaptic_matrix[idx_pre_numpy_matrix][:, TRANSLATION], numpy.zeros(synaptic_row_length-len(numpy_synaptic_matrix[idx_pre_numpy_matrix][:, TRANSLATION]))])
                
            output_matrix[idx_pre_output] = numpy.concatenate([output_matrix[idx_pre_output][0, :OUT_HEADER], padded_translation])

            if DEBUG:   print output_matrix[idx_pre_output]
#            output_matrix[idx_pre_output] = numpy.concatenate([output_matrix[idx_pre_output][0, :OUT_HEADER], numpy_synaptic_matrix[idx_pre_numpy_matrix][:, TRANSLATION]])
        
        # calculating the offset for chip/core # TODO insert grouping as well        
        chip_core_offset = ((pre_proc_coord['p'] & 0xF) << 11) | ((pre_proc_coord['y'] & 0xFF) << 16) | ((pre_proc_coord['x'] & 0xFF)<< 24)
        
        output_matrix[:, OUT_PRE] = numpy.bitwise_or(output_matrix[:, OUT_PRE].astype('uint32'), numpy.ones(len(output_matrix[:, OUT_PRE]), dtype='uint32')*chip_core_offset)
        # pre is now offset by core id

        # reshaping the block in a monodimensional vector for packing (size x row_length)
        output_vector_to_be_packed = numpy.reshape(output_matrix.astype('uint32'), presynaptic_population['size']*(synaptic_row_length+OUT_HEADER))
        
        # packing into unsigned ints
        for p in output_vector_to_be_packed:    out_string += struct.pack("I", p)

    # write lookup_data
    CONCLUSIVE_WORDS_SIZE = 5
    LOOKUP_HEADER_SIZE = 1
    lookup_file = open(lookup_file_name,'w+')
    # calculates the size in bytes of the LUT
    lookup_file.write(struct.pack("<I", len(lookup_data) * LOOKUP_WORDS_PER_ENTRY * 4 + (CONCLUSIVE_WORDS_SIZE * 4)))

    # sorts the lookup table # do I need to sort it???
    lookup_data_sorted = sorted(lookup_data, key=lambda k: k['r_key'])
    
    for l in lookup_data_sorted:
        lookup_file.write(struct.pack("<IIIII", 
                                        l['r_key'],         #   fasc addr
                                        l['mask'],          #   mask
                                        l['memory_pointer'], 
                                        0,                  #   linear search
                                        l['synaptic_row_length']
                                        ))

    # writes footer and closes the file
    lookup_file.write(struct.pack("<IIIII", 0xFFFFFFFF, 0, 0, 0, 0))
    lookup_file.close()
        
    return(out_string)
        


def synapse_writer(db):
    """
    calls the synapse generator for PACMAN
    """
    global n_synapses
    n_synapses = 0
    print("[ synapse writer ] : Loading DB: %g" %timer.elapsedTime())
    
    image_map = db.get_image_map()
    
    # TODO 48 only cycle the existing processors
    
    chip_map = [ (c['x'],c['y']) for c in image_map ]
    chip_map = list(set(chip_map))  # removing duplicates http://love-python.blogspot.co.uk/2008/09/remove-duplicate-items-from-list-using.html

    if PARALLEL:        
        num_processes = pacman.pacman_configuration.getint('synapse_writer', 'processes')      # number of processes - why do I need this here?
        print '[ synapse writer ] : Spawning %d parallel threads' % num_processes
        # parallelizing http://stackoverflow.com/questions/8521883/multiprocessing-pool-map-and-function-with-two-arguments
        # 16.6.1.5. Using a pool of workers: http://docs.python.org/library/multiprocessing.html                
        # not with SQLITE apparently
        from multiprocessing import Process, Queue

        chip_map = [ (c['x'],c['y']) for c in image_map ]
        chip_map = list(set(chip_map))  # removing duplicates http://love-python.blogspot.co.uk/2008/09/remove-duplicate-items-from-list-using.html

        #    http://stackoverflow.com/questions/312443/how-do-you-split-a-list-into-evenly-sized-chunks-in-python/312644    
    
        num_processes = min(num_processes, len(chip_map))
        if len(chip_map)%num_processes==0:
            chunk_size = len(chip_map)/num_processes
        else:
            chunk_size = len(chip_map)/num_processes+1
        chunks = [ chip_map[i:i+chunk_size] for i in range(0, len(chip_map), chunk_size)]    

        # Instantiates the db_writer    
        global queue
        global db_global
        db_global = db
        queue = Queue(0)    

        db_queue = db_writer()
        db_queue.start()
        db_queue.setDaemon = True
                        
        processes = list()
        for p in range(num_processes):
            print "[ synapse writer ] : [ Process %d ] : chunk: %s" % (p, chunks[p])
            if len(chunks) > p:
                processes.append(Process(target=callback, args=(db.db_abs_path, chunks[p], p)))
                processes[p].start()

        for p in range(num_processes):  
            processes[p].join()     # wait for all processes to finish
            
            
        db_queue.running = False

        queue.put(None)
        db_queue.join()

        
        
    else:            
        [ compute_sdram_file(chip_map[i][0], chip_map[i][1], db, image_map) for i in range(len(chip_map)) ]    # creates all the files for the system size # TODO reorganize to use only used chips

        
    print "[ synapse writer ] : Written %d synapses" % n_synapses

def callback(db_path, chunk, i):
    
    db = pacman.load_db(db_path)
    image_map = db.get_image_map()
        
    for c in chunk:
        if INFO:    print "[ synapse writer ] : [ Process %d ] : evaluating chip %d %d" % (i, c[0],c[1])
        compute_sdram_file(c[0],c[1],db,image_map,process_id=i)
    
    db.close_connection(commit=False)
    

def compute_sdram_file(x,y,db,image_map,process_id=0):
    filename = '%s/SDRAM_%d_%d.dat' % (pacman.BINARIES_DIRECTORY, x, y)
    if INFO:    print "[ synapse writer ] : evaluating chip %d %d" % (x,y)    
    used_cores = [ el for el in image_map if el['x']==x and el['y']==y ]
    out_file_string = ""
    memory_pointer = BASE
    
    for c in used_cores:
        out_file_string += compute_sdram_entries(db, c, memory_pointer, process_id=process_id)
        memory_pointer = BASE + len(out_file_string)
    
    # If there's something write the file
    if len(out_file_string) > 0:            
        f = open(filename,'w+')
        f.write(out_file_string)
        f.close()


if __name__ == '__main__':
    if DEBUG:	print "\n----- creating SDRAM files"
    db = pacman.load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)
    global n_synapses
    n_synapses = 0
    print("Loading DB: %g" %timer.elapsedTime())
    
    image_map = db.get_image_map()

    chip_map = [ (c['x'],c['y']) for c in image_map ]
    chip_map = list(set(chip_map))  # removing duplicates http://love-python.blogspot.co.uk/2008/09/remove-duplicate-items-from-list-using.html

    
    for c in chip_map:
        x = c[0]
        y = c[1]    
        filename = './binaries/SDRAM_%d_%d.dat' % (x, y)
#            filename = '/tmp/SDRAM_%d_%d.dat' % (x, y)
        used_cores = [ el for el in image_map if el['x']==x and el['y']==y ]
        out_file_string = ""
        memory_pointer = BASE
        
        f = open(filename,'w+')            
        
        for c in used_cores:
            out_file_string = compute_sdram_entries(db, c, memory_pointer)
            f.write(out_file_string)
            memory_pointer = BASE + len(out_file_string)
        
            
        f.close()

    print("Closing connection to db: %g" %timer.elapsedTime())    
    db.close_connection()
    print "List Writer Execution Time: %g - Written %d synapses" % (timer.elapsedTime(), n_synapses)
    
    
def return_random_weights(db, distribution_id, length):
    """
    used to return numpy random sequences for weights (floats) starting from a distribution
    """
    
    distribution = db.get_random_distributions()[distribution_id]
    rng_function = eval("numpy.random.%s" % distribution['distribution'])
    rng_parameters = eval(db.get_random_distributions()[distribution_id]['parameters'])
    
    out_weights = rng_function(rng_parameters[0], rng_parameters[1], length)
    if DEBUG:   print  "[ synapse_writer ] : random weights:", distribution['distribution'], rng_parameters, out_weights
        
    return(out_weights)
    
#    rng_parameters = db.get_random_distributions()[distribution_id]['parameters']
#    rng_parameters = eval(rng_parameters)                
#    return(rng_parameters[1]*numpy.random.random_sample(length)+rng_parameters[0])


def return_random_delays(db, distribution_id, length):
    """
    used to return numpy random sequences for delays (ints) starting from a distribution
    """

    distribution = db.get_random_distributions()[distribution_id]
    rng_function = eval("numpy.random.%s" % distribution['distribution'])
    rng_parameters = eval(db.get_random_distributions()[distribution_id]['parameters'])
    delay_list = rng_function(rng_parameters[0], rng_parameters[1], length).astype('int')
    return(delay_list)


#    rng_parameters = db.get_random_distributions()[distribution_id]['parameters']
#    rng_parameters = eval(rng_parameters)                
#    return(numpy.random.randint(rng_parameters[0], rng_parameters[1]+1, length))      # +1 because in randint high is exclusive

