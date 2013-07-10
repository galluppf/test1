#!/usr/bin/python
"""
Initializer for PACMAN. 
Defines the DB interface to PACMAN and contains the runtime system.

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester, francesco.galluppi@cs.man.ac.uk
"""
import sqlite3
import os
import ConfigParser
import sys
import signal
import socket 
import struct

from dao import db

import time

import binary_files_generation

#BINARIES_DIRECTORY = '%s/binaries' % os.path.dirname(binary_files_generation.__file__)

# reads the configuration file
pacman_cfg_filename = "pacman.cfg"
print "[ pacman ] : Loading config file: %s" % pacman_cfg_filename
pacman_configuration = ConfigParser.ConfigParser()                               
pacman_configuration.readfp(open(os.path.join(os.path.dirname(__file__),pacman_cfg_filename)))

# sets PACMAN_DIR (following links)
PACMAN_DIR = os.path.abspath(os.path.dirname(__file__))
if os.path.islink(PACMAN_DIR):      PACMAN_DIR = os.readlink(PACMAN_DIR)

BINARIES_DIRECTORY = os.path.abspath('%s/../binaries' % PACMAN_DIR) 

# sets the directory from which the pynn script is executed
original_pynn_script_directory = ""
print "[ pacman ] : running pacman from", PACMAN_DIR

def return_configuration():
    return pacman_configuration

def load_db(path_to_db):
    """
    Instantiates a new network db in the path_to_db file
    Imports the configuration DB (model library)
    Imports the system DB (system library defined in pacman.cfg [board])    
    Sets Sqlite3 parameters
    Returns a connection to the DB
    """
    db_run = db(path_to_db)        # Instantiates the DB by reading the file
    db_run.import_config_db()      # Imports configuration DB
    db_run.conn.row_factory = sqlite3.Row   # Better select results
    return(db_run)    

def signal_handler(signal, frame):
    """
    Event handler for the SIGINT signal
    """
    print "[ pacman ] : Pacman Interrupted! Saving DB..."
    db.close_connection()
    sys.exit(10)

# catches CTRL-C
signal.signal(signal.SIGINT, signal_handler)


def run_pacman(db, simulator='pynn'):
    """
    Runs PACMAN on the db passed.
     - pre-splitter cleans the *.dat files in binaries
     - splitter
     - post-splitter (empty)
     - grouper (disabled in pacman.cfg for this version)
     - mapper
     - post-mapper (stdp_table_generator, allocate_monitoring_cores) (pynn)
     - create_core_list
    
    Generates the following files:
       - routing
       - patch_router_for_robot
       - patch_routing_tables for unused chips
       - synapse generation
       - lookup_table_generator
       - neuron generation
       - spike_source_array structures (pynn)
       - ybug_file_writer
    
    """
    import splitter
    import grouper
    import mapper
    import binary_files_generation.stdp_table_generator as stdp_table_generator    
    import binary_files_generation.invoker as invoker
    import binary_files_generation.synapse_writer as synapse_writer
#    import binary_files_generation.data_structure_creator as data_structure_creator
    import binary_files_generation.neuron_writer as neuron_writer
    import binary_files_generation.ybug_file_writer as ybug_file_writer
    import binary_files_generation.routing_patcher as routing_patcher           #   needed to patch routing tables for non used chips and robots
    t0 = time.time()
    
    def tstamp(t0): return(time.time() - t0)
    
    
    
    #### PRE-PACMAN
    if pacman_configuration.getboolean('pre-pacman', 'run') == True:
        print "\n[ pacman ] : Running pre-pacman plugins"
        print "[ pacman ] : Cleaning *.dat files in %s" % BINARIES_DIRECTORY
        import glob
        for f in glob.glob("%s/*.dat" % BINARIES_DIRECTORY):    os.remove(f)
        

    #### SPLITTING        
    print "\n[ pacman ] : Splitting....."
    t0 = time.time()
    splitter.split_populations(db)
    splitter.split_projections(db)
    splitter.split_probes(db)
    print "\n[ pacman ] : Splitting done, commit...", tstamp(t0)    
    db.commit() 

    #### POST-SPLITTING    
    if pacman_configuration.getboolean('post-splitter', 'run') == True:
        print "\n[ pacman ] : Running post-splitter plugins"

    #### GROUPER
    print "[ pacman ] : Running grouper"
    t0 = time.time()
    if pacman_configuration.getboolean('grouper', 'run'):
        raise NotImplementedError("Grouping is not supported. Please turn it off in pacman.cfg")
        # deprecated
        groups = grouper.get_groups(db)
        grouper(db, groups)
        print "[ pacman ] : grouping terminated, going with mapping"
        grouper.update_core_offsets(db)

    else:
        print "[ pacman ] : Bypassing Grouping"        
        grouper.bypass_grouping(db)

    print "\n[ pacman ] : Grouping done, commit...", tstamp(t0)
    db.commit()     

    #### MAPPER            
    print "[ pacman ] : Mapping..."    
    t0 = time.time()
    mapper.mapper(db)
    
	# check for post-mapper triggers
    if pacman_configuration.getboolean('post-mapper', 'run'):
        print "[ pacman ] : Running post-mapper plugins"

		# stdp table generation
        if pacman_configuration.getboolean('post-mapper', 'generate_stdp_tables') and simulator=='pynn':
            print "[ pacman ] : running compile_stdp_table_from_db"
            stdp_table_generator.compile_stdp_table_from_db(db)

		# allocating app monitoring
        if pacman_configuration.getboolean('post-mapper', 'allocate_monitoring_cores') and simulator=='pynn':
            print "[ pacman ] : running allocate_monitoring_cores"
            mapper.allocate_monitoring_cores(db)
    
    # core list creation
    mapper.create_core_list(db)

    print "[ pacman ] : Mapping done, commit...", tstamp(t0)
    db.commit() 
            
    #### BINARY FILES GENERATION    #####

    #### ROUTING
    print "\n[ pacman ] : Routing..."        
    t0 = time.time()    
    inv1  = invoker.invoker(db)
    inv1.file_gen()

    print "[ pacman ] : Routing done, commit...", tstamp(t0)
    db.commit() 
    
    if pacman_configuration.getboolean('routing', 'patch_routing_tables') == True:
        print "\n[ pacman ] : Patching routing tables to handle non used chips"
        routing_patcher.patch_routing_entries_missing_chips(db)

    if pacman_configuration.getboolean('routing', 'patch_routing_for_robot') == True:
        print "\n[ pacman ] : Patching routing tables for robotic use"
        routing_patcher.patch_router_for_robot(db)
        routing_patcher.patch_router_for_sensors(db)

    
    #### SDRAM
    print "\n[ pacman ] : Synapse generation..."
    t0 = time.time()
    synapse_writer.synapse_writer(db)
    
    print "[ pacman ] : Synapse generation done, commit...", tstamp(t0)   
    
