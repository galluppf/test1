"""
DAO:        Database Access Object: a model of the DB 
            This file contains all the functions needed to read/write data from the DB. 
            Queries are defined in sql/queries.cfg

.. moduleauthor:: Francesco Galluppi, SpiNNaker Project, University of Manchester. francesco.galluppi@cs.man.ac.uk
"""

import sqlite3
import os, sys
import ConfigParser
import pacman 

DEBUG = False

class db():
    def __init__(self,filename='net.db'):
        self.mydir = os.path.dirname(__file__)
        self.db_abs_path = os.path.abspath(filename)
        self.filename = filename
        
        # Reading querys
        if DEBUG:   print '[ dao ] : Reading configuration file %s'   % os.path.join(self.mydir,'sql/queries.cfg')
        self.queries = ConfigParser.ConfigParser()                              # Queries will be defined in an external file
        self.queries.readfp(open(os.path.join(self.mydir,'sql/queries.cfg')))   # Reads the query configuration file
        
        if DEBUG:   print '[ dao ] : opening DB %s'   % filename
        
        self.conn = sqlite3.connect(filename
#            , isolation_level=None     # isolation_level=None sets autocommit for transactions. not enabled by default
            , check_same_thread = False
            )    
        self.conn.execute("PRAGMA journal_mode = OFF;")         
        self.conn.row_factory = sqlite3.Row
            

    def close_connection(self, commit=True):
        """
        This function will commit the transaction and close the connection to the DB
        """
        if commit:  self.conn.commit()    
        self.conn.close()

    def commit(self):
        """
        This function will commit the transaction
        """
        self.conn.commit()    

    
    def init_db(self):
        """
        init_db initializes the DB by performing the following actions:
            - if the DB is already created it will clean it (TBD)
            - tables are created from the create_specs_db script
            - configuration db is imported
        """
        self.import_config_db()

        try:
            self.execute_from_file(os.path.join(self.mydir, self.queries.get('sql_scripts', 'create_specs_db')))
        except sqlite3.OperationalError:
            print "Unexpected error:", sys.exc_info()
        
        

    def move_db(self, destination):
