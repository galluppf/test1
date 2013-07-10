"""
Utilities for running simulations on board and retrieving results:

 - spike_receiver : automatically started by PyNN if run_app_dump=true in pacman.cfg, is a utility to collect spikes coming from the spinnaker board. It can be used with the .record() population method, and will enable the use of the .getSpikes() and .printSpikes() methods. Temporary spikes are saved to $PACKAGE_ROOT/binaries/temp_spikes.dat. The spike receiver can also be instantiated manually as::
 
 spike_receiver <database network file> <log file>

"""
