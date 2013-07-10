PRAGMA foreign_keys = ON;

--################# MODEL LIBRARY

PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE connector_types (id INTEGER PRIMARY KEY, name TEXT);
INSERT INTO "connector_types" VALUES(2,'AllToAllConnector');
INSERT INTO "connector_types" VALUES(3,'OneToOneConnector');
INSERT INTO "connector_types" VALUES(5,'FixedProbabilityConnector');
INSERT INTO "connector_types" VALUES(6,'FromListConnector');


CREATE TABLE processors (
    status TEXT, 
    is_eth INTEGER, 
    id INTEGER PRIMARY KEY, 
    is_monitor INTEGER, 
    p INTEGER, 
    x INTEGER, 
    y INTEGER
);
INSERT INTO "processors" VALUES('OK',1,1,1,0,0,0);
INSERT INTO "processors" VALUES('OK',1,2,0,1,0,0);
INSERT INTO "processors" VALUES('OK',1,3,0,2,0,0);
INSERT INTO "processors" VALUES('OK',1,4,0,3,0,0);
INSERT INTO "processors" VALUES('OK',1,5,0,4,0,0);
INSERT INTO "processors" VALUES('OK',1,6,0,5,0,0);
INSERT INTO "processors" VALUES('OK',1,7,0,6,0,0);
INSERT INTO "processors" VALUES('OK',1,8,0,7,0,0);
INSERT INTO "processors" VALUES('OK',1,9,0,8,0,0);
INSERT INTO "processors" VALUES('OK',1,10,0,9,0,0);
INSERT INTO "processors" VALUES('OK',1,11,0,10,0,0);
INSERT INTO "processors" VALUES('OK',1,12,0,11,0,0);
INSERT INTO "processors" VALUES('OK',1,13,0,12,0,0);
INSERT INTO "processors" VALUES('OK',1,14,0,13,0,0);
INSERT INTO "processors" VALUES('OK',1,15,0,14,0,0);
INSERT INTO "processors" VALUES('OK',1,16,0,15,0,0);
INSERT INTO "processors" VALUES('OK',1,17,0,16,0,0);
INSERT INTO "processors" VALUES('OK',1,18,0,17,0,0);
INSERT INTO "processors" VALUES('OK',0,19,1,0,0,1);
INSERT INTO "processors" VALUES('OK',0,20,0,1,0,1);
INSERT INTO "processors" VALUES('OK',0,21,0,2,0,1);
INSERT INTO "processors" VALUES('OK',0,22,0,3,0,1);
INSERT INTO "processors" VALUES('OK',0,23,0,4,0,1);
INSERT INTO "processors" VALUES('OK',0,24,0,5,0,1);
INSERT INTO "processors" VALUES('OK',0,25,0,6,0,1);
INSERT INTO "processors" VALUES('OK',0,26,0,7,0,1);
INSERT INTO "processors" VALUES('OK',0,27,0,8,0,1);
INSERT INTO "processors" VALUES('OK',0,28,0,9,0,1);
INSERT INTO "processors" VALUES('OK',0,29,0,10,0,1);
INSERT INTO "processors" VALUES('OK',0,30,0,11,0,1);
INSERT INTO "processors" VALUES('OK',0,31,0,12,0,1);
INSERT INTO "processors" VALUES('OK',0,32,0,13,0,1);
INSERT INTO "processors" VALUES('OK',0,33,0,14,0,1);
INSERT INTO "processors" VALUES('OK',0,34,0,15,0,1);
INSERT INTO "processors" VALUES('OK',0,35,0,16,0,1);
INSERT INTO "processors" VALUES('OK',1,36,0,17,0,0);
INSERT INTO "processors" VALUES('OK',0,37,1,0,1,0);
INSERT INTO "processors" VALUES('OK',0,38,0,1,1,0);
INSERT INTO "processors" VALUES('OK',0,39,0,2,1,0);
INSERT INTO "processors" VALUES('OK',0,40,0,3,1,0);
INSERT INTO "processors" VALUES('OK',0,41,0,4,1,0);
INSERT INTO "processors" VALUES('OK',0,42,0,5,1,0);
INSERT INTO "processors" VALUES('OK',0,43,0,6,1,0);
INSERT INTO "processors" VALUES('OK',0,44,0,7,1,0);
INSERT INTO "processors" VALUES('OK',0,45,0,8,1,0);
INSERT INTO "processors" VALUES('OK',0,46,0,9,1,0);
INSERT INTO "processors" VALUES('OK',0,47,0,10,1,0);
INSERT INTO "processors" VALUES('OK',0,48,0,11,1,0);
INSERT INTO "processors" VALUES('OK',0,49,0,12,1,0);
INSERT INTO "processors" VALUES('OK',0,50,0,13,1,0);
INSERT INTO "processors" VALUES('OK',0,51,0,14,1,0);
INSERT INTO "processors" VALUES('OK',0,52,0,15,1,0);
INSERT INTO "processors" VALUES('OK',0,53,0,16,1,0);
INSERT INTO "processors" VALUES('OK',1,54,0,17,0,0);
INSERT INTO "processors" VALUES('OK',0,55,1,0,1,1);
INSERT INTO "processors" VALUES('OK',0,56,0,1,1,1);
INSERT INTO "processors" VALUES('OK',0,57,0,2,1,1);
INSERT INTO "processors" VALUES('OK',0,58,0,3,1,1);
INSERT INTO "processors" VALUES('OK',0,59,0,4,1,1);
INSERT INTO "processors" VALUES('OK',0,60,0,5,1,1);
INSERT INTO "processors" VALUES('OK',0,61,0,6,1,1);
INSERT INTO "processors" VALUES('OK',0,62,0,7,1,1);
INSERT INTO "processors" VALUES('OK',0,63,0,8,1,1);
INSERT INTO "processors" VALUES('OK',0,64,0,9,1,1);
INSERT INTO "processors" VALUES('OK',0,65,0,10,1,1);
INSERT INTO "processors" VALUES('OK',0,66,0,11,1,1);
INSERT INTO "processors" VALUES('OK',0,67,0,12,1,1);
INSERT INTO "processors" VALUES('OK',0,68,0,13,1,1);
INSERT INTO "processors" VALUES('OK',0,69,0,14,1,1);
INSERT INTO "processors" VALUES('OK',0,70,0,15,1,1);
INSERT INTO "processors" VALUES('OK',0,71,0,16,1,1);
INSERT INTO "processors" VALUES('OK',1,72,0,17,0,0);
CREATE TABLE plasticity_models (id INTEGER PRIMARY KEY, model_name TEXT);
INSERT INTO "plasticity_models" VALUES(1,'AdditiveWeightDependence');
INSERT INTO "plasticity_models" VALUES(2,'SpikePairRule');
INSERT INTO "plasticity_models" VALUES(3,'FullWindow');

