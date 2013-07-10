#!/usr/bin/python
"""
automatic ybug script generator

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester. francesco.galluppi@cs.man.ac.uk
"""

import pacman
import ConfigParser
import string
import unicodedata

DEBUG = False
CONFIGURATION_FILE_NAME = '%s/../binary_files_generation/ybug.cfg' % pacman.PACMAN_DIR

# monitoring core will always be allocated even if it's not used so monitoring can be switched on at run time using the allocate_monitoring_cores in pacman.cfg


if DEBUG:   print '[ ybug writer ] : Reading configuration file %s'   % CONFIGURATION_FILE_NAME
configuration = ConfigParser.ConfigParser()                              # Queries will be defined in an external file
configuration.readfp(open(CONFIGURATION_FILE_NAME))   # Reads the query configuration file


def compose_app_name(original_image_name, plasticity_suffix):
    #the structure of the application name is app_name.aplx
    #the final structure of the application name should be app_name_suffix.aplx
    #converting the unicode data coming from the database into standard string, avoiding "strange" characters
    temp = unicodedata.normalize('NFKD', original_image_name).encode('ascii','ignore')
    #split the application name
    temp = temp.partition(".aplx")
    #add the suffix
    final_app_name = temp[0]+plasticity_suffix+temp[1]
    if DEBUG:   print final_app_name
    return final_app_name


def write_data_structures(db):
    out = ""
    # get plasticity instantiations
    plasticity_parameters = db.get_plasticity_parameters()
#    print plasticity_parameters
    # write structure data
    core_map = db.get_image_map()

#    # UNCOMMENT for 48 node board        
    x_size = max([ c['x'] for c in core_map]) + 1 # + 1 because chip numbering starts from (0,0)
    y_size = max([ c['y'] for c in core_map]) + 1
    
