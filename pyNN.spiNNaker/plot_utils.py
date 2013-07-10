import os
from pacman import *

global instDir
instDir = os.path.join(PACMAN_DIR,'../')

sdram_map = {}
sdram_map['RASTER_PLOT'] = pacman_configuration.get('memory_addresses', 'raster_plot')


def get_v_plot_address(p):
    """
    Given a processor p [1-16] returns the memory address for the v plot data
    """    
    return hex(int('0x%s' % pacman_configuration.get('memory_addresses', 'v_hex'), 16) + (int('0x400000', 16))*(p-1))[2:]


### staticPrintSpikes reads the data from the chip (if retrieve=True) 
### parses it and translates it into neurotools format
### it is possible to access the x_chip, y_chip chip at the address spinnChipAddr
def staticPrintSpikes(filename, nNeurons, time, offs, x_chip=0, y_chip=0, spinnChipAddr='amu13', retrieve=False):
#    global spiNNaker_parameters        
#    spiNNaker_parameters["nuroPerFasc"] = nNeurons
#    spiNNaker_parameters["endTime"] = time
    if retrieve==True:
        sourcedir = os.getcwd() # saves directory
        rp_filename = '/tmp/FTiming_%d_%d_proc%d_\(0-%d\).dat' % (x_chip, y_chip, 0, (nNeurons-1))
        os.chdir(os.path.join(instDir,'./tools/'))
        #####ATTENTION#######
        os.system('./get_outputs.sh %s %s %s %s %s %s %s' % (spinnChipAddr, rp_filename, sdram_map['RASTER_PLOT'], offs["ftiming"], x_chip, y_chip, 0))    
        os.chdir(sourcedir)
    
    f = open(filename, 'w')
    data = getHexRaster(nNeurons, x_chip, y_chip)
    writeSpikesData(f, data, nNeurons)


def staticPrintV(fname, nNeurons, offs=None, x_chip=0, y_chip=0, spiNNChipAddr='amu16', retrieve=False, memory_addr=0):
    """****f* __init__/getPopulations
    Utility used to download, parse and save membrane potential logging information form the chip
    staticPrintV(fname, nNeurons, time, x_chip=0, y_chip=0, spinnChipAddr='spinn-1', retrieve=False):
    fname = NeuroTools compatible output file
    nNeurons = number of neurons in the population (needed to write the headers, equals the number of neurons in the chip since loggin is all or none)
    time = simulated time
    x_chip, y_chip = chip ID (used for multichip simulations)
    spinnChipAddr = IP address of the spiNNaker chip
    retrieve = if True will download raw data from the chip before parse it 
    
    """

    if retrieve==True:  # If retrieve==true get the stuff from the chip
        sourcedir = os.getcwd() # saves directory
        v_filename = '/tmp/OutputHex_V_%d_%d_proc%d_\(0-%d\).dat' % (x_chip, y_chip, 0, (nNeurons-1))
        os.chdir(os.path.join(instDir,'./tools/'))
        print os.getcwd()
        print spiNNChipAddr, v_filename, memory_addr, offs['v_values'], x_chip, y_chip, 0
        os.system('./get_outputs.sh %s %s %s %s %s %s %s' % (spiNNChipAddr, v_filename, memory_addr, offs["v_values"], x_chip, y_chip, 0))
        print './get_outputs.sh %s %s %s %s %s %s %s' % (spiNNChipAddr, v_filename, memory_addr, offs["v_values"], x_chip, y_chip, 0)    
        os.chdir(sourcedir)
        
    data = getHexData(fname, nNeurons, type='V', x_chip=x_chip, y_chip=y_chip)
    return data
    #writeHexData(f, data)        
    """
    ****"""

