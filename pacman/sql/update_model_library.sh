#This script needs to be called upon any svn update that involves the model_library.sql file


#Author: Francesco Galluppi, SpiNNaker Project, Computer School, University of Manchester, July 2011
#Email:  francesco.galluppi@cs.man.ac.uk

#!/bin/bash
> model_library.db
sqlite3 model_library.db < model_library.sql