#    for y_chip in range(eval(configuration.get('system', 'y_size'))):
#        for x_chip in range(eval(configuration.get('system', 'x_size'))):        
    for y_chip in range(y_size):
        for x_chip in range(x_size):        
            chip_map = db.get_image_map(x_chip, y_chip)
            
            if len(chip_map) > 0 and chip_map[0]['is_io'] == 0:     # will exclude io (virtual) chips and empty chips
                out += ("\n### CHIP %d %d ###\n" % (x_chip, y_chip))                
                out += (select_core(x_chip, y_chip, 0))     # selecting the monitor processor as everything below will be in SDRAM
                out += ("\n# SDRAM data (routing, lookup, synaptic structures, stdp)\n")
                out += ("\n# per chip structures\n")
                
                #write routing
                out += ("sload %s/routingtbl_%d_%d.dat %s\n" % (configuration.get('paths', 'binaries_location'), x_chip, y_chip, hex(int(configuration.get('memory', 'ROUTING_OFFSET'), 16))[2:]))
            
                #write sdram
                out += ("sload %s/SDRAM_%d_%d.dat %s\n" % (configuration.get('paths', 'binaries_location'), x_chip, y_chip, hex(int(configuration.get('memory', 'SDRAM_OFFSET'), 16))[2:]))
                
                #write barrier
                out += ("sload %s/barrier.dat %s\n" % (configuration.get('paths', 'binaries_location'), hex(int(configuration.get('memory', 'BARRIER_OFFSET'), 16))[2:]))
            

                
                out += ("\n# per core structures\n")                                            
                for p in chip_map:
                    if p['status'] == 'MONITORING':     
                        # write neural data
                        neural_memory_offset = hex(int(configuration.get('memory', 'NEURAL_DATA_OFFSET'), 16) + (int(configuration.get('memory', 'NEURAL_DATA_SIZE'), 16))*(p['p']-1))[2:]
                        
                        out += ("sload %s/neural_data_%d_%d_%d.dat %s\n" % (configuration.get('paths', 'binaries_location'), p['x'], p['y'], p['p'], neural_memory_offset))

                    else:
    #                    out += (select_core(x_chip, y_chip, p['p']))
                        # cycle proc
                        # write neural data
                        neural_memory_offset = hex(int(configuration.get('memory', 'NEURAL_DATA_OFFSET'), 16) + (int(configuration.get('memory', 'NEURAL_DATA_SIZE'), 16))*(p['p']-1))[2:]
                        
                        out += ("sload %s/neural_data_%d_%d_%d.dat %s\n" % (configuration.get('paths', 'binaries_location'), p['x'], p['y'], p['p'], neural_memory_offset))
                        # write lktbl
                        lktbl_memory_offset = hex(int(configuration.get('memory', 'LOOKUPTABLE_DATA_OFFSET'), 16) + (int(configuration.get('memory', 'LOOKUPTABLE_DATA_SIZE'), 16))*(p['p']-1))[2:]

                        out += ("sload %s/lktbl_%d_%d_%d.dat %s\n" % (configuration.get('paths', 'binaries_location'), p['x'], p['y'], p['p'], lktbl_memory_offset))
                    
                        #write stdp
                        stdp_table_memory_offset = hex(int(configuration.get('memory', 'STDP_TABLE_DATA_OFFSET'), 16) + (int(configuration.get('memory', 'STDP_TABLE_DATA_SIZE'), 16))*(p['p']-1))[2:]
                        plasticity_on = [ (a['x'] == p['x'] and a['y'] == p['y'] and a['p'] == p['p']) for a in plasticity_parameters ]    # is there plasticity on that core?
                        if True in plasticity_on:
                            out += ("sload %s/stdp_table_%d_%d_%d.dat %s\n" % (configuration.get('paths', 'binaries_location'), p['x'], p['y'], p['p'], stdp_table_memory_offset))
                                            
                        out += "\n"     # ends the for loop for single processors

                out += ("\n# DTCM (applications)\n")
                # now select processors and load the application in DTCM
                for p in chip_map:
                    out += (select_core(x_chip, y_chip, p['p']))
                    #compose application name
                    image_name = p['image_name']
                    plasticity_on = [ (a['x'] == p['x'] and a['y'] == p['y'] and a['p'] == p['p']) for a in plasticity_parameters ]    # is there plasticity on that core?
                    if True in plasticity_on:
                        if DEBUG:   print "p: ", p
                        if DEBUG:   print "plasticity_parameters: ", plasticity_parameters[0]
                        image_name = compose_app_name(p['image_name'], plasticity_parameters[0]['suffix'])
                    # load application
                    out += ("sload %s/%s %s\n" % (configuration.get('paths', 'binaries_location'), image_name, configuration.get('memory', 'APPLICATION_OFFSET')[2:]))

                                
                
                
                
    return(out)


    
def set_header():
    out = "\n### HEADER ####\n"
    out += configuration.get("strings", "header")
    return out
    
def set_iptags():
    out = "\n### IP TAGS ####\n"    
    for o in eval(configuration.get("strings", "iptags")):  out += o + "\n"
    return out

def set_multichip_preamble():
    out = "\n### MULTICHIP PREAMBLE ####\n"
    out += configuration.get("strings", "multichip_preamble")
    return out

def select_core(x, y, p=0):
    return("sp %d %d %s\n" % (x,y,p))

def set_application_monitors():
    #chips = db.get_probe_map()
    chips = [{'x': 0, 'y': 0}]
    out = "\n\n### LOADING MONITORING APPLICATIONS ####\n"    
    for c in chips:
        out += select_core(c['x'], c['y'], APPLICATION_MONITORING_PROCESSOR)
        out += configuration.get("monitoring", "load_application_monitoring")
        out += "\n\n"
    return out

def write_run(db):
    out = ""
    out = "\n### Running applications\n"

    core_map = db.get_image_map()

#    # UNCOMMENT for 48 node board        
    x_size = max([ c['x'] for c in core_map]) + 1 # + 1 because chip numbering starts from (0,0)
    y_size = max([ c['y'] for c in core_map]) + 1
    
    
#    for y_chip in range(eval(configuration.get('system', 'y_size'))-1,-1,-1):               # doing it in reverse order
#        for x_chip in range(eval(configuration.get('system', 'x_size'))-1,-1,-1):           # doing it in reverse order
    for y_chip in range(y_size-1,-1,-1):               # doing it in reverse order
        for x_chip in range(x_size-1,-1,-1):           # doing it in reverse order
            chip_map = db.get_image_map(x_chip, y_chip)
            processor_bitmap = 0