def staticPrintI(fname, nNeurons, time, x_chip=0, y_chip=0, spiNNChipAddr='spinn-1', retrieve=False, offs=None):
    """****f* __init__/getPopulations
    Utility used to download, parse and save membrane potential logging information form the chip
    staticPrintV(fname, nNeurons, time, x_chip=0, y_chip=0, spinnChipAddr='spinn-1', retrieve=False):
    fname = NeuroTools compatible output file
    nNeurons = number of neurons in the population (needed to write the headers, equals the number of neurons in the chip since loggin is all or none)
    time = simulated time
    x_chip, y_chip = chip ID (used for multichip simulations)
    spinnChipAddr = IP address of the spiNNaker chip
    retrieve = if True will download raw data from the chip before parse it 
    
    """
    global spiNNaker_parameters        
    spiNNaker_parameters["nuroPerFasc"] = nNeurons
    spiNNaker_parameters["endTime"] = time
    if offs==None:	offs = getOutputValues()

    if retrieve==True:  # If retrieve==true get the stuff from the chip
        sourcedir = os.getcwd() # saves directory
        v_filename = '/tmp/OutputHex_I_%d_%d_proc%d_\(0-%d\).dat' % (x_chip, y_chip, 0, (nNeurons-1))
        os.chdir(os.path.join(instDir,'./tools/'))
        os.system('./get_outputs.sh %s %s %s %s %s %s %s' % (spiNNChipAddr, v_filename, sdram_map['HEX_I'], offs["i_values"], x_chip, y_chip, 0))
        print './get_outputs.sh %s %s %s %s %s %s %s' % (spiNNChipAddr, v_filename, sdram_map['HEX_I'], offs["i_values"], x_chip, y_chip, 0)    
        os.chdir(sourcedir)
        
    data = getHexData(fname, nNeurons, type='I', x_chip=x_chip, y_chip=y_chip)
    #writeHexData(f, data)        
    """
    ****"""
    
    
    def printSpikes(self, filename, nNeurons):        # SPINNAKER SPECIFIC OVVERRRIDE FUNCTION
        #print "into printSpikes"
        global runScripts, spiNNChipAddr
        if runScripts == 1:
            f = open(filename, 'w')
            data = getHexRaster()
            writeSpikesData(f, data, nNeurons)
            #print data                                    
        else:
            raise Exception("RUNNING_FROM_PYNN must be set to 1 in order to gather debugging information")
    
    def print_v(self, filename):            # SPINNAKER SPECIFIC OVVERRRIDE FUNCTION
        global runScripts, spiNNChipAddr
        if runScripts == 1:
            offs = getOutputValues()
            print offs
            #os.system('./get_outputs.sh %s "%s" %s' % (spiNNChipAddr, filename, offs["v_values"]))
            getHexData(filename, spiNNaker_parameters['nuroPerFasc'], self)       # Extract data from whole hex file
            getHexData('%s.i' % filename, spiNNaker_parameters['nuroPerFasc'], self, type='I')       # Extract data from whole hex file
#            getHexData('%s.islow' % filename, spiNNaker_parameters['nuroPerFasc'], self, type='ISLOW')       # Extract data from whole hex file
        else:
            raise Exception("RUNNING_FROM_PYNN must be set to 1 in order to gather debugging information")


def getHexData(filename, nNeurons, pop=None, type='V', x_chip=0, y_chip=0, parse_result=True):
    """
    Loads raw data from OutputHex_V file and returns it into pyNN/NeuroTools format
    Can be used to retrieve other Hex data from other files (ex. u, I, Islow) if type is specified
    """
    from struct import unpack    
    p1 = 256.0            # .0 converts it to floats        
    
    global instDir
    
    # Reads a line from the I/V Hex file
    def read_bin_line(f, nNeurons):
        out = []
        for i in range(nNeurons):
            l = f.read(2)
            if l == "":
                return False        # Reached the end of the file
            c = unpack('h', l)
            out.append(c[0]/p1)
        return out
    
            
    f = open(os.path.join('/tmp/OutputHex_%s_%d_%d_proc0_(0-%d).dat' % (type, x_chip, y_chip, (nNeurons -1))), 'r')    
    
    s = 0
    
    out = []
    
    while True:
        l = read_bin_line(f, nNeurons)
        if l == False:
            break
    #    print "second", s
        else: 
            out.append(l)
        
    f.close()    
    
    # if results need not to be parsed into a txt file the function will end here and result
    if parse_result==False:
        return out
    
    
    f = open(filename , 'w')
    
#    print "ranging", range(len(out))
    
#    for i in range(len(out[0])):
#        print "Extracting line %d" % i
#        f.write("%d : " % i)
#        f.write(" ".join(["%.2f" % el for el in zip(*out)[i]]))
#        f.write("\n")

    
    #### HASH PRINTS -- STUBS ! NOW COLLECTING EVERY DATA
    f.write("# first_id = 0\n")
    f.write("# n = %d\n" % (nNeurons-1))
    f.write("# dt = 1.0\n")
    f.write("# dimensions = [%d]\n" % (nNeurons))
    f.write("# last_id = %d\n" % (nNeurons-1))   
     
    for sec in out:
        for j in range(len(sec)):
            f.write("%.4f\t%.1f\n" % (sec[j], j))
        
    f.close()
    
    
def getHexRaster(nNeurons, x_chip=0, y_chip=0):
    from math import ceil
    from struct import unpack
    from operator import lshift
    
    # All the neurons in the processor are saved to a raw file 
#    nNeurons = spiNNaker_parameters["nuroPerFasc"]
#    time = spiNNaker_parameters["endTime"]    
    nWords = int(ceil(nNeurons/32.0))        # Number of words in column (number of neurons)
    
    #print nNeurons, time, nWords
    
    # open raw file
    f = open(os.path.join('/tmp/FTiming_%d_%d_proc0_(0-%d).dat' % (x_chip, y_chip, (nNeurons -1))), 'r')
    
    temp = []
    # Reads row by row (row = ms timestamp)
    msec = 0
    out = []    
    print "retrieving with %d words" % nWords
    while True:
        l = f.read(4*nWords)    # read 32 bit words        
        if l =="":  # no more input
            break   # exit while

        bitstring = ""
        
        for w in range(nWords):
            value = unpack("<I", l[w*4:(w+1)*4])[0]     # little endian black magic going on here
            bits = bin( value ).lstrip('-0b')           # gets the binary value
            bits = bits.rjust(32, '0')                  # pads it with zeros on the right
            bitstring += bits[::-1]                     # reverts it...
            
