/*
 * ArtilleryTask.cpp
 *
 *  Created on: Jan 6, 2016
 *      Author: rlcevg
 */

#include "task/fighter/ArtilleryTask.h"
#include "task/TaskManager.h"
#include "module/MilitaryManager.h"
#include "setup/SetupManager.h"
#include "terrain/TerrainManager.h"
#include "terrain/ThreatMap.h"
#include "terrain/PathFinder.h"
#include "unit/EnemyUnit.h"
#include "unit/action/MoveAction.h"
#include "CircuitAI.h"
#include "util/utils.h"

#include "AISCommands.h"
#include "Map.h"

namespace circuit {

using namespace springai;

CArtilleryTask::CArtilleryTask(ITaskManager* mgr)
		: IFighterTask(mgr, FightType::ARTY)
{
	position = manager->GetCircuit()->GetSetupManager()->GetBasePos();
}

CArtilleryTask::~CArtilleryTask()
{
	PRINT_DEBUG("Execute: %s\n", __PRETTY_FUNCTION__);
}

bool CArtilleryTask::CanAssignTo(CCircuitUnit* unit) const
{
	return units.empty() && unit->GetCircuitDef()->IsRoleArty();
}

void CArtilleryTask::AssignTo(CCircuitUnit* unit)
{
	IFighterTask::AssignTo(unit);

	int squareSize = manager->GetCircuit()->GetPathfinder()->GetSquareSize();
	CMoveAction* moveAction = new CMoveAction(unit, squareSize);
	unit->PushBack(moveAction);
	moveAction->SetActive(false);
}

void CArtilleryTask::RemoveAssignee(CCircuitUnit* unit)
{
	IFighterTask::RemoveAssignee(unit);
	if (units.empty()) {
		manager->AbortTask(this);
	}
}

void CArtilleryTask::Execute(CCircuitUnit* unit)
{
	Execute(unit, false);
}

void CArtilleryTask::Update()
{
	bool isExecute = (++updCount % 4 == 0);
	for (CCircuitUnit* unit : units) {
		if (unit->IsForceExecute() || isExecute) {
			Execute(unit, true);
		} else {
			IFighterTask::Update();
		}
	}
}

void CArtilleryTask::Execute(CCircuitUnit* unit, bool isUpdating)
{
	IUnitAction* act = static_cast<IUnitAction*>(unit->End());
	if (!act->IsEqual(IUnitAction::Mask::MOVE)) {
		return;
	}
	CMoveAction* moveAction = static_cast<CMoveAction*>(act);

	CCircuitAI* circuit = manager->GetCircuit();
	int frame = circuit->GetLastFrame();
	const AIFloat3& pos = unit->GetPos(frame);
	std::shared_ptr<F3Vec> pPath = std::make_shared<F3Vec>();
	CEnemyUnit* bestTarget = FindTarget(unit, pos, *pPath);

	if (bestTarget != nullptr) {
		unit->GetUnit()->Attack(bestTarget->GetUnit(), UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		unit->GetUnit()->ExecuteCustomCommand(CMD_UNIT_SET_TARGET, {(float)bestTarget->GetId()});
		moveAction->SetActive(false);
		return;
	} else if (!pPath->empty()) {
		if (pPath->size() > 2) {
			moveAction->SetPath(pPath);
			moveAction->SetActive(true);
			unit->Update(circuit);
		} else {
			unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
		}
		return;
	}

	CTerrainManager* terrainManager = circuit->GetTerrainManager();
	CThreatMap* threatMap = circuit->GetThreatMap();
	const AIFloat3& threatPos = moveAction->IsActive() ? position : pos;
	bool proceed = isUpdating && (threatMap->GetThreatAt(unit, threatPos) < threatMap->GetUnitThreat(unit));
	if (!proceed) {
		position = circuit->GetMilitaryManager()->GetScoutPosition(unit);
	}

	if (utils::is_valid(position) && terrainManager->CanMoveToPos(unit->GetArea(), position)) {
		AIFloat3 startPos = pos;
		AIFloat3 endPos = position;

		CPathFinder* pathfinder = circuit->GetPathfinder();
		pathfinder->SetMapData(unit, threatMap, frame);
		pathfinder->MakePath(*pPath, startPos, endPos, pathfinder->GetSquareSize());

		if (!pPath->empty()) {
			moveAction->SetPath(pPath);
			moveAction->SetActive(true);
			unit->Update(circuit);
			return;
		}
	}

	if (proceed) {
		return;
	}
	float x = rand() % (terrainManager->GetTerrainWidth() + 1);
	float z = rand() % (terrainManager->GetTerrainHeight() + 1);
	position = AIFloat3(x, circuit->GetMap()->GetElevationAt(x, z), z);
	unit->GetUnit()->Fight(position, UNIT_COMMAND_OPTION_RIGHT_MOUSE_KEY, frame + FRAMES_PER_SEC * 60);
	moveAction->SetActive(false);
}

void CArtilleryTask::OnUnitIdle(CCircuitUnit* unit)
{
	IFighterTask::OnUnitIdle(unit);

	if (units.find(unit) != units.end()) {
		RemoveAssignee(unit);
	}
}

CEnemyUnit* CArtilleryTask::FindTarget(CCircuitUnit* unit, const AIFloat3& pos, F3Vec& path)
{
	auto fallback = [this](CCircuitAI* circuit, const AIFloat3& pos, F3Vec& path, CPathFinder* pathfinder) {
		position = circuit->GetSetupManager()->GetBasePos();
		AIFloat3 startPos = pos;
		AIFloat3 endPos = position;
		pathfinder->MakePath(path, startPos, endPos, pathfinder->GetSquareSize());
	};

	CCircuitAI* circuit = manager->GetCircuit();
	CPathFinder* pathfinder = circuit->GetPathfinder();
	CThreatMap* threatMap = circuit->GetThreatMap();
	CCircuitDef* cdef = unit->GetCircuitDef();
	const int canTargetCat = cdef->GetTargetCategory();
	const int noChaseCat = cdef->GetNoChaseCategory();
	const float range = std::max(unit->GetUnit()->GetMaxRange(), (float)threatMap->GetSquareSize() * 2);
	const float minSqDist = SQUARE(range);

	F3Vec enemyPositions;
	threatMap->SetThreatType(unit);
	pathfinder->SetMapData(unit, threatMap, circuit->GetLastFrame());
	bool isPosSafe = (threatMap->GetThreatAt(pos) <= THREAT_MIN);

	const CCircuitAI::EnemyUnits& enemies = circuit->GetEnemyUnits();
	if (isPosSafe) {
		// Select target or position to attack
		float maxThreat = .0f;
		CEnemyUnit* bestTarget = nullptr;
		CEnemyUnit* mediumTarget = nullptr;
		CEnemyUnit* worstTarget = nullptr;
		for (auto& kv : enemies) {
			CEnemyUnit* enemy = kv.second;
			if (enemy->IsHidden() ||
				(!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
			{
				continue;
			}

			if (enemy->GetCircuitDef() == nullptr) {
				continue;
			}
			int targetCat = enemy->GetCircuitDef()->GetCategory();
			if ((targetCat & canTargetCat) == 0) {
				continue;
			}

			const float sqDist = pos.SqDistance2D(enemy->GetPos());
			if (enemy->IsInRadarOrLOS() && (sqDist < minSqDist)) {
				if (enemy->GetThreat() > maxThreat) {
					bestTarget = enemy;
					maxThreat = enemy->GetThreat();
				} else if (bestTarget == nullptr) {
					if ((targetCat & noChaseCat) == 0) {
						mediumTarget = enemy;
					} else if (mediumTarget == nullptr) {
						worstTarget = enemy;
					}
				}
				continue;
			}

			if (enemy->GetCircuitDef()->IsMobile() || ((targetCat & noChaseCat) != 0)) {
				continue;
			}
			if (sqDist < SQUARE(2000.f)) {  // maxSqDist
				enemyPositions.push_back(enemy->GetPos());
			}
		}
		if (bestTarget == nullptr) {
			bestTarget = (mediumTarget != nullptr) ? mediumTarget : worstTarget;
		}
		if (bestTarget != nullptr) {
			position = bestTarget->GetPos();
			return bestTarget;
		}

		path.clear();
		if (enemyPositions.empty()) {
			return nullptr;
		}

		AIFloat3 startPos = pos;
		pathfinder->FindBestPath(path, startPos, threatMap->GetSquareSize(), enemyPositions);

		// Check if safe path exists and shrink the path to maxRange position
		if (path.empty()) {
			fallback(circuit, pos, path, pathfinder);
			return nullptr;
		}
		const float sqRange = SQUARE(range);
		auto it = path.rbegin();
		for (; it != path.rend(); ++it) {
			if (path.back().SqDistance2D(*it) > sqRange) {
				break;
			}
		}
		--it;
		if (threatMap->GetThreatAt(*it) > THREAT_MIN) {
			fallback(circuit, pos, path, pathfinder);
		} else {
			position = path.back();
			path.resize(std::distance(it, path.rend()));
		}
	} else {
		// Avoid closest units and choose safe position
		for (auto& kv : enemies) {
			CEnemyUnit* enemy = kv.second;
			if (enemy->IsHidden() ||
				(!cdef->HasAntiWater() && (enemy->GetPos().y < -SQUARE_SIZE * 5)))
			{
				continue;
			}

			if ((enemy->GetCircuitDef() == nullptr) || enemy->GetCircuitDef()->IsMobile()) {
				continue;
			}
			int targetCat = enemy->GetCircuitDef()->GetCategory();
			if (((targetCat & canTargetCat) == 0) || ((targetCat & noChaseCat) != 0)) {
				continue;
			}

			const float sqDist = pos.SqDistance2D(enemy->GetPos());
			if (sqDist < minSqDist) {
				continue;
			}

			if (sqDist < SQUARE(2000.f)) {  // maxSqDist
				enemyPositions.push_back(enemy->GetPos());
			}
		}

		path.clear();
		if (enemyPositions.empty()) {
			return nullptr;
		}

		AIFloat3 startPos = pos;
		pathfinder->FindBestPath(path, startPos, range, enemyPositions);

		// Check if safe path exists and shrink the path to maxRange position
		if (path.empty()) {
			fallback(circuit, pos, path, pathfinder);
			return nullptr;
		}
		if (threatMap->GetThreatAt(path.back()) > THREAT_MIN) {
			fallback(circuit, pos, path, pathfinder);
		} else {
			position = path.back();
		}
	}

	return nullptr;
}

} // namespace circuit
