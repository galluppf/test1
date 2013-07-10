#!/usr/bin/python
"""
Mapper: will map groups to cores in the system following constraints	

The process is set up by calling the following functions (see also __main__):
   - mapper:               maps the groups to processors available in the machine    
   - create_core_list:     gets the cores used in the simulation (needed by barrier synch)

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester, July 2011, francesco.galluppi@cs.man.ac.uk
"""

import struct
import sys
import ConfigParser     # will get pacman_configuration


import pacman

import binary_files_generation.stdp_table_generator as stdp_table_generator

DEBUG = pacman.pacman_configuration.getboolean('mapper', 'debug')


def mapper(db):
    """
    The Mapper gets a list of available groups and processors and combines them accordingly to the group constraints
    Writes the map table in the specs db
    """

    # get available processors (id and coordinates)
    processor_list = db.get_processors()        # gets all the available processors in the system
    group_list = db.get_groups()                # gets all the groups
    
    print "[ mapper ] : Available processors:", len(processor_list)
    print "[ mapper ] : Used processors:", len(group_list)

    if len(group_list) > len(processor_list):
        db.conn.commit()
        print '\n[ mapper ] : ERROR : more groups than processors - either you have a problem with your mapping or you need a bigger system!'
        sys.exit(1)

#    if DEBUG:   print "[ mapper ] : processor_list:", processor_list
    if DEBUG:   print "[ mapper ] : group_list:", group_list
        

    # get constrained groups and cycle them
    for g in group_list:
        if DEBUG:   print "[ mapper ] : evaluating group", g
        # start allocating constrained groups
        if g['constrain'] != None:
            constrain = eval(g['constrain'])
            if DEBUG:   print "[ mapper ] : group", g['group_id'], "has constraints:", constrain

            # checks constraints on x and y (mandatory) and on p (optional)                                
            if 'p' in constrain:
                available_processors = [ p['id'] for p in processor_list if (p['x'] == constrain['x'] and 
                            p['y'] == constrain['y'] 
                            and p['p'] == constrain['p'])]
            else:   # only x and y checked
                available_processors = [ p['id'] for p in processor_list if (p['x'] == constrain['x'] and 
                                            p['y'] == constrain['y'])]

            
            
            
            if DEBUG:   print "[ mapper ] : number of available processors that match the constraint:", available_processors
                
            ### if a subsequent constraint asks from the same resource it will raise an exception
            if len(available_processors)==0:
                print '\n[ mapper ] : ERROR : No processor available with your constrain: %s on group: %d check your mapping!\n' % (constrain, g['group_id'])
                db.commit()
                db.close_connection()
                sys.exit(1)
                
            db.insert_group_into_map(available_processors[0], g['group_id'])    # will pick the first available processor
            if DEBUG:   print "[ mapper ] : --- group number %d will be in processor %d" % (g['group_id'], available_processors[0])
            ### once the constrained group is assigned to a processor, that processor is removed from the processor_list
            processor_list[:] = [ p for p in processor_list if p['id'] != available_processors[0] ]
                
#    if DEBUG:   print "[ mapper ] : processor_list:", processor_list
    if DEBUG:   print "[ mapper ] : group_list:", group_list
    
    # Now assigning all the rest
    next_available_processor = 0     # will be used to cycle available_processors
    for i in group_list:
        if i['constrain'] == None:  # pick only unconstrained groups
            db.insert_group_into_map(processor_list[next_available_processor]['id'], i['group_id'])
            if DEBUG:   print "[ mapper ] : --- group %d will be in processor %d" % (i['group_id'], processor_list[next_available_processor]['id'])
            next_available_processor += 1
            

def create_core_list(db):
    """
    Creates the barrier.dat file in binaries. The file is used for the barrier synchronization at the beginning of the simulation, and is an array of integers of dimension X*Y, where each int is a bitmap of the cores running in that chip.
    """
    
    barrier_file = open('%s/barrier.dat' % pacman.BINARIES_DIRECTORY,'wb')
    barrier_file_content = ""
    
    core_map = db.get_image_map()

