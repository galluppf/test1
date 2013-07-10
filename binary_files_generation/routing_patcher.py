#!/usr/bin/python
"""
This file is used to patch routing tables so to deal with unasssigned chips in the way.
   - get unused chips on the same row as the last used one
   - open the routing table file
   - reorganize it writing filename.patched which contains routing tables as they need to be loaded with ybug
   - write patch_routing.ybug which loads the patched routing tables in ybug

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester. francesco.galluppi@cs.man.ac.uk
"""

import struct
import pacman
import os
import sys

#from spin1_api
#define RTR_BASE 		0xf1000000
#define SYS_MC_ENTRIES 24
#define RTR_MCRAM_TOP 		(RTR_MCRAM_BASE + MC_TABLE_SIZE * 4)
#define RTR_MCRAM_BASE 		(RTR_BASE + 0x00004000)
#define RTR_MCKEY_BASE 		(RTR_BASE + 0x00008000)
#define RTR_MCMASK_BASE		(RTR_BASE + 0x0000c000)

#/* router MC tables */
#static volatile uint * const rtr_mcrte = (uint *) RTR_MCRAM_BASE;
#static volatile uint * const rtr_mckey = (uint *) RTR_MCKEY_BASE;
#static volatile uint * const rtr_mcmsk = (uint *) RTR_MCMASK_BASE;


############## routing key
OFFSET_CORE_ID = 11
MASK_CORE_ID = 0xF # (4 bits)

OFFSET_X_CHIP_ID = 24
MASK_X_CHIP_ID = 0xFF # 8bits

OFFSET_Y_CHIP_ID = 16
MASK_Y_CHIP_ID = 0xFF # 8bits

DEBUG = False
RTR_BASE_YBUG   = hex(int(pacman.pacman_configuration.get('routing', 'ROUTER_BASE_MEMORY_ADDRESS'), 16))[2:]
RTR_BASE        = eval(pacman.pacman_configuration.get('routing', 'ROUTER_BASE_MEMORY_ADDRESS'))
SYS_MC_ENTRIES  = 24
RTR_MCRAM_BASE  = 		hex(RTR_BASE + 0x00004000 + SYS_MC_ENTRIES*4)
RTR_MCKEY_BASE  =		hex(RTR_BASE + 0x00008000 + SYS_MC_ENTRIES*4)
RTR_MCMASK_BASE =		hex(RTR_BASE + 0x0000c000 + SYS_MC_ENTRIES*4)

X = 0
Y = 1

R_KEY_MGMT = [250, 250]    # used to send mgmt packets from proxys to robot

GET_COORDINATES_OUTPUT_CORES_ROBOT = """SELECT part_populations.id, populations.label, x, y, p FROM part_populations 
    JOIN populations on part_populations.population_id = populations.id 
    JOIN map ON map.processor_group_id = part_populations.processor_group_id 
    JOIN processors on map.processor_id = processors.id 
    JOIN probes on probes.population_id = populations.id 
    WHERE probes.save_to = 'ROBOT_OUTPUT';"""

GET_PROXY_POPULATIONS = """SELECT populations.label, populations.id as population_id, populations.size, part_populations.id as part_population_id, x, y, p
    FROM part_populations 
    JOIN populations on part_populations.population_id = populations.id
    JOIN map ON map.processor_group_id = part_populations.processor_group_id 
    JOIN processors on map.processor_id = processors.id 
    JOIN cell_instantiation ON populations.cell_instantiation_id = cell_instantiation.id
    JOIN cell_methods ON cell_methods.cell_instantiation_id = cell_instantiation.id
    JOIN cell_types ON cell_methods.method_type = cell_types.id
    WHERE cell_types.name = 'ProxyNeuron';"""


file_to_patch = '%s/routingtbl_0_0.dat' % pacman.BINARIES_DIRECTORY

def patch_routing_entries_missing_chips(db):    
    ybug_filename = '%s/patched_routing_tables.ybug.dat' % pacman.BINARIES_DIRECTORY
    
    ybug_file = open(ybug_filename,'w')
    ybug_file.write("\n\n### automatically generated ybug script used to patch unused routers\n")

    chips_to_patch = get_chips_to_patch(db)
    
    for c in chips_to_patch:
        filename = '%s/routingtbl_%d_%d.dat' % (pacman.BINARIES_DIRECTORY, c[X], c[Y])                
        routes, masks, keys = get_routing_file(filename)
                
        if len(routes) > 0:     
            ybug_file.write("sp %d %d\n" % (c[X], c[Y]))        
            for i in range(len(routes)):
