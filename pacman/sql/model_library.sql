PRAGMA foreign_keys=OFF;
BEGIN TRANSACTION;
CREATE TABLE connector_types (id INTEGER PRIMARY KEY, name TEXT UNIQUE);
INSERT INTO "connector_types" VALUES(1,'AllToAllConnector');
INSERT INTO "connector_types" VALUES(2,'OneToOneConnector');
INSERT INTO "connector_types" VALUES(3,'FixedProbabilityConnector');
INSERT INTO "connector_types" VALUES(4,'FromListConnector');
CREATE TABLE plasticity_models (id INTEGER PRIMARY KEY, model_name TEXT);
INSERT INTO "plasticity_models" VALUES(1,'AdditiveWeightDependence');
INSERT INTO "plasticity_models" VALUES(2,'SpikePairRule');
INSERT INTO "plasticity_models" VALUES(3,'FullWindow');
CREATE TABLE plasticity_parameters (id INTEGER PRIMARY KEY, model_id INTEGER, param_name TEXT);
INSERT INTO "plasticity_parameters" VALUES(1,1,'w_min');
INSERT INTO "plasticity_parameters" VALUES(2,1,'w_max');
INSERT INTO "plasticity_parameters" VALUES(3,1,'A_plus');
INSERT INTO "plasticity_parameters" VALUES(4,1,'A_minus');
INSERT INTO "plasticity_parameters" VALUES(5,2,'tau_plus');
INSERT INTO "plasticity_parameters" VALUES(6,2,'tau_minus');
INSERT INTO "plasticity_parameters" VALUES(7,3,'tau_plus');
INSERT INTO "plasticity_parameters" VALUES(8,3,'tau_minus');
CREATE TABLE cell_parameters (
    "position" INTEGER,
    id INTEGER PRIMARY KEY,
    "model_id" INTEGER,
    "param_name" TEXT,
    "type" TEXT,
    "translation" TEXT
);
INSERT INTO "cell_parameters" VALUES(1,1,1,'v_init','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(2,2,1,'v_rest','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(3,3,1,'v_reset','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(4,4,1,'v_thresh','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(5,5,1,'tau_m','i','int(p2/value)');
INSERT INTO "cell_parameters" VALUES(6,6,1,'cm','i','int(params[''tau_m''][i]/params[''cm''][i]*p2)');
INSERT INTO "cell_parameters" VALUES(7,7,1,'tau_refrac','i','int(value)');
INSERT INTO "cell_parameters" VALUES(8,8,1,'tau_refrac_clock','i','0');
INSERT INTO "cell_parameters" VALUES(3,9,2,'a','h','int(params[''a''][i]*params[''b''][i]*p2)');
INSERT INTO "cell_parameters" VALUES(4,10,2,'b','h','int(-params[''a''][i]*p2)');
INSERT INTO "cell_parameters" VALUES(5,11,2,'c','h','int(params[''c''][i]*p1)');
INSERT INTO "cell_parameters" VALUES(6,12,2,'d','h','int(params[''d''][i]*p1)');
INSERT INTO "cell_parameters" VALUES(1,13,2,'v_init','i','int(value*p1)');
INSERT INTO "cell_parameters" VALUES(2,14,2,'u_init','i','int(value*p1)');
INSERT INTO "cell_parameters" VALUES(7,15,2,'tau_syn_E','H','int(p2/params[''tau_syn_E''][i])');
INSERT INTO "cell_parameters" VALUES(8,16,2,'tau_syn_I','H','int(p2/params[''tau_syn_I''][i])');
INSERT INTO "cell_parameters" VALUES(9,17,2,'i_offset','i','int(params[''i_offset''][i]*p1)');
INSERT INTO "cell_parameters" VALUES(9,18,1,'i_offset','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(10,19,1,'tau_syn_E','i','int(p2/value)');
INSERT INTO "cell_parameters" VALUES(11,20,1,'tau_syn_I','i','int(p2/value)');
INSERT INTO "cell_parameters" VALUES(1,27,3,'decoder_0','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(1,28,5,'v_init','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(2,29,5,'v_rest','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(3,30,5,'tau_m','H','int(p2/params[''tau_m''][i])');
INSERT INTO "cell_parameters" VALUES(4,31,5,'resistance','H','1');
INSERT INTO "cell_parameters" VALUES(5,32,5,'v_reset','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(6,33,5,'v_thresh','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(7,34,5,'tau_syn_E','H','int(p2/params[''tau_syn_E''][i])');
INSERT INTO "cell_parameters" VALUES(8,35,5,'tau_syn_I','H','int(p2/params[''tau_syn_I''][i])');
INSERT INTO "cell_parameters" VALUES(9,36,5,'i_offset','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(10,37,5,'tau_refrac','H','int(params[''tau_refrac''][i])');
INSERT INTO "cell_parameters" VALUES(11,38,5,'tau_refrac_clock','H','0');
INSERT INTO "cell_parameters" VALUES(1,39,6,'packets','I','int(params[''packets''][i])');
INSERT INTO "cell_parameters" VALUES(2,40,6,'time_per_neuron','I','int(params[''time_per_neuron''][i])');
INSERT INTO "cell_parameters" VALUES(3,41,6,'lookup_instructions','I','int(params[''lookup_instructions''][i])');
INSERT INTO "cell_parameters" VALUES(4,42,6,'dma_done_instructions','I','int(params[''dma_done_instructions''][i])');
INSERT INTO "cell_parameters" VALUES(1,43,97,'v_init','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(2,44,97,'encoder_0','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(3,45,97,'i_offset','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(4,46,97,'value_current','i','0');
INSERT INTO "cell_parameters" VALUES(5,47,97,'tau_refrac_clock','I','0');
INSERT INTO "cell_parameters" VALUES(1,48,4,'v_init','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(2,49,4,'v_rest','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(3,50,4,'v_reset','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(4,51,4,'v_thresh','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(5,52,4,'tau_m','i','int(p2/value)');
INSERT INTO "cell_parameters" VALUES(6,53,4,'cm','i','int(params[''tau_m''][i]/params[''cm''][i]*p2)');
INSERT INTO "cell_parameters" VALUES(7,54,4,'tau_refrac','i','int(value)');
INSERT INTO "cell_parameters" VALUES(8,55,4,'tau_refrac_clock','i','0');
INSERT INTO "cell_parameters" VALUES(9,56,4,'i_offset','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(10,57,4,'tau_syn_E','i','int(p2/value)');
INSERT INTO "cell_parameters" VALUES(11,58,4,'tau_syn_I','i','int(p2/value)');
INSERT INTO "cell_parameters" VALUES(12,59,4,'e_rev_E','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(13,60,4,'e_rev_I','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(1,61,8,'decoder_0','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(1,62,7,'v_init','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(2,63,7,'encoder_0','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(4,64,7,'i_offset','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(5,65,7,'value_current','i','0');
INSERT INTO "cell_parameters" VALUES(6,66,7,'tau_refrac_clock','I','0');
INSERT INTO "cell_parameters" VALUES(3,67,7,'encoder_1','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(2,68,8,'decoder_1','i','int(value*p2)');
INSERT INTO "cell_parameters" VALUES(1,69,9,'x_source','I','int(value)');
INSERT INTO "cell_parameters" VALUES(2,70,9,'y_source','I','int(value)');
INSERT INTO "cell_parameters" VALUES(1,86,100,'rate','Q','int(math.exp(-1000/params[''rate''][i])*math.pow(2,64))');
INSERT INTO "cell_parameters" VALUES(2,87,100,'start','I','int(params[''start''][i])');
INSERT INTO "cell_parameters" VALUES(3,88,100,'duration','I','int(params[''start''][i] + params[''duration''][i])');
INSERT INTO "cell_parameters" VALUES(4,89,100,'time_to_next_spike','i','poisson.rvs(1000/params[''rate''][i])');
CREATE TABLE "flags" (
    "id" INTEGER PRIMARY KEY AUTOINCREMENT,
    "flag_name" TEXT,
    "position" INTEGER
);
INSERT INTO "flags" VALUES(1,'spikes_SDRAM',0);
INSERT INTO "flags" VALUES(2,'v_SDRAM',1);
INSERT INTO "flags" VALUES(3,'spikes_eth',4);
INSERT INTO "flags" VALUES(4,'v_eth',5);
INSERT INTO "flags" VALUES(5,'rate_eth',6);
INSERT INTO "flags" VALUES(6,'gsyn_SDRAM',2);
INSERT INTO "flags" VALUES(7,'VALUE_ROBOT_OUTPUT',7);
CREATE TABLE cell_types (
    id INTEGER PRIMARY KEY,
    "max_nuro_per_fasc" INTEGER,
    "name" TEXT
, "image_name" TEXT);
INSERT INTO "cell_types" VALUES(1,100,'IF_curr_exp','app_frame_lif.aplx');
INSERT INTO "cell_types" VALUES(2,10,'IZK_curr_exp','app_frame_izhikevich.aplx');
INSERT INTO "cell_types" VALUES(3,300,'NEF_OUT_1D','app_frame_nef_out_1d.aplx');
INSERT INTO "cell_types" VALUES(4,50,'IF_cond_exp','app_frame_lif_cond.aplx');
INSERT INTO "cell_types" VALUES(5,50,'IF_curr_exp_32','app_frame_lif_32.aplx');
INSERT INTO "cell_types" VALUES(6,1000,'Dummy','app_frame_dummy.aplx');
INSERT INTO "cell_types" VALUES(7,200,'IF_NEF_2D','app_frame_lif_nef_2d.aplx');
INSERT INTO "cell_types" VALUES(8,200,'NEF_OUT_2D','app_frame_nef_out_2d.aplx');
INSERT INTO "cell_types" VALUES(9,2048,'ProxyNeuron','app_proxy.aplx');
INSERT INTO "cell_types" VALUES(97,300,'IF_NEF_1D','app_frame_lif_nef_1d.aplx');
INSERT INTO "cell_types" VALUES(99,2048,'SpikeSourceArray','app_frame_spike_source_array.aplx');
INSERT INTO "cell_types" VALUES(100,2048,'SpikeSourcePoisson','app_frame_spike_source_poisson.aplx');
INSERT INTO "cell_types" VALUES(101,1,'SpikeSink','app_frame_spike_sink.aplx');
INSERT INTO "cell_types" VALUES(102,1,'Recorder','app_monitoring.aplx');
INSERT INTO "cell_types" VALUES(103,2048,'SpikeSource','app_frame_spike_source.aplx');
CREATE TABLE synapse_types (
    id INTEGER PRIMARY KEY,
    "synapse_name" TEXT,
    "synapse_flag" INTEGER,
    "cell_type_id" INTEGER,
    "translation" TEXT
);
INSERT INTO "synapse_types" VALUES(1,'excitatory',0,1,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0x1FFF, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 13]]');
INSERT INTO "synapse_types" VALUES(2,'inhibitory',1,1,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, -256, 0x1FFF, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 13]]');
INSERT INTO "synapse_types" VALUES(3,'excitatory',0,2,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0x1FFF, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 13]]');
INSERT INTO "synapse_types" VALUES(4,'inhibitory',1,2,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0x1FFF, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 13]]');
INSERT INTO "synapse_types" VALUES(5,'excitatory',0,100,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(6,'inhibitory',1,100,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(9,'excitatory',0,3,'[[0, 0, 0, 0], [0, 1, 0x7FF, 21], [0, 65536, 0xFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 20]]');
INSERT INTO "synapse_types" VALUES(10,'inhibitory',1,3,'[[0, 0, 0, 0], [0, 1, 0x7FF, 21], [0, 65536, 0xFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 20]]');
INSERT INTO "synapse_types" VALUES(13,'excitatory',0,6,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(14,'excitatory',0,99,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(15,'excitatory',0,101,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(16,'excitatory',0,102,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 256, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(17,'excitatory',0,4,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 65536, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(18,'inhibitory',1,4,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 65536, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(19,'excitatory',0,103,'[[-1, 1, 0xF, 28],[0, 1, 0x7FF, 16],[0, 65536, 0xFFF8, 0],[0, 1, 0x1, 27],[0, 1, 0x7, 0]]');
INSERT INTO "synapse_types" VALUES(20,'excitatory',0,8,'[[0, 0, 0, 0], [0, 1, 0x7FF, 21], [0, 65536, 0xFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 20]]');
INSERT INTO "synapse_types" VALUES(21,'inhibitory',1,8,'[[0, 0, 0, 0], [0, 1, 0x7FF, 21], [0, 65536, 0xFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 20]]');
INSERT INTO "synapse_types" VALUES(22,'excitatory',0,7,'[[0, 0, 0, 0], [0, 1, 0x7FF, 21], [0, 65536, 0xFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 20]]');
INSERT INTO "synapse_types" VALUES(23,'inhibitory',1,7,'[[0, 0, 0, 0], [0, 1, 0x7FF, 21], [0, 65536, 0xFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 20]]');
INSERT INTO "synapse_types" VALUES(24,'excitatory',0,5,'[[0, 0, 0, 0], [0, 1, 0x7F, 25], [0, 65536, 0xFFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 24]]');
INSERT INTO "synapse_types" VALUES(25,'inhibitory',1,5,'[[0, 0, 0, 0], [0, 1, 0x7F, 25], [0, 65536, 0xFFFFFF, 0], [0, 0, 0, 0], [0, 1, 0x1, 24]]');
CREATE TABLE plasticity_suffix (
    "id" INTEGER PRIMARY KEY,
    "model_name" TEXT,
    "executable_suffix" TEXT
);
INSERT INTO "plasticity_suffix" VALUES(1,'SpikePairRule','_stdp_sp');
INSERT INTO "plasticity_suffix" VALUES(2,'FullWindow','_stdp');
INSERT INTO "plasticity_suffix" VALUES(3,'TimeToSpike','_tts');
DELETE FROM sqlite_sequence;
INSERT INTO "sqlite_sequence" VALUES('flags',7);
CREATE UNIQUE INDEX idx_cell_types_name ON cell_types (name);
CREATE UNIQUE INDEX idx_synapse_types ON synapse_types (cell_type_id, synapse_name);
COMMIT;
