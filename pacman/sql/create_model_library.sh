#Creates the model library from a sql file
#Author: Francesco Galluppi, SpiNNaker Project, Computer School, University of Manchester, July 2011
#Email:  francesco.galluppi@cs.man.ac.uk




#!/bin/bash
rm -f model_library.db
sqlite3 model_library.db < model_library.sql
