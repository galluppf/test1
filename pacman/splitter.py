#!/usr/bin/python
"""
Splitter:   Splits Populations that can't fit into one core into more proto_populations. 
            Rearranges Projections and Probes accordingly.

The process is set up by calling the following functions (see also __main__):
            
    - split_populations:    splits Populations accordingly to the maximum number of neurons for that model

    - split_projections:    splits Projections accordingly to proto_populations. recalculates offsets for ids (FromListConnector)

    - split_probes:         splits Probes accordingly to proto_populations

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester, francesco.galluppi@cs.man.ac.uk
"""
    
import numpy
import pacman
#import pickle
import cPickle as pickle



DEBUG = pacman.return_configuration().getboolean('splitter', 'debug')
INFO = True


#### population_splitter    
def split_populations(db):
    if INFO:    print "[ splitter ] : Splitting Populations"    
    populations = db.get_populations_size_type_method()     # gets info from the populations
    if DEBUG:   print "[ splitter ] : populations:", populations
    for p in populations:                                   # cycle all populations
        if DEBUG:  print p['label'],p['name'],p['size'], p['max_nuro_per_fasc'], p['splitter_constraint']
        
        if p['splitter_constraint'] > 0:    max_nuro_per_fasc = min(p['max_nuro_per_fasc'], p['splitter_constraint'])
        else:                               max_nuro_per_fasc =p['max_nuro_per_fasc']
        
        offset = 0                                          # initializes offset
        remaining_neurons = p['size']                       # initializes remaining neurons
        
        if p['size'] <= max_nuro_per_fasc:              # if population can fit in a core
            if DEBUG:   print "[ splitter ] : Population %s doesn't need any splitting"    % p['label']
            db.insert_part_population(p['id'], remaining_neurons, offset)   # insert the population in the part_population table
        else:                                               # population needs splitting
            while remaining_neurons > max_nuro_per_fasc:
                if DEBUG:   print "[ splitter ] : Population %s will be splitted in one subpopulation of %d neurons starting from %d"    % (p['label'], max_nuro_per_fasc, offset)
                db.insert_part_population(p['id'], max_nuro_per_fasc, offset)
                offset += max_nuro_per_fasc                # increments the offset with the number of neurons fit in
                remaining_neurons -= max_nuro_per_fasc     #             
            if DEBUG:   print "[ splitter ] : Population %s will be splitted in one subpopulation of %d neurons starting from %d"    % (p['label'], remaining_neurons, offset)
            db.insert_part_population(p['id'], remaining_neurons, offset)

#### projection_splitter    
def split_projections(db):
    if INFO:    print "[ splitter ] : Splitting Projections"    
    # Cycle every presynaptic part_population
    # Cycle every postsynaptic_part_population connected to the former
    # Copy projection object        
    part_populations = db.get_part_populations()                # getting all part_populations
    for pre_part_population in part_populations:                # cycle every part_population as a presynaptic population
        if DEBUG:   print "\n[ splitter ] : pre_part_population %d, parent population id %d, extracting projection from the source population" % (pre_part_population['id'], pre_part_population['population_id'])
        post_projections = db.get_postsynaptic_populations(pre_part_population['population_id'])    # gets all the projections where the parent population id is a presynaptic population
        for post_projection in post_projections:    # cycles every projection where the presynaptic population is the parent population id
            connector_type = db.get_connector_type (post_projection['id'], part_projection=False)
            is_list_connector = (connector_type=='FromListConnector' or connector_type=='OneToOneConnector')
            if is_list_connector:
                if DEBUG:   print "Loading /tmp/proj_%d.raw" % post_projection['id']
                original_connection_list = pickle.load( open( "/tmp/proj_%d.raw" % post_projection['id'], "rb" ))
                
            
            for post_part_population in db.get_part_populations(population_id=post_projection['postsynaptic_population_id'] ):   # gets every child part_population from the postsynaptic_population
                if DEBUG:   print "[ splitter ] : connects to part_population", post_part_population['id']     # connects the 2 part_populations