#            if len(chip_map) > 0:
            if len(chip_map) > 0 and chip_map[0]['is_io'] == 0:     # will exclude io (virtual) chips and empty chips           
                out += (select_core(x_chip, y_chip, 0))
                for p in chip_map:
                    processor_bitmap |= 1 << p['p']
                out += "as %s %s\n\n" % (configuration.get('memory', 'APPLICATION_OFFSET')[2:], hex(processor_bitmap)[2:])
    return(out)      


def load_spike_source_data(db):

    if DEBUG:   print " [ ybug_file_writer ] : into load_spike_source_data"
    out = ""
    out = "\n### SpikeSourceArray data structures\n"
    # FIXME for multiple cores
    current_core = (0,0,0)
    X_CHIP = 0
    Y_CHIP = 1
    PROC_ID = 2
    
    out += (select_core(current_core[X_CHIP], current_core[Y_CHIP], 0))
    
    # this code is copied from the generate_data_structure file
    processors = db.get_processors()

    # get the method associated with the SpikeSourceArray
    method_id = [ t['method_type'] for t in db.get_distinct_cell_method_types() if t['name'] == 'SpikeSourceArray']

    if method_id == []:
#        print 'SpikeSourceArray: nothing to do'
        return ("")
    
    method_id = method_id[0]        # there will be only one


    for processor in processors:                                        
        # getting the part populations for that processor
        part_populations = db.get_part_populations_from_processor(processor['id'])
        # print part_populations
        if len(part_populations) > 0: #FIXME get used processors only!!!
            if part_populations[0]['method_type'] == method_id:       # searching for a SpikeSourceArray (every part population within a processor only has one cell type, hence evaluating only the first one)
                if DEBUG:   print 'found SpikeSourceArray', part_populations[0], processor
                if (processor['x'] != current_core[X_CHIP] or processor['y'] != current_core[Y_CHIP]):
                    current_core = (processor['x'], processor['y'], 0)
                    out += (select_core(current_core[X_CHIP], current_core[Y_CHIP], 0))
                                    
                    
                memory_offset = hex(int(configuration.get('spike_source_array', 'BASE_OFFSET'), 16) + (int(configuration.get('spike_source_array', 'SIZE'), 16))*(processor['p']-1))[2:]
                out += "sload %s/spike_source_data_%d_%d_%d.dat %s\n" % (configuration.get('paths', 'binaries_location'), processor['x'], processor['y'], processor['p'], memory_offset)
    
    return out
                
    

def write_ybug_file(db, file_name):
    if DEBUG:   print "writing file %s" % file_name
    out_file = open(file_name,'w')
    out_file.write("### automatically generated ybug script\n")
    out_file.write(set_header())
    out_file.write(set_iptags())
    
    image_map = db.get_image_map()    
    if DEBUG:   print image_map
    out_file.write(write_data_structures(db))
    
    if pacman.pacman_configuration.getboolean('neural_data_structure_creator', 'compile_spike_source_arrays'):
        out_file.write(load_spike_source_data(db))    
        
    if pacman.pacman_configuration.getboolean('routing', 'patch_routing_tables') == True:
        out_file.write("@ %s/patched_routing_tables.ybug.dat" % pacman.BINARIES_DIRECTORY)
        
    out_file.write(write_run(db))
    
    out_file.close()


if __name__ == '__main__':
    import sys
    import binary_files_generation
    SCRIPT_LOCATION = '%s/binaries/automatic.ybug' % os.path.dirname(binary_files_generation.__file__)
    if os.path.islink(SCRIPT_LOCATION):
        PACMAN_DIR = os.readlink(SCRIPT_LOCATION)

    
    print "\n----- generating ybug script %s" % SCRIPT_LOCATION
    if len(sys.argv) < 3:
        print "Usage: ybug_file_writer.py source_db"
        exit()	
    db = load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)
    write_ybug_file(db, SCRIPT_LOCATION)
    
    
