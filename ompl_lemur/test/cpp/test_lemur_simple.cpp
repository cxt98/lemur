/*! \file test_lemur_simple.cpp
 * \author Chris Dellin <cdellin@gmail.com>
 * \copyright 2015 Carnegie Mellon University
 * \copyright License: BSD
 */

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/chrono.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <ompl/base/ScopedState.h>
#include <ompl/base/spaces/RealVectorStateSpace.h>
#include <ompl/base/SpaceInformation.h>
#include <ompl/base/ProblemDefinition.h>
#include <ompl/base/Planner.h>
#include <ompl/geometric/PathGeometric.h>
#include <ompl/datastructures/NearestNeighbors.h>

#include <pr_bgl/compose_property_map.hpp>
#include <pr_bgl/vector_ref_property_map.h>
#include <pr_bgl/edge_indexed_graph.h>
#include <pr_bgl/heap_indexed.h>
#include <pr_bgl/string_map.h>
#include <pr_bgl/overlay_manager.h>
#include <ompl_lemur/config.h>
#include <ompl_lemur/util.h>
#include <ompl_lemur/BisectPerm.h>
#include <ompl_lemur/NearestNeighborsLinearBGL.h>
#include <ompl_lemur/Roadmap.h>
#include <ompl_lemur/TagCache.h>
#include <ompl_lemur/UtilityChecker.h>
#include <ompl_lemur/LEMUR.h>
#include <ompl_lemur/RoadmapHalton.h>

#include <gtest/gtest.h>

class CountingRealVectorStateSpace: public ompl::base::RealVectorStateSpace
{
public:
   mutable unsigned int states_allocated;
   mutable unsigned int states_freed;
   CountingRealVectorStateSpace(unsigned int dim = 0):
      ompl::base::RealVectorStateSpace(dim),
      states_allocated(0), states_freed(0)
   {
   }
   virtual ~CountingRealVectorStateSpace()
   {
   }
   virtual ompl::base::State * allocState() const
   {
      states_allocated++;
      return ompl::base::RealVectorStateSpace::allocState();
   }
   virtual void freeState(ompl::base::State * state) const
   {
      states_freed++;
      ompl::base::RealVectorStateSpace::freeState(state);
   }
};

bool isvalid(const ompl::base::State * state)
{
   double * values = state->as<
      ompl::base::RealVectorStateSpace::StateType>()->values;
   if (values[0] < 0.5 && values[1] < 0.5)
      return false;
   else
      return true;
}

ompl::base::ScopedState<ompl::base::RealVectorStateSpace>
make_state(const ompl::base::StateSpacePtr space, double x, double y)
{
   ompl::base::ScopedState<ompl::base::RealVectorStateSpace>
      state(space);
   double * values = state->as<
      ompl::base::RealVectorStateSpace::StateType>()->values;
   values[0] = x;
   values[1] = y;
   return state;
}

ompl::base::ScopedState<ompl::base::RealVectorStateSpace>
#ifdef OMPL_LEMUR_HAS_BOOSTSMARTPTRS
get_path_state(boost::shared_ptr<ompl::geometric::PathGeometric> path, size_t idx)
#else
get_path_state(std::shared_ptr<ompl::geometric::PathGeometric> path, size_t idx)
#endif
{
   ompl::base::StateSpacePtr space = path->getSpaceInformation()->getStateSpace();
   ompl::base::ScopedState<ompl::base::RealVectorStateSpace>
      traj_state(space, path->getState(idx));
   return traj_state;
}

TEST(LemurSimpleTestCase, LemurSimpleTest)
{
   // state space
#ifdef OMPL_LEMUR_HAS_BOOSTSMARTPTRS
   boost::shared_ptr<ompl::base::RealVectorStateSpace> space(
      new CountingRealVectorStateSpace(2));
#else
   std::shared_ptr<ompl::base::RealVectorStateSpace> space(
      new CountingRealVectorStateSpace(2));
#endif
   space->setBounds(0.0, 1.0);
   space->setLongestValidSegmentFraction(
      0.001 / space->getMaximumExtent());
   space->setup();
   
   // space info
   ompl::base::SpaceInformationPtr si(
      new ompl::base::SpaceInformation(space));
   si->setStateValidityChecker(isvalid);
   si->setStateValidityChecker(ompl::base::StateValidityCheckerPtr(
      new ompl_lemur::BinaryUtilityChecker(si, si->getStateValidityChecker(),
         space->getLongestValidSegmentLength())));
   si->setup();
   
   // problem definition
   ompl::base::ProblemDefinitionPtr pdef(
      new ompl::base::ProblemDefinition(si));
   pdef->addStartState(make_state(space, 0.25, 0.75));
   pdef->setGoalState(make_state(space, 0.75, 0.25));
   
   // planner
   ompl::base::PlannerPtr planner(new ompl_lemur::LEMUR(si));
   planner->as<ompl_lemur::LEMUR>()->setCoeffDistance(1.);
   planner->as<ompl_lemur::LEMUR>()->setCoeffCheckcost(0.);
   planner->as<ompl_lemur::LEMUR>()->setCoeffBatch(0.);
   
   // roadmap
   planner->as<ompl_lemur::LEMUR>()->registerRoadmapType<ompl_lemur::RoadmapHalton>("Halton");
   planner->as<ompl_lemur::LEMUR>()->setRoadmapType("Halton");
   planner->params().setParam("roadmap.num", "30");
   planner->params().setParam("roadmap.radius", "0.3");
   
   // solve
   planner->setProblemDefinition(pdef);
   ompl::base::PlannerStatus status = planner->solve(
      ompl::base::plannerNonTerminatingCondition());
   ASSERT_EQ(status, ompl::base::PlannerStatus::EXACT_SOLUTION);
   
   // check resulting path
#ifdef OMPL_LEMUR_HAS_BOOSTSMARTPTRS
   boost::shared_ptr<ompl::geometric::PathGeometric> path = 
      boost::dynamic_pointer_cast<ompl::geometric::PathGeometric>(
      pdef->getSolutionPath());
#else
   std::shared_ptr<ompl::geometric::PathGeometric> path = 
      std::dynamic_pointer_cast<ompl::geometric::PathGeometric>(
      pdef->getSolutionPath());
#endif
   ASSERT_TRUE(path.get());
   ASSERT_EQ(4, path->getStateCount());
   ASSERT_EQ(make_state(space, 0.25, 0.75),       get_path_state(path,0));
   ASSERT_EQ(make_state(space, 0.40625, 14./27.), get_path_state(path,1));
   ASSERT_EQ(make_state(space, 0.68750, 13./27.), get_path_state(path,2));
   ASSERT_EQ(make_state(space, 0.75, 0.25),       get_path_state(path,3));
   
   pdef.reset();
   planner.reset();
   path.reset();
   ASSERT_EQ(1323, space->as<CountingRealVectorStateSpace>()->states_allocated);
   ASSERT_EQ(1323, space->as<CountingRealVectorStateSpace>()->states_freed);
}

int main(int argc, char **argv)
{
   testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}
