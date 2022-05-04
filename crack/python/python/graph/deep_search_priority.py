
## never used
class graph:
    def __init__(self,gdict=None):
        print("Graph Init")
        if gdict is None:
            gdict = {}
        self.gdict = gdict
        
# Check for the visisted and unvisited nodes
def dfs(graph, start, visited = None):
    if visited is None:
        visited = set()

    visited.add(start)
    print(start)
    print(graph[start])
    for next in graph[start] - visited:
        dfs(graph, next, visited) # recurse on the first vertic in edge, so deep first 

    return visited


gdict = { 
   "a" : set(["c", "b"]),
   "b" : set(["a", "d"]),
   "c" : set(["a", "d"]),
   "d" : set(["e"]),
   "e" : set(["a"]) # must contain this to return to root
}

dfs(gdict, 'a')

