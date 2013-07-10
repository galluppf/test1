"""
.. moduleauthor:: Sergio Davies, SpiNNaker Project, University of Manchester. sergio.davies@cs.man.ac.uk
"""


class router:

	def __init__ (self, x, y):
		self.entries = list()
		self.hops = list()
		self.x_pos = x
		self.y_pos = y
		self.neuronID = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
		#do something to initialize
		return

	def setPos (self, x, y):
		self.x_pos = x
		self.y_pos = y

	def addHop (self, routingKey, mask, direction, source):
		self.hops.append([routingKey, mask, direction, source])

	def getHop (self, id):
		return self.hops[id]

	def delHop (self, id):
		del self.hops[id]

	def sizeHops (self):
		return len(self.hops)

	def addEntry (self, routingKey, mask, direction):
		self.entries.append([routingKey, mask, direction])

	def getEntry (self, id):
		return self.entries[id]

	def delEntry (self, id):
		del self.entries[id]

	def sizeEntries (self):
		return len(self.entries)