#                print hex(routes[i])
                ybug_file.write( "sw %s %s\n" % (hex(eval(RTR_MCRAM_BASE)+i*4)[2:].replace("L",""), hex(routes[i])[2:].replace("L","")) )
                ybug_file.write( "sw %s %s\n" % (hex(eval(RTR_MCKEY_BASE)+i*4)[2:].replace("L",""), hex(keys[i])[2:].replace("L","")) )
                ybug_file.write( "sw %s %s\n" % (hex(eval(RTR_MCMASK_BASE)+i*4)[2:].replace("L",""), hex(masks[i])[2:].replace("L","")) )                
            

    ybug_file.write("\n")                                    
        
def get_routing_file(filename):
    """
    patch_routing_file takes a file and patches it so that it can be loaded directly with ybug
    """        
    size = 0
    routing_file_content = read_routing_file(filename)
#    print routing_file_content
    
    if routing_file_content == None or len(routing_file_content) == 0:        return [],[],[] # 0
    
    
    # TODO patching
    routes  = [ r['destination'] for r in routing_file_content]
    masks   = [ r['mask'] for r in routing_file_content]
    keys    = [ r['key'] for r in routing_file_content]        
    
#    print routes, masks, keys
    
    return routes, masks, keys
    
    

def read_routing_file(filename):
    """
    reads the routing file
    """

    if os.path.exists(filename) == False:
        if DEBUG:   
            print "[ routing_patcher ] : ignoring file: ", filename
        return None
            
    f = open(filename,'r')
            
    num_entries = struct.unpack('<I', f.read(4))[0]
    size_in_bytes = num_entries*3*4

    if DEBUG:   print "[ routing_patcher ] : %s - size routing tables: %d entries, 0x%08X bytes" % (filename, num_entries, size_in_bytes)

    out_dict = list()
    for i in range(num_entries):    
        out_dict_element = dict()   # temporary dictionary for the current entry
        out_dict_element['key'], out_dict_element['mask'], out_dict_element['destination'] = struct.unpack('<III', f.read(4*3))    
        out_dict.append(out_dict_element)

    if DEBUG:   print "[ routing_patcher ] : Routes: %s" % out_dict
    f.close()
    return out_dict
    
    
def get_chips_to_patch(db):
    """
    The functionr returns all the unassigned chip on the same row of the top one selected
    """
    # get used chips
    used_image_map = db.get_image_map()
    used_chip_map = [ (c['x'],c['y']) for c in used_image_map ]
    used_chip_map = list(set(used_chip_map))  # removing duplicates http://love-python.blogspot.co.uk/2008/09/remove-duplicate-items-from-list-using.html
    
    # get available chips
    image_map = db.get_processors()
    chip_map = [ (c['x'],c['y']) for c in image_map ]
    chip_map = list(set(chip_map))  # removing duplicates
        
    # calculating the top used chip
    max_used_y_chip = max(c[Y] for c in used_chip_map)
    max_used_x_chip = max(c[X] for c in used_chip_map if c[Y] == max_used_y_chip)    
    print "[ routing_patcher ] : max used chip: ", max_used_x_chip, max_used_y_chip
    
    # calculating the chips to patch
    chips_to_patch = [ c for c in chip_map if c[Y] == max_used_y_chip and c[X] > max_used_x_chip ]    
    print "[ routing_patcher ] : chips to patch: ", chips_to_patch
    
    return chips_to_patch
    
    
    
def patch_router_for_robot(db):
    """
    This function will search all the populations that have a ROBOT_OUTPUT flag set (xo,yo,po) and add a route from each of this cores to go through link 4 in chip 0,0
    
    the table probes will contain a tuple with the population_id, "VALUE", "ROBOT"
    
    """

    # get all ROBOT_OUTPUT populations    
    procs = db.execute_and_parse_select(GET_COORDINATES_OUTPUT_CORES_ROBOT)
#    print procs

    # open the routing table file and get routes
    routes = read_routing_file(file_to_patch)
    
    new_routes = []
    
    for p in procs:
        r_key = 0
        r_key = r_key | ((p['x'] &MASK_X_CHIP_ID) << OFFSET_X_CHIP_ID) | ((p['y'] & MASK_Y_CHIP_ID) << OFFSET_Y_CHIP_ID) | ((p['p'] & MASK_CORE_ID) << OFFSET_CORE_ID)
            
        new_routes.append({'destination' : 1 << 4, 'mask' : 0xFFFFF800, 'key' : r_key})
        
    for r in routes:
        new_routes.append(r)
        
#    print new_routes
    
    write_routing_file(file_to_patch, new_routes)
    

# moved below
#    num_entries = len(new_routes)
#    f = open(file_to_patch, 'w')
#    f.write(struct.pack("<I", num_entries))
#    
#    for r in new_routes:
#        f.write(struct.pack('<III', r['key'], r['mask'], r['destination']))
#    
#    
#    f.close()

