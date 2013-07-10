class StepCurrentSource():
    pass
    
    def __init__(self, times, amplitudes):
        """Construct the current source.
        
        Arguments:
            times      -- list/array of times at which the injected current changes.
            amplitudes -- list/array of current amplitudes to be injected at the
                          times specified in `times`.
                          
        The injected current will be zero up until the first time in `times`. The
        current will continue at the final value in `amplitudes` until the end
        of the simulation.
        """
        pass

class DCSource(StepCurrentSource):
    """Source producing a single pulse of current of constant amplitude."""
    
    def __init__(self, amplitude=1.0, start=0.0, stop=None):
        pass    
