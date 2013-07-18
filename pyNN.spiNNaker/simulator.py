# encoding: utf-8
"""
Implementation of the "low-level" functionality used by the common
implementation of the API.

Functions and classes useable by the common implementation:

Functions:
    create_cells()
    reset()
    run()

Classes:
    ID
    Recorder
    ConnectionManager
    Connection
    
Attributes:
    state -- a singleton instance of the _State class.
    recorder_list

All other functions and classes are private, and should not be used by other
modules.

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester. francesco.galluppi@cs.man.ac.uk
"""

import sqlite3
from pacman.dao import db
from pyNN import common, errors, core
import os
import pacman
import numpy

import time
t0 = time.time()

#TMP_RASTER_FILE = '/tmp/temp.spikes'
TMP_RASTER_FILE = '%s/temp_spikes.dat' % pacman.BINARIES_DIRECTORY
complete_spike_list = 0


DEBUG = pacman.pacman_configuration.getboolean('pyNN.spiNNaker', 'debug')
global original_pynn_script_directory

def setup(db_name = 'net.db', **extra_params):
    """
    Sets up the simulator
    if db_name is specified the network will be saved into the specified db-file
    """
    global db_run               # Imports the DB from the simulator
    
#        # If the file already exists delete it
    if DEBUG:   print "[ pyNN ] : Opening DB", os.path.abspath(db_name)
    if os.path.exists(db_name):
        if DEBUG:   print "[ pyNN ] : DB already initialized... cleaning up... removing file %s"   %   db_name
        os.remove(db_name)
    db_run = db(db_name)        # Creates the DB    
    db_run.init_db()            # Initializes the DB
    return(db_run)

def set_db(db):
    """
    links an existing db with pyNN.spiNNaker
    """
    global db_run               # Imports the DB from the simulator
    db_run=db

def get_db():
    return(db_run)


def run(simtime):
    """ 
    Commits the DB and runs the simulation.
    Switches on what to run are in the pyNN.spiNNaker section of pacman.cfg:
    """
#    global original_pynn_script_directory
    pacman.original_pynn_script_directory = os.getcwd()
    
    print "[ pyNN ] : Running simulation - connection  with the DB will be now committed"
    db_run.set_runtime(simtime)
    db_run.close_connection()   # FIXME why do I need to close and reopen?
    
    
    if pacman.pacman_configuration.getboolean('pyNN.spiNNaker', 'run_pacman'):
        print "\n[ pyNN ] : Running pacman from", os.path.dirname(pacman.PACMAN_DIR)
        os.chdir(pacman.PACMAN_DIR)
#        os.system('./pacman.sh %s' % db_run.db_abs_path)
#        os.system('./pacman %s' % db_run.db_abs_path)        
        # FIXME FIXME FIXME
        db = pacman.load_db(db_run.db_abs_path)
        db.clean_part_db()              # cleans the part_* tables
        pacman.run_pacman(db)
        
    print "[ pyNN ] : Building network : %f seconds" % (time.time()-t0)
        

    if pacman.pacman_configuration.getboolean('pyNN.spiNNaker', 'run_simulation'):
        board_address = pacman.pacman_configuration.get('board', 'default_board_address')
        if pacman.pacman_configuration.getboolean('pyNN.spiNNaker', 'run_pacman') == False:
            print "[ pyNN ] : cannot run simulation before pacman. change your %s file" % pacman_cfg_filename
            quit(1)

        if pacman.pacman_configuration.getboolean('pyNN.spiNNaker', 'run_app_dump'):
            print "[ pyNN ] : ...running app_dump server and save results to %s"    % TMP_RASTER_FILE
            # TODO better
            os.chdir(pacman.PACMAN_DIR)
            os.chdir(os.pardir)
            os.chdir('tools')
            os.system('python ./spike_receiver.py %s %s noplot &' % (db_run.db_abs_path, TMP_RASTER_FILE))
            os.system('sleep 1')

            
        print "\n[ pyNN ] : Loading simulation on board %s (%s/tools/run.sh %s)\n" % (board_address, os.path.dirname(pacman.PACMAN_DIR), board_address)

        os.chdir(pacman.PACMAN_DIR)
        os.chdir(os.pardir)        
        os.chdir('tools')
#        os.system('./run.sh %s > /dev/null' % board_address)
        os.system('./run.sh %s' % board_address)
        print "[ pyNN ] : ... done ... waiting for simulation on board %s on to finish..." % (board_address)
        pacman.wait_for_simulation()    # will wait for the simulation to finish it run_simulation is set to true in pacman.cfg
        print "[ pyNN ] : ...done!\n"
        os.system('sleep 2')
        global complete_spike_list
        complete_spike_list = numpy.loadtxt(TMP_RASTER_FILE)

        os.chdir(pacman.original_pynn_script_directory)

    os.chdir(pacman.original_pynn_script_directory)
    


def get_db_path():
    """ 
    will return the full path of the database
    """
    
    return(db_run.db_abs_path)
    
def get_v_plot_address(p):
    """
    Given a processor p [1-16] returns the memory address for the v plot data
    """    
    return hex(int('0x%s' % pacman.pacman_configuration.get('memory_addresses', 'v_hex'), 16) + (int('0x400000', 16))*(p-1))[2:]

def get_gsyn_plot_address(p):
    """
    Given a processor p [1-16] returns the memory address for the v plot data
    """    
    return hex(int('0x%s' % pacman.pacman_configuration.get('memory_addresses', 'i_hex'), 16) + (int('0x400000', 16))*(p-1))[2:]



def reset():
    pass

def end():
    try:
        db_run.close_connection()
    except  sqlite3.ProgrammingError:
        print "DB already closed"


