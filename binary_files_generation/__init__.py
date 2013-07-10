"""
Binary files generation package

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester. francesco.galluppi@cs.man.ac.uk
"""
import sqlite3

#import pyNN.spiNNaker as p
#from pacman.dao import db
#from binary_files_generation.invoker import *


# Imports the DB and attaches the configuration DB
def load_db(path_to_db):
    db_run = db(path_to_db)        # Instantiates the DB by reading the file
    db_run.import_config_db()      # Imports configuration DB
    db_run.conn.row_factory = sqlite3.Row   # Better select results
    return(db_run)  
    
if __name__ == '__main__':
    import sys
    db = load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)
    inv1  = invoker(db)
    inv1.file_gen()
    db.close_connection()           # will close the connection to the db and commit the transaction 
