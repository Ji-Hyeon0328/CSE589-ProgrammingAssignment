from simulator.node import Node
import json

class Distance_Vector_Split_Horizon_Node(Node):

    INF=200
    def __init__(self, id):
        super().__init__(id)
        # Add your own instance variables here if needed.
        self.links ={}
        self.neighbor_vectors = {}
        self.distance_vector ={self.id : 0}
        self.next_hop={self.id: self.id}

        self.known_destinations={self.id}
        self.last_sent={}

    def __str__(self):
        # Return a human-readable string describing this node's current state
        # (e.g., routing table, distance vector). Used for debugging output.
        lines=[f"Node {self.id}"]
        lines.append(f"Links: {dict(sorted(self.links.items()))}")
        
        lines.append("Distance vector:")
        for dest in sorted(self.distance_vector):
            cost=self.distance_vector[dest]
            hop=self.next_hop.get(dest,-1)
            lines.append(f" - dest={dest}\n - cost={cost}\n - next_hop={hop}")

        return "\n".join(lines)

    def link_has_been_updated(self, neighbor, latency):
        """
        Called by the simulator whenever a direct link to a neighbor changes.

        Parameters:
            neighbor (int): The ID of the neighboring node.
            latency (int): The new latency (cost) of the link.
                           A value of -1 means the link has been deleted.

        You should update your local link information, recompute your distance
        vector using the Bellman-Ford equation, and send updated routing
        messages to your neighbors if your distance vector has changed.

        This node must implement split horizon (with or without poisoned
        reverse) to prevent routing loops: do not advertise a route back to
        the neighbor through which you learned it.
        """
        old_distvec=self.distance_vector.copy()
        old_next_hop=self.next_hop.copy()

        self.known_destinations.add(neighbor)

        if latency == -1: 
            self.links.pop(neighbor,None)
            self.neighbor_vectors.pop(neighbor,None)

            self.last_sent.pop(neighbor,None)

        else:
            self.links[neighbor]=latency
            self.neighbor_vectors.setdefault(neighbor,{neighbor:0})
        
        self._recompute_routes()

        if old_distvec!= self.distance_vector or old_next_hop != self.next_hop:
            self._send_distvec_poisoned_reverse()

    def process_incoming_routing_message(self, m):
        """
        Called by the simulator when a routing message arrives from a neighbor.

        Parameters:
            m (str): The message string sent by a neighboring node.
                     You decide the format — it must match what you send.

        You should parse the message, update your knowledge of the neighbor's
        distance vector, recompute your own distance vector, and propagate
        updates to your neighbors if anything changed.

        Remember to apply split horizon when sending updates: suppress (or
        poison) routes that were learned from the recipient neighbor.
        """
        try:
            msg=json.loads(m)
        except:
            return
        
        if not isinstance(msg,dict):
            return
        if "source"not in msg or "distance_vector" not in msg:
            return
        
        try:
            source=int(msg["source"])
        except:
            return
        
        if source not in self.links:
            return

        advertised=msg["distance_vector"]
        if not isinstance(advertised,dict):
            return
        
        received_distvec={}
        for dest,cost in advertised.items():
            try:
                dest = int(dest)
                cost = int(cost)
            except:
                continue

            if cost>= self.INF:
                received_distvec[dest]= self.INF
            else:
                received_distvec[dest]= cost

        old_distvec=self.distance_vector.copy()
        old_next_hop=self.next_hop.copy()

        self.known_destinations.update(received_distvec.keys())
        
        self.neighbor_vectors[source] = received_distvec
        self._recompute_routes()

        if old_distvec!= self.distance_vector or old_next_hop != self.next_hop:
            self._send_distvec_poisoned_reverse() 
        
        

    def get_next_hop(self, destination):
        """
        Called by the simulator to look up the next hop for a destination.

        Parameters:
            destination (int): The ID of the destination node.

        Returns:
            int: The ID of the neighboring node to forward to, or -1 if the
                 destination is unreachable.
        """
        try:
            dest = int(destination)
        except:
            return -1

        if dest == self.id:
            return self.id
        
        if self.distance_vector.get(dest,self.INF) >= self.INF:
            return -1

        return self.next_hop.get(dest,-1)


    def _recompute_routes(self):
        #Bellman Ford distance vector : D_x(y) = min_v [ c(x, v) + D_v(y) ]
        new_distvec = {self.id:0}
        new_next_hop={self.id: self.id}

        dests =set(self.known_destinations)#{self.id}
        dests.update(self.links.keys())

        for vector in self.neighbor_vectors.values():
            dests.update(vector.keys())

        self.known_destinations.update(dests)

        for dest in dests:
            if dest == self.id:
                continue

            best_cost = self.INF
            best_next_hop=-1

            if dest in self.links:
                best_cost=self.links[dest]
                best_next_hop=dest
            
            for neighbor, link_cost in self.links.items():
                neighbor_vector= self.neighbor_vectors.get(neighbor,{})
                neighbor_cost = neighbor_vector.get(dest, self.INF)
                cand_cost = link_cost + neighbor_cost

                if cand_cost <best_cost:
                    best_cost=cand_cost
                    best_next_hop=neighbor

                elif cand_cost == best_cost and best_next_hop != -1:
                    if neighbor <best_next_hop:
                        best_next_hop=neighbor

            
            if best_next_hop != -1 and best_cost<self.INF:
                new_distvec[dest] = best_cost
                new_next_hop[dest]=best_next_hop
            else:

                if self._all_neighbors_say_unreachable(dest):
                    self.known_destinations.discard(dest)
                    continue

                new_distvec[dest]=self.INF
                new_next_hop[dest]=-1

        self.distance_vector = new_distvec
        self.next_hop=new_next_hop

    
    def _send_distvec_poisoned_reverse(self):
        for neighbor in list(self.links.keys()):
            poisoned_distvec={}

            for dest,cost in self.distance_vector.items():
                if dest!= self.id and dest != neighbor and self.next_hop.get(dest) == neighbor:
                    poisoned_distvec[dest]=self.INF

                elif cost >= self.INF:
                    poisoned_distvec[dest]=self.INF

                else:
                    poisoned_distvec[dest]=cost

            
            if self.last_sent.get(neighbor) == poisoned_distvec:
                continue

            self.last_sent[neighbor]=poisoned_distvec.copy()

            msg = json.dumps({"source":self.id, "distance_vector":poisoned_distvec})
            self.send_to_neighbor(neighbor, msg)
    
    def _all_neighbors_say_unreachable(self,dest):
        if dest in self.links:
            return False
        
        for neighbor in self.links:
            vector = self.neighbor_vectors.get(neighbor,{})
            cost = vector.get(dest,self.INF)

            if cost < self.INF:
                return False
            
        return True
