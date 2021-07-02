//
// Copyright (C) 2014-2021 Michele Segata <segata@ccs-labs.org>
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "OvertakeTrafficManager.h"

namespace plexe {

Define_Module(OvertakeTrafficManager);


void OvertakeTrafficManager::initialize(int stage)
{
    PlatoonsTrafficManager::initialize(stage);

    if (stage == 0) {
        insertOvertakerMessage = new cMessage("");
        scheduleAt(platoonInsertTime + SimTime(5), insertOvertakerMessage);
    }
}

void OvertakeTrafficManager::handleSelfMsg(cMessage* msg)
{

    PlatoonsTrafficManager::handleSelfMsg(msg);

    if (msg == insertOvertakerMessage) {
        insertOvertaker();
    }
}

void OvertakeTrafficManager::insertOvertaker()
{
    automated.position = 0;
    automated.lane = 0;
    addVehicleToQueue(0, automated);
}

OvertakeTrafficManager::~OvertakeTrafficManager()
{
    cancelAndDelete(insertOvertakerMessage);
    insertOvertakerMessage = nullptr;
}

} // namespace plexe
