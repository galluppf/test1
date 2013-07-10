#!/usr/bin/python
"""
Grouper:

The Grouper will join homogeneous proto_populations together up to the maximum number of neurons for that model

The process is set up by calling the following functions (see also __main__):

    - get_groups:           Retrieves all the Populations that can be grouped together. 
                            Such Populations are homogeneous for neural model and plasticity instantiations.
                            outputs a list of lists where each element is a list of groupable Populations
                            
    - grouper:              groups populations accordingly to the maximum number of neurons
                            for that neural model

    - update_core_offsets:  sets the core offset for that Population (position of the Population in the group)
    
.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester, July 2011. francesco.galluppi@cs.man.ac.uk
"""

import pacman
import struct

DEBUG = pacman.pacman_configuration.getboolean('grouper', 'debug')

# Size of the available system  # FIXME query db
SYSTEM_SIZE_X = 2
SYSTEM_SIZE_Y = 2



def get_groups(db):
    """
    get_groups is the function that implements the constraints needed to group part_populations together. It returns lists of groupable part_populations.
    This version for the SNN framework implements the following constraints (see queries - sectop grouper for details):
        - part_populations with the same cell type can be grouped together
        - part_populations with different plasticity_instantiation_id cannot be grouped together
        - part_populations with different constraints cannot be grouped together
    """
    groups = []
    
    # The first group query gets the combination of constraints in the network
    grouping_rules = db.get_grouping_rules()
    if DEBUG:   print "grouping_rules:", grouping_rules

    # The second query gets all the part populations in that group of constraints (groupable part_populations)    
    for r in grouping_rules:
        extracted_populations = db.get_part_populations_by_rules(r)
        if len(extracted_populations) > 0:      # why should it be less than 1??? sometimes the query does not answer correctly // FIXME
            groups.append([ part_population['pop_id'] for part_population in extracted_populations ] )
    
    
    
    print "GROUPING RESULT:", groups
    return groups                            

def grouper(db, groups):
    group = 0
    for groupable_part_population_ids in groups:
        groupable_part_populations = db.get_part_populations_size_type_method(','.join(map(str, groupable_part_population_ids)))
        neurons_left_in_group = groupable_part_populations[0]['max_nuro_per_fasc']       # gets the first population max_nuro_per_fasc (is the same for all the groupable populations)
        max_nuro_per_fasc = groupable_part_populations[0]['max_nuro_per_fasc']
        position_in_group = 0
        for p in groupable_part_populations:        
#            print group, p['id'], p['size'], p['max_nuro_per_fasc']
            if p['size'] <= neurons_left_in_group:  # is there place left in the group?
                # add the population to the group
                if DEBUG:   print 'p_id', p['id'], p['size'], 'neurons, will be assigned to group', group, 'position', position_in_group
                db.update_part_popoulation_group(p['id'], group, position_in_group)
            else:   # create a new group
                group += 1
                position_in_group = 0                 
                neurons_left_in_group = max_nuro_per_fasc
                if DEBUG:   print 'p_id', p['id'], p['size'], 'neurons, will be assigned to group', group, 'position', position_in_group
                db.update_part_popoulation_group(p['id'], group, position_in_group)
            # decrements neuron left in the group and increase position
            neurons_left_in_group -= p['size']
            position_in_group += 1                 
        if DEBUG:   print "---- incrementing group due to groupable_population change"
        group += 1


def update_core_offsets(db):
    """
    calculating core offsets for groups (more than one part-population in a group)
    """
    
    group_list = db.get_groups()
    for group in group_list:
        if DEBUG:   print "calculating core offsets for group #", group['group_id']
        db.set_part_population_core_offset(group['group_id'])

def bypass_grouping(db):
    groups = [ [ p['id'] ] for p in db.get_part_populations() ]
    grouper(db, groups)
    update_core_offsets(db)
    return 0
    
if __name__ == '__main__':
    """
    if called from the shell will run the partitioner on the db passed as the first arguments    

        # cycle cell types
        # divide them accordingly to plasticity instantiations
        # group them until max_nuro_per_fasc

    """
    db = pacman.load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)

    if pacman_configuration.getboolean('grouper', 'run'):
        groups = get_groups(db)
        print "\n----- Grouping....."
        grouper(db, groups)
        print "grouping terminated, going with mapping"
        update_core_offsets(db)

    else:
        # TODO insert bypass grouping (1 population per core)    
        bypass_grouping(db)

    db.close_connection()   # will close the connection to the db and commit the transaction     

    



