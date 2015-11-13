/* File: lifelong_planning_astar.h
 * Author: Chris Dellin <cdellin@gmail.com>
 * Copyright: 2015 Carnegie Mellon University
 * License: BSD
 */

namespace pr_bgl
{

// for now, we assume an undirected graph
// (that is, update_edge will attempt an upate in both directions)
// we assume that everything is constant (graph structure)
// rhs = one-step-lookahead (based on d/g)
// d[DynamicSWSF-FP] = g[LPA*] = saved value = distance map
template <typename Graph, typename AStarHeuristic,
   typename LPAStarVisitor, typename PredecessorMap,
   typename DistanceMap, typename DistanceLookaheadMap,
   typename WeightMap, typename VertexIndexMap,
   typename CompareFunction, typename CombineFunction,
   typename CostInf, typename CostZero>
class lifelong_planning_astar
{
public:
   typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
   typedef typename boost::graph_traits<Graph>::vertex_iterator VertexIter;
   typedef typename boost::graph_traits<Graph>::edge_descriptor Edge;
   typedef typename boost::graph_traits<Graph>::out_edge_iterator OutEdgeIter;
   typedef typename boost::graph_traits<Graph>::in_edge_iterator InEdgeIter;
   
   const Graph & g;
   Vertex v_start;
   Vertex v_goal;
   AStarHeuristic h;
   LPAStarVisitor vis;
   PredecessorMap predecessor;
   DistanceMap distance;
   DistanceLookaheadMap distance_lookahead;
   WeightMap weight;
   VertexIndexMap index_map;
   CompareFunction compare;
   CombineFunction combine;
   CostInf inf;
   CostZero zero;
   
   HeapIndexed< std::pair<unsigned int, unsigned int> > queue;
   
   lifelong_planning_astar(
      const Graph & g,
      Vertex v_start, Vertex v_goal,
      AStarHeuristic h,
      LPAStarVisitor vis,
      PredecessorMap predecessor,
      DistanceMap distance, DistanceLookaheadMap distance_lookahead,
      WeightMap weight,
      VertexIndexMap index_map,
      CompareFunction compare, CombineFunction combine,
      CostInf inf, CostZero zero):
      g(g), v_start(v_start), v_goal(v_goal),
      h(h), vis(vis), predecessor(predecessor),
      distance(distance), distance_lookahead(distance_lookahead),
      weight(weight), index_map(index_map),
      compare(compare), combine(combine),
      inf(inf), zero(zero)
   {
      VertexIter vi, vi_end;
      for (boost::tie(vi,vi_end)=vertices(g); vi!=vi_end; ++vi)
      {
         put(distance_lookahead, *vi, inf);
         put(distance, *vi, inf);
      }
      put(predecessor, v_start, v_start);
      put(distance_lookahead, v_start, zero);
      queue.insert(get(index_map,v_start), std::make_pair(h(v_start),0));
   }
   
   inline std::pair<unsigned int, unsigned int> calculate_key(Vertex u)
   {
      unsigned int minval
         = std::min(get(distance,u), get(distance_lookahead,u));
      return std::make_pair(minval+h(u), minval);
   }
   
   inline void update_vertex(Vertex u)
   {
      size_t u_idx = get(index_map,u);
      if (u != v_start)
      {
         unsigned int rhs = inf;
         InEdgeIter ei, ei_end;
         for (boost::tie(ei,ei_end)=in_edges(u,g); ei!=ei_end; ei++)
         {
            unsigned int val = combine(get(distance,source(*ei,g)), get(weight,*ei));
            if (val < rhs)
            {
               rhs = val;
               put(predecessor, u, source(*ei,g));
            }
         }
         put(distance_lookahead, u, rhs);
      }
      if (queue.contains(u_idx))
         queue.remove(u_idx);
      if (get(distance,u) != get(distance_lookahead,u))
         queue.insert(u_idx, calculate_key(u));
   }
   
   void compute_shortest_path()
   {
      while (queue.top_key() < calculate_key(v_goal)
         || get(distance_lookahead,v_goal) != get(distance,v_goal))
      {
         Vertex u = vertex(queue.top_idx(), g);
         vis.examine_vertex(u, g);
         queue.remove_min();
         if (get(distance,u) > get(distance_lookahead,u))
         {
            put(distance, u, get(distance_lookahead,u));
            OutEdgeIter ei, ei_end;
            for (boost::tie(ei,ei_end)=out_edges(u,g); ei!=ei_end; ei++)
               update_vertex(target(*ei,g));
         }
         else
         {
            put(distance, u, inf);
            update_vertex(u);
            OutEdgeIter ei, ei_end;
            for (boost::tie(ei,ei_end)=out_edges(u,g); ei!=ei_end; ei++)
               update_vertex(target(*ei,g));
         }
      }
   }
};

} // namespace pr_bgl
