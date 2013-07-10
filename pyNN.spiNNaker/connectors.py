from pyNN.space import Space


class OneToOneConnector():
    def __init__(self, weights=0.0, delays=1):
        self.__name__ = "OneToOneConnector"
        self.weights = weights
        self.delays = delays
        

class AllToAllConnector():
    def __init__(self, weights=0.0, delays=1, allow_self_connections=True):
        self.__name__ = "AllToAllConnector"
        self.weights = weights
        self.delays = delays
        self.allow_self_connections = allow_self_connections
        

class FixedProbabilityConnector():
    def __init__(self, p_connect, weights=0.0, delays=1, allow_self_connections=True):
        self.__name__ = "FixedProbabilityConnector"
        self.p_connect = p_connect
        self.weights = weights
        self.delays = delays
        self.allow_self_connections = allow_self_connections
        

class FromListConnector():
    def __init__(self, conn_list, safe=True, verbose=False):
        self.__name__ = "FromListConnector"
        self.conn_list = conn_list
        pass
        

class DistanceDependentProbabilityConnector():
    def __init__(self, d_expression, allow_self_connections=True, weights=0.0, delays=None, space=Space(), safe=True, verbose=False, n_connections=None):
        self.__name__ = "DistanceDependentProbabilityConnector"
        self.d_expression = d_expression
        self.weights = weights
        self.delays = delays
        self.allow_self_connections = allow_self_connections
        self.space = space
        self.n_connections = n_connections
        
