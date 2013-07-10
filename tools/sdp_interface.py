import struct
import threading
import socket
import time
import sys

SPINN_HOST_PORT = 54322         # UDP port on the host system
SPINN_BOARD_IP = 'fiercebeast'        # Address of the spiNNaker board
SPINN_BOARD_PORT = 17893        # SDP port on the SpiNNaker chip

SDP_HEADER = '<HBBBBHHIIII'
SDP_HEADER_SIZE = struct.calcsize(SDP_HEADER)   

# SDP HEADER
# ushort magic values
# uchar flags
# uchar tag
# uchar dest_port
# uchar source_port

# ushort dest_addr;		// SDP destination address
# ushort srce_addr;		// SDP source address

# uint cmd_rc;			// Command/Return Code
# uint arg1;			// Arg 1
# uint arg2;			// Arg 2
# uint arg3;			// Arg 3

# Set filter to True to filter only packets arriving from FILTER_HOST (useful if other spinnaker boards are on the same net)
FILTER = False
FILTER_HOST = '130.88.193.234'      # amu16


class Packet():
    '''
    The class packet implements a dictionary for receiving and sending packets in SDP format
    '''
    def __init__(self):
        self.rx_packet = dict()
        self.tx_packet = dict()
        
        self.tx_packet['ip_time_out'] = 0x01;
#        self.tx_packet['pad'] = 0;
        self.tx_packet['flags'] = 0x07;
        self.tx_packet['tag'] = 255;
        self.tx_packet['dst_port'] = 1 << 5 | 1;       # port << 5 | virtual_core_id
        self.tx_packet['src_port'] = 255;
        self.tx_packet['dst_addr'] = 0;                # x_chip << 8 | y_chip
        self.tx_packet['src_addr'] = 0;
        self.tx_packet['cmd'] = 257;
        self.tx_packet['arg1'] = 0;
        self.tx_packet['arg2'] = 0;
        self.tx_packet['arg3'] = 0;
        self.tx_packet['packet_payload'] = None;


    def set_destination_core(x_chip=0, y_chip=0, core_id=1, port=1):
        self.tx_packet['dst_port'] = port << 5 | core_id
        self.tx_packet['dst_addr'] = x_chip << 8 | y_chip;
        
    def set_command(self, command_code):
        self.tx_packet['cmd'] = command_code;

    def pack_sdp_header(self):
        '''Return headers of an SDP message packet into a string for transmission
           over Ethernet. No endianness option is given: SDP messages are processed
           by system software (not bootROM) which receives all network comms in
           little endian order.'''
        return struct.pack(SDP_HEADER,
                        self.tx_packet['ip_time_out'],    
                        self.tx_packet['flags'],
                        self.tx_packet['tag'],
                        self.tx_packet['dst_port'],
                        self.tx_packet['src_port'],
                        self.tx_packet['dst_addr'],
                        self.tx_packet['src_addr'],
                        self.tx_packet['cmd'],
                        self.tx_packet['arg1'],
                        self.tx_packet['arg2'],
                        self.tx_packet['arg3']
#                        self.tx_packet['packed_payload']
                        )

#    def send_packet(self, interface):
#        '''
#        this should be in the interface class
#        '''
#        message = self.pack_sdp_header() + self.tx_packet['packed_payload']
#        interface.sock.sendto(message, (SPINN_BOARD_IP, SPINN_BOARD_PORT))


    def pack_payload(self, mask, data):
        self.tx_packet['packed_payload'] = struct.pack(mask, *data)
        self.tx_packet['arg3'] = struct.calcsize(mask)/4
                        
        
        
    def unpack_rx_packet(self, data):
        unpack = struct.unpack(SDP_HEADER, data[:SDP_HEADER_SIZE])
#        self.payload = struct.unpack('<' + 'i'*((len(data)-SDP_HEADER_SIZE)/4), data[SDP_HEADER_SIZE:])
        self.rx_packet['flags'] = unpack[0]
        self.rx_packet['ip_tag'] = unpack[1]
        self.rx_packet['dst_port'] = unpack[2]
        self.rx_packet['src_port'] = unpack[3]
        self.rx_packet['dst_chip'] = unpack[4]
        self.rx_packet['src_chip'] = unpack[5]
        self.rx_packet['cmd'] = unpack[6]
        self.rx_packet['arg1'] = unpack[8]
        self.rx_packet['arg2'] = unpack[9]
        self.rx_packet['arg3'] = unpack[10]
        self.rx_packet['payload'] = data[SDP_HEADER_SIZE:]
        

        

class SDP_interface():
    def __init__(self, spinnaker_chip_address, spinnaker_chip_port, receiver = True):
        self.spinnaker_chip_address = spinnaker_chip_address
        self.spinnaker_chip_port = spinnaker_chip_port
        self.rx_packet = dict()     # This dictionary contains the last received packet
        if receiver == True:        # if the interface is started with a listener start it up
            self.setup_listener()

    def setup_listener(self, address='0.0.0.0', port=SPINN_HOST_PORT):
        '''
        This method will start  a receiving thread
        '''
##        print 'binding'
        self.sock = socket.socket( socket.AF_INET,  # Internet
               socket.SOCK_DGRAM )                  # UDP
#        self.sock.bind( ('0.0.0.0',SPINN_HOST_PORT) )
        self.sock.bind( (address,port) )            # binding
        self.listenThread = Listen(self)            # starting the listener and passing the interface
        self.listenThread.start()


    def send_packet(self, packet):
        '''
        this method will pack the packet and send it
        '''
        message = packet.pack_sdp_header() + packet.tx_packet['packed_payload']
        self.sock.sendto(message, (self.spinnaker_chip_address, self.spinnaker_chip_port))
        
    
class Listen ( threading.Thread ):
    def __init__(self, interface):        
        threading.Thread.__init__(self)
        self.setDaemon(True)            # Optional; means thread will exit when main thread does        
        self.interface = interface      # Listener class needs an interface to bind with
#        self.sock = interface.sock
        
    def run ( self ):
        while True:
            data, addr = self.interface.sock.recvfrom( 1024 ) # buffer size is 1024 bytes
            print 'received from', addr, len(data), SDP_HEADER_SIZE
            if (FILTER == False) or (FILTER==True and addr[0]==FILTER_HOST):
#                print struct.unpack(SDP_HEADER, data[:SDP_HEADER_SIZE])
                packet = Packet()
                packet.unpack_rx_packet(data)
                self.interface.rx_packet = packet.rx_packet
#                print packet.rx_packet    
            
# old test
#if __name__ == '__main__':
#    server = SDP_interface('amu16', 54321)
#    print "run as main" 
#    for i in range(10):
#        p = Packet()        
#        data = range(5)
##        mask = '<' + 'i'*len(data)
##        print mask, data
#        p.pack_payload('<' + 'I'*len(data), data)       # packs the mask as integers
##        p.send_packet(server)        
#        server.send_packet(p)
#        time.sleep(1)
#        
#    server.listenThread.join(1)    
#    sys.exit()
    

# set individual currents test
#####ipython sdp_interface.py
#####server = SDP_interface('fiercebeast', 17893)
#####p = Packet()
#####p.tx_packet['cmd']=0x104
#####p.tx_packet['arg1']=0
#####p.tx_packet['arg3']=1
#####p.pack_payload("Hh", [3,1*256])
#####server.send_packet(p)



