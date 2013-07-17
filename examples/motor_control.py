#!/usr/bin/python
"""

"""

#!/usr/bin/python
simulator_name = 'spiNNaker'
#simulator_name = 'nest'


from pyNN.utility import get_script_args
from pyNN.errors import RecordingError

exec("import pyNN.%s as p" % simulator_name)


p.setup(timestep=1.0,min_delay=1.0,max_delay=10.0, db_name='if_cond.sqlite')



cell_params = {     'tau_m': 32, 'i_offset' : [0,0,0,0,0,0,0],    
                    'tau_refrac' : 3.0, 'v_rest' : -65.0,
                    'v_thresh' : -51.0,  'tau_syn_E'  : 2.0,
                    'tau_syn_I': 5.0,    'v_reset'    : -70.0,
                    'e_rev_E'  : 0.,     'e_rev_I'    : -80.}

ifcell = p.Population(7, p.IF_cond_exp, cell_params, label='IF_cond_exp')
ifcell.set('i_offset', [0,0,0,0,1,0,0])


motors = p.Population(7, p.MotorControl,  {}, label='MotorControl')
# this is ugly I know I will write a pynn function
p.simulator.db_run.insert_probe(motors.id, 'VALUE', 'ROBOT_OUTPUT')     

connE = p.Projection(ifcell,motors, 
    p.OneToOneConnector(weights=1, delays=1), target='motor_input')
    
ifcell.record_v()
ifcell.record()

p.run(200.0)

p.end()

