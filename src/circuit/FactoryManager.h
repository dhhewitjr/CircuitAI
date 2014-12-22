/*
 * FactoryManager.h
 *
 *  Created on: Dec 1, 2014
 *      Author: rlcevg
 */

#ifndef SRC_CIRCUIT_FACTORYMANAGER_H_
#define SRC_CIRCUIT_FACTORYMANAGER_H_

#include "Module.h"
#include "FactoryTask.h"

#include <map>
#include <list>

namespace springai {
	class UnitDef;
}

namespace circuit {

class CFactoryManager: public virtual IModule {
public:
	CFactoryManager(CCircuitAI* circuit);
	virtual ~CFactoryManager();

	virtual int UnitCreated(CCircuitUnit* unit, CCircuitUnit* builder);
	virtual int UnitFinished(CCircuitUnit* unit);
	virtual int UnitIdle(CCircuitUnit* unit);
	virtual int UnitDestroyed(CCircuitUnit* unit, CCircuitUnit* attacker);

	CFactoryTask* EnqueueTask(CFactoryTask::Priority priority,
							  springai::UnitDef* buildDef,
							  const springai::AIFloat3& position,
							  CFactoryTask::TaskType type,
							  int quantity,
							  float radius);
	void DequeueTask(CFactoryTask* task);
	float GetFactoryPower();
	bool CanEnqueueTask();
	const std::list<CFactoryTask*>& GetTasks() const;
	CCircuitUnit* NeedUpgrade();
	CCircuitUnit* GetRandomFactory();

	CCircuitUnit* GetClosestHaven(CCircuitUnit* unit) const;

private:
	void Watchdog();
	void AssignTask(CCircuitUnit* unit);
	void ExecuteTask(CCircuitUnit* unit);

	Handlers1 finishedHandler;
	Handlers1 idleHandler;
	Handlers2 destroyedHandler;

	std::map<CCircuitUnit*, CFactoryTask*> unfinishedUnits;
	std::map<CFactoryTask*, std::list<CCircuitUnit*>> unfinishedTasks;
	std::list<CFactoryTask*> factoryTasks;  // owner
	float factoryPower;

	std::map<CCircuitUnit*, std::list<CCircuitUnit*>> factories;
	springai::UnitDef* assistDef;

	std::set<CCircuitUnit*> havens;
};

} // namespace circuit

#endif // SRC_CIRCUIT_FACTORYMANAGER_H_