/*
 * component_management.h
 *
 *  Created on: Aug 23, 2012
 *      Author: Marc Thurley
 */

#ifndef COMPONENT_MANAGEMENT_H_
#define COMPONENT_MANAGEMENT_H_

#include "component_types/component.h"
#include "component_cache.h"
#include "alt_component_analyzer.h"
//#include "component_analyzer.h"

// #include <vector>
#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <random>
#include <gmpxx.h>
#include "containers.h"
#include "stack.h"
#include "structures.h"
#ifdef DOPCC
#include "clhash/clhash.h"
#else
#include "clhash/minim.h"
#endif
#include "solver_config.h"


using namespace std;

typedef AltComponentAnalyzer ComponentAnalyzer;

class ComponentManager
{
public:
  // Define callback function types
  using IsExistVariableCallback = std::function<bool(unsigned)>;
  using ProbabilityCallback = std::function<double(unsigned)>;

  ComponentManager(SolverConfiguration &config, DataAndStatistics &statistics,
                   LiteralIndexedVector<TriValue> &lit_values,
                   set<unsigned> &independent_support_, bool &perform_projected_model_counting,
                   IsExistVariableCallback isExistVariableCB, ProbabilityCallback probabilityCB) : config_(config), statistics_(statistics), cache_(statistics, config_),
                                                                  ana_(statistics, lit_values, independent_support_, perform_projected_model_counting),
                                                                  isExistVariable_(isExistVariableCB),
                                                                  probability_(probabilityCB)
  {
  }

  void initialize(LiteralIndexedVector<Literal> &literals,
                  vector<LiteralID> &lit_pool, unsigned num_variables);

  unsigned scoreOf(VariableIndex v)
  {
    return ana_.scoreOf(v);
  }

  void cacheModelCountOf(unsigned stack_comp_id, const double &value)
  {
    if (config_.perform_component_caching)
      cache_.storeValueOf(component_stack_[stack_comp_id]->id(), value);
  }

  Component &superComponentOf(StackLevel &lev)
  {
    assert(component_stack_.size() > lev.super_component());
    return *component_stack_[lev.super_component()];
  }

  unsigned component_stack_size()
  {
    return component_stack_.size();
  }

  void cleanRemainingComponentsOf(StackLevel &top)
  {
    while (component_stack_.size() > top.remaining_components_ofs())
    {
      if (cache_.hasEntry(component_stack_.back()->id()))
        cache_.entry(component_stack_.back()->id()).set_deletable();
      delete component_stack_.back();
      component_stack_.pop_back();
    }
    assert(top.remaining_components_ofs() <= component_stack_.size());
  }

  Component &currentRemainingComponentOf(StackLevel &top)
  {
    assert(component_stack_.size() > top.currentRemainingComponent());
    return *component_stack_[top.currentRemainingComponent()];
  }

  // checks for the next yet to explore remaining component of top
  // returns true if a non-trivial non-cached component
  // has been found and is now stack_.TOS_NextComp()
  // returns false if all components have been processed;
  inline bool findNextRemainingComponentOf(StackLevel &top);

  inline void recordRemainingCompsFor(StackLevel &top);

  inline void sortComponentStackRange(unsigned start, unsigned end);

  inline float cacheScoreOf(VariableIndex v);

  inline void increasecachescores();

  inline void decreasecachescore(Component &comp);

  void gatherStatistics()
  {
    cache_.compute_byte_size_infrasture();
  }

  void removeAllCachePollutionsOf(StackLevel &top);
  vector<void *> seedforCLHASH;
  void getrandomseedforclhash()
  {
    std::random_device rd;     //Get a random seed from the OS entropy device, or whatever
    std::mt19937_64 eng(rd()); //Use the 64-bit Mersenne Twister 19937 generator
                               //and seed it with entropy.
    std::uniform_int_distribution<unsigned long long> distr;
    seedforCLHASH.reserve(config_.hashrange);
    for (int i = 0; i < config_.hashrange; i++)
    {
      seedforCLHASH[i] =
          get_random_key_for_clhash(distr(eng), distr(eng));
    }
  }

private:
  SolverConfiguration &config_;
  DataAndStatistics &statistics_;

