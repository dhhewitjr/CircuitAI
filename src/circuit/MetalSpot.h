/*
 * MetalSpot.h
 *
 *  Created on: Aug 11, 2014
 *      Author: rlcevg
 */

#ifndef METALSPOT_H_
#define METALSPOT_H_

#include "AIFloat3.h"

#include <vector>

namespace circuit {

using Metal = struct Metal {
	float income;
	springai::AIFloat3 position;
};

class CMetalSpot {
public:
	CMetalSpot(const char* setupMetal);
	virtual ~CMetalSpot();

public:
	std::vector<Metal> spots;
	std::vector<std::vector<Metal>> clusters;
	int mexPerClusterAvg;
};

} // namespace circuit

#endif // METALSPOT_H_