#!/usr/bin/python

import pylab
import pacman
import socket
import struct
import sys
import threading
import time



class SpikeReceiver():

    def __init__(self, sock, address, db_address):
        self.running = True
        
        self.sock = sock
        self.sock.settimeout(1)
        
        self.address = address
        self.db_address = db_address

        self.SDP_HEADER_FORMAT = '=HBBBBHHIIII'
        self.SDP_HEADER_SIZE = struct.calcsize(self.SDP_HEADER_FORMAT)

        self.routing_keys = list()


    def log(self, data):
        if len(data) >= self.SDP_HEADER_SIZE:
            sdp_message = self.unpack_sdp_message(data)
            if sdp_message['command'] == 80: #TODO define clean value
                time = sdp_message['arg1']
                for i in range(0, len(sdp_message['payload']), 4):
                    a = ord(sdp_message['payload'][i + 3]) << 24
                    b = ord(sdp_message['payload'][i + 2]) << 16
                    c = ord(sdp_message['payload'][i + 1]) << 8
                    d = ord(sdp_message['payload'][i])
                    routing_key = a | b | c | d
                    self.routing_keys.append((routing_key, time))
            elif sdp_message['command'] == 81: #TODO define clean value
                self.running = False
                


    def plot(self, filename, plotting=False):
        db = pacman.load_db(self.db_address)

        # Find a global base neuron ID for each population
        populations = db.get_populations_size_type_method()
        base = 0
        for pop in populations:
            pop['base_id'] = base
            base += pop['size']

        # Fetch the routing key map and index it according to chip/processor coordinates
        key_map = db.get_routing_key_map()
        max_x = max(pop['x'] for pop in key_map) + 1
        max_y = max(pop['y'] for pop in key_map) + 1
        max_p = max(pop['p'] for pop in key_map) + 1
        map_temp = [[[[] for p in range(max_p)] for y in range(max_y)] for x in range(max_x)]
        for pop in key_map:
            map_temp[pop['x']][pop['y']][pop['p']].append(pop)

        key_map = map_temp;

        # Translate the routing keys into sequential global neuron IDs
        spikes = list()
        for (key, time) in self.routing_keys:
            chip_x = key >> 24 & 0xFF
            chip_y = key >> 16 & 0xFF
            processor = key >> 11 & 0x1F
            neuron_id = key & 0x7FF
            for pop in key_map[chip_x][chip_y][processor]:
                if neuron_id >= pop['start_id'] and neuron_id <= pop['end_id']:
                    base_id = populations[pop['population_id'] - 1]['base_id'] # 'population_id - 1' because DB is 0 indexed
                    neuron_id = base_id + neuron_id - pop['start_id'] + pop['offset'] #TODO 'offset' should be called 'part_population_offset'
                    spikes.append((neuron_id, time))
                    break

        # Save the spikes in neurotools format
        self.write_neurotools_spike_file(filename, spikes)
        
        # Plot the spikes!
        if plotting:    
            pylab.scatter([time for (neuron_id, time) in spikes], [neuron_id for (neuron_id, time) in spikes], c='green', s=1)

            # Draw lines showing different populations
            for pop in populations:
                pylab.axhline(y=pop['base_id'], color='red', alpha=0.25)
                pylab.annotate(pop['label'], (0, pop['base_id']), color='red', alpha=0.25)


        # Set the plot axis and show
        #pylab.axis([0, 1000, 0, 100]) #TODO better mechanism for setting axis
        pylab.show()


    def start(self):
        self.sock.bind(('0.0.0.0', self.address[1])) # Bind all addresses on given port
        
        while self.running:
            try:
                data = self.sock.recv(1024)
                self.log(data)
            
            except socket.timeout:
                pass


    def unpack_sdp_message(self, data):
        sdp_message = {
            'reserved': 0,
            'flags': 0,
            'ip_tag': 0,

            'destination_port': 0,
            'source_port': 0,
            'destination_chip': 0,
            'source_chip': 0,

            'command': 0,
            'arg1': 0,
            'arg2': 0,
            'arg3': 0,

            'payload': []
        }

        unpack = struct.unpack(self.SDP_HEADER_FORMAT, data[:self.SDP_HEADER_SIZE])
        sdp_message['reserved'] = unpack[0]
        sdp_message['flags'] = unpack[1]
        sdp_message['ip_tag'] = unpack[2]
        sdp_message['destination_port'] = unpack[3]
        sdp_message['source_port'] = unpack[4]
        sdp_message['destination_chip'] = unpack[5]
        sdp_message['source_chip'] = unpack[6]
        sdp_message['command'] = unpack[7]
        sdp_message['arg1'] = unpack[8]
        sdp_message['arg2'] = unpack[9]
        sdp_message['arg3'] = unpack[10]
        sdp_message['payload'] = data[self.SDP_HEADER_SIZE:]

        return sdp_message


    def write_neurotools_spike_file(self, filename, spikes):

        f = open(filename, 'w')

        try:
            lo = min([key for (key, time) in spikes])
            hi = max([key for (key, time) in spikes])
        except ValueError:
            print "[ spike_receiver ] : No Spikes Captured"
            f.close()
            return


        f.write('# first_id = %d\n' % (lo,))
        f.write('# dimensions = None\n')
        f.write('# last_id = %d\n' % (hi,))


        for (key, time) in spikes:
            f.write("%d\t%d\n" % (time, key))

        f.close()


if __name__ == '__main__':
    
    if len(sys.argv) < 3:
        print "Usage: spike_receiver <database file> <log file>"
        exit()
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    address = ('spinn-7', 54321)
    database_address = sys.argv[1]
    filename = sys.argv[2]
    
    try:
        print "\n[ spike_receiver ] : capturing spikes....\n"
        receiver = SpikeReceiver(sock, address, database_address)
        receiver.start()


    except KeyboardInterrupt:
        receiver.running = False
    print "[ spike_receiver ] : simulation completed, sorting spikes and writing them to %s..." % (filename)       
    receiver.plot(filename)
    print "[ spike_receiver ] : ...done!"
