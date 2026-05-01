from simulator.node import Node
import json

class Distance_Vector_Node(Node):

    def __init__(self, id):
        super().__init__(id)
        # Add your own instance variables here if needed.
        self.links = {}
        self.dv = {id: {}}
        self.rt = {id: id}

    def __str__(self):
        # Return a human-readable string describing this node's current state
        # (e.g., routing table, distance vector). Used for debugging output.
        lines = [f"Node {self.id}", "Distance Vector:"]
        for d in sorted(self.dv[self.id].keys()):
            cost = self.dv[self.id][d]
            next_hop = self.rt[d] if d in self.rt else -1
            cost_str = "INF" if cost == float('inf') else str(cost)
            lines.append(f"  dest={d} cost={cost_str} next_hop={next_hop}")
        return "\n".join(lines)

    def update_dv_and_send_messages(self):
        all_nodes = set(self.dv[self.id].keys())
        all_nodes.update(self.links.keys())
        for id, dv in self.dv.items():
            if id != self.id:
                all_nodes.update(dv.keys())

        changed = False
        for d in all_nodes:
            if d != self.id:
                cost_prev = self.dv[self.id][d] if d in self.dv[self.id] else float('inf')
                next_hop_prev = self.rt[d] if d in self.rt else -1

                cost = self.links[d] if d in self.links else float('inf')
                next_hop = d if d in self.links else -1

                for v, c in self.links.items():
                    if v == d:
                        c_v_d = 0
                    else:
                        if v in self.dv and d in self.dv[v]:
                            c_v_d = self.dv[v][d]
                        else:
                            c_v_d = float('inf')
                    if c_v_d < float('inf'):
                        c_v_d = c + c_v_d
                        if c_v_d < cost:
                            cost = c_v_d
                            next_hop = v
                if cost != float('inf'):
                    self.dv[self.id][d] = cost
                    self.rt[d] = next_hop
                else:
                    if d in self.dv[self.id]:
                        del self.dv[self.id][d]
                        del self.rt[d]
                        next_hop = -1

                if cost != cost_prev or next_hop != next_hop_prev:
                    changed = True
        if changed:
            msg = json.dumps({
                "node_id": self.id,
                "node_dv": self.dv[self.id]
            })
            self.send_to_neighbors(msg)
        
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
        if latency == -1:
            self.links.pop(neighbor, None)
            self.dv.pop(neighbor, None)
        else:
            self.links[neighbor] = latency

        self.update_dv_and_send_messages()
            

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

        routing_msg = json.loads(m)
        node_id = int(routing_msg["node_id"])
        node_dv = routing_msg["node_dv"]

        node_dv_dict = {}

        if node_id in self.links:
            for d, c in node_dv.items():
                c = int(c)
                if c < float('inf'):
                    node_dv_dict[int(d)] = c
            self.dv[node_id] = node_dv_dict
            self.update_dv_and_send_messages()

    def get_next_hop(self, destination):
        """
        Called by the simulator to look up the next hop for a destination.

        Parameters:
            destination (int): The ID of the destination node.

        Returns:
            int: The ID of the neighboring node to forward to, or -1 if the
                 destination is unreachable.
        """
        if destination in self.rt:
            return self.rt[destination]
        else:
            return -1