  vector<Component *> component_stack_;
  ComponentCache cache_;
  ComponentAnalyzer ana_;
  vector<float> cachescore_;
  IsExistVariableCallback isExistVariable_;
  ProbabilityCallback probability_;
};

float ComponentManager::cacheScoreOf(VariableIndex v)
{
  return cachescore_[v];
}

void ComponentManager::increasecachescores()
{
  for (unsigned i = 0; i < cachescore_.size(); i++)
  {
    cachescore_[i] *= 0.5;
  }
}
void ComponentManager::decreasecachescore(Component &comp)
{
  for (vector<VariableIndex>::const_iterator it = comp.varsBegin();
       *it != varsSENTINEL; it++)
  {
    cachescore_[*it] -= 1;
  }
}

void ComponentManager::sortComponentStackRange(unsigned start, unsigned end)
{
  assert(start <= end);
  // sort the remaining components for processing
  for (unsigned i = start; i < end; i++)
    for (unsigned j = i + 1; j < end; j++)
    {
      if (component_stack_[i]->num_variables() < component_stack_[j]->num_variables())
        swap(component_stack_[i], component_stack_[j]);
    }
}

bool ComponentManager::findNextRemainingComponentOf(StackLevel &top)
{
  // record Remaining Components if there are none!
  std::cout << "find next remaining component of: " << top.getbranchvar() << " branch: " << top.isSecondBranch() << std::endl;
  top.printbranchvalue();
  if (component_stack_.size() <= top.remaining_components_ofs())
    recordRemainingCompsFor(top);
  std::cout << "has unprocessed components: " << top.hasUnprocessedComponents() << std::endl;
  // assert(!top.branch_found_unsat());
  if (top.hasUnprocessedComponents())
    return true; 
  // if no component remains -> meaning that no matter what the value of this variable is -> it will be satisfied -> prob = 1
  // make sure, at least that the current branch is considered SAT
  top.includeSolution(1);
  // if(!top.branch_found_unsat()){
  //   // std::cout << "current remain comp: " << top.currentRemainingComponent() << std::endl;
  //   std::cout << "findNext component stack: " << component_stack_.back()->num_variables() << std::endl;
  //   bool satisfied = ana_.isSatisfied(LiteralID(top.getbranchvar(), true));
  //   // if (top.isSecondBranch()){
  //   //   std::cout << "findNext var: " << top.getbranchvar() << " branch: " << top.isSecondBranch() << std::endl;
  //   //   std::cout << "findNext satisfied(true): " << ana_.isSatisfied(LiteralID(top.getbranchvar(), true)) << std::endl;
  //   //   std::cout << "findNext satisfied(false): " << ana_.isSatisfied(LiteralID(top.getbranchvar(), false)) << std::endl;
  //   // }
  //   // else {
  //   //   std::cout << "findNext var: " << top.getbranchvar() << " branch: " << top.isSecondBranch() << std::endl;
  //   //   std::cout << "findNext satisfied(true): " << ana_.isSatisfied(LiteralID(top.getbranchvar(), true)) << std::endl;
  //   //   std::cout << "findNext satisfied(false): " << ana_.isSatisfied(LiteralID(top.getbranchvar(), false)) << std::endl;
  //   // }
  //   cout << "findNext satisfied: " << satisfied << endl;
  //   if (satisfied){
  //     top.includeSolution(1);
  //   }
  //   else {
  //     for (auto vt = component_stack_.back()->varsBegin(); *vt != varsSENTINEL; vt++){
  //       std::cout << "findNext component var: " << *vt << std::endl;
  //       if(*vt != top.getbranchvar() && !isExistVariable_(*vt)){
  //         bool pos_var = ana_.isSatisfied(LiteralID(*vt, true));
  //         if (pos_var){
  //           top.includeSolution(probability_(*vt));
  //         }
  //         else{
  //           top.includeSolution(1-probability_(*vt));
  //         }
  //         // std::cout << "branch 0 sat(false): " << ana_.isSatisfied(LiteralID(*vt, false)) << std::endl;
  //         // std::cout << "branch 1 sat(true): " << ana_.isSatisfied(LiteralID(*vt, true)) << std::endl;
  //         // std::cout << "branch 0 rel(false): " << ana_.isResolved(LiteralID(*vt, false)) << std::endl;
  //         // std::cout << "branch 1 rel(true): " << ana_.isResolved(LiteralID(*vt, true)) << std::endl;
  //         // std::cout << "branch 0 act(false): " << ana_.isActive(LiteralID(*vt, false)) << std::endl;
  //         // std::cout << "branch 1 act(true): " << ana_.isActive(LiteralID(*vt, true)) << std::endl;
  //         // if (!ana_.isSatisfied(LiteralID(*vt, false))){
  //         //   top.includeSolution(probability_(*vt));
  //         // }
  //         // else{
  //         //   top.includeSolution(1-probability_(*vt));
  //         // }
  //       }
  //       else if(*vt != top.getbranchvar()) {
  //         top.includeSolution(1);
  //       }
  //       // else if (*vt != top.getbranchvar() && satisfied){
  //       //   top.includeSolution(1);
  //       // }
  //     }
  //   }
    
  //   cout << "findNext remain comp of: " << top.literal_stack_ofs() << endl;
  //   // component
  //   // top.includeSolution(1);
  //   std::cout<<"findNext var: "<<top.getbranchvar() <<" branch: "<< top.isSecondBranch()<<" prob: "<<1<<'\n';
  //   // if(isExistVariable_(top.getbranchvar())){
  //   //   top.includeSolution(1);}
  //   // else{
  //   //   auto prob =probability_(top.getbranchvar());
  //   //   std::cout<<"findNext var"<<top.getbranchvar()<<"prob"<<prob<<'\n';
  //   //   if(top.isSecondBranch()) top.includeSolution(prob);
  //   //   else top.includeSolution(1-prob); // this is a model count prob = 1
  //   // }
  //   // std::cout << "no remaining components, include solution 1" << std::endl;
  // }
  // top.includeSolution(1);
  
  return false;
}

