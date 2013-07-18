# -*- coding: utf-8 -*-
"""
SpiNNaker implementation of the PyNN API

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester, January 2013. francesco.galluppi@cs.man.ac.uk
"""

import pickle
import sys, os


from simulator import *
from pyNN.spiNNaker.connectors import *
from pyNN.spiNNaker.electrodes import *
from pyNN.spiNNaker.standardmodels.synapses import *
from pyNN.spiNNaker.standardmodels.cells import *


#from pyNN.random import RandomDistribution
import pyNN.random
import numpy
from pyNN import standardmodels
import pyNN.space

import pacman


from binary_files_generation import synapse_writer

def rank():
    return 0

TIME = 0
VALUE = 1



board_address = pacman.pacman_configuration.get('board', 'default_board_address')

def list_standard_models():
    """Return a list of all the StandardCellType classes available for this simulator."""
    return [obj.__name__ for obj in globals().values() if isinstance(obj, type) and issubclass(obj, standardmodels.StandardCellType)]


def parse_analog_signal(filename, nNeurons, type='V'):
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

    f = open(filename)        
    out = []
    while True:
        l = read_bin_line(f, nNeurons)
        if l == False:  break
        else:           out.append(l)        
    f.close()    
    
    return out



class Population():
    def __init__(self, size, cellclass, cellparams=None, structure=None, label=None):
        """
        Create a population of neurons all of the same type.

        size - number of cells in the Population. For backwards-compatibility,
               n may also be a tuple giving the dimensions of a grid,
               e.g. n=(10,10) is equivalent to n=100 with structure=Grid2D()
        cellclass should either be a standardized cell class (a class inheriting
                from common.standardmodels.StandardCellType) or a string giving the
                name of the simulator-specific model that makes up the population.
        cellparams should be a dict which is passed to the neuron model
          constructor
        structure should be a Structure instance.
        label is an optional name for the population.
        """
        if not isinstance(size, int):  # also allow a single integer, for a 1D population
            assert isinstance(size, tuple), "`size` must be an integer or a tuple of ints. You have supplied a %s" % type(size)
            # check the things inside are ints
            for e in size:
                assert isinstance(e, int), "`size` must be an integer or a tuple of ints. Element '%s' is not an int" % str(e)
#        assert( cellclass in list_standard_models() ), "cell class %s not supported" % cellclass

        self.size       = size
        self.cellclass  = cellclass
#        self.cellparams = self.set_default_paramters(self.cellclass.extras)         # TODO
        self.cellparams = self.cellclass.default_parameters
        for p in cellparams:
            self.cellparams[p] = cellparams[p]
#        if hasattr(self.cellclass, 'extras'): self.cellparams.update(self.cellclass.extras)
        self.structure  = structure
        self.positions  = None
        self.label      = label
        self.id         = simulator.db_run.insert_population(self)
                
        if self.structure != None:      # a spatial structure has been passed, exploding the ids
            self.positions = self.structure.generate_positions(self.size)

    def __add__(self, other):
        """
        A Population/PopulationView can be added to another Population,
        PopulationView or Assembly, returning an Assembly.
        
        Not implemented yet        
        """
        assert isinstance(other, Population)
        return Assembly([self, other]) 

    def __getitem__(self, index):
        class returnable:
            def __init__(self, position):
                self.position = position            
        return(returnable(numpy.array([self.positions[0][index], self.positions[1][index], self.positions[2][index]])))
        
    def __len__(self):
        return int(self.size)

    def get(self,parameter):
        return(eval(simulator.db_run.get_population_parameters(self.id))[parameter])

            
    def randomInit(self, rand_distr):
        v_inits = []
        for i in range(self.size):
        #    v_inits.append(-55-70.0/(nNeurons)*i)
            v_inits.append(rand_distr.next())
        self.set('v_init', v_inits)
        
    def rset(self, param_name, rand_distr):
        """
        rset(param_name, rand_distr): sets the parameter param_name with random samples from the distribution rand_distr
        """
        params = []
        for i in range(self.size):
        #    v_inits.append(-55-70.0/(nNeurons)*i)
            params.append(rand_distr.next())
        self.set(param_name, params)



    def initialize(self, variable, value):
        """
        initialize(self, variable, value)

        Set the initial value of one of the state variables of the neurons in
        this population.

        `value` may either be a numeric value (all neurons set to the same
                value) or a `!RandomDistribution` object (each neuron gets a
                different value)
        """
        if variable=='v': variable = 'v_init'
        out = []        # the list which will contain the random instantiations.
        
        if isinstance(value, RandomDistribution):     
            for i in range(self.size):  
                out.append(value.next())
                
        else:   out = [ value for i in range(self.size) ]
        
        self.set(variable, out)


    def record_variable(self, variable, save_to='SDRAM'):
