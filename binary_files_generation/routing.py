"""
.. moduleauthor:: Sergio Davies, SpiNNaker Project, University of Manchester. sergio.davies@cs.man.ac.uk
"""


from math import *
from struct import *
from router import *
from sys import *
import pacman

DEBUG = False

class routing:

    def __init__ (self, xsize, ysize, population_data, wrap_around):
        self.pop_ex = population_data
        self.x_size = xsize
        self.y_size = ysize
        self.system = [[router(x,y) for x in range (0,ysize)] for y in range (0,xsize)]
        #print "Created routing system: xsize =",xsize,"; ysize =",ysize
        if wrap_around:
            self.wrap_around = True
        else:
            self.wrap_around = False

    def loadPopulations(self):
        #compute starting id, ending id and mask for each population
        for i in range (1, self.pop_ex.get_num_part_populations()+1):
            src_pop = self.pop_ex.get_part_population_loc(i)
            src_loc_x, src_loc_y, src_loc_core = src_pop['x'], src_pop['y'], src_pop['p']

#            print "[ routing ] : Chip X = ",src_loc_x,", Y = ",src_loc_y,", Core = ",src_loc_core         
            population_key_start = self.system[src_loc_x][src_loc_y].neuronID[src_loc_core]

            if (population_key_start > 2048):
                if DEBUG: print "Chip X = ",src_loc_x,", Y = ",src_loc_y,", Core = ",src_loc_core,": too many neurons mapped in this core"
                if DEBUG: print "population_key_start = ",population_key_start
                exit("Mapping error")

            if (population_key_start + self.pop_ex.get_part_population_size(i) > 2048):
                if DEBUG: print "Chip X = ",src_loc_x,", Y = ",src_loc_y,", Core = ",src_loc_core,": too many neurons mapped in this core"
                if DEBUG: print "population_key_end = ",population_key_start + get_part_population_size(i)
                exit("Mapping error")


            population_bits = int(ceil(log(self.pop_ex.get_part_population_size(i), 2)))

            population_key_end = int(population_key_start + pow(2, population_bits) - 1)
            self.system[src_loc_x][src_loc_y].neuronID[src_loc_core] = population_key_end+1

            population_mask = long((pow(2,32)-1)) & (~(population_key_end ^ population_key_start))
            #the formula to compute the mask is valid only if the size of the population is a power of 2 number of neurons
            #and if all the populations preceeding are made by a power of two number of neurons. Moreover the population
            #must be ordered by decreasing size.

            if DEBUG: print "Population ", i, " size: ", self.pop_ex.get_part_population_size(i), " population bits: ", population_bits
            if DEBUG: print "Population ", i, " start: ", hex(population_key_start), " end: ", hex(population_key_end), " mask: ", hex(population_mask)
            self.pop_ex.set_start_id(i, population_key_start)
            self.pop_ex.set_end_id(i, population_key_end)
            self.pop_ex.set_mask(i, population_mask)
        return

    def routeProjections(self):
        numberOfProjections = self.pop_ex.get_num_part_projections()
        for i in range (1, numberOfProjections+1):
            if DEBUG: print "Routing projection ", i, " of ", numberOfProjections
            proj = self.pop_ex.get_part_projection(i)[0]
            src, dst = proj['presynaptic_part_population_id'], proj['postsynaptic_part_population_id']

            src_pop = self.pop_ex.get_part_population_loc(src)
            src_loc_x, src_loc_y, src_loc_core = src_pop['x'], src_pop['y'], src_pop['p']
            
            src_pop_data = self.pop_ex.get_part_populations(part_population_id=src)[0]
            src_population_key_start, src_population_key_end, src_population_mask = src_pop_data['start_id'], src_pop_data['end_id'], src_pop_data['mask']

            dst_pop = self.pop_ex.get_part_population_loc(dst)
            dst_loc_x, dst_loc_y, dst_loc_core = dst_pop['x'], dst_pop['y'], dst_pop['p']            
            
            if (self.wrap_around):
                #horizontal hops
                if (dst_loc_x >= src_loc_x):
                    hops_dx = dst_loc_x - src_loc_x
                else:
                    hops_dx = self.x_size - (src_loc_x - dst_loc_x)

                hops_sx = self.x_size - hops_dx

                if (hops_dx <= hops.sx):
                    x_diff = hops_dx
                else:
                    x_diff = - hops_sx

                #vertical hops
                if (dst_loc_y >= src_loc_y):
                    hops_up = dst_loc_y - src_loc_y
                else:
                    hops_up = self.y_size - (src_loc_y - dst_loc_y)

                hops_dn = self.y_size - hops_up

                if (hops_up <= hops.dn):
                    y_diff = hops_up
                else:
                    y_diff = - hops_dn

            else:
                x_diff = dst_loc_x - src_loc_x
                y_diff = dst_loc_y - src_loc_y

            current_pos_x = src_loc_x
            current_pos_y = src_loc_y

            hops = [0,0,0,0,0,0]
            current_hops = [0,0,0,0,0,0]

            hops[0] = max(0, x_diff)
            hops[1] = max(0,min(x_diff, y_diff))
            hops[2] = max(0, y_diff)
            hops[3] = max(0, -x_diff)
            hops[4] = max(0, min(-x_diff, -y_diff))
            hops[5] = max(0, -y_diff)

            hops[0] = max(0, (hops[0] - hops[1]))
            hops[2] = max(0, (hops[2] - hops[1]))

            hops[3] = max(0, (hops[3] - hops[4]))
            hops[5] = max(0, (hops[5] - hops[4]))

            if DEBUG: print "Source chip: X = ", src_loc_x, ", Y = ", src_loc_y
            if DEBUG: print "Destination chip: X = ", dst_loc_x, ", Y = ", dst_loc_y
            if hops[0] > 0:
                if DEBUG: print "Hops in X+ direction: ", hops[0]
            if hops[1] > 0:
                if DEBUG: print "Hops in X+,Y+ direction: ", hops[1]
            if hops[2] > 0:
                if DEBUG: print "Hops in Y+ direction: ", hops[2]
            if hops[3] > 0:
                if DEBUG: print "Hops in X- direction: ", hops[3]
            if hops[4] > 0:
                if DEBUG: print "Hops in X-,Y- direction: ", hops[4]
            if hops[5] > 0:
                if DEBUG: print "Hops in Y- direction: ", hops[5]

            source = -1

            while current_pos_x != dst_loc_x or current_pos_y != dst_loc_y:
                if DEBUG: print "Source = ", source, ", Position X = ", current_pos_x, ", Position Y = ", current_pos_y
