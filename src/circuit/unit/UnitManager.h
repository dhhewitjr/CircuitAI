/*
 * UnitManager.h
 *
 *  Created on: Jan 15, 2015
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_UNIT_UNITMANAGER_H_
#define SRC_CIRCUIT_UNIT_UNITMANAGER_H_

namespace circuit {

class CCircuitUnit;
class CCircuitAI;
class IUnitTask;
class CIdleTask;
class CRetreatTask;

class IUnitManager {
protected:
	IUnitManager(CCircuitAI* circuit);
public:
	virtual ~IUnitManager();

	virtual CCircuitAI* GetCircuit() = 0;
	virtual void AssignTask(CCircuitUnit* unit) = 0;
	// TODO: Remove
//	virtual void ExecuteTask(CCircuitUnit* unit) = 0;
	virtual void AbortTask(IUnitTask* task) = 0;
	virtual void SpecialCleanUp(CCircuitUnit* unit) = 0;
	virtual void SpecialProcess(CCircuitUnit* unit) = 0;
	virtual void FallbackTask(CCircuitUnit* unit) = 0;

	CIdleTask* GetIdleTask();
	CRetreatTask* GetRetreatTask();

protected:
	CIdleTask* idleTask;
	CRetreatTask* retreatTask;
};

} // namespace circuit

#endif // SRC_CIRCUIT_UNIT_UNITMANAGER_H_