#    # UNCOMMENT for 48 node board        
#    x_size = max([ c['x'] for c in core_map]) + 1 # + 1 because chip numbering starts from (0,0)
#    y_size = max([ c['y'] for c in core_map]) + 1
    
    square_size = 8       #   FIXME for now the barrier works with a square
        
    # dynamical board size won't work with the retina # FIXME
    for x in range(square_size):
        for y in range(square_size):
            flag = 0
            if DEBUG:   print "[ mapper ] : inserting cores for the barrier sync", db.get_barrier_processors(x, y)
            for processor in db.get_barrier_processors(x, y):
                if processor['is_io'] == 0:     flag |= (1 << processor['p'])   # will exclude the processors marked as is_io (virtual chips for AER sensors)
            if flag > 0:    flag |= 1   # monitor processor used FIXME remove
            barrier_file_content += struct.pack("<I", flag)

    barrier_file.write(barrier_file_content)
    barrier_file.close()




def allocate_monitoring_cores(db):
    """
    Little PACMAN for monitors. Monitoring core will always be allocated even if it's not used so monitoring can be switched on on run time. 
    Monitoring core is mapped in the processor table with STATUS='MONITORING'. only works with 1 core now
    """
    
    import pyNN.spiNNaker as p      # needed for populations
    p.simulator.set_db(db)          # using db as simulator.db_run

    probes = db.get_probes()
    probes = [ i for i in probes if i['save_to'] == 'eth' ]     # will only get the ethernet probes
    
    
    # creating population
    monitoring_pop = p.Population(1, p.Recorder, {}, label='app_monitoring')    
    monitoring_pop_id = monitoring_pop.id
    
    # creating projections
    for probe in probes:
        db.insert_monitoring_projection(probe['population_id'], monitoring_pop_id)

    # creating part_population
    monitoring_part_pop_id = db.insert_part_population(monitoring_pop_id, 1, 0)   # insert the population in the part_population table
    monitoring_pop.set_mapping_constraint({'x':0, 'y':0})        
    
    # update core_group_id and map
    group_id = db.generic_select('max(processor_group_id) as g','part_populations')[0]['g'] + 1    
    db.update_part_popoulation_group(monitoring_part_pop_id, group_id, position_in_group=0)
    db.set_part_population_core_offset(group_id)    
    monitoring_processor = db.generic_select('id','processors WHERE status = \'MONITORING\'')[0]['id']   # will get only the first one
    db.insert_group_into_map(monitoring_processor, group_id)    # will pick the first available processor
    
    # part_projections
    for projection in db.get_presynaptic_populations(monitoring_pop_id):        
        for pre_part_population in db.get_part_populations(population_id=projection['presynaptic_population_id'] ):   # gets every child part_population from the presynaptic_population        
            db.insert_monitoring_part_projection(projection['id'], pre_part_population['id'], monitoring_part_pop_id)
        

if __name__ == '__main__':
    """
    if called from the shell will run the mapper on the db passed as the first arguments    
    """
    db = load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)
    print "[ mapper ] : \n----- Mapping....."
    mapper(db)
    
	# check for post-mapper triggers
    if pacman_configuration.getboolean('post-mapper', 'run'):
        print "[ mapper ] : \n----- Running post-mapper plugins"

		# stdp table generation
        if pacman_configuration.getboolean('post-mapper', 'generate_stdp_tables'):
            print "[ mapper ] : running compile_stdp_table_from_db"
            stdp_table_generator.compile_stdp_table_from_db(db)

		# allocating app monitoring
        if pacman_configuration.getboolean('post-mapper', 'allocate_monitoring_cores'):
            print "[ mapper ] : running allocate_monitoring_cores"
            allocate_monitoring_cores(db)

        

    
    create_core_list(db)
    
    db.close_connection()   # will close the connection to the db and commit the transaction
        
     
