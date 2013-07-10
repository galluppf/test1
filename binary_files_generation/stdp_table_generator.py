#!/usr/bin/python

"""
Utility to generate an STDP table for SpiNNaker

__author__="francesco"
__date__ ="$22-Mar-2011 18:01:14$"

"""

BINARY_DIRECTORY = '../binaries/'

import ConfigParser, sys
from numpy import arange, zeros
from math import exp, log
from struct import pack
from pacman import *


# packs an array with a given mask for every element. maybe there's a python function doing this? like [ out += pack(mask,i) for i in array ] 
def packArray(array, mask):
    out = ""
    for i in array:
        out += pack(mask,i)            # h = 4bit words
    return out



DEBUG = pacman_configuration.getboolean('stdp_table_generator', 'debug')
p1 = 256

# packs an array with a given mask for every element
def packArray(array, mask):
    out = ""
    for i in array:
        out += pack(mask,i)            # h = 4bit words
#    print out
    return out

def setHeaders(w_min, w_max, ltp_time_window, ltd_time_window, resolution, words):
    s = pack("<h", w_min*p1)
    s += pack("<h", w_max*p1)
    s += pack("<b", ltp_time_window)
    s += pack("<b", ltd_time_window)
    s += pack("<b", resolution)
    s += pack("<b", int( log(resolution, 2)) )
    s += pack("<b", words)
    return s

def calc_STDP_table(ltp_time_window, ltd_time_window, resolution, A_plus, A_minus, tau_plus, tau_minus, words, zero_value=0):    
#    print ltd_time_window+ltp_time_window, resolution*words    
    assert ltd_time_window+ltp_time_window < resolution*words*32*2, "Time window exceeds maxmimum size of %d msec. Decrease ltd/ltp time window or resolution" % (resolution*words*32*2+1)

    ltd = arange(resolution*32*4, 0, -resolution)
    ltp = arange(resolution, resolution*4*32+resolution, resolution)
    
    if DEBUG:   print "[ stdp_table_generator ] :" ,ltd, ltp

    out = []
    for l in ltd:
        out.append( (A_minus*exp(float(-l)/tau_minus) + A_minus*exp(float(-(l+1))/tau_minus))/2 )

    if zero_value != 0:   print "[ stdp_table_generator ] : setting value in dt = 0 to %f" % zero_value
    out.append(zero_value)

    for l in ltp:
        out.append( (A_plus*exp(float(-l)/tau_plus) + A_plus*exp(float(-(l+1))/tau_plus))/2 )

#   Scaling
    out = [ int(i*p1) for i in out ]
#    words*32 is the size of the ltp and ltd window. The whole table is words*32 + 1 (value in 0) + words*32 = words*32*2+1 bytes long
#    left_bound = int(resolution*32*4-ltd_time_window/resolution)
#    right_bound = int(words*32+1+ltp_time_window/resolution)
    left_bound = 128-ltd_time_window
    right_bound = 129 + ltp_time_window    
#   Truncating the time window with the one specified by ltd/ltp_time_window
#   print left_bound, right_bound
    out[:left_bound] = zeros(left_bound, 'int')
    out[right_bound:] = zeros(128-ltp_time_window, 'int')

    if DEBUG:   print out
    return out


def compile_stdp_table(cfg, out_filename):    
    """
    compiles an stdp table given dictionary cfg and an output file name
    cfg is in the format
    cfg['ltp_time_window'], 
    cfg['ltd_time_window'], 
    cfg['resolution'], 
    cfg['A_plus'],
    cfg['A_minus'], 
    cfg['tau_plus'], 
    cfg['tau_minus'], 
    cfg['words'],
    cfg['zero_value']
    """
    print "[ stdp_table_generator ] : Writing file", out_filename
    f = open(out_filename, mode='w+')

    print "[ stdp_table_generator ] : Writing headers"
    f.write(setHeaders(cfg['w_min'], cfg['w_max'], cfg['ltd_time_window'], cfg['ltp_time_window'], cfg['resolution'], cfg['words']))

    s = calc_STDP_table(cfg['ltp_time_window'], 
                    cfg['ltd_time_window'], 
                    cfg['resolution'], 
                    cfg['A_plus'],
                    cfg['A_minus'], 
                    cfg['tau_plus'], 
                    cfg['tau_minus'], 
                    cfg['words'],
                    cfg['zero_value'])

    f.write(packArray(s,'<b'))
   
    f.close()
    print "[ stdp_table_generator ] : Done!"


def compile_stdp_tts_table(cfg, out_filename):    
    """
    compiles an stdp table given dictionary cfg and an output file name
    cfg is in the format
    cfg['ltp_time_window'], 
    cfg['ltd_time_window'], 
    cfg['resolution'], 
    cfg['A_plus'],
    cfg['A_minus'], 
    cfg['tau_plus'], 
    cfg['tau_minus'], 
    cfg['words'],
    cfg['zero_value']
    """
    print "[ stdp_table_generator ] : Writing file", out_filename
    f = open(out_filename, mode='w+')

    print "Writing headers"
    f.write(setHeaders(cfg['w_min'], cfg['w_max'], cfg['ltd_time_window'], cfg['ltp_time_window'], cfg['resolution'], cfg['words']))

    s = calc_STDP_table(cfg['ltp_time_window'], 
                    cfg['ltd_time_window'], 
                    cfg['resolution'], 
                    cfg['A_plus'],
                    cfg['A_minus'], 
                    cfg['tau_plus'], 
                    cfg['tau_minus'], 
                    cfg['words'],
                    cfg['zero_value'])

    f.write(packArray(s,'<b'))

    f.write(pack("<h", cfg['L_parameter']))
   
    f.close()
    print "Done!"


def compile_stdp_table_from_db(db):
    print "\n[ stdp_table_generator ] : calculating STDP tables"
    plasticity_parameters = db.get_plasticity_parameters()
    
    if len(plasticity_parameters) < 1:
        print "[ stdp_table_generator ] : Nothing to do...\n"
        return
    
    for p in plasticity_parameters:
        if DEBUG:   print p
        out_file_name = BINARY_DIRECTORY + "stdp_table_" + str(p['x']) + "_" + str(p['y']) + "_" + str(p['p']) + ".dat"
        # FIXME read defaults from pacman cfg
        parameters = eval (p['parameters'])
        if DEBUG:   print parameters
        if 'ltd_time_window' not in parameters.keys():       parameters['ltd_time_window'] = pacman_configuration.getint('stdp_table_generator', 'ltd_time_window')
        if 'ltp_time_window' not in parameters.keys():       parameters['ltp_time_window'] = pacman_configuration.getint('stdp_table_generator', 'ltp_time_window')
        if 'words' not in parameters.keys():                 parameters['words'] = pacman_configuration.getint('stdp_table_generator', 'words')
        if 'zero_value' not in parameters.keys():            parameters['zero_value'] = eval(pacman_configuration.get('stdp_table_generator', 'zero_value'))
        
        if DEBUG:   
            print "[ stdp_table_generator ] : parameters: ", parameters
            print "[ stdp_table_generator ] : p: ", p
                        
        if p['method'] == 'FullWindow':
            print "[ stdp_table_generator ] : computing STDP table for FullWindow rule"
            compile_stdp_table(parameters, out_file_name)
        if p['method'] == 'SpikePairRule':
            print "[ stdp_table_generator ] : computing STDP table for SpikePair rule"
            compile_stdp_table(parameters, out_file_name)
        if p['method'] == 'TimeToSpike':
            print "[ stdp_table_generator ] : computing STDP table for TimeToSpike rule"
            compile_stdp_tts_table(parameters, out_file_name)
                    

if __name__ == "__main__":    
    db = load_db(sys.argv[1])       # IMPORTS THE DB (it will also load the model libraray by default)
    compile_stdp_table_from_db(db)

