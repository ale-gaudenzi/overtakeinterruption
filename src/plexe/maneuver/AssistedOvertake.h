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

#ifndef ASSISTEDOVERTAKE_H_
#define ASSISTEDOVERTAKE_H_

#include <algorithm>

#include "plexe/maneuver/OvertakeManeuver.h"
#include "plexe/utilities/BasePositionHelper.h"

#include "veins/modules/mobility/traci/TraCIConstants.h"
#include "veins/base/utils/Coord.h"

using namespace veins;

namespace plexe {

class AssistedOvertake : public OvertakeManeuver {

public:
    /**
     * Constructor
     *
     * @param app pointer to the generic application used to fetch parameters and inform it about a concluded maneuver
     */
    AssistedOvertake(GeneralPlatooningApp* app);
    virtual ~AssistedOvertake(){};

    /**
     * This method is invoked by the generic application to start the maneuver
     *
     * @param parameters parameters passed to the maneuver
     */
    virtual void startManeuver(const void* parameters) override;

    /**
     * Handles the abortion of the maneuver when required by the generic application.
     * This method does currently nothing and it is meant for future used and improved maneuvers.
     */
    virtual void abortManeuver() override;

    /**
     * This method is invoked by the generic application when a beacon message is received
     * The maneuver must not free the memory of the message, as this might be needed by other maneuvers as well.
     */
    virtual void onPlatoonBeacon(const PlatooningBeacon* pb) override;

    /**
     * This method is invoked by the generic application when a failed transmission occurred, indicating the packet for which transmission has failed
     * The manuever must not free the memory of the message, as this might be needed by other maneuvers as well.
     */
    virtual void onFailedTransmissionAttempt(const ManeuverMessage* mm) override;

    virtual void handleOvertakeRequest(const OvertakeRequest* msg) override;

    virtual void handleOvertakeResponse(const OvertakeResponse* msg) override;

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

    /** data that a overtaker stores about a Platoon it wants to overtake */
    struct TargetPlatoonData {
        int platoonId; ///< the id of the platoon to overtake
        int platoonLeader; ///< the id ot the leader of the platoon
        int platoonLane; ///< the lane the platoon is driving on
        double platoonSpeed; ///< the speed of the platoon
        Coord lastFrontPos; ///< the last kwown position of the front vehicle

        /** c'tor for TargetPlatoonData */
        TargetPlatoonData()
        {
            platoonId = INVALID_PLATOON_ID;
            platoonLeader = TraCIConstants::INVALID_INT_VALUE;
            platoonLane = TraCIConstants::INVALID_INT_VALUE;
            platoonSpeed = TraCIConstants::INVALID_DOUBLE_VALUE;
        }
    };

    /** data that a leader stores about a overtaking vehicle */
    struct OvertakerData {
        int overtakerId; ///< the id of the vehicle Overtaking the Platoon
        int overtakerLane; ///< the lane chosen for Overtakeing the Platoon
        std::vector<int> newFormation;

        /** c'tor for OvertakerData */
        OvertakerData()
        {
            overtakerId = TraCIConstants::INVALID_INT_VALUE;
            overtakerLane = TraCIConstants::INVALID_INT_VALUE;
        }

        /**
         * Initializes the OvertakerData object with the contents of a
         * OvertakeRequest
         *
         * @param OvertakeRequest jr to initialize from
         * @see OvertakeRequest
         */
        void from(const OvertakeRequest* msg)
        {
            overtakerId = msg->getVehicleId();
        }
    };

    /** the current state in the overtake maneuver */
    OvertakeState overtakeState;

    /** the data about the target platoon */
    std::unique_ptr<TargetPlatoonData> targetPlatoonData;

    /** the data about the current overtaker */
    std::unique_ptr<OvertakerData> overtakerData;

    /** initializes an overtake maneuver, setting up required data */
    bool initializeOvertakeManeuver(const void* parameters);

    /** initializes the handling of an overtake request */
    bool processOvertakeRequest(const OvertakeRequest* msg);
};

} // namespace plexe

#endif
