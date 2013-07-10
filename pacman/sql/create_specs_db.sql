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