#        print "loading db %s into %s" % (source, destination)
#        source_db = sqlite3.connect(source)
        source_db = self.conn
        dest_db = sqlite3.connect(destination) # create a memory database    
        query = "".join(line for line in source_db.iterdump())
        # Dump old database in the new one. 
        dest_db.executescript(query)
        self.conn = dest_db
        self.conn.row_factory = sqlite3.Row
        self.import_config_db()



    def import_config_db(self):
        """
        Attaches the configuration db to the current schema (model + system libraries). System library can be defined in the pacman.cfg [board] section
        """
        query = eval(self.queries.get('db_mgmt', 'import_model_library'))
        if DEBUG:   print '[ dao ] : importing model library: ', query
        self.get_cursor().execute(query)
        
        system_library = pacman.return_configuration().get('board', 'system_library')
        query = eval(self.queries.get('db_mgmt', 'import_system_library'))
        if DEBUG:   print '[ dao ] : importing model library: ', query
        self.get_cursor().execute(query)
        
        

    def clean_db(self):
        """ 
        Deletes entries in the network db
        """
        self.execute_from_file(os.path.join(self.mydir, self.queries.get('sql_scripts', 'clean_net_db')))
        
    def clean_part_db(self):
        self.execute_from_file(os.path.join(self.mydir, self.queries.get('sql_scripts', 'clean_part_db')))
    
        
    def execute_from_file(self, filename):
        """
        Executes an SQL script contained in a file
        """
        f = open(filename)
        cfg = f.read()
        out = self.get_cursor().executescript(cfg)
        f.close()
        self.get_cursor().close()
        return out

    def get_cursor(self):
        """
        Returns the cursor for the DB
        """
        return self.conn.cursor()

    def get_last_inserted_row_id(self):
        """
        Returns the id (PRIMARY KEY if defined) of the last inserted row
        """
        last_id = self.get_cursor().execute(eval(self.queries.get('insert', 'last_inserted_row_id')))
        last_id = last_id.fetchone()[0]
        return(last_id)

    def insert_population(self, pop):
        """
        Inserts a Population in the network DB
        """
        # step 1 - inserts an entry with parameters in the cell_instantiation table        
        query = eval(self.queries.get('insert', 'insert_cell_instantiation', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        cell_instantiation_id = self.get_last_inserted_row_id()

        # step 2 - inserts the 'neural_model' entry in the cell_methods
        # PyNN will only have 'neural_model' as a method_name. this is done for compatibility with LENS
        method_name = 'neural_model'
        query = eval(self.queries.get('insert', 'insert_cell_method', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        cell_instantiation_id = self.get_last_inserted_row_id()

        # step 3 - inserts the population in the populations table
        query = eval(self.queries.get('insert', 'insert_population', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def set_population_parameters(self, parameters, id):
        query = eval(self.queries.get('update', 'update_population_parameters', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())        


    def generic_update(self, table_name, set_clause, where_condition):
        """
        table_name is the name of the table
        set_clause is in the format column1=value, column2=value2,
        where_condition is in the format some_column=some_value
        """
        query = "UPDATE %s SET %s WHERE %s;" % (table_name, set_clause, where_condition)
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())        



    def insert_projection(self, proj):
#        print proj.presynaptic_population.id, proj.postsynaptic_population.id, proj.method.__name__, proj.parameters, proj.size, proj.target, proj.label

        if proj.synapse_dynamics == None:        
            query = eval(self.queries.get('insert', 'insert_projection_no_plasticity', 1))
        else:
            query = eval(self.queries.get('insert', 'insert_projection', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())        


    def insert_probe(self, pop_id, variable, save_to):
        query = eval(self.queries.get('insert', 'insert_probe', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)

    def insert_assembly(self, assembly):
        query = eval(self.queries.get('insert', 'insert_assembly', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def insert_population_into_assembly(self, assembly, member, member_type):
        query = eval(self.queries.get('insert', 'insert_population_into_assembly', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)

    def insert_plasticity_instantiation(self, plasticity_instantiation):
        # step 1 - inserts an entry with parameters in the cell_instantiation table        
        query = eval(self.queries.get('insert', 'insert_plasticity_instantiation', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        plasticity_instantiation_id = self.get_last_inserted_row_id()

        # step 2 - inserts the methods in the plasticity_methods table
        # PyNN will only have 'neural_model' as a method_name. this is done for compatibility with LENS
        if plasticity_instantiation.fast!=None:
            pass        # no STP mechanism in SpiNN yet
        if plasticity_instantiation.slow!=None:
            if plasticity_instantiation.slow.timing_dependence != None:
                method_name = 'timing_dependence'
                method_type = plasticity_instantiation.slow.timing_dependence.__name__
                query = eval(self.queries.get('insert', 'insert_plasticity_method', 1))
                if DEBUG:   print query
                self.get_cursor().execute(query)


            if plasticity_instantiation.slow.weight_dependence != None:
                method_name = 'weight_dependence'
                method_type = plasticity_instantiation.slow.weight_dependence.__name__
                query = eval(self.queries.get('insert', 'insert_plasticity_method', 1))
                if DEBUG:   print query
                self.get_cursor().execute(query)


        return(plasticity_instantiation_id)
        
    def insert_rng(self, rng):
        query = eval(self.queries.get('insert', 'insert_rng', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def insert_random_distribution(self, distr):
        query = eval(self.queries.get('insert', 'insert_random_distribution', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())
 
# PARTITIONER INSERT
    def insert_part_population(self, population_id, remaining_neurons, offset):
        query = eval(self.queries.get('insert', 'insert_part_population', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def insert_part_projection(self, presynaptic_part_population_id, postsynaptic_part_population_id, projection):
        query = eval(self.queries.get('insert', 'insert_part_projection', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def insert_part_probe(self, part_population_id, probe):
        query = eval(self.queries.get('insert', 'insert_part_probe', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())
        

# GROUPING and MAPPING
    def update_part_popoulation_group(self, part_population_id, group, position_in_group):
        query = eval(self.queries.get('update', 'update_part_popoulation_group', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())
        
    def set_mapping_constraint(self, population_id, constraint):
        query = eval(self.queries.get('update', 'set_mapping_constraint', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def set_population_splitting_constraint(self, population, n):
        """
        Function to set the maximum numbers of neuron per core for a particular population
         - population: a Population object
         - n: the maximum number of neurons per core for that particular population
        """
        query = eval(self.queries.get('update', 'set_population_splitting_constraint', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())
    

    def set_number_of_neurons_per_core(self, neuron_name, number_of_neurons):
            query = eval(self.queries.get('update', 'update_number_of_neurons_per_core', 1))
            if DEBUG:   print query
            self.get_cursor().execute(query)
            return(self.get_last_inserted_row_id())        
        

        
    def get_processors(self): 
        #select id from processors where is_monitor==0 and STATUS = 'OK'
        available_processors = self.generic_select('id, x, y, p', 'processors where is_monitor==0 and STATUS = \"OK\"')    
        return ([{'id':c['id'],'x':c['x'],'y':c['y'],'p':c['p']} for c in available_processors])    # returns a list (list comprehension)

    def get_used_processors(self): 
        temp = self.generic_select('processor_id', 'map')    # will return the count as an integer
        return ([c['processor_id'] for c in temp])    # returns a list (list comprehension)
        
    def get_groups(self): 
        temp = self.generic_select('distinct(processor_group_id), constraints', 'part_populations part, populations pop WHERE part.population_id=pop.id ORDER BY processor_group_id')    # will return the count as an integer
        return ([{'group_id':c['processor_group_id'], 'constrain':c['constraints']} for c in temp])    # returns a list (list comprehension)

    def insert_group_into_map(self, processor_id, processor_group_id):
        query = eval(self.queries.get('insert', 'insert_group_into_map', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())        
        
    def get_image_map(self, x='x NOT NULL', y='y NOT NULL'):
        if x != "x NOT NULL": x = "x = %d" % x
        if y != "y NOT NULL": y = "y = %d" % y
        where_condition = "WHERE %s AND %s" % (x, y)
        query = eval(self.queries.get('select', 'get_image_map',1))
        if DEBUG:   print query        
        return(self.execute_and_parse_select(query))
        
    def get_grouping_rules(self):
        """
        Grouping query #1 : get the constraints for part_population grouping
        """
        query = eval(self.queries.get('grouper', 'get_grouping_rules', 1))
        if DEBUG:   print query
        return(self.execute_and_parse_select(query))

    def get_part_populations_by_rules(self, rules):
        """
        Grouping query #2 : get the part populations for a given set of constraints
        """
        for r in rules:            
            if rules[r] == None:       rules[r] = 'null'                
            elif isinstance(rules[r],int):  pass
            else:   rules[r] = "\"%s\"" % rules[r]
                
        query = eval(self.queries.get('grouper', 'get_part_populations_by_rules', 1))
        if DEBUG:   print query
        return(self.execute_and_parse_select(query))


        
### DB MGMT        
    def load_specs_db(self, path_to_db):
        self.conn = sqlite3.connect(path_to_db, isolation_level=None)
 
 
 
 
### SELECTS        

    def execute_and_parse_select(self, query):          # will execute the select and return a list of dictionaries        
        if DEBUG:   print query
        cursor = self.get_cursor().execute(query)
        out = []
        for row in cursor:
            temp_dict = {}
            for i in range(len(row)):
                temp_dict[row.keys()[i]] = row[i]
            out.append(temp_dict)
#        values = c.fetchall()
        return out

    def generic_select(self, fields, from_where):
        query = eval(self.queries.get('select', 'generic_select', 1))
        return(self.execute_and_parse_select(query))

    def get_population_parameters(self, population_id):
        return(self.generic_select('parameters', 'populations p, cell_instantiation i WHERE p.id = %d and p.cell_instantiation_id = i.id' % population_id)[0]['parameters'])    

    def get_population_label(self, population_id):
        return(self.generic_select('label', 'populations p WHERE p.id = %d' % population_id)[0]['label'])    



    def get_part_populations(self, population_id=None, part_population_id=None):
        if (population_id != None) and (part_population_id != None): 
            print "you can only specify one key"
            quit(2)
        elif population_id != None:
            query = eval(self.queries.get('select', 'get_part_populations_from_population_id', 1))
        elif part_population_id != None:
            query = eval(self.queries.get('select', 'get_part_populations_from_part_population_id', 1))
        else:
            query = eval(self.queries.get('select', 'get_part_populations', 1))
        return(self.execute_and_parse_select(query))

    def get_num_part_populations(self):
        return(self.generic_select('count(*) as c', 'part_populations')[0]['c'])    # will return the count as an integer


    def get_part_projection(self, id=None):
        if id==None:
            out = self.generic_select('*', 'part_projections')
        else:
            out = self.generic_select('*', 'part_projections WHERE id = %d' % id)
        return(out)

    def get_num_part_projections(self):
        return(self.generic_select('count(*) as c', 'part_projections')[0]['c'])    # will return the count as an integer

    def get_probes(self):
        return(self.generic_select('*', 'probes'))

    def get_probe_map(self):
        """
        returns the (x,y) coordinates of all the chips that need an application monitoring for probing
        """
        query = eval(self.queries.get('select', 'get_probe_map',1))
        if DEBUG:   print query        
        resultset = self.execute_and_parse_select(query)     # will only get the first hit
        
        return(resultset)


    def get_connector_type(self, projection_id, part_projection=True):      # by default it will retrieve the part_projection
        if part_projection == True:     table_name = 'part_projections'
        else:                           table_name = 'projections'
        query = eval(self.queries.get('select', 'get_connector_type' ,1))
        if DEBUG:   print query
        return(self.execute_and_parse_select(query)[0]['name']) # will return the 1st connector name as a astring

    def get_synaptic_flag(self, projection_id):
        query = eval(self.queries.get('select', 'get_synaptic_flag', 1))
        return(self.execute_and_parse_select(query)[0]['synapse_flag']) # will return the synaptic flag


    def get_plasticity_parameters(self):
        query = eval(self.queries.get('select', 'get_plasticity_parameters', 1))
        return(self.execute_and_parse_select(query)) # will return a list of plasticity instantiations and how they are mapped

                
    def get_distinct_cell_method_types(self):
        query = eval(self.queries.get('select', 'get_distinct_cell_method_types', 1))
        return(self.execute_and_parse_select(query))
        
    def get_populations_size_type_method(self):
        query = eval(self.queries.get('select', 'get_populations_size_type_method', 1))
        return(self.execute_and_parse_select(query))

    def get_presynaptic_populations(self, postsynaptic_population_id):
        query = eval(self.queries.get('select', 'get_presynaptic_populations', 1))
        return(self.execute_and_parse_select(query))

    def get_postsynaptic_populations(self, presynaptic_population_id):
        query = eval(self.queries.get('select', 'get_postsynaptic_populations', 1))
        return(self.execute_and_parse_select(query))
        
    def get_part_population_size(self, part_population_id):
        return(self.generic_select('size', 'part_populations where id=%d' % part_population_id)[0]['size'])    # will return the count as an integer
            
    def get_cell_parameters(self, model_id):
        return(self.generic_select('*', 'cell_parameters where model_id=%d' % model_id))    # will return the count as an integer

    def get_part_populations_size_type_by_method(self, method_id):
        query = eval(self.queries.get('select', 'get_part_populations_size_type_by_method', 1))
        return(self.execute_and_parse_select(query))
        
    def get_part_populations_size_type_method(self, part_population_id):
        query = eval(self.queries.get('select', 'get_part_populations_size_type_method', 1))
        if DEBUG:   print query
        return(self.execute_and_parse_select(query))        

    def get_part_populations_from_processor(self, processor_id):
        query = eval(self.queries.get('select', 'get_part_populations_from_processor', 1))
        return(self.execute_and_parse_select(query))        
        

    def get_distinct_part_plasticity_instantiation(self, postsynaptic_part_population_id):
        out = []
        res = self.generic_select('DISTINCT plasticity_instantiation_id', 'part_projections where postsynaptic_part_population_id=%d AND plasticity_instantiation_id!=0' % postsynaptic_part_population_id)
        for r in res:
            out.append(r['plasticity_instantiation_id'])
        return(out)   
        
    def get_barrier_processors(self, x, y):
        query = eval(self.queries.get('select', 'get_barrier_processors', 1))
        return(self.execute_and_parse_select(query))        
        
### SPECIAL functions
    def get_absolute_id_list(self, part_population_id):
        part_population = self.get_part_populations(part_population_id=part_population_id)[0]  # will just get 1 row        
        return(range(part_population['offset'], part_population['offset']+part_population['size']))

    def get_part_population_loc(self, part_population_id):
        query = eval(self.queries.get('select', 'get_part_population_loc',1))
        return(self.execute_and_parse_select(query)[0])         # [0] - will just get 1 row     

    def get_relative_id(self, population_id, id):
        out = {}        # -1 will be returned if no match found
        part_populations = self.get_part_populations(population_id=population_id)
        for p in part_populations:
            if id >= p['offset'] and id < (p['offset']+p['size']):
                if DEBUG:   print "found neuron %d as neuron %d in part_population %d" % (id, id - p['offset'], p['id'])
                out['relative_id'] = id - p['offset']
                out['part_population_id'] = p['id']
        return(out)
        
    def get_synaptic_plasticity(self, part_projection_id):
        out = self.generic_select('plasticity_instantiation_id', 'part_projections where id=%d' % part_projection_id)[0]['plasticity_instantiation_id']
        if out == 0:
            return 0
        else:
            return 1

    def set_part_population_core_offset(self, group_id):
        query = eval(self.queries.get('select', 'get_part_population_core_offset',1))
        if DEBUG:   print query        
        for part_populations_core_offsets in self.execute_and_parse_select(query):
            if part_populations_core_offsets['population_core_offset']==None:   # if it's None set it to 0 (maybe DEFAULT value of 0 a more elegant solution...)
                part_populations_core_offsets['population_core_offset'] = 0
            query = eval(self.queries.get('update', 'update_core_offsets',1))            
            if DEBUG:   print query
            self.get_cursor().execute(query)            
        return(0)

    def get_flag_position(self, flag_name):
        return(self.generic_select('position', 'model_library.flags where flag_name="%s"' % flag_name)[0]['position'])    # will return the position  of the flag corresponding to flag_name

        
    def set_flag(self, part_population_id, flag_value):
        query = eval(self.queries.get('update', 'update_flag', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())        

### core configuration and neural data structure

    def set_runtime(self, runtime):  # TODO : STUB
        query = eval(self.queries.get('update', 'update_runtime', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())


    def get_runtime(self):  # TODO : STUB
        return(self.generic_select('value', 'options where name="runtime"')[0]['value']) 

    def get_synaptic_row_length(self, processor_id):
        query = eval(self.queries.get('select', 'get_synaptic_row_length',1))
        return(self.execute_and_parse_select(query)[0]['get_synaptic_row_length'])         # will return get_synaptic_row_length as an integer
    
    def get_max_delay(self):  # TODO : STUB
        return(16)


    def get_synaptic_translation(self, x, y, p):
        query = eval(self.queries.get('select', 'get_synaptic_translation',1))
        result = self.execute_and_parse_select(query)
        out = []
        for r in result:
            out.append(eval(r['synapse_translation']))
        return(out)         


### routing updates

    def set_start_id(self, part_population_id, population_key_start):
        query = eval(self.queries.get('update', 'update_part_population_start_id', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())
        
    def set_end_id(self, part_population_id, population_key_end):
        query = eval(self.queries.get('update', 'update_part_population_end_id', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())
        
    def set_mask(self, part_population_id, mask):
        query = eval(self.queries.get('update', 'update_part_population_mask', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def set_synaptic_row_length(self, part_projection_id, synaptic_row_length):
        query = eval(self.queries.get('update', 'update_synaptic_row_length', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def set_projection_address(self, part_projection_id, memory_address):
        query = eval(self.queries.get('update', 'update_projection_address', 1))
        if DEBUG:  print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())

    def get_parameters_translations(self, method_id):
        return(self.generic_select('*', 'cell_parameters where model_id=%d order by position' % method_id)) 

    def get_synaptic_row_length(self, processor_id):
        query = eval(self.queries.get('select', 'get_synaptic_row_length',1))
        if DEBUG:   print query
        out = self.execute_and_parse_select(query)[0]['synaptic_row_length'] # [0] - will just get the synaptic_row_length as an int
        if out == None: out = 0 # 0 if populations on chip only send spikes
        return(out)         


    def update_parameters_part_projection(self, part_projection_id,parameters):
        query = eval(self.queries.get('update', 'update_parameters_part_projection', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)
        return(self.get_last_inserted_row_id())
        
    def get_processor_connections_map(self):
        query = eval(self.queries.get('select', 'get_processor_connections_map',1))
        if DEBUG:   print query        
        return(self.execute_and_parse_select(query))

            
        
### random distribution
    def get_random_number_generators(self):
        return(self.generic_select('*', 'rng'))    # will return the count as an integer

    def get_random_distributions(self):
        return(self.generic_select('*', 'random_distribution'))    # will return the count as an integer
    
    
### utilities
    def translate_routing_key(self, key):
        """
        translate_routing_key translates a routing key back to its neural model coordinates
        
        The routing key is in the format |x (8 bit) | y (8 bit) | p (5 bit) | neuron_id (11 bit) | 
        
        The neuron_id is in the format | population_id | neuron_id |, the mask field in the database determines the size of the population_id field
        
        The function returns a dictionary containing the following entries:
        {'population_id'} : the absolute id of the population containing the neuron
        {'neuron_id'} : the relative id of the neuron in the population
        {'population_label'}
        {'population_size'}
        """
                    
        # Extracting coordinates from routing key
        x = (key >> 24) & 0xFF        
        y = (key >> 16) & 0xFF
        p = (key >> 11) & 0x1F
        neuron_id = key & 0x7FF
        
        where_condition = 'WHERE x = %d AND y = %d and p = %d AND %d >= start_id AND %d <= end_id' % (x,y,p, neuron_id, neuron_id)        # this will be read by the get_map query
        
        query = eval(self.queries.get('select', 'get_map',1))
        if DEBUG:   print query        
        mapped_id = self.execute_and_parse_select(query)[0]     # will only get the first hit
        
        relative_neuron_id = neuron_id - mapped_id['start_id'] + mapped_id['offset']
        
#        print "population_id:", mapped_id['population_id'], "neuron_id:", neuron_id - mapped_id['start_id'] + mapped_id['offset']
        return({'population_id':mapped_id['population_id'], 'population_label':mapped_id['label'], 'population_size':mapped_id['size'],'neuron_id': relative_neuron_id})


    def get_routing_key_map(self):
        """        
            get_routing_key_map translates a routing key back to its neural model coordinates
            
            The routing key is in the format |x (8 bit) | y (8 bit) | p (5 bit) | neuron_id (11 bit) | 
            
            The neuron_id is in the format | population_id | neuron_id |, the mask field in the database determines the size of the population_id field
            
            The function returns a dictionary containing the following entries:
            {'population_id'} : the absolute id of the population containing the neuron        
            {'population_label'}
            {'population_size'}
            {'offset'}: offset to apply to the relative neuron_id in the part_population to get its absolute id in the population        
            {'start_id'}: starting id for the part_population in the core, to be subtracted to the neuron_id in the core to get the relative neuron_id in the part_population
            
            relative_neuron_id = neuron_id - resultset['start_id'] + resultset['offset']
            
        """
        
        where_condition = ""
        
        query = eval(self.queries.get('select', 'get_map',1))
        if DEBUG:   print query        
        resultset = self.execute_and_parse_select(query)     # will only get the first hit
        
        return(resultset)
            

#### APP MONITORING

    def insert_monitoring_part_projection(self, projection_id, pre_part_population_id, monitoring_part_pop_id):
        query = eval(self.queries.get('application_monitoring', 'insert_monitoring_part_projection', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)


    def insert_monitoring_projection(self, presynaptic_population_id, monitoring_population_id):
        query = eval(self.queries.get('application_monitoring', 'insert_monitoring_projection', 1))
        if DEBUG:   print query
        self.get_cursor().execute(query)



