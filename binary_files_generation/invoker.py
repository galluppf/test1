"""
.. moduleauthor:: Sergio Davies, SpiNNaker Project, University of Manchester. sergio.davies@cs.man.ac.uk
"""

#from population_example2 import *
#from memory_writer import *
#from routing import *
from binary_files_generation.routing import routing

    
class invoker:
    def __init__(self, db):
        self.wrap_around = False
        core_map = db.get_image_map()
        
#        self.x_size = 8 #the retina sits in the coordinate x=2, y=0
#        self.y_size = 8
        
        self.x_size = max([ c['x'] for c in core_map]) + 1 # + 1 because chip numbering starts from (0,0)
        self.y_size = max([ c['y'] for c in core_map]) + 1

        
        self.number_of_cores = 18
        self.pop_ex = db
        self.r = routing (self.x_size, self.y_size, self.pop_ex, self.wrap_around) #add wrap_around

    def file_gen(self):        
        self.r.loadPopulations()
        self.r.routeProjections()
        self.r.sortAndAggregateEntries()
        self.r.generateRoutingTables()
        self.r.generateBinaries()        

#        self.m.computeIncomingConnections()
#        self.m.computeSynapsesByIncomingConnections()
#        print "timings:", time.time() - t0, "seconds - files generation"

#        self.m.computeSDRAMentries()
#        self.m.generateBinaryEntries()
#        self.m.padSynapticRows()
#        self.m.computeBlockOffset()
#        self.m.writeBinarySDRAM()