#    # FIXME horrible hack for avoiding db locked errors
#    try:    db.commit()
#    except: pass

    
#    #### LOOKUP TABLE GENERATOR
#    print "\n[ pacman ] : Lookup table generation..."
#    t0 = time.time()
#    curdir = os.getcwd()
#    os.chdir(PACMAN_DIR)
#    os.chdir(os.pardir)
#    os.chdir('binary_files_generation')
    
##    print "./lookup_table_generator %s %s/sql/model_library.db" % (db.db_abs_path, PACMAN_DIR)            
#    model_library_address = "%s/sql/model_library.db" % PACMAN_DIR
#    system_library_address = "%s/sql/%s" % (PACMAN_DIR, pacman_configuration.get('board', 'system_library'))
#    LEGACY_LOOKUP = pacman_configuration.getboolean('synapse_writer', 'run_legacy_lookup')
#    if LEGACY_LOOKUP:   os.system("./lookup_table_generator %s %s %s" % (db.db_abs_path, model_library_address, system_library_address))
#    print "[ pacman ] : Lookup table generation done, commit...", tstamp(t0)        
#    os.chdir(curdir)        
    
    ### DATA STRUCTURE CREATOR
    print "\n[ pacman ] : Neuron generation..."
    t0 = time.time()
#    data_structure_creator.write_neural_data_structures(db)
    neuron_writer.write_neural_data_structures(db)
    db.commit()    # not needed?
    print "[ pacman ] : Neuron generation done, commit...", tstamp(t0)
    
    
    if pacman_configuration.getboolean('neural_data_structure_creator', 'compile_spike_source_arrays') and simulator=='pynn':
        import binary_files_generation.generate_input_spikes as generate_input_spikes
        
        print "[ pacman ] : running compile_spike_source_arrays"
        generate_input_spikes.compile_spike_source_array(db)

    
    #### YBUG file writer
    print "\n[ pacman ] : yBug file writer..."
    t0 = time.time()
    
    SCRIPT_LOCATION = '%s/automatic.ybug' % BINARIES_DIRECTORY
    ybug_file_writer.write_ybug_file(db, SCRIPT_LOCATION)
    print "[ pacman ] : yBug file writer done...", tstamp(t0)
    
    #### DONE!
    if pacman_configuration.getboolean('pyNN.spiNNaker', 'run_simulation') == False and simulator=='pynn':     
        print "[ pacman ] : closing db...."
        db.close_connection()       # will close the connection to the db and commit the transaction    


def wait_for_simulation():
    """
    if run_simulation is set to true in pacman, right after compiling with pacman ./run.sh <board name> will be called. 
    
    wait_for_simulation waits for an empty SDP packet, arriving from any spinnaker board sending messages to the pc at the end of the simulation, and returns control to the user after receiving this packet. It is a useful utility in case users want to use data analysis routines as printSpikes and printV within the same pynn script.
    
     - MGMT_PORT is the management port, identified with tag 2
     - SIMULATION_END_CMD_RC is the command code of the SDP packet signalling the end of the simulation
     
    they need to be consistent with what found in comms.c, comms.h and main.c where it is supposed that core 0 0 1 send mentioned packet at the end of the simulation
    """
    MGMT_PORT = 54322
    SIMULATION_END_CMD_RC = 80
    SDP_HEADER_FORMAT = '<HBBBBHHIiii'
    SDP_HEADER_SIZE = struct.calcsize(SDP_HEADER_FORMAT)
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1)    
    sock.bind(('0.0.0.0', MGMT_PORT)) # Bind all addresses on given port
                
    while True:
        try:
            data = sock.recv(1024)                
            unpack = struct.unpack(SDP_HEADER_FORMAT, data[:SDP_HEADER_SIZE])
            command = unpack[7]                
            if command == SIMULATION_END_CMD_RC:   break
        except socket.timeout:
            pass


if __name__ == '__main__':        
    
    print "[ pacman ] : Loading DB: %s\n" % sys.argv[1]                
    db = load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)
    print "[ pacman ] : cleaning up part-db..."
    db.clean_part_db()              # cleans the part_* tables
    
    run_pacman(db)
    db.close_connection()       # will close the connection to the db and commit the transaction   

