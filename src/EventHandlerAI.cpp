/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "EventHandlerAI.h"
#include "circuit/utils.h"

// generated by the C++ Wrapper scripts
#include "OOAICallback.h"
#include "Unit.h"
#include "UnitDef.h"
#include "Game.h"
#include "Log.h"

namespace eventhandlerai {

using namespace circuit;

CEventHandlerAI::CEventHandlerAI(springai::OOAICallback* callback) :
		callback(callback),
		skirmishAIId(callback != NULL ? callback->GetSkirmishAIId() : -1),
		circuit(NULL)
{
	circuit = new CCircuit(callback);
}

CEventHandlerAI::~CEventHandlerAI()
{
	delete circuit;
}

int CEventHandlerAI::HandleEvent(int topic, const void* data)
{
	int ret = ERROR_UNKNOWN;

	switch (topic) {
		case EVENT_INIT: {
			struct SInitEvent* evt = (struct SInitEvent*)data;
			ret = circuit->Init(evt->skirmishAIId, evt->callback);
			break;
		}
		case EVENT_RELEASE: {
			struct SReleaseEvent* evt = (struct SReleaseEvent*)data;
			ret = circuit->Release(evt->reason);
			break;
		}
		case EVENT_UPDATE: {
			struct SUpdateEvent* evt = (struct SUpdateEvent*)data;
			ret = circuit->Update(evt->frame);
			break;
		}
		case EVENT_MESSAGE: {
			struct SMessageEvent* evt = (struct SMessageEvent*)data;
			std::string msgText = std::string(evt->message) + std::string(", player: ") + utils::int_to_string(evt->player);
			callback->GetLog()->DoLog(msgText.c_str());
			ret = 0;
			break;
		}
		case EVENT_UNIT_CREATED: {
			//struct SUnitCreatedEvent* evt = (struct SUnitCreatedEvent*) data;
			//int unitId = evt->unit;

			// TODO: wrapp events and commands too

			const std::vector<springai::Unit*> friendlyUnits = callback->GetFriendlyUnits();
			std::string msgText = std::string("Hello Engine (from CircuitAI), my units num is ") + utils::int_to_string(friendlyUnits.size());
			if (!friendlyUnits.empty()) {
				springai::Unit* unit = friendlyUnits[0];
				springai::UnitDef* unitDef = unit->GetDef();
				std::string unitDefName = unitDef->GetName();
				msgText = msgText + ", first friendly units def name is: " + unitDefName;
			}
			callback->GetGame()->SendTextMessage("Hallo World!", 0);
			callback->GetGame()->SendTextMessage(msgText.c_str(), 0);
			ret = 0;
			break;
		}
		case EVENT_LUA_MESSAGE: {
			struct SLuaMessageEvent* evt = (struct SLuaMessageEvent*) data;
			ret = circuit->LuaMessage(evt->inData);
			break;
		}
		default: {
			std::string msgText = std::string("<CircuitAI> warning topic: ") + utils::int_to_string(topic);
			callback->GetLog()->DoLog(msgText.c_str());
			ret = 0;
			break;
		}
	}

	return ret;
}

} // namespace eventhandlerai