void ComponentManager::recordRemainingCompsFor(StackLevel &top)
{
  Component &super_comp = superComponentOf(top);
  unsigned new_comps_start_ofs = component_stack_.size();

  ana_.setupAnalysisContext(top, super_comp);
  std::cout << "record remaining comps for: " << top.getbranchvar() << std::endl;
  for (auto vt = super_comp.varsBegin(); *vt != varsSENTINEL; vt++)
  {
    std::cout << "record remaining comps for: " << *vt << " isUnseen and activate: " << ana_.isUnseenAndActive(*vt) << std::endl;
    if (ana_.isUnseenAndActive(*vt) && ana_.exploreRemainingCompOf(*vt, isExistVariable_(*vt), probability_(*vt))) //hjko1107
    {

      Component *p_new_comp = ana_.makeComponentFromArcheType();
      CacheableComponent *packed_comp = NULL;
      if (config_.perform_pcc)
      {
#ifdef DOPCC
        packed_comp = new CacheableComponent(seedforCLHASH, ana_.getArchetype().current_comp_for_caching_);
#endif
      }
      else
      {
        packed_comp = new CacheableComponent(ana_.getArchetype().current_comp_for_caching_);
      }
      if (!cache_.manageNewComponent(top, *packed_comp))
      {
        component_stack_.push_back(p_new_comp);
        p_new_comp->set_id(cache_.storeAsEntry(*packed_comp, super_comp.id()));
      }
      else
      {
        //cache score should be decreased since we have a cache hit
        if (config_.use_csvsads)
        {
          statistics_.numcachedec_++;
          if (statistics_.numcachedec_ % 128 == 0)
          {
            increasecachescores();
          }
          for (vector<VariableIndex>::const_iterator it = p_new_comp->varsBegin(); *it != varsSENTINEL; it++)
          {
            cachescore_[*it] -= 1;
          }
        }
        delete packed_comp;
        delete p_new_comp;
      }
    }
  }
  top.set_unprocessed_components_end(component_stack_.size());
  sortComponentStackRange(new_comps_start_ofs, component_stack_.size());
}

#endif /* COMPONENT_MANAGEMENT_H_ */
