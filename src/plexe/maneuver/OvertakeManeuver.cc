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

#include "plexe/maneuver/OvertakeManeuver.h"
#include "plexe/apps/GeneralPlatooningApp.h"

namespace plexe {

OvertakeManeuver::OvertakeManeuver(GeneralPlatooningApp *app) :
        Maneuver(app) {
}

void OvertakeManeuver::onManeuverMessage(const ManeuverMessage *mm) {
    if (const OvertakeRequest *msg = dynamic_cast<const OvertakeRequest*>(mm)) {
        handleOvertakeRequest(msg);
    } else if (const OvertakeResponse *msg =
            dynamic_cast<const OvertakeResponse*>(mm)) {
        handleOvertakeResponse(msg);
    } else if (const PositionAck *msg = dynamic_cast<const PositionAck*>(mm)) {
        onPositionAck(msg);
    } else if (const PauseOvertakeOrder *msg = dynamic_cast<const PauseOvertakeOrder*>(mm)) {
        handlePauseOvertakeOrder(msg);
    }

}

OvertakeResponse* OvertakeManeuver::createOvertakeResponse(int vehicleId,
        std::string externalId, int platoonId, int destinationID,
        bool permitted) {
    OvertakeResponse *msg = new OvertakeResponse("OvertakeResponse");
    app->fillManeuverMessage(msg, vehicleId, externalId, platoonId,
            destinationID);
    msg->setPermitted(permitted);
    return msg;
}

OvertakeRequest* OvertakeManeuver::createOvertakeRequest(int vehicleId,
        std::string externalId, int platoonId, int destinationID) {
    OvertakeRequest *msg = new OvertakeRequest("OvertakeRequest");
    app->fillManeuverMessage(msg, vehicleId, externalId, platoonId,
            destinationID);

    return msg;
}

PositionAck* OvertakeManeuver::createPositionAck(int vehicleId,
        std::string externalId, int platoonId, int destinationID,
        double position) {
    PositionAck *msg = new PositionAck("PositionAck");
    app->fillManeuverMessage(msg, vehicleId, externalId, platoonId,
            destinationID);

    msg->setPosition(position);

    return msg;
}

OvertakeFinishAck* OvertakeManeuver::createOvertakeFinishAck(int vehicleId,
        std::string externalId, int platoonId, int destinationID) {
    OvertakeFinishAck *msg = new OvertakeFinishAck("OvertakeFinishAck");
    app->fillManeuverMessage(msg, vehicleId, externalId, platoonId,
            destinationID);
    return msg;
}

PauseOvertakeOrder* OvertakeManeuver::createPauseOvertakeOrder(int vehicleId,
        std::string externalId, int platoonId, int destinationID,
        int tempLeader) {

    PauseOvertakeOrder *msg = new PauseOvertakeOrder("PauseOvertakeOrder");
    app->fillManeuverMessage(msg, vehicleId, externalId, platoonId,
            destinationID);

    msg->setTempLeader(tempLeader);

    return msg;
}

} // namespace plexe