#        print variable
        simulator.db_run.insert_probe(self.id, variable, save_to)


    def record(self, save_to=None, to_file=False):
        if save_to == None:
            if(to_file==False):
                save_to='eth'
            else:
                raise NotImplementedError("Can't save spikes to SDRAM")
                save_to='SDRAM'
                                        
        self.record_variable('spikes', save_to)     # will execute the query as well

    def record_v(self, to_file=True):
        """
        Record the membrane potential for all cells in the Population.
        """
        if(to_file==False):
            save_to='eth'
        else:
            save_to='SDRAM'
            
        self.record_variable('v', save_to)

    def record_gsyn(self, to_file=True):
        """
        population.record_gsyn()
        
        DESCRIPTION: Record synaptic conductances (currents) for all cells in the Population.
        
        """
        if(to_file==False):
            save_to='eth'
        else:
            save_to='SDRAM'
            
        variable = 'gsyn'
        self.record_variable(variable, save_to)

    def set_mapping_constraint(self, constraint):
        """
        This function specifies the constraint for allocating the population in the SpiNNaker system.
        constraint is a dictionary in the format {'x': x_chip, 'y': y_chip, 'p': p_chip}
        """
        simulator.db_run.set_mapping_constraint(self.id, constraint)

    def set_population_splitting_constraint(self, n):
        """
        This function specifies the number of neurons n that can be mapped in a single core for this population
        """
        simulator.db_run.set_population_splitting_constraint(self, n)

    
    def set(self, parameter, value):
        """
        Set one parameter for every cell in the population.

        param is a string giving the parameter name
        val is the parameter value.
        """
        parameters = eval(simulator.db_run.get_population_parameters(self.id))
        parameters[parameter] = value
        simulator.db_run.set_population_parameters(parameters, self.id)
        
        
    def getSpikes(self, gather=False):
        """
        Returns spikes in a list of (time, neural_id)
        """
        out = []
        rk_map = simulator.db_run.get_routing_key_map()
        entries = [ r for r in rk_map if r['population_id']==self.id ]
        
        start_id = entries[0]['absolute_start_id']
        end_id = start_id + entries[0]['size']
        try:
            with open(TMP_RASTER_FILE, 'r') as fsource:
                read_data = fsource.readlines()

            for r in read_data:
                if r.startswith('#'):   pass    # comment
                else: 
                    values = r.split("\t")
                    neural_id = eval(values[VALUE])
                    time = eval(values[TIME])
                    if neural_id in range(start_id, end_id): 
                        out.append((time, neural_id-start_id))
    #                    f.write("%s\t%d\n"    % (values[TIME], eval(values[VALUE])-start_id))
            # Convert list to numpy array
            spikeArray = numpy.asarray(out)
            # Switch the columns. 0 is the neuronID and 1 
            # is the time of the spike
            spikeArray[:,[0,1]] = spikeArray[:,[1,0]]
            # Sort by neuron ID and not by time 
            # First get the indeces to do that  
            spikeIndex = numpy.lexsort((spikeArray[:,1],spikeArray[:,0]))
            out = spikeArray[spikeIndex]
        except:
            print "[ pyNN.spiNNaker ] : no spikes in population", self.label
            out = []
        return out

    
        
    def printSpikes(self, filename):
#        rk_map = simulator.db_run.get_routing_key_map()
#        entries = [ r for r in rk_map if r['population_id']==self.id ]
#        TMP_RASTER_FILE = '/tmp/temp.spikes'
        
#        start_id = entries[0]['absolute_start_id']
#        end_id = start_id + entries[0]['size']
                
        #### WRITING FILE: HEADER
        print "[ pyNN ] : Writing file: ", filename
        f = open(filename , 'w')
        f.write("# first_id = 0\n")
        f.write("# n = %d\n" % (self.size-1))
        f.write("# dimensions = [%d]\n" % (self.size))
        f.write("# last_id = %d\n" % (self.size-1))   
     
        
        spike_data = self.getSpikes()
        for values in spike_data:
            f.write("%d\t%d\n"    % (values[VALUE], values[TIME])) #(values[TIME], values[VALUE]))
        
#        with open(TMP_RASTER_FILE, 'r') as fsource:
#            read_data = fsource.readlines()
        
#        for r in read_data:
#            if r.startswith('#'):   pass    # comment
#            else: 
#                values = r.split("\t")
#                TIME = 0
#                VALUE = 1
#                if eval(values[VALUE]) in range(start_id, end_id): 
#                    f.write("%s\t%d\n"    % (values[TIME], eval(values[VALUE])-start_id))
        
        f.close()        


    def get_v(self, retrieve=True, gather=True):
        """
        Utility used to download, parse and save membrane potential logging information form the chip
        """
        try:
            runtime = eval(simulator.db_run.get_runtime())
        except sqlite3.ProgrammingError:
            print "\n[ pacman ] : ERROR ! DB closed - have you called run? set run_simulation and board_address in pacman.cfg?"
            sys.exit(2)
        rk_map = simulator.db_run.get_routing_key_map()
        entries = [ r for r in rk_map if r['population_id']==self.id ]
        data = []        
        
#        if os.path.isabs(filename) == False:     filename = "%s/%s" % (pacman.original_pynn_script_directory, filename)

        for e in entries:        
            if retrieve==True:  # If retrieve==true get the stuff from the chip
                neurons_in_population = e['size']    
                nNeurons = e['part_population_size']
                spiNNChipAddr = board_address                
                length_v = hex(e['part_population_size']*runtime*2)[2:]            
                memory_addr=simulator.get_v_plot_address(e['p'])
                
                sourcedir = os.getcwd() # saves directory
                v_filename = '%s/OutputHex_V_%d_%d_proc%d_\(0-%d\).dat' % (pacman.BINARIES_DIRECTORY, e['x'], e['y'], e['p'], (nNeurons-1))
                os.chdir(os.path.join(pacman.PACMAN_DIR,'../tools/'))
        #        if DEBUG:   print spiNNChipAddr, v_filename, memory_addr, offs['v_values'], x_chip, y_chip, p
                print '[ pyNN ] : ./get_outputs.sh %s %s %s %s %s %s %s' % (spiNNChipAddr, v_filename, memory_addr, length_v, e['x'], e['y'], e['p'])    
                os.system('./get_outputs.sh %s %s %s %s %s %s %s > /dev/null' % (spiNNChipAddr, v_filename, memory_addr, length_v, e['x'], e['y'], e['p']))
                os.chdir(sourcedir)
            
            data.append(parse_analog_signal('%s/OutputHex_V_%d_%d_proc%d_(0-%d).dat' % (pacman.BINARIES_DIRECTORY, e['x'], e['y'], e['p'], (nNeurons-1)), nNeurons, type='V'))
        
        out = []
        # data is a list of matrices with runtime rows and size_part_population neurons that need to be merged
        for t in range(runtime):
            row = []
            for d in range(len(data)):  # for every chunk
                for i in data[d][t]:
                    row.append(i)
            out.append(row)


        if gather==False:           # raw data     
            out = numpy.array(out)
        else:                       # data organized in (id, time, value)
            gathered_output = []
            for time in range(len(out)):
                for neuron_id in range(len(out[time])):
                    gathered_output.append((neuron_id, time, out[time][neuron_id]))
            out = numpy.array(gathered_output)
        
        return out


    def get_gsyn(self, retrieve=True, gather=True):
        """
        Utility used to download, parse and save membrane potential logging information form the chip
        staticPrintV(fname, nNeurons, time, x_chip=0, y_chip=0, spinnChipAddr='spinn-1', retrieve=False):
        fname = NeuroTools compatible output file
        nNeurons = number of neurons in the population (needed to write the headers, equals the number of neurons in the chip since loggin is all or none)
        time = simulated time
        x_chip, y_chip = chip ID (used for multichip simulations)
        spinnChipAddr = IP address of the spiNNaker chip
        retrieve = if True will download raw data from the chip before parse it 
        
        """
        try:
            runtime = eval(simulator.db_run.get_runtime())
        except sqlite3.ProgrammingError:
            print "\n[ pacman ] : ERROR ! DB closed - have you called run? set run_simulation and board_address in pacman.cfg?"
            sys.exit(2)
        rk_map = simulator.db_run.get_routing_key_map()
        entries = [ r for r in rk_map if r['population_id']==self.id ]
        data = []        
        
