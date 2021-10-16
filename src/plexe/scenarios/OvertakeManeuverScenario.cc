//
// Copyright (C) 2012-2021 Michele Segata <segata@ccs-labs.org>
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

#include "OvertakeManeuverScenario.h"

namespace plexe {

Define_Module(OvertakeManeuverScenario);

void OvertakeManeuverScenario::initialize(int stage) {

    BaseScenario::initialize(stage);

    if (stage == 2) {
        app = FindModule<GeneralPlatooningApp*>::findSubModule(
                getParentModule());
        prepareManeuverCars(0);
    }
}

void OvertakeManeuverScenario::setupFormation() {
    std::vector<int> formation;
    for (int i = 0; i < 8; i++)
        formation.push_back(i);
    positionHelper->setPlatoonFormation(formation);
}

void OvertakeManeuverScenario::prepareManeuverCars(int platoonLane) {

    switch (positionHelper->getId()) {

    case 0: {
        // this is the leader
        plexeTraciVehicle->setCruiseControlDesiredSpeed(100.0 / 3.6);
        plexeTraciVehicle->setActiveController(ACC);
        plexeTraciVehicle->setFixedLane(platoonLane);
        app->setPlatoonRole(PlatoonRole::LEADER);
        break;
    }

    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    {
        // these are the followers which are already in the platoon
        plexeTraciVehicle->setCruiseControlDesiredSpeed(130.0 / 3.6);
        plexeTraciVehicle->setActiveController(CACC);
        plexeTraciVehicle->setFixedLane(platoonLane);
        app->setPlatoonRole(PlatoonRole::FOLLOWER);
        break;
    }

    case 8: {
        // this is the car which will overtake
        plexeTraciVehicle->setCruiseControlDesiredSpeed(130.0 / 3.6);
        plexeTraciVehicle->setActiveController(ACC);
        plexeTraciVehicle->setFixedLane(platoonLane);

        // after 30 seconds of simulation, start the maneuver
        startManeuver = new cMessage();
        scheduleAt(simTime() + SimTime(30), startManeuver);
        std::cout << " sending self message  \n"; //debugc
        break;
    }
    }
}

OvertakeManeuverScenario::~OvertakeManeuverScenario() {
    cancelAndDelete(startManeuver);
    startManeuver = nullptr;
}

void OvertakeManeuverScenario::handleSelfMsg(cMessage *msg) {
    // this takes car of feeding data into CACC and reschedule the self message
    BaseScenario::handleSelfMsg(msg);

    if (msg == startManeuver) {
        std::cout << " starting maneuver \n"; //debugc
        app->startOvertakeManeuver(0, 0);
        //oppositeVehicle = new cMessage();
        //scheduleAt(SimTime(44), oppositeVehicle);
    }
/*
    if (msg == oppositeVehicle) {
        std::cout << "vehicle incoming"; //debugc
        app->pauseOvertakeManeuver();
    }*/
}

} // namespace plexe
