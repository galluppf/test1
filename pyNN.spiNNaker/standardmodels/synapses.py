
class STDPMechanism():
    def __init__(self, timing_dependence=None, weight_dependence=None, voltage_dependence=None, dendritic_delay_fraction=1.0):
        self.timing_dependence = timing_dependence
        self.weight_dependence = weight_dependence
        self.voltage_dependence = voltage_dependence
#        self.__name__ = timing_dependence.__name__ + '_' + weight_dependence.__name__        
        pass


class AdditiveWeightDependence():
    """
    The amplitude of the weight change is fixed for depression (`A_minus`)
    and for potentiation (`A_plus`).
    If the new weight would be less than `w_min` it is set to `w_min`. If it would
    be greater than `w_max` it is set to `w_max`.
    """
    def __init__(self, w_min=0.0, w_max=1.0, A_plus=0.01, A_minus=0.01): # units?
        self.__name__ = 'AdditiveWeightDependence'
        self.parameters = {'w_min':w_min, 'w_max':w_max, 'A_plus':A_plus, 'A_minus':A_minus}
#        self.w_min = w_min
#        self.w_max = w_max
#        self.A_plus = A_plus
#        self.A_minus = A_minus



class SpikePairRule():
    def __init__(self, tau_plus=20.0, tau_minus=20.0, resolution=1):
        self.__name__ = 'SpikePairRule'
        self.parameters = {'tau_plus':tau_plus, 'tau_minus':tau_minus, 'resolution':resolution}


class FullWindow():
    def __init__(self, tau_plus=20.0, tau_minus=20.0, resolution=1):
        self.__name__ = 'FullWindow'
        self.parameters = {'tau_plus':tau_plus, 'tau_minus':tau_minus, 'resolution':resolution}