#        if os.path.isabs(filename) == False:     filename = "%s/%s" % (pacman.original_pynn_script_directory, filename)

        for e in entries:        
            if retrieve==True:  # If retrieve==true get the stuff from the chip
                neurons_in_population = e['size']    
                nNeurons = e['part_population_size']
                spiNNChipAddr = board_address                
                length_v = hex(e['part_population_size']*runtime*2)[2:]            
                memory_addr=simulator.get_gsyn_plot_address(e['p'])
                
                sourcedir = os.getcwd() # saves directory
                v_filename = '%s/OutputHex_GSYN_%d_%d_proc%d_\(0-%d\).dat' % (pacman.BINARIES_DIRECTORY, e['x'], e['y'], e['p'], (nNeurons-1))
                os.chdir(os.path.join(pacman.PACMAN_DIR,'../tools/'))
        #        if DEBUG:   print spiNNChipAddr, v_filename, memory_addr, offs['v_values'], x_chip, y_chip, p
                print '[ pyNN ] : ./get_outputs.sh %s %s %s %s %s %s %s' % (spiNNChipAddr, v_filename, memory_addr, length_v, e['x'], e['y'], e['p'])    
                os.system('./get_outputs.sh %s %s %s %s %s %s %s > /dev/null' % (spiNNChipAddr, v_filename, memory_addr, length_v, e['x'], e['y'], e['p']))
                os.chdir(sourcedir)
            
            data.append(parse_analog_signal('%s/OutputHex_GSYN_%d_%d_proc%d_(0-%d).dat' % (pacman.BINARIES_DIRECTORY, e['x'], e['y'], e['p'], (nNeurons-1)), nNeurons, type='V'))
        
        out = []
        # data is a list of matrices with runtime rows and size_part_population neurons that need to be merged

        for t in range(runtime):
            row = []
            for d in range(len(data)):  # for every chunk
                for i in data[d][t]:
                    row.append(i)
            out.append(row)
        
        if gather==False:           # raw data     
            out = numpy.array(out)
        else:                       # data organized in (id, time, value)
            gathered_output = []
            for time in range(len(out)):
                for neuron_id in range(len(out[time])):
                    gathered_output.append((neuron_id, time, out[time][neuron_id]))
            out = numpy.array(gathered_output)

        return out
            


        
    def print_v(self, filename, retrieve=True):
        """
        Utility to get and print to a txt file the recorded membrane potentials of a population. 
        Spans multiple cores/chips as it needs to download all the data, parse it and merge it in a single txt file
        Currently works only with absolute paths
        """
          
        #### WRITING FILE: HEADER
        print "[ pyNN ] : Writing file: ", filename
        f = open(filename , 'w')
        f.write("# first_id = 0\n")
        f.write("# n = %d\n" % (self.size-1))
        f.write("# dt = 1.0\n")
        f.write("# dimensions = [%d]\n" % (self.size))
        f.write("# last_id = %d\n" % (self.size-1))   
        
        # get_v does all the job
        out = self.get_v(gather=False)
             
        for sec in out:
            for j in range(len(sec)):
                f.write("%.4f\t%.1f\n" % (sec[j], j))
            
        f.close()
            
    def print_gsyn(self, filename, retrieve=True):
        """
        Utility to get and print to a txt file the recorded conductance (current) potentials of a population. 
        """
          
        #### WRITING FILE: HEADER
        print "[ pyNN ] : Writing file: ", filename
        f = open(filename , 'w')
        f.write("# first_id = 0\n")
        f.write("# n = %d\n" % (self.size-1))
        f.write("# dt = 1.0\n")
        f.write("# dimensions = [%d]\n" % (self.size))
        f.write("# last_id = %d\n" % (self.size-1))   
        
        # get_v does all the job
        out = self.get_gsyn(gather=False)
             
        for sec in out:
            for j in range(len(sec)):
                f.write("%.4f\t%.1f\n" % (sec[j], j))
            
        f.close()

                
    @property
    def size(self):
        return (self.size)

    def __len__(self):
        """Return the total number of cells in the population (all nodes)."""
        return self.size
        
    def _get_position(self):
        return(0)


