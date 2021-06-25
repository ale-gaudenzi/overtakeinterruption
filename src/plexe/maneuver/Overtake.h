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

#ifndef OVERTAKE_H_
#define OVERTAKE_H_

#include <algorithm>

#include "plexe/maneuver/JoinManeuver.h"
#include "plexe/utilities/BasePositionHelper.h"

#include "veins/modules/mobility/traci/TraCIConstants.h"
#include "veins/base/utils/Coord.h"

using namespace veins;

namespace plexe {

struct OvertakeParameters {
    int platoonId;
    int leaderId;
    int position;
};


class Overtake : public Maneuver {

public:
    /**
     * Constructor
     *
     * @param app pointer to the generic application used to fetch parameters and inform it about a concluded maneuver
     */
    Overtake(GeneralPlatooningApp* app);
    virtual ~Overtake(){};

    virtual void startManeuver(const void* parameters) override;

    virtual void abortManeuver() override;

    virtual void onPlatoonBeacon(const PlatooningBeacon* pb) override;

    virtual void onFailedTransmissionAttempt(const ManeuverMessage* mm) override;

    virtual void handleOvertakeRequest(const OvertakeRequest* msg) override;
    virtual void handleOvertakeResponse(const OvertakeResponse* msg) override;
    //virtual void handlePositionAck(const PositionAck* msg) override;
    //virtual void handleChangeFAck(const ChangeFAck* msg) override;
    //virtual void handlePauseOrd(const PauseOrd* msg) override;
    //virtual void handleOpenAck(const OpenAck* msg) override;
    //virtual void handleLaneAck(const LaneAck* msg) override;
    //virtual void handleJoinAck(const JoinAck* msg) override;
    //virtual void handleResumeAck(const ResumeAck* msg) override;
    //virtual void handleOtFinish(const OtFinish* msg) override;

    OvertakeRequest* createOvertakeRequest(int vehicleId, std::string externalId, int platoonId, int destinationId, int currentLaneIndex, double xPos, double yPos);


    OvertakeResponse* createOvertakeResponse(int vehicleId, std::string externalId, int platoonId, int destinationId, bool permitted)


protected:
    /** Possible states a vehicle can be in during a overtake maneuver */
    enum class OvertakeState {
        IDLE, ///< The maneuver did not start
        // Overtake
        M_WAIT_REPLY,
        M_OT,
        M_WAIT_GAP,
        M_MOVE_LANE,
        M_FOLLOW,
        // Leader
        L_DECISION,
        L_WAIT_POSITION,
        L_DECIDEF,
        L_WAIT_JOIN,
        L_WAIT_DANGER,
        // F temporary leader
        F_WAIT,
        F_OPEN_GAP,
        F_TEMP_LEADER,
        F_CLOSE_GAP,
        // Vehicle V
        V_FOLLOWF,
    };

    /** data that a joiner stores about a Platoon it wants to join */
    struct TargetPlatoonData {
        int platoonId; ///< the id of the platoon to join
        int platoonLeader; ///< the if ot the leader of the platoon
        int platoonLane; ///< the lane the platoon is driving on
        double platoonSpeed; ///< the speed of the platoon
        int joinIndex; ///< position in the new platoon formation, 0 based !
        std::vector<int> newFormation; ///< the new formation of the platoon
        Coord lastFrontPos; ///< the last kwown position of the front vehicle

        TargetPlatoonData()
        {
            platoonId = INVALID_PLATOON_ID;
            platoonLeader = TraCIConstants::INVALID_INT_VALUE;
            platoonLane = TraCIConstants::INVALID_INT_VALUE;
            platoonSpeed = TraCIConstants::INVALID_DOUBLE_VALUE;
            joinIndex = TraCIConstants::INVALID_INT_VALUE;
        }

        void from(const MoveToPosition* msg)
        {
            platoonId = msg->getPlatoonId();
            platoonLeader = msg->getVehicleId();
            platoonLane = msg->getPlatoonLane();
            platoonSpeed = msg->getPlatoonSpeed();
            newFormation.resize(msg->getNewPlatoonFormationArraySize());
            for (unsigned int i = 0; i < msg->getNewPlatoonFormationArraySize(); i++) {
                newFormation[i] = msg->getNewPlatoonFormation(i);
            }
            const auto it = std::find(newFormation.begin(), newFormation.end(), msg->getDestinationId());
            if (it != newFormation.end()) {
                joinIndex = std::distance(newFormation.begin(), it);
                ASSERT(newFormation.at(joinIndex) == msg->getDestinationId());
            }
        }

        int frontId() const
        {
            return newFormation.at(joinIndex - 1);
        }
    };

    struct OvertakerData {
        int overtakerId;
        int overtakerLane;
        std::vector<int> newFormation;

        OvertakerData()
        {
            overtakerId = TraCIConstants::INVALID_INT_VALUE;
            overtakerLane = TraCIConstants::INVALID_INT_VALUE;
        }


        void from(const OvertakePlatoonRequest* msg)
        {
            overtakerId = msg->getVehicleId();
            overtakerLane = msg->getCurrentLaneIndex();
        }
    };

    /** the current state in the overtake maneuver */
    OvertakeState overtakeState;

    /** the data about the target platoon */
    std::unique_ptr<TargetPlatoonData> targetPlatoonData;

    /** the data about the current overtaker */
    std::unique_ptr<JoinerData> overtakerData;

    /** initializes a overtake maneuver, setting up required data */
    bool initializeOvertakeManeuver(const void* parameters);

    /** initializes the handling of a overtake request */
    bool processOvertakeRequest(const OvertakePlatoonRequest* msg);

    OvertakeRequest* createOvertakeRequest(int vehicleId, std::string externalId, int platoonId, int destinationId, int currentLaneIndex, double xPos, double yPos);

    OvertakeResponse* createOvertakeResponse(int vehicleId, std::string externalId, int platoonId, int destinationId, int currentLaneIndex, double xPos, double yPos);


};

} // namespace plexe

#endif
