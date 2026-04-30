from simulator.node import Node
import json

class Distance_Vector_Node(Node):
    INF = 10**9

    def __init__(self, id):
        super().__init__(id)
        # Add your own instance variables here if needed.
        self.links ={}
        self.neighbor_vectors = {}
        self.distance_vector ={self.id : 0}
        self.next_hop={self.id: self.id}

        self.known_destinations={self.id}
        self.last_sent={}

        self.paths={self.id:[self.id]}
        self.neighbor_paths={}

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
        """
        old_distvec=self.distance_vector.copy()
        old_next_hop=self.next_hop.copy()

        self.known_destinations.add(neighbor)

        if latency == -1: 

            self.links.pop(neighbor,None)
            self.neighbor_vectors.pop(neighbor,None)
            self.neighbor_paths.pop(neighbor,None)
        else:
            self.links[neighbor]=latency
            self.neighbor_vectors.setdefault(neighbor,{neighbor:0})
            self.neighbor_paths.setdefault(neighbor,{neighbor:[neighbor]})
        
        self._recompute_routes()

        if old_distvec!= self.distance_vector or old_next_hop != self.next_hop:
            self._send_distance_vector()
            

    def process_incoming_routing_message(self, m):
        """
        Called by the simulator when a routing message arrives from a neighbor.

        Parameters:
            m (str): The message string sent by a neighboring node.
                     You decide the format — it must match what you send.

        You should parse the message, update your knowledge of the neighbor's
        distance vector, recompute your own distance vector, and propagate
        updates to your neighbors if anything changed.
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

        advertised_paths = msg.get("path_vector",{})
        received_paths = {}
        for dest, path in advertised_paths.items():
            try:
                dest = int(dest)
            except:
                continue

            if isinstance(path,list):
                try:
                    received_paths[dest]=[int(x) for x in path]
                except:
                    received_paths[dest]=[]
            
        old_distvec=self.distance_vector.copy()
        old_next_hop=self.next_hop.copy()

        self.known_destinations.update(received_distvec.keys())

        self.neighbor_vectors[source] = received_distvec
        self.neighbor_paths[source]=received_paths

        self._recompute_routes()

        if old_distvec!= self.distance_vector or old_next_hop != self.next_hop:
            self._send_distance_vector()

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
        new_paths = {self.id:[self.id]}

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
            best_path =[]

            if dest in self.links:
                best_cost=self.links[dest]
                best_next_hop=dest
                best_path=[self.id,dest]

            for neighbor, link_cost in self.links.items():
                neighbor_vector= self.neighbor_vectors.get(neighbor,{})
                neighbor_cost = neighbor_vector.get(dest, self.INF)

                if neighbor_cost >= self.INF:
                    continue
                
                neighbor_path = self.neighbor_paths.get(neighbor,{}).get(dest)

                if neighbor_path is None:
                    if dest == neighbor:
                        neighbor_path = [neighbor]
                    else:
                        continue
                
                if not isinstance(neighbor_path,list):
                    continue

                if self.id in neighbor_path:
                    continue

                cand_cost = link_cost + neighbor_cost
                cand_path = [self.id]+neighbor_path

                if cand_cost <best_cost:
                    best_cost=cand_cost
                    best_next_hop=neighbor
                    best_path = cand_path
 
                elif cand_cost == best_cost and best_next_hop != -1:
                    if len(cand_path) < len(best_path):
                        best_next_hop = neighbor
                        best_path = cand_path
                    elif len(cand_path)==len(best_path) and neighbor < best_next_hop:
                        best_next_hop = neighbor
                        best_path=cand_path
            

            if best_next_hop != -1 and best_cost<self.INF:
                new_distvec[dest] = best_cost
                new_next_hop[dest]=best_next_hop 
                new_paths[dest]=best_path 
            else:
                new_distvec[dest]=self.INF
                new_next_hop[dest]=-1
                new_paths[dest]=[]
                

        self.distance_vector = new_distvec
        self.next_hop=new_next_hop
        self.paths=new_paths

    def _send_distance_vector(self):
        advertised_state = {
            "distance_vector": self.distance_vector.copy(),
            "path_vector":{d: list(p) for d,p in self.paths.items()}
        }

        if self.last_sent == advertised_state:
            return

      
        self.last_sent = advertised_state

        msg = json.dumps({"source": self.id, "distance_vector": self.distance_vector, "path_vector":self.paths})

        self.send_to_neighbors(msg)