CREATE TABLE plasticity_parameters (
    id INTEGER PRIMARY KEY, 
    model_id INTEGER, 
    param_name TEXT, 
    FOREIGN KEY (model_id) REFERENCES plasticity_models(id)
);
INSERT INTO "plasticity_parameters" VALUES(1,1,'w_min');
INSERT INTO "plasticity_parameters" VALUES(2,1,'w_max');
INSERT INTO "plasticity_parameters" VALUES(3,1,'A_plus');
INSERT INTO "plasticity_parameters" VALUES(4,1,'A_minus');
INSERT INTO "plasticity_parameters" VALUES(5,2,'tau_plus');
INSERT INTO "plasticity_parameters" VALUES(6,2,'tau_minus');
INSERT INTO "plasticity_parameters" VALUES(7,3,'tau_plus');
INSERT INTO "plasticity_parameters" VALUES(8,3,'tau_minus');

CREATE TABLE cell_types (
    "id" INTEGER PRIMARY KEY,
    "max_neuron_per_fasc" INTEGER,
    "name" TEXT
);
INSERT INTO "cell_types" VALUES(1,512,'IF_curr_exp');
INSERT INTO "cell_types" VALUES(2,512,'IZK_curr_exp');
COMMIT;



CREATE TABLE cell_parameters (
    "position" INTEGER,
    "id" INTEGER PRIMARY KEY,    
    "model_id" INTEGER,
    "param_name" TEXT,
    "type" TEXT,
    "translation" TEXT,
    FOREIGN KEY (model_id) REFERENCES cell_types(id)
);