#        print bitstring

        bitstring = zip(bitstring)        # now stripped into a tuple

#        print s

        for n in range(len(bitstring)):
#            print "evaluating neuron %d at msec %d. has fired? %s" % (nNeurons-n, msec, bitstring[n][0])
            if bitstring[n][0] == '1':
#                print msec, n
                   out.append( (msec, n) )
            

        msec += 1  # increments timestamp
        

    print "Extracted %d spikes" % len(out)
#    print out
    return(out)

"""
        value = unpack(mask, l)
        
        #### reads the couple into a list
        if len(value)>1:
            value_temp = value[0]
            for i in range(1, len(value)):
                value_temp += lshift(value[i], 32*i)
            value = value_temp
        else:
            value = value[0]
        temp.append(bitconverter(value, 4*nWords*8))            
    
    #print "len(temp) %d x %d" % (len(temp), len(temp[0]))
    
    #print temp[23]
    
    
    out = []
    
    #decodes the temp list
    for i in range(len(temp)):
        for j in range(len(temp[i])):            
            if eval(temp[i][j]) == 1:                
                out.append((i,abs(j-(4*nWords*8-1))))           # Reverts the neuron id

"""
    
    
def writeSpikesData(f, data, nNeurons):
    global spiNNaker_parameters
#    nNeurons = spiNNaker_parameters['nuroPerFasc']
    f.write("# first_id = 0\n")
    f.write("# n = %d\n" % (nNeurons-1))
    f.write("# dt = 1.0\n")
    f.write("# dimensions = [%d]\n" % (nNeurons))
    f.write("# last_id = %d\n" % (nNeurons-1))
    for d in data:
        f.write("%.1f\t%.1f\n" % (d[0], d[1]))

#####################################    

def get_v(fname, nNeurons, length, x_chip, y_chip, spiNNChipAddr, memory_addr, p=1, retrieve=True):
    """****f* __init__/getPopulations
    Utility used to download, parse and save membrane potential logging information form the chip
    staticPrintV(fname, nNeurons, time, x_chip=0, y_chip=0, spinnChipAddr='spinn-1', retrieve=False):
    fname = NeuroTools compatible output file
    nNeurons = number of neurons in the population (needed to write the headers, equals the number of neurons in the chip since loggin is all or none)
    time = simulated time
    x_chip, y_chip = chip ID (used for multichip simulations)
    spinnChipAddr = IP address of the spiNNaker chip
    retrieve = if True will download raw data from the chip before parse it 
    
    """

    if retrieve==True:  # If retrieve==true get the stuff from the chip
        sourcedir = os.getcwd() # saves directory
        v_filename = '/tmp/OutputHex_V_%d_%d_proc%d_\(0-%d\).dat' % (x_chip, y_chip, p, (nNeurons-1))
        os.chdir(os.path.join(PACMAN_DIR,'../tools/'))
#        if DEBUG:   print spiNNChipAddr, v_filename, memory_addr, offs['v_values'], x_chip, y_chip, p
        print '[ pyNN ] : ./get_outputs.sh %s %s %s %s %s %s %s' % (spiNNChipAddr, v_filename, memory_addr, length, x_chip, y_chip, p)    
        os.system('./get_outputs.sh %s %s %s %s %s %s %s > /dev/null' % (spiNNChipAddr, v_filename, memory_addr, length, x_chip, y_chip, p))
        os.chdir(sourcedir)
        
    data = parse_analog_signal(fname, nNeurons, type='V', x_chip=x_chip, y_chip=y_chip, p=p)
    return data
    """
    ****"""


def parse_analog_signal(filename, nNeurons, x_chip=0, y_chip=0, p=1, parse_result=True, type='V'):
    """
    Loads raw data from OutputHex_V file and returns it into pyNN/NeuroTools format
    Can be used to retrieve other Hex data from other files (ex. u, I, Islow) if type is specified
    """
    from struct import unpack    
    p1 = 256.0            # .0 converts it to floats        
    
    # Reads a line from the I/V Hex file
    def read_bin_line(f, nNeurons):
        out = []
        for i in range(nNeurons):
            l = f.read(2)
            if l == "":
                return False        # Reached the end of the file
            c = unpack('h', l)
            out.append(c[0]/p1)
        return out
    
            
    f = open(os.path.join('/tmp/OutputHex_%s_%d_%d_proc%d_(0-%d).dat' % (type, x_chip, y_chip, p, (nNeurons -1))), 'r')        
    out = []
    while True:
        l = read_bin_line(f, nNeurons)
        if l == False:  break
        else:           out.append(l)        
    f.close()    
    
    return out

