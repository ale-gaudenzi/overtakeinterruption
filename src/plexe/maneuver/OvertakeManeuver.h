//
// Copyright (C) 2012-2021 Michele Segata <segata@ccs-labs.org>
// Copyright (C) 2018-2021 Julian Heinovski <julian.heinovski@ccs-labs.org>
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

#ifndef OVERTAKEMANEUVER_H_
#define OVERTAKEMANEUVER_H_

#include "plexe/maneuver/Maneuver.h"

#include "plexe/messages/OvertakeResponse_m.h"
#include "plexe/messages/OvertakeRequest_m.h"
#include "plexe/messages/OvertakeFinishAck_m.h"
#include "plexe/messages/OvertakeRestart_m.h"
#include "plexe/messages/PositionAck_m.h"
#include "plexe/messages/PauseOrder_m.h"
#include "plexe/messages/OpenGapAck_m.h"
#include "plexe/messages/JoinAck_m.h"

namespace plexe {

struct OvertakeParameters {
    int platoonId;
    int leaderId;
};

class OvertakeManeuver: public Maneuver {

public:
    /**
     * Constructor
     *
     * @param app pointer to the generic application used to fetch parameters and inform it about a concluded maneuver
     */
    OvertakeManeuver(GeneralPlatooningApp *app);
    virtual ~OvertakeManeuver() {
    }
    ;

    virtual void onManeuverMessage(const ManeuverMessage *mm) override;

    virtual void changeLane() = 0;

    virtual void fakeEmergencyStart() = 0;

    virtual void fakeEmergencyFinish() = 0;

protected:
    OvertakeRequest* createOvertakeRequest(int vehicleId,
            std::string externalId, int platoonId, int destinationID);

    OvertakeResponse* createOvertakeResponse(int vehicleId,
            std::string externalId, int platoonId, int destinationID,
            bool permitted);

    OvertakeFinishAck* createOvertakeFinishAck(int vehicleId,
            std::string externalId, int platoonId, int destinationID);

    PositionAck* createPositionAck(int vehicleId, std::string externalId,
            int platoonId, int destinationID, double position);

    OpenGapAck* createOpenGapAck(int vehicleId, std::string externalId,
            int platoonId, int destinationID);

    JoinAck* createJoinAck(int vehicleId, std::string externalId, int platoonId,
            int destinationID);

    PauseOrder* createPauseOrder(int vehicleId, std::string externalId,
            int platoonId, int destinationID, int overtakerId, bool tail);

    OvertakeRestart* createOvertakeRestart(int vehicleId, std::string externalId,
                int platoonId, int destinationID);

    virtual void handleOvertakeResponse(const OvertakeResponse *msg) = 0;

    virtual void handleOvertakeRequest(const OvertakeRequest *msg) = 0;

    virtual void handleOvertakeRestart(const OvertakeRestart *msg) = 0;

    virtual void onPositionAck(const PositionAck *msg) = 0;

    virtual void onOvertakeFinishAck(const OvertakeFinishAck *msg) = 0;

    virtual void handlePauseOrder(const PauseOrder *msg) = 0;

    virtual void handleOpenGapAck(const OpenGapAck *msg) = 0;

    virtual void handleJoinAck(const JoinAck *msg) = 0;

    virtual void overtakerPause() = 0;

};

} // namespace plexe

#endif