class PopulationView():
    """
    Not implemented yet
    """
    def __init__(self):
        raise NotImplementedError("Not implemented yet!")

class Projection():
    """
    A container for all the connections of a given type (same synapse type and
    plasticity mechanisms) between two populations, together with methods to
    set parameters of those connections, including of plasticity mechanisms.
    """

    def __init__(self, presynaptic_population, postsynaptic_population, method, source=None, target='excitatory', synapse_dynamics=None, label=None, rng=None):
        """
        presynaptic_population and postsynaptic_population - Population, PopulationView
                                                            or Assembly objects.

        source - string specifying which attribute of the presynaptic cell
                 signals action potentials. This is only needed for
                 multicompartmental cells with branching axons or
                 dendrodendriticsynapses. All standard cells have a single
                 source, and this is the default.

        target - string specifying which synapse on the postsynaptic cell to
                 connect to. For standard cells, this can be 'excitatory' or
                 'inhibitory'. For non-standard cells, it could be 'NMDA', etc.
                 If target is not given, the default values of 'excitatory' is
                 used.

        method - a Connector object, encapsulating the algorithm to use for
                 connecting the neurons.

        synapse_dynamics - a `standardmodels.SynapseDynamics` object specifying
                 which synaptic plasticity mechanisms to use.

        rng - specify an RNG object to be used by the Connector.
        """

        self.presynaptic_population = presynaptic_population
        self.postsynaptic_population = postsynaptic_population
        self.method = method
        self.source = source
        self.target = target
        self.synapse_dynamics = synapse_dynamics
        self.label = label
        self.rng = rng


        ### check if it's a distance probability connector and translates it to a from list connector
        if isinstance(self.method, connectors.DistanceDependentProbabilityConnector):   
            self.method = self.translate_distance_to_list_connector()            ### // FIXME
            
                            
        self.size = 0
        self.compute_size()

        # Getting projection parameters. If weights/delays are not specified they will be defaulted to 1        
        self.set_parameters()

        if synapse_dynamics == None:
            self.plasticity_id = 0
        else:
            self.plasticity_id = synapse_dynamics.id

        self.source = 0
        self.target = target

        self.id = simulator.db_run.insert_projection(self)

        if isinstance(self.method, connectors.FromListConnector):      
            self.spool_projection_list_to_raw_file(numpy.asarray(self.parameters['list']))
            
        if isinstance(self.method, connectors.OneToOneConnector):
            size_pop = min(self.presynaptic_population.size, self.postsynaptic_population.size)
            conn_list_array = numpy.ones((size_pop,4))
            conn_list_array[:,0] = numpy.arange((size_pop))
            conn_list_array[:,1] = numpy.arange((size_pop))
            
            # Ugly method to cope with random distributions
            
            if isinstance(self.parameters['weights'], dict): conn_list_array[:,2] = synapse_writer.return_random_weights(simulator.db_run, self.parameters['weights']['r']-1, size_pop)                   # It's a distribution
            else:       conn_list_array[:,2] = self.parameters['weights']       # It's a scalar

            if isinstance(self.parameters['delays'], dict): conn_list_array[:,3] = synapse_writer.return_random_delays(simulator.db_run, self.parameters['delays']['r']-1, size_pop)                   # It's a distribution            
            else:   conn_list_array[:,3] = self.parameters['delays']            # It's a scalar
        
            self.spool_projection_list_to_raw_file(conn_list_array)


    def spool_projection_list_to_raw_file(self, connection_list):
        """
        this function writes a connection list (OneToOneConnector, SpaceDependentConnector, FromListConnector) to a file so that the splitter can pick it up
        """
        pickle.dump(connection_list, open( "/tmp/proj_%d.raw" % self.id, "wb" ) )

    def translate_distance_to_list_connector(self):
        conn_list = []
        d = 1
        if isinstance(eval(self.method.d_expression)/1.0, float):  d_expression = '%s>numpy.random.rand()'  % self.method.d_expression #d_expression needs a random distribution 
        elif isinstance(eval(self.method.d_expression), bool): d_expression = self.method.d_expression  #d_expression is a whole expression
        print d_expression
        # start making connections
        for pre in range(self.presynaptic_population.size):
            for post in range(self.postsynaptic_population.size):
                d = pyNN.space.distance(self.presynaptic_population[pre], self.postsynaptic_population[post])
                if(eval(d_expression)) == True:   
                    # checking weight
                    if (isinstance(self.method.weights, str)):   weight = eval(self.method.weights)
                    elif (isinstance(self.method.weights/1.0, float)): weight = self.method.weights
                    # checking delay
                    if (isinstance(self.method.delays, str)):   delay = eval(self.method.delays)
                    elif (isinstance(self.method.delays/1.0, float)): delay = self.method.delays
                    conn_list.append([pre, post, weight, delay])
                    
                    