INSERT INTO "cell_parameters" VALUES(1,1,1,'v_rest','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(2,2,1,'tau_m','H','65535/x');
INSERT INTO "cell_parameters" VALUES(3,3,1,'tau_refrac','H','int(x)');
INSERT INTO "cell_parameters" VALUES(4,4,1,'tau_syn_E','H','int(x)');
INSERT INTO "cell_parameters" VALUES(5,5,1,'tau_syn_I','H','int(x)');
INSERT INTO "cell_parameters" VALUES(6,6,1,'v_reset','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(7,7,1,'v_rest','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(8,8,1,'v_init','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(1,9,2,'a','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(2,10,2,'b','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(3,11,2,'c','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(4,12,2,'d','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(5,13,2,'v_init','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(6,14,2,'u_init','H','int(x*p1)');
INSERT INTO "cell_parameters" VALUES(7,15,2,'tau_syn_E','H','int(x)');
INSERT INTO "cell_parameters" VALUES(8,16,2,'tau_syn_I','H','int(x)');

CREATE TABLE synapse_types (
    "id" INTEGER PRIMARY KEY,    
    "synapse_name" TEXT,
    "synapse_flag" INTEGER,
    "cell_type_id" INTEGER,
    FOREIGN KEY (cell_type_id) REFERENCES cell_types(id)
);

-- ################## NETWORK SPECS DB

PRAGMA foreign_keys = ON;

CREATE TABLE cell_instantiation (
   id INTEGER PRIMARY KEY,
   parameters TEXT
);

CREATE TABLE cell_methods (
    id INTEGER PRIMARY KEY, 
    cell_instantiation_id INTEGER, 
    method_type INTEGER,
    method_name TEXT,
--    FOREIGN KEY (method_type) REFERENCES cell_types(id), 
    FOREIGN KEY (cell_instantiation_id) REFERENCES cell_instantiation(id)
);



CREATE TABLE populations (
   label TEXT, 
   cell_instantiation_id INTEGER, 
   id INTEGER PRIMARY KEY, 
   size INTEGER,
   constraints TEXT,
   splitter_constraint INTEGER DEFAULT 0,
   estimate_firing_rate INTEGER DEFAULT 0,
   FOREIGN KEY (cell_instantiation_id) REFERENCES cell_instantiation(id)
);
CREATE UNIQUE INDEX idx_populations_cells ON populations (id, cell_instantiation_id);


CREATE TABLE plasticity_instantiation (
    id INTEGER PRIMARY KEY, 
    parameters TEXT
);

CREATE TABLE projections (
    presynaptic_population_id INTEGER, 
    size INTEGER, 
    method INTEGER, 
    postsynaptic_population_id INTEGER, 
    id INTEGER PRIMARY KEY, 
    label TEXT, 
    parameters TEXT, 
    plasticity_instantiation_id INTEGER DEFAULT 0,  -- if no plasticity specified it will be set to 0 
--    source TEXT, 
    target TEXT, 
--    FOREIGN KEY (method) REFERENCES connector_types(id),     -- refer to model library
--    FOREIGN KEY (plasticity_instantiation_id) REFERENCES plasticity_instantiation(id),
    FOREIGN KEY (presynaptic_population_id) REFERENCES populations(id), 
    FOREIGN KEY (postsynaptic_population_id) REFERENCES populations(id)

);


CREATE TABLE currents (
   id INTEGER PRIMARY KEY, 
   parameters TEXT, 
   population_id INTEGER, 
   start_id INTEGER,
   end_id INTEGER,
   FOREIGN KEY (population_id) REFERENCES populations(id)
);


CREATE TABLE probes (
    id INTEGER PRIMARY KEY, 
    population_id INTEGER, 
    variable TEXT, 
    save_to TEXT,
    FOREIGN KEY (population_id) REFERENCES populations(id) 
);

--CREATE TABLE population_view (
--    cell_ids TEXT, 
--    id INTEGER PRIMARY KEY, 
--    label TEXT, 
--    population_id INTEGER, 
--    size INTEGER,
--    FOREIGN KEY (population_id) REFERENCES populations(id)
--);

CREATE TABLE assemblies (
    assembly_id INTEGER PRIMARY KEY, 
    assembly_label TEXT
);

CREATE TABLE assembly_associations (
    id INTEGER PRIMARY KEY,
    assembly_id INTEGER, 
    member_id INTEGER,
    type TEXT,
    FOREIGN KEY (assembly_id) REFERENCES assemblies(assembly_id)
);


CREATE TABLE plasticity_methods (
    id INTEGER PRIMARY KEY, 
    plasticity_instantiation_id INTEGER, 
    method_type INTEGER,
    method_name TEXT,
--    FOREIGN KEY (method_type) REFERENCES plasticity_models(id),      -- MODEL LIBRARY FAKE FK
    FOREIGN KEY (plasticity_instantiation_id) REFERENCES plasticity_instantiation(id)
);

CREATE TABLE part_populations (
    id INTEGER PRIMARY KEY,
    population_id INTEGER,
    size INTEGER,
    offset INTEGER,
    processor_group_id INTEGER,
    start_id INTEGER, 
    end_id INTEGER,
    mask TEXT,
    lookup_mask TEXT,    
    population_order_id INTEGER,
    population_core_offset,
    flags INTEGER DEFAULT 0,    
    FOREIGN KEY (population_id) REFERENCES populations(id)
);

CREATE TABLE part_projections (
    id INTEGER PRIMARY KEY,
    projection_id INTEGER,  
    presynaptic_part_population_id INTEGER, 
    postsynaptic_part_population_id INTEGER, 
    size INTEGER, 
    method TEXT, 
    parameters TEXT, 
    plasticity_instantiation_id INTEGER, 
--    source TEXT, 
    target TEXT,
--    label TEXT, 
    sdram_address TEXT,  
    synaptic_row_length INTEGER,  
    FOREIGN KEY (projection_id) REFERENCES projections(id),     
    FOREIGN KEY (presynaptic_part_population_id) REFERENCES part_populations(id), 
    FOREIGN KEY (postsynaptic_part_population_id) REFERENCES part_populations(id),
    FOREIGN KEY (plasticity_instantiation_id) REFERENCES plasticity_instantiation(id)
);

CREATE TABLE map (
   id INTEGER PRIMARY KEY, 
   processor_id INTEGER, 
   processor_group_id INTEGER, 
--   FOREIGN KEY (processor_id) REFERENCES processors(id), 
   FOREIGN KEY (processor_group_id) REFERENCES part_populations(processor_group_id)
);

CREATE TABLE part_currents (
   id INTEGER PRIMARY KEY, 
   parameters TEXT,
   current_id INTEGER,
   part_population_id INTEGER, 
   start_id INTEGER,
   end_id INTEGER,
   FOREIGN KEY (part_population_id) REFERENCES part_populations(id),
   FOREIGN KEY (current_id) REFERENCES currents(id)
);


CREATE TABLE part_probes (
    id INTEGER PRIMARY KEY,     
    part_population_id INTEGER, 
    probe_id INTEGER,
    variable TEXT, 
    save_to TEXT,
    FOREIGN KEY (part_population_id) REFERENCES part_populations(id),
    FOREIGN KEY (probe_id) REFERENCES probes(id)      
);

-- Random number generation
CREATE TABLE rng (
   id INTEGER PRIMARY KEY, 
   type TEXT, 
   seed INTEGER
);

CREATE TABLE random_distribution (
   id INTEGER PRIMARY KEY, 
   label TEXT, 
   distribution TEXT,
   parameters TEXT, 
   rng_id INTEGER,   
   FOREIGN KEY (rng_id) REFERENCES rng(id)
);

CREATE TABLE "options" (
    "id" INTEGER PRIMARY KEY AUTOINCREMENT,
    "name" TEXT,
    "value" TEXT
);
INSERT INTO "options" VALUES(1,'runtime','0');

