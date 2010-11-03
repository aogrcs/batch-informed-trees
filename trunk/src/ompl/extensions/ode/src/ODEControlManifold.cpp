/*********************************************************************
* Software License Agreement (BSD License)
* 
*  Copyright (c) 2010, Rice University
*  All rights reserved.
* 
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
* 
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Rice University nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
* 
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#include "ompl/extensions/ode/ODEControlManifold.h"

namespace ompl
{

    /// @cond IGNORE
    struct CallbackParam
    {
	const control::ODEEnvironment *env;
	bool                           collision;
    };
    /// @cond

    void nearCallback(void *data, dGeomID o1, dGeomID o2)
    {
        dBodyID b1 = dGeomGetBody(o1);
	dBodyID b2 = dGeomGetBody(o2);
        
	if (b1 && b2 && dAreConnectedExcluding(b1, b2, dJointTypeContact)) return;
	
	CallbackParam *cp = reinterpret_cast<CallbackParam*>(data);
		
	const unsigned int maxContacts = cp->env->getMaxContacts(o1, o2);
	dContact contact[maxContacts];
	for (unsigned int i = 0; i < maxContacts; ++i)
	    cp->env->setupContact(contact[i]);
	
	if (int numc = dCollide(o1, o2, maxContacts, &contact[0].geom, sizeof(dContact)))
	{
	    for (int i = 0; i < numc; ++i)
	    {
		dJointID c = dJointCreateContact(cp->env->world, cp->env->contactGroup, contact + i);
		dJointAttach(c, b1, b2);
		if (!cp->env->isValidCollision(o1, o2, contact[i]))
		    cp->collision = true;
	    }
	}
    }
}

void ompl::control::ODEControlManifold::propagate(const base::State *state, const Control* control, const double duration, const unsigned int step, base::State *result) const
{
    const ODEEnvironment &env = stateManifold_->as<ODEStateManifold>()->getEnvironment();
    env.mutex.lock();
    
    // place the ODE world at the start state
    stateManifold_->as<ODEStateManifold>()->writeState(state);

    // apply the controls
    env.applyControl(control->as<RealVectorControlManifold::ControlType>()->values, step);

    // created contacts as needed
    CallbackParam cp = { &env, false };    
    for (unsigned int i = 0 ; i < env.collisionSpaces.size() ; ++i)
	dSpaceCollide(env.collisionSpaces[i],  &cp, &nearCallback);
    
    // propagate one step forward
    dWorldQuickStep(env.world, duration);
    
    // remove created contacts
    dJointGroupEmpty(env.contactGroup);
    
    // read the final state from the ODE world
    stateManifold_->as<ODEStateManifold>()->readState(result);
    
    env.mutex.unlock();
    
    // update the collision flag for the start state, if needed
    if (state->as<ODEStateManifold::StateType>()->collision == ODEStateManifold::STATE_COLLISION_UNKNOWN)
	state->as<ODEStateManifold::StateType>()->collision = cp.collision ? ODEStateManifold::STATE_COLLISION_TRUE : ODEStateManifold::STATE_COLLISION_FALSE;
}