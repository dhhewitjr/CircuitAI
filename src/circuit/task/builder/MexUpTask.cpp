/*
 * MexUpTask.cpp
 *
 *  Created on: Jun 21, 2021
 *      Author: rlcevg
 */

#include "task/builder/MexUpTask.h"
#include "task/TaskManager.h"
#include "map/ThreatMap.h"
#include "module/BuilderManager.h"
#include "module/EconomyManager.h"
#include "resource/MetalManager.h"
#include "CircuitAI.h"
#include "util/Utils.h"

#include "spring/SpringCallback.h"
#include "spring/SpringMap.h"

#include "AISCommands.h"

namespace circuit {

using namespace springai;

CBMexUpTask::CBMexUpTask(ITaskManager* mgr, Priority priority,
						 CCircuitDef* buildDef, int spotId, const AIFloat3& position,
						 float cost, int timeout)
		: IBuilderTask(mgr, priority, buildDef, position, Type::BUILDER, BuildType::MEXUP, cost, 0.f, timeout)
		, spotId(spotId)
		, reclaimMex(nullptr)
{
	manager->GetCircuit()->GetEconomyManager()->SetUpgradingMexSpot(spotId, true);
}

CBMexUpTask::CBMexUpTask(ITaskManager* mgr)
		: IBuilderTask(mgr, Type::BUILDER, BuildType::MEXUP)
		, spotId(-1)
		, reclaimMex(nullptr)
{
}

CBMexUpTask::~CBMexUpTask()
{
}

void CBMexUpTask::Finish()
{
	IBuilderTask::Finish();

	CCircuitAI* circuit = manager->GetCircuit();
	const float maxRange = buildDef->GetExtrRangeM();
	CCircuitUnit* oldMex = nullptr;
	const auto& unitIds = circuit->GetCallback()->GetFriendlyUnitIdsIn(buildPos, maxRange, false);
	float curExtract = buildDef->GetExtractsM();
	for (ICoreUnit::Id unitId : unitIds) {
		CCircuitUnit* curMex = circuit->GetTeamUnit(unitId);
		if ((curMex == nullptr) || !curMex->GetCircuitDef()->IsMex()) {
			continue;
		}
		const float extract = curMex->GetCircuitDef()->GetExtractsM();
		if (curExtract > extract) {
			oldMex = curMex;
			break;
		}
	}
	if (oldMex != nullptr) {
		circuit->GetBuilderManager()->EnqueueReclaim(priority, oldMex);
	}

	// FIXME: Won't work with EnqueueReclaim
	circuit->GetEconomyManager()->SetUpgradingMexSpot(spotId, false);
	circuit->GetBuilderManager()->UnregisterReclaim(reclaimMex);
}

void CBMexUpTask::Cancel()
{
	IBuilderTask::Cancel();

	CCircuitAI* circuit = manager->GetCircuit();
	if (spotId >= 0) {  // for broken Load
		circuit->GetEconomyManager()->SetUpgradingMexSpot(spotId, false);
	}
	circuit->GetBuilderManager()->UnregisterReclaim(reclaimMex);
}

void CBMexUpTask::Execute(CCircuitUnit* unit)
{
	executors.insert(unit);

	CCircuitAI* circuit = manager->GetCircuit();
	TRY_UNIT(circuit, unit,
		unit->CmdPriority(ClampPriority());
	)

	const int frame = circuit->GetLastFrame();
	if (target != nullptr) {
		TRY_UNIT(circuit, unit,
			unit->CmdRepair(target, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
		)
		return;
	}
	if (utils::is_valid(buildPos)) {
		if (circuit->GetMap()->IsPossibleToBuildAt(buildDef->GetDef(), buildPos, facing)) {
			TRY_UNIT(circuit, unit,
				unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
			)
			return;
		}
	}

	circuit->GetThreatMap()->SetThreatType(unit);

	// FIXME: short on purpose, won't work with EnqueueReclaim() in Finish()
	const float searchRadius = /*buildDef->GetDef()->GetResourceExtractorRange(metalRes) + */SQUARE_SIZE * 4;
	FindBuildSite(unit, position, searchRadius);

	if (utils::is_valid(buildPos)) {
		TRY_UNIT(circuit, unit,
			unit->CmdBuild(buildDef, buildPos, facing, 0, frame + FRAMES_PER_SEC * 60);
		)
	} else {
		CCircuitUnit* oldMex = nullptr;
		const auto& unitIds = circuit->GetCallback()->GetFriendlyUnitIdsIn(position, searchRadius, false);
		for (ICoreUnit::Id unitId : unitIds) {
			CCircuitUnit* curMex = circuit->GetTeamUnit(unitId);
			if (curMex == nullptr) {
				continue;
			}
			if (curMex->GetCircuitDef()->GetExtractsM() > 0.f) {
				oldMex = curMex;
				break;
			}
		}
		if (oldMex != nullptr) {
			state = State::ENGAGE;  // reclaim finished => UnitIdle => build on 2nd try
			reclaimMex = oldMex;
			circuit->GetBuilderManager()->RegisterReclaim(reclaimMex);
			TRY_UNIT(circuit, unit,
				unit->CmdReclaimUnit(oldMex, UNIT_CMD_OPTION, frame + FRAMES_PER_SEC * 60);
			)
		} else {
			manager->AbortTask(this);
		}
	}
}

void CBMexUpTask::OnUnitIdle(CCircuitUnit* unit)
{
	if (State::ENGAGE == state) {
		state = State::ROAM;
		manager->GetCircuit()->GetBuilderManager()->UnregisterReclaim(reclaimMex);
		Execute(unit);
		return;
	}

	IBuilderTask::OnUnitIdle(unit);
}

#define SERIALIZE(stream, func)	\
	utils::binary_##func(stream, spotId);		\
	utils::binary_##func(stream, reclaimMexId);

bool CBMexUpTask::Load(std::istream& is)
{
	CCircuitUnit::Id reclaimMexId;

	IBuilderTask::Load(is);
	SERIALIZE(is, read)

	CCircuitAI* circuit = manager->GetCircuit();
	reclaimMex = circuit->GetTeamUnit(reclaimMexId);

	if ((spotId < 0) || ((size_t)spotId >= circuit->GetMetalManager()->GetSpots().size())) {
		spotId = -1;
		return false;
	}
	circuit->GetEconomyManager()->SetUpgradingMexSpot(spotId, true);
	return true;
}

void CBMexUpTask::Save(std::ostream& os) const
{
	CCircuitUnit::Id reclaimMexId = (reclaimMex != nullptr) ? reclaimMex->GetId() : -1;

	IBuilderTask::Save(os);
	SERIALIZE(os, write)
}

}
// namespace circuit