#                    print "making connection between neuron", pre, "and",post, "distance", self.presynaptic_population[pre].position, self.postsynaptic_population[post].position, d
        return(FromListConnector(conn_list))


    def compute_size(self):
        """
        Returns the number of single connections in the projection
        """
        if isinstance(self.method, connectors.OneToOneConnector):     
            self.size = min(self.presynaptic_population.size, self.postsynaptic_population.size)
        elif isinstance(self.method, connectors.AllToAllConnector):   
            self.size = self.presynaptic_population.size * self.postsynaptic_population.size
            if self.method.allow_self_connections==False:
                self.size -= min(self.presynaptic_population.size, self.postsynaptic_population.size)
        elif isinstance(self.method, connectors.FixedProbabilityConnector):   
            self.size = self.presynaptic_population.size * self.postsynaptic_population.size * self.method.p_connect
        elif isinstance(self.method, connectors.FromListConnector):   
            self.size = len(self.method.conn_list)
        elif isinstance(self.method, connectors.DistanceDependentProbabilityConnector):   
            pass

    def set_parameters(self):
        self.parameters = {}
        # if is a FromListConnector then put the list as the parameter and return - everything is explicit in the list
        if isinstance(self.method, connectors.FromListConnector):
            self.parameters['list'] = self.method.conn_list
            return (0)

        # check if weights is a random distribution object. if so parse distr_id into a string as a parameters
        weights = self.method.weights
        if isinstance(weights, RandomDistribution):
#            weights = 'distr_%d'    %   self.method.weights.id
#            weights = 'self.random_distributions[%d].next()' % (self.method.weights.id-1)   # -1 because DB ids start from 1, arrays indexing from 0... very confusing.... # FIXME MODIFIED FOR PACMAN 48
            weights = {'r': self.method.weights.id }
        
        # check if delays is a random distribution object. if so parse distr_id into a string as a parameters
        delays = self.method.delays
        if isinstance(delays, RandomDistribution):
#            delays = 'distr_%d'    %   self.method.delays.id
            delays = 'int(self.random_distributions[%d].next())' % (self.method.delays.id-1) # -1 because DB ids start from 1, arrays indexing from 0... very confusing....
            delays = {'r': self.method.delays.id }            
        
        self.parameters['weights'] = weights
        self.parameters['delays'] = delays

        # checking if self connection are allowed
        if (isinstance(self.method, connectors.AllToAllConnector) or isinstance(self.method, connectors.FixedProbabilityConnector)):   
            if self.method.allow_self_connections == False:
                self.parameters['allow_self_connections'] = False
            else:
                self.parameters['allow_self_connections'] = True

        # if is a FixedProbabilityConnector then p_connect needs to be passed as a parameter as well
        if isinstance(self.method, connectors.FixedProbabilityConnector):   
            assert self.method.p_connect > 0 and self.method.p_connect <= 1, "p_connect needs to be betwqeen 0 and 1, instead is %d" % self.method.p_connect
            self.parameters['p_connect'] = self.method.p_connect
        
        # checking if a rng has been passed. if so i will be parsed as an entry 'rng':'rng_id' into the parameters
        if self.rng != None:
            self.parameters['rng'] = 'rng_%d'   %   self.rng.id



class Assembly(object):
    """
    A group of neurons, may be heterogeneous, in contrast to a Population where
    all the neurons are of the same type.
    
    Not implemented yet
    """

    def __init__(self, populations, label=None):
        """
        Creates the assembly and adds the populations list to it
        """
        
        raise NotImplementedError("Not implemented yet!")
        
        self.label = label
        self.populations = populations
        self.id = simulator.db_run.insert_assembly(self)

        if isinstance(populations, Population): # populations can be a single element or a list
            populations = [populations,]            # if is a single element put it into a dummy list
        for p in populations:
            simulator.db_run.insert_population_into_assembly(self, p, 'POPULATION')
            

    def __add__(self, other):
        """
        An Assembly may be added to a Population, PopulationView or Assembly
        with the '+' operator, returning a new Assembly, e.g.:
        
            a2 = a1 + p
        """
        if isinstance(other, BasePopulation):
            return Assembly(*(self.populations + [other]))
        elif isinstance(other, Assembly):
            return Assembly(*(self.populations + other.populations))
        else:
            raise TypeError("can only add a Population or another Assembly to an Assembly")

    def __iadd__(self, other):
        """
        A Population, PopulationView or Assembly may be added to an existing
        Assembly using the '+=' operator, e.g.:
        
            a += p
        """
        if isinstance(other, BasePopulation):
            self._insert(other)
        elif isinstance(other, Assembly):
            for p in other.populations:
                self._insert(p)
        else:
            raise TypeError("can only add a Population or another Assembly to an Assembly")
        return self
    

class SynapseDynamics():
    def __init__(self, slow=None, fast=None):
        self.slow = slow
        self.fast = fast
        self.parameters = {}
        global simulator
        if fast!=None:
            pass        # no STP mechanism in SpiNN yet
        if slow!=None:
            if slow.timing_dependence != None:
#                print slow.timing_dependence.parameters
                self.parameters = dict(self.parameters.items() + slow.timing_dependence.parameters.items())
            if slow.weight_dependence != None:
#                print slow.weight_dependence.parameters
                self.parameters = dict(self.parameters.items() + slow.weight_dependence.parameters.items())
        self.id = simulator.db_run.insert_plasticity_instantiation(self)

#        print self.parameters


class NumpyRNG(pyNN.random.NumpyRNG):
    """Wrapper for the numpy.random.RandomState class (Mersenne Twister PRNG)."""
    
    def __init__(self, seed=None, parallel_safe=True):
        # TODO insert into rng table
        if seed==None:   seed = numpy.random.randint(10000)
        pyNN.random.NumpyRNG.__init__(self, seed, parallel_safe)        
        self.type = "NumpyRNG"
        self.id = simulator.db_run.insert_rng(self)


class RandomDistribution(pyNN.random.RandomDistribution):
    """
    Class which defines a next(n) method which returns an array of n random
    numbers from a given distribution.
    """
    def __init__(self, distribution='uniform', parameters=[], rng=None, boundaries=None, constrain="clip"):
        pyNN.random.RandomDistribution.__init__(self, distribution, parameters, rng, boundaries, constrain)
        # TODO insert into rng table
        self.distribution = distribution
        self.parameters = parameters
        self.id = simulator.db_run.insert_random_distribution(self)
        
