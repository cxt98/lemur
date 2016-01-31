/* File: inc_bi.h
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
// d (DynamicSWSF-FP) = g (LPA*) value = distance map
// rhs (DynamicSWSF-FP) = rhs (LPA*) = distance_lookahead_map
//
// see "Efficient Point-to-Point Shortest Path Algorithms"
// by Andrew V. Goldberg et. al
// for correct bidirection Dijkstra's termination condition
template <typename Graph,
   typename StartPredecessorMap,
   typename StartDistanceMap, typename StartDistanceLookaheadMap,
   typename GoalPredecessorMap,
   typename GoalDistanceMap, typename GoalDistanceLookaheadMap,
   typename WeightMap,
   typename VertexIndexMap, typename EdgeIndexMap,
   typename CompareFunction, typename CombineFunction,
   typename CostInf, typename CostZero>
class inc_bi
{
public:
   
   typedef typename boost::graph_traits<Graph>::vertex_descriptor Vertex;
   typedef typename boost::graph_traits<Graph>::vertex_iterator VertexIter;
   typedef typename boost::graph_traits<Graph>::edge_descriptor Edge;
   typedef typename boost::graph_traits<Graph>::out_edge_iterator OutEdgeIter;
   typedef typename boost::graph_traits<Graph>::in_edge_iterator InEdgeIter;
   typedef typename boost::property_traits<WeightMap>::value_type weight_type;
   
   struct conn_key
   {
      weight_type path_length;
      weight_type start_dist;
      weight_type goal_dist;
      conn_key():
         path_length(weight_type()), start_dist(weight_type()), goal_dist(weight_type())
      {
      }
      conn_key(weight_type path_length, weight_type start_dist, weight_type goal_dist):
         path_length(path_length), start_dist(start_dist), goal_dist(goal_dist)
      {
      }
      bool operator<(const conn_key & rhs) const
      {
         return path_length < rhs.path_length;
      }
      bool operator>(const conn_key & rhs) const
      {
         return path_length > rhs.path_length;
      }
      bool operator<=(const conn_key & rhs) const
      {
         return path_length <= rhs.path_length;
      }
   };
   
   const Graph & g;
   Vertex v_start;
   Vertex v_goal;
   StartPredecessorMap start_predecessor;
   StartDistanceMap start_distance;
   StartDistanceLookaheadMap start_distance_lookahead;
   GoalPredecessorMap goal_predecessor;
   GoalDistanceMap goal_distance;
   GoalDistanceLookaheadMap goal_distance_lookahead;
   WeightMap weight;
   VertexIndexMap vertex_index_map;
   EdgeIndexMap edge_index_map;
   CompareFunction compare;
   CombineFunction combine;
   CostInf inf;
   CostZero zero;
   weight_type goal_margin;
   
   HeapIndexed< weight_type > start_queue;
   HeapIndexed< weight_type > goal_queue;
   
   // contains the indices of all edges connecting one start-tree vertex to one goal-tree vertex
   // that are both consistent, sorted by start_distance + edge_weight + goal_distance
   HeapIndexed< conn_key > conn_queue;
   
   inc_bi(
      const Graph & g,
      Vertex v_start, Vertex v_goal,
      StartPredecessorMap start_predecessor,
      StartDistanceMap start_distance, StartDistanceLookaheadMap start_distance_lookahead,
      GoalPredecessorMap goal_predecessor,
      GoalDistanceMap goal_distance, GoalDistanceLookaheadMap goal_distance_lookahead,
      WeightMap weight,
      VertexIndexMap vertex_index_map,
      EdgeIndexMap edge_index_map,
      CompareFunction compare, CombineFunction combine,
      CostInf inf, CostZero zero,
      weight_type goal_margin):
      g(g), v_start(v_start), v_goal(v_goal),
      start_predecessor(start_predecessor),
      start_distance(start_distance),
      start_distance_lookahead(start_distance_lookahead),
      goal_predecessor(goal_predecessor),
      goal_distance(goal_distance),
      goal_distance_lookahead(goal_distance_lookahead),
      weight(weight),
      vertex_index_map(vertex_index_map),
      edge_index_map(edge_index_map),
      compare(compare), combine(combine),
      inf(inf), zero(zero),
      goal_margin(goal_margin)
   {
      VertexIter vi, vi_end;
      for (boost::tie(vi,vi_end)=vertices(g); vi!=vi_end; ++vi)
      {
         put(start_distance_lookahead, *vi, inf);
         put(start_distance, *vi, inf);
         put(goal_distance_lookahead, *vi, inf);
         put(goal_distance, *vi, inf);
      }
      put(start_distance_lookahead, v_start, zero);
      put(goal_distance_lookahead, v_goal, zero);
      start_queue.insert(get(vertex_index_map,v_start), zero);
      goal_queue.insert(get(vertex_index_map,v_goal), zero);
   }
   
   inline weight_type start_calculate_key(Vertex u)
   {
      return std::min(get(start_distance,u), get(start_distance_lookahead,u));
   }
   
   inline weight_type goal_calculate_key(Vertex u)
   {
      return std::min(get(goal_distance,u), get(goal_distance_lookahead,u));
   }
   
   // this must be called when an edge's cost changes
   // in case it should be added/removed from the connection queue
   void update_edge(Edge e)
   {
      // should this edge be our connection queue?
      size_t eidx = get(edge_index_map, e);
      weight_type elen = get(weight, e);
      Vertex va = source(e,g);
      Vertex vb = target(e,g);
      // should it be in the queue?
      bool is_valid = false;
      do
      {
         if (elen == inf) break;
         if (start_queue.contains(get(vertex_index_map,va))) break;
         if (goal_queue.contains(get(vertex_index_map,vb))) break;
         if (get(start_distance,va) == inf) break;
         if (get(goal_distance,vb) == inf) break;
         is_valid = true;
      }
      while (0);
      // should we remove it?
      if (!is_valid)
      {
         if (conn_queue.contains(eidx))
            conn_queue.remove(eidx);
      }
      else
      {
         // ok, edge should be there!
         weight_type pathlen = combine(combine(get(start_distance,va), elen), get(goal_distance,vb));
         conn_key new_key(pathlen, get(start_distance,va), get(goal_distance,vb));
         if (conn_queue.contains(eidx))
            conn_queue.update(eidx, new_key);
         else
            conn_queue.insert(eidx, new_key);
      }
   }
   
   inline void start_update_vertex(Vertex u)
   {
      size_t u_idx = get(vertex_index_map,u);
      // when update is called on the start vertex itself,
      // don't touch it's lookahead distance -- it should stay 0.0
      if (u != v_start)
      {
         weight_type rhs = inf;
         InEdgeIter ei, ei_end;
         for (boost::tie(ei,ei_end)=in_edges(u,g); ei!=ei_end; ei++)
         {
            weight_type val = combine(get(start_distance,source(*ei,g)), get(weight,*ei));
            if (val < rhs)
            {
               rhs = val;
               put(start_predecessor, u, source(*ei,g));
            }
         }
         put(start_distance_lookahead, u, rhs);
      }
      weight_type u_dist = get(start_distance,u);
      bool is_consistent = (u_dist == get(start_distance_lookahead,u));
      if (is_consistent)
      {
         if (start_queue.contains(u_idx))
         {
            start_queue.remove(u_idx);
            // we're newly consistent, so insert any new conn_queue edges from us
            if (u_dist != inf)
            {
               OutEdgeIter ei, ei_end;
               for (boost::tie(ei,ei_end)=out_edges(u,g); ei!=ei_end; ei++)
               {
                  Vertex v_target = target(*ei,g);
                  size_t idx_target = get(vertex_index_map, v_target);
                  
                  weight_type goaldist_target = get(goal_distance,v_target);
                  if (!goal_queue.contains(idx_target) && goaldist_target != inf)
                  {
                     conn_key new_key(combine(combine(u_dist, get(weight,*ei)), goaldist_target),
                        u_dist, goaldist_target);
                     conn_queue.insert(get(edge_index_map,*ei), new_key);
                  }
               }
            }
         }
      }
      else // not consistent
      {
         if (start_queue.contains(u_idx))
            start_queue.update(u_idx, start_calculate_key(u));
         else
         {
            start_queue.insert(u_idx, start_calculate_key(u));
            // we're newly inconsistent, so remove any conn_queue edges from us
            OutEdgeIter ei, ei_end;
            for (boost::tie(ei,ei_end)=out_edges(u,g); ei!=ei_end; ei++)
            {
               size_t edge_index = get(edge_index_map,*ei);
               if (conn_queue.contains(edge_index))
                  conn_queue.remove(edge_index);
            }
         }
      }
   }
   
   inline void goal_update_vertex(Vertex u)
   {
      size_t u_idx = get(vertex_index_map,u);
      // when update is called on the goal vertex itself,
      // don't touch it's lookahead distance -- it should stay 0.0
      if (u != v_goal)
      {
         weight_type rhs = inf;
         InEdgeIter ei, ei_end;
         for (boost::tie(ei,ei_end)=in_edges(u,g); ei!=ei_end; ei++)
         {
            weight_type val = combine(get(goal_distance,source(*ei,g)), get(weight,*ei));
            if (val < rhs)
            {
               rhs = val;
               put(goal_predecessor, u, source(*ei,g));
            }
         }
         put(goal_distance_lookahead, u, rhs);
      }
      weight_type u_dist = get(goal_distance,u);
      bool is_consistent = (u_dist == get(goal_distance_lookahead,u));
      if (is_consistent)
      {
         if (goal_queue.contains(u_idx))
         {
            goal_queue.remove(u_idx);
            // we're newly consistent, so insert any new conn_queue edges to us
            if (u_dist != inf)
            {
               InEdgeIter ei, ei_end;
               for (boost::tie(ei,ei_end)=in_edges(u,g); ei!=ei_end; ei++)
               {
                  Vertex v_source = source(*ei,g);
                  size_t idx_source = get(vertex_index_map, v_source);
                  
                  weight_type startdist_source = get(start_distance,v_source);
                  if (!start_queue.contains(idx_source) && startdist_source != inf)
                  {
                     conn_key new_key(combine(combine(startdist_source, get(weight,*ei)), u_dist),
                        startdist_source, u_dist);
                     conn_queue.insert(get(edge_index_map,*ei), new_key);
                  }
               }
            }
         }
      }
      else // not consistent
      {
         if (goal_queue.contains(u_idx))
            goal_queue.update(u_idx, goal_calculate_key(u));
         else
         {
            goal_queue.insert(u_idx, goal_calculate_key(u));
            // we're newly inconsistent, so remove any conn_queue edges to us
            InEdgeIter ei, ei_end;
            for (boost::tie(ei,ei_end)=in_edges(u,g); ei!=ei_end; ei++)
            {
               size_t edge_index = get(edge_index_map,*ei);
               if (conn_queue.contains(edge_index))
                  conn_queue.remove(edge_index);
            }
         }
      }
   }
   
   // returns index of middle edge!
   std::pair<size_t,bool> compute_shortest_path()
   {
      for (;;)
      {
         weight_type start_top = start_queue.size() ? start_queue.top_key() : inf;
         weight_type goal_top = goal_queue.size() ? goal_queue.top_key() : inf;
         if (start_top == inf && goal_top == inf)
            return std::make_pair(0, false);
         
         // termination condition is rather complicated!
         do
         {
            if (!conn_queue.size()) break;
            if (combine(conn_queue.top_key().path_length,goal_margin) > combine(start_top,goal_top)) break;
            if (combine(conn_queue.top_key().start_dist,goal_margin) > start_top) break;
            if (combine(conn_queue.top_key().goal_dist,goal_margin) > goal_top) break;
            return std::make_pair(conn_queue.top_idx(), true);
         }
         while (0);
         
         if (start_top < goal_top)
         {
            size_t u_idx = start_queue.top_idx();
            Vertex u = vertex(u_idx, g);
            
            start_queue.remove_min();
            if (get(start_distance,u) > get(start_distance_lookahead,u))
            {
               weight_type u_dist = get(start_distance_lookahead,u);
               put(start_distance, u, u_dist);
               
               // update any successors that they may now be inconsistent
               // also, this start vertex just became consistent,
               // so add any out_edges to consistent goal-tree vertices
               OutEdgeIter ei, ei_end;
               for (boost::tie(ei,ei_end)=out_edges(u,g); ei!=ei_end; ei++)
               {
                  Vertex v_target = target(*ei,g);
                  size_t idx_target = get(vertex_index_map, v_target);
                  
                  start_update_vertex(v_target);
                  
                  weight_type goaldist_target = get(goal_distance,v_target);
                  if (u_dist != inf && !goal_queue.contains(idx_target) && goaldist_target != inf)
                  {
                     conn_key new_key(combine(combine(u_dist, get(weight,*ei)), goaldist_target),
                        u_dist, goaldist_target);
                     conn_queue.insert(get(edge_index_map,*ei), new_key);
                  }
               }
            }
            else
            {
               put(start_distance, u, inf);
               start_update_vertex(u);
               OutEdgeIter ei, ei_end;
               for (boost::tie(ei,ei_end)=out_edges(u,g); ei!=ei_end; ei++)
                  start_update_vertex(target(*ei,g));
            }
         }
         else
         {
            size_t u_idx = goal_queue.top_idx();
            Vertex u = vertex(u_idx, g);
            
            goal_queue.remove_min();
            if (get(goal_distance,u) > get(goal_distance_lookahead,u))
            {
               weight_type u_dist = get(goal_distance_lookahead,u);
               put(goal_distance, u, u_dist);
               
               // update any predecessors that they may now be inconsistent
               // also, this goal vertex just became consistent,
               // so add any in_edges from consistent start-tree vertices
               InEdgeIter ei, ei_end;
               for (boost::tie(ei,ei_end)=in_edges(u,g); ei!=ei_end; ei++)
               {
                  Vertex v_source = source(*ei,g);
                  size_t idx_source = get(vertex_index_map, v_source);
                  
                  goal_update_vertex(v_source);
                  
                  weight_type startdist_source = get(start_distance,v_source);
                  if (u_dist != inf && !start_queue.contains(idx_source) && startdist_source != inf)
                  {
                     conn_key new_key(combine(combine(startdist_source, get(weight,*ei)), u_dist),
                        startdist_source, u_dist);
                     conn_queue.insert(get(edge_index_map,*ei), new_key);
                  }
               }
            }
            else
            {
               put(goal_distance, u, inf);
               goal_update_vertex(u);
               OutEdgeIter ei, ei_end;
               for (boost::tie(ei,ei_end)=out_edges(u,g); ei!=ei_end; ei++)
                  goal_update_vertex(target(*ei,g));
            }
         }
      }
   }

};

template <typename Graph,
   typename StartPredecessorMap,
   typename StartDistanceMap, typename StartDistanceLookaheadMap,
   typename GoalPredecessorMap,
   typename GoalDistanceMap, typename GoalDistanceLookaheadMap,
   typename WeightMap,
   typename VertexIndexMap, typename EdgeIndexMap,
   typename CompareFunction, typename CombineFunction,
   typename CostInf, typename CostZero>
inc_bi<Graph,
   StartPredecessorMap,StartDistanceMap,StartDistanceLookaheadMap,
   GoalPredecessorMap,GoalDistanceMap,GoalDistanceLookaheadMap,
   WeightMap,VertexIndexMap,EdgeIndexMap,
   CompareFunction,CombineFunction,CostInf,CostZero>
make_inc_bi(
   const Graph & g,
   typename boost::graph_traits<Graph>::vertex_descriptor v_start,
   typename boost::graph_traits<Graph>::vertex_descriptor v_goal,
   StartPredecessorMap start_predecessor,
   StartDistanceMap start_distance, StartDistanceLookaheadMap start_distance_lookahead,
   GoalPredecessorMap goal_predecessor,
   GoalDistanceMap goal_distance, GoalDistanceLookaheadMap goal_distance_lookahead,
   WeightMap weight,
   VertexIndexMap vertex_index_map,
   EdgeIndexMap edge_index_map,
   CompareFunction compare, CombineFunction combine,
   CostInf inf, CostZero zero,
   typename boost::property_traits<WeightMap>::value_type goal_margin)
{
   return inc_bi<
         Graph,
         StartPredecessorMap,StartDistanceMap,StartDistanceLookaheadMap,
         GoalPredecessorMap,GoalDistanceMap,GoalDistanceLookaheadMap,
         WeightMap,VertexIndexMap,EdgeIndexMap,
         CompareFunction,CombineFunction,CostInf,CostZero>(
      g,v_start,v_goal,
      start_predecessor,start_distance,start_distance_lookahead,
      goal_predecessor,goal_distance,goal_distance_lookahead,
      weight,vertex_index_map,edge_index_map,
      compare,combine,inf,zero,goal_margin);
}

} // namespace pr_bgl