#                print post_projection
                                    
                # splitting lists
                if is_list_connector:
                    connection_list = original_connection_list
                    if DEBUG:   print "[ splitter ] : pre between", pre_part_population['offset'], pre_part_population['offset']+pre_part_population['size']
                    if DEBUG:   print "[ splitter ] : post between", post_part_population['offset'], post_part_population['offset']+post_part_population['size']
                    
                    if DEBUG:   print connection_list

                    idx = numpy.logical_and((connection_list[:,0] >= pre_part_population['offset']), 
                                            (connection_list[:,0] <  pre_part_population['offset']+pre_part_population['size']))
                                            
                    idx = numpy.logical_and(idx, (connection_list[:,1] >= post_part_population['offset']))
                    idx = numpy.logical_and(idx, (connection_list[:,1] <  (post_part_population['offset']+post_part_population['size'])))                    
                    
                    connection_list = connection_list[idx]
                                            
                    
                                                                                                    
                    if DEBUG:   print "[ splitter ] : original", connection_list
                    #recalculate offsets to make sublist relative to part_populations
                    connection_list[:,0] -= pre_part_population['offset']
                    connection_list[:,1] -= post_part_population['offset']
                    
                    if DEBUG:   print "[ splitter ] : recalculated", connection_list
                    
                    
                    parameters = {}     # Needed for the insert
                    
                    
                    if len(connection_list) > 0:    # avoids writing empty connector lists (eg. one to one)                            
                        part_projection_id = db.insert_part_projection(pre_part_population['id'], post_part_population['id'], post_projection)   # Do the actual insert
                        pickle.dump(connection_list, open( "/tmp/part_proj_%d.raw" % part_projection_id, "wb" ) )
                        if DEBUG:    print 'computed part_projection:', part_projection_id
                        if INFO and part_projection_id % 1024==0 :    print '[ splitter ] : computing part_projection:', part_projection_id
                    else:
                        print '[ splitter ] : No synapses in this part_projection - skipping part_projection from pre', pre_part_population['id'], post_part_population['id'], "projection_id", post_projection['id']

                        
                else:   # is not a list connector_type so we can insert it directly without examination
                    part_projection_id = db.insert_part_projection(pre_part_population['id'], post_part_population['id'], post_projection)   # Do the actual insert
                                                                                    
                    
        
                        
                        


def split_probes(db):
    probes = db.get_probes()    # get all the probes in the system
    for probe in probes:        # cycle them
        part_populations = db.get_part_populations(population_id=probe['population_id'])    # get all the part population id associated with that probe
        for part_population in part_populations:    # cycle all the part populations
            db.insert_part_probe(part_population['id'], probe)  # insert a part_probe linked to the part population
            flag_position = db.get_flag_position('%s_%s' % (probe['variable'], probe['save_to']))   # retrieve the flag position
            current_flag = db.get_part_populations(part_population_id=part_population['id'])[0]['flags']        # retrieve the flag value
            flag = current_flag | (1 << flag_position)  # calculate the new flag by setting the corresponding bit to 1        
            db.set_flag(part_population['id'], flag)    # write back the flag to the db


#if __name__ == '__main__':
#    """
#    Calling the splitter initiates PACMAN
#    
#    pre-pacman options are run before the splitter
#    post-pacman options are run after the splitter
#    
#    - split_populations:    splits Populations accordingly to the maximum number of neurons for that model
#    - split_projections:    splits Projections accordingly to proto_populations. recalculates offsets for ids (FromListConnector)
#    - split_probes:         splits Probes accordingly to proto_populations
#    """
#        
#    print "[ splitter ] : Loading DB: %s\n" % sys.argv[1]                
#    db = load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)    
#    db.clean_part_db()              # cleans the part_* tables
#        
#    
#    if pacman_configuration.getboolean('pre-pacman', 'run') == True:
#        print "[ splitter ] : \n----- Running pre-pacman plugins"
#        
#    print "[ splitter ] : \n----- Splitting....."
#    split_populations(db)
#    split_projections(db)
#    split_probes(db)
#    db.close_connection()       # will close the connection to the db and commit the transaction 
#    
#    if pacman_configuration.getboolean('post-splitter', 'run') == True:
#        print "[ splitter ] : \n----- Running post-splitter plugins"

    