#                if DEBUG: print "Current hops 0:", current_hops[0], " 1: ", current_hops[1], " 2: ", current_hops[2], " 3: ", current_hops[3], " 4: ", current_hops[4], " 5: ", current_hops[5]
#                if DEBUG: print "Hops 0:", hops[0], " 1: ", hops[1], " 2: ", hops[2], " 3: ", hops[3], " 4: ", hops[4], " 5: ", hops[5]

                for i in range(0,6):
                    if (current_hops[i] == hops[i] and hops[i] > 0):
                        hops[i] = 0;

                if (hops[1] >= max(hops[0], hops[2], hops[3], hops[4], hops[5]) and current_hops[1] < hops[1]):
                    if DEBUG: print "Following direction 1"
                    direction = 1
                    current_hops[1] += 1
                    new_pos_x = current_pos_x + 1
                    new_pos_y = current_pos_y + 1

                elif (hops[4] >= max(hops[0], hops[1], hops[2], hops[3], hops[5]) and current_hops[4] < hops[4]):
                    if DEBUG: print "Following direction 4"
                    direction = 4
                    current_hops[4] += 1
                    new_pos_x = current_pos_x - 1
                    new_pos_y = current_pos_y - 1

                elif (hops[0] >= max(hops[1], hops[2], hops[3], hops[4], hops[5]) and current_hops[0] < hops[0]):
                    if DEBUG: print "Following direction 0"
                    direction = 0
                    current_hops[0] += 1
                    new_pos_x = current_pos_x + 1
                    new_pos_y = current_pos_y

                elif (hops[3] >= max(hops[0], hops[1], hops[2], hops[4], hops[5]) and current_hops[3] < hops[3]):
                    if DEBUG: print "Following direction 3"
                    direction = 3
                    current_hops[3] += 1
                    new_pos_x = current_pos_x - 1
                    new_pos_y = current_pos_y

                elif (hops[2] >= max(hops[0], hops[1], hops[3], hops[4], hops[5]) and current_hops[2] < hops[2]):
                    if DEBUG: print "Following direction 2"
                    direction = 2
                    current_hops[2] += 1
                    new_pos_x = current_pos_x
                    new_pos_y = current_pos_y + 1

                elif (hops[5]>= max(hops[0], hops[1], hops[2], hops[3], hops[4]) and current_hops[5] < hops[5]):
                    if DEBUG: print "Following direction 5"
                    direction = 5
                    current_hops[5] += 1
                    new_pos_x = current_pos_x
                    new_pos_y = current_pos_y - 1
                
                self.system[current_pos_x][current_pos_y].addHop([src_loc_x, src_loc_y, src_loc_core, src_population_key_start], src_population_mask, 1 << direction, source)

                current_pos_x = new_pos_x
                current_pos_y = new_pos_y
                source = ((direction + 3) % 6)

            self.system[current_pos_x][current_pos_y].addHop([src_loc_x, src_loc_y, src_loc_core, src_population_key_start], src_population_mask, int(pow(2,(dst_loc_core+6))), source)
            if DEBUG: print "dst_loc_core: %d, var: %s" % (dst_loc_core, hex(int(pow(2,(dst_loc_core+6)))))
            if DEBUG: print "Routed!"

    def sortAndAggregateEntries(self):
        for x in range (0,self.x_size):
            for y in range (0,self.y_size):
                self.system[x][y].hops.sort()
                z = 0
                while z < self.system[x][y].sizeHops()-1:
                    if self.system[x][y].hops[z][0] == self.system[x][y].hops[z+1][0]:
                        #if DEBUG: print "self.system[x][y].hops[z][2] |= self.system[x][y].hops[z+1][2]: %s - %s - %s", (hex(self.system[x][y].hops[z][2]), hex(self.system[x][y].hops[z+1][2]), hex( self.system[x][y].hops[z][2] | self.system[x][y].hops[z+1][2]))
                        self.system[x][y].hops[z][2] |= self.system[x][y].hops[z+1][2]
                        self.system[x][y].delHop(z+1)
                    else:
                        z += 1

    def generateRoutingTables(self):
        for x in range (0,self.x_size):
            for y in range (0,self.y_size):
                for z in range (0,self.system[x][y].sizeHops()):
                    entry = self.system[x][y].getHop(z)
                    direction = entry[2]
                    source = entry[3]
                    #(direction & (direction - 1)) == 0 condition to test if direction has only one bit set
                    #direction == pow(2, (source + 3)%6) condition to test if the output and the source are on the opposite sides of the chip
                    #source == -1 packet generated in the current chip
                    if (source == -1 or (direction & (direction - 1)) != 0 or direction != pow(2, (source + 3)%6)): #avoid entry for default routing
                        key = entry[0][0] << 24 | entry[0][1] << 16 | entry[0][2] << 11 | entry[0][3]
                        mask = entry[1]
                        self.system[x][y].addEntry(key, mask, direction)
#                        if DEBUG: print "direction", hex(key), hex(eval(mask)), hex(direction)

    def generateBinaries(self):
        for x in range (0,self.x_size):
            for y in range (0,self.y_size):
                fname = "%s/routingtbl_%d_%d.dat" % (pacman.BINARIES_DIRECTORY,x,y)
                ftable = open(fname, "wb")
                data = pack("<I", self.system[x][y].sizeEntries())
                ftable.write(data)
                for z in range (0,self.system[x][y].sizeEntries()):
                    routing_entry = self.system[x][y].getEntry(z)
                    if DEBUG: print routing_entry
                    data = pack ("<III",routing_entry[0],eval(routing_entry[1]),routing_entry[2])
                    ftable.write(data)
                ftable.close()