def write_routing_file(filename, routes):
    """
    Takes a set of routes in a form of a list of dictionaries [r['key'], r['mask'], r['destination']] and writes them to filename
    """
    f = open(filename, 'w')
    num_entries = len(routes)
    f.write(struct.pack("<I", num_entries))
    
    for r in routes:
        f.write(struct.pack('<III', r['key'], r['mask'], r['destination']))
    
    f.close()


def patch_router_for_sensors(db):
    # get all the proxy populations
    proxy_populations = db.execute_and_parse_select(GET_PROXY_POPULATIONS)
#    print proxy_populations
    populations = list(set([ p['population_id'] for p in proxy_populations ]))
    
    
#    # get the first proxy part_population for each proxy population
    earliest_proxys = []
    for p in populations:        
        min_part_population = min([ part_pop['part_population_id'] for part_pop in proxy_populations if part_pop['population_id'] == p ])
        earliest_proxys.append([ part_pop for part_pop in proxy_populations if part_pop['part_population_id'] == min_part_population][0])    
        # get the x_source and y_source for the proxy
        params = eval(db.get_population_parameters(p))
        earliest_proxys[-1]['x_source'] = params['x_source']
        earliest_proxys[-1]['y_source'] = params['y_source']

#    for p in proxy_populations:
#        assert (p['y'] == 0),  "WRONG! only the first row y=0 can be used"
#        params = eval(db.get_population_parameters(p['population_id']))
#        p['x_source'] = params['x_source']
#        p['y_source'] = params['y_source']
        
    if len(earliest_proxys) == 0:     
        print '[ routing patcher ] : patching sensors, nothing to do...'
        return

    max_router = max([ p['x'] for p in earliest_proxys])

        
    if DEBUG:   print '[ routing patcher ] : patching routers up to', max_router

    routers = []    
    # get all the routers <= rightmost proxy and save them to a list
    for i in range(max_router+1):
        filename = '%s/routingtbl_%d_0.dat' % (pacman.BINARIES_DIRECTORY, i)
        routers.append(read_routing_file(filename))

#    print 'unpatched routes:', routers
    
    for p in earliest_proxys:
        if DEBUG:   print p
#        key =  ((p['x_source'] &MASK_X_CHIP_ID) << OFFSET_X_CHIP_ID) | ((p['y_source'] & MASK_Y_CHIP_ID) << OFFSET_Y_CHIP_ID) | ((p['p'] & MASK_CORE_ID) << OFFSET_CORE_ID)
        key =  ((p['x_source'] &MASK_X_CHIP_ID) << OFFSET_X_CHIP_ID) | ((p['y_source'] & MASK_Y_CHIP_ID) << OFFSET_Y_CHIP_ID)
        mask = 0xFFFF0000       # will get all routing keys mapped to the core
        for i in range(max_router+1):
            if i < p['x']:
                destination = 1 # EAST
                routers[i].append(({'destination' : 1, 'mask' : mask, 'key' : key}))
                if DEBUG:   print "[ routing patcher ] : patching router", i, "with route entry", {'destination' : 1, 'mask' : mask, 'key' : key}, "(route east)"
            elif p['x'] == i:
                destination = 1 << (p['p']+6) # consume local
                if DEBUG:   print "[ routing patcher ] : patching router", i, "with route entry", {'destination' : destination, 'mask' : mask, 'key' : key}, "(consume)"
                routers[i].append(({'destination' : destination, 'mask' : mask, 'key' : key}))
#                print "patching local", i
            else:
#                print "do nothing", i
                pass
                
            if DEBUG:   print routers[i]
    
    #   patch all the previous routers to route the packet EAST
    #   patch the local router to consume the packet with the first proxy part_population
    #
    # cycle all the routers and write back files
    
    # consuming MGMT packets with key (250, 250)        # FIXME only works in 0,0 - if Proxys are in x > 0 they need to be route west
    r_key_mgmt = 0
    r_key_mgmt = r_key_mgmt | ((R_KEY_MGMT[X] & MASK_X_CHIP_ID) << OFFSET_X_CHIP_ID) | ((R_KEY_MGMT[Y] & MASK_Y_CHIP_ID) << OFFSET_Y_CHIP_ID)
    for i in range(max_router+1):
        if i ==0:
            # consume in 0,0
            routers[0].append({'destination' : 1 << 4, 'mask' : 0xFFFF0000, 'key' : r_key_mgmt})
        else:
            # route west
            routers[0].append({'destination' : 4, 'mask' : 0xFFFF0000, 'key' : r_key_mgmt})
    
    for i in range(max_router+1):
        filename = '%s/routingtbl_%d_0.dat' % (pacman.BINARIES_DIRECTORY, i)
        write_routing_file(filename, routers[i])


if __name__ == '__main__':
    print "\n----- patching router"
    db = pacman.load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)
    routes = patch_router_for_robot(db)
    patch_router_for_sensors(db)
        
    # writeback routing file
    
