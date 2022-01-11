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

#include "plexe/maneuver/AssistedOvertake.h"
#include "plexe/apps/GeneralPlatooningApp.h"

namespace plexe {

AssistedOvertake::AssistedOvertake(GeneralPlatooningApp *app) :
        OvertakeManeuver(app), overtakeState(OvertakeState::IDLE) {
}

bool AssistedOvertake::initializeOvertakeManeuver(const void *parameters) {

    OvertakeParameters *pars = (OvertakeParameters*) parameters;

    if (overtakeState == OvertakeState::IDLE) {
        if (app->isInManeuver()) {
            std::cout << positionHelper->getId()
                    << " cannot begin the maneuver because already involved in another one"
                    << " -time:(" << simTime() << ") \n";
            return false;
        }

        app->setInManeuver(true, this);
        app->setPlatoonRole(PlatoonRole::OVERTAKER);

        // collect information about target Platoon
        targetPlatoonData.reset(new TargetPlatoonData());
        targetPlatoonData->platoonId = pars->platoonId;
        targetPlatoonData->platoonLeader = pars->leaderId;

        // after successful initialization we are going to send a request and wait for a reply
        overtakeState = OvertakeState::M_WAIT_REPLY;
        std::cout << "maneuver initialized" << " -time:(" << simTime()
                << ") \n";
        return true;
    } else {
        std::cout << "invalid state" << " -time:(" << simTime() << ") \n";
        return false;
    }
}

void AssistedOvertake::startManeuver(const void *parameters) {
    if (initializeOvertakeManeuver(parameters)) {
        // send overtake request to leader
        std::cout << positionHelper->getId()
                << " sending OvertakePlatoonRequest to platoon with id "
                << targetPlatoonData->platoonId << " (leader id "
                << targetPlatoonData->platoonLeader << ")" << " -time:("
                << simTime() << ") \n";
        OvertakeRequest *req = createOvertakeRequest(positionHelper->getId(),
                positionHelper->getExternalId(), targetPlatoonData->platoonId,
                targetPlatoonData->platoonLeader);
        app->sendUnicast(req, targetPlatoonData->platoonLeader);
    }

}

void AssistedOvertake::handleOvertakeRequest(const OvertakeRequest *msg) {
    if (msg->getPlatoonId() != positionHelper->getPlatoonId())
        return;

    if (app->getPlatoonRole() != PlatoonRole::LEADER)
        return;

    bool permission = app->isOvertakeAllowed();

    // send response to the overtaker
    std::cout << positionHelper->getId()
            << " sending OvertakeResponse to vehicle with id "
            << msg->getVehicleId() << " (permission to overtake: "
            << (permission ? "permitted" : "not permitted") << ") "
            << " -time:(" << simTime() << ") \n";

    OvertakeResponse *response = createOvertakeResponse(positionHelper->getId(),
            positionHelper->getExternalId(), msg->getPlatoonId(),
            msg->getVehicleId(), permission);
    app->sendUnicast(response, msg->getVehicleId());

    if (!permission)
        return;

    app->setInManeuver(true, this);
    app->setPlatoonRole(PlatoonRole::LEADER);

    overtakerData.reset(new OvertakerData());
    overtakerData->from(msg);

    overtakeState = OvertakeState::L_WAIT_POSITION;
    std::cout << "responso positivo" << " -time:(" << simTime() << ") \n";
}

void AssistedOvertake::onPlatoonBeacon(const PlatooningBeacon *pb) {
    if (overtakeState == OvertakeState::M_OT) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::OVERTAKER);

        veins::TraCICoord traciPosition =
                mobility->getManager()->getConnection()->omnet2traci(
                        mobility->getPositionAt(simTime()));

        if (pb->getVehicleId() == targetPlatoonData->platoonLeader) { // solo se pb viene da leader perche mi dava errore massimum unicast retries
                /*
                 PositionAck *ack = createPositionAck(positionHelper->getId(),
                 positionHelper->getExternalId(),
                 targetPlatoonData->platoonId,
                 targetPlatoonData->platoonLeader, traciPosition.x);

                 app->sendUnicast(ack, targetPlatoonData->platoonLeader);
                 */
            double leaderPosition = pb->getPositionX();

            distanceFromLeader = leaderPosition - traciPosition.x;

            if (distanceFromLeader < -10) {
                std::cout << positionHelper->getId()
                        << " finishing overtake and returning to main lane"
                        << " -time:(" << simTime() << ") \n";
                plexeTraciVehicle->changeLaneRelative(-1, 1);
                overtakeState = OvertakeState::IDLE;
                OvertakeFinishAck *ack = createOvertakeFinishAck(
                        positionHelper->getId(),
                        positionHelper->getExternalId(),
                        targetPlatoonData->platoonId,
                        targetPlatoonData->platoonLeader);
                app->sendUnicast(ack, targetPlatoonData->platoonLeader);

            }
        }

        if (pb->getVehicleId() == 6) {
            if (inPause == true) {
                double lastPosition = pb->getPositionX();
                distanceFromLast = lastPosition - traciPosition.x;
                if (distanceFromLast > 10) {
                    plexeTraciVehicle->changeLaneRelative(-1, 1);
                    plexeTraciVehicle->setCruiseControlDesiredSpeed(
                            100.0 / 3.6);
                    inPause = false;
                }
            }
        }

    } else if (overtakeState == OvertakeState::L_WAIT_POSITION
            && app->getPlatoonRole() == PlatoonRole::LEADER) {
        carPositions[pb->getVehicleId()] = pb->getPositionX();
        veins::TraCICoord traciPosition =
                mobility->getManager()->getConnection()->omnet2traci(
                        mobility->getPositionAt(simTime()));
        carPositions[0] = traciPosition.x;
        // std::cout << " posizione numero " << pb->getVehicleId() << " aggiunta al vettore \n";
    }
}

void AssistedOvertake::onPositionAck(const PositionAck *ack) {
    /*
     if (overtakeState == OvertakeState::L_WAIT_POSITION) {
     ASSERT(app->getPlatoonRole() == PlatoonRole::LEADER);

     double overtakerPosition = ack->getPosition();

     for (int i = 1; i < 7; i++) {
     if (carPositions[i] != 0 && carPositions[i] < overtakerPosition
     && carPositions[i - 1] >= overtakerPosition) {
     std::cout << "Nuovo leader temporaneo: " << i << " -time:("
     << simTime() << ") \n";
     tempLeaderId = i;
     } else if (i == 1 && carPositions[i] < overtakerPosition) {
     tempLeaderId = 0; // il temp leader è il leader stesso, che rallenta e fa passare senza modificare la formazione del platoon
     }
     }
     } */
    if (overtakeState == OvertakeState::L_WAIT_JOIN) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::LEADER);

        double overtakerPosition = ack->getPosition();

        for (int i = 4; i < 7; i++) {
            if (carPositions[i] != 0 && carPositions[i] < overtakerPosition
                    && carPositions[i - 1] >= overtakerPosition) {
                std::cout << "Nuovo leader temporaneo: " << i << " -time:("
                        << simTime() << ") \n";
                tempLeaderId = i - 3;
            } else if ((i < 4) && carPositions[i] < overtakerPosition) {
                tempLeaderId = 0; // il temp leader è il leader stesso, che rallenta e fa passare senza modificare la formazione del platoon
            }
        }

        PauseOrder *msgPauseF = createPauseOrder(positionHelper->getId(),
                positionHelper->getExternalId(), positionHelper->getPlatoonId(),
                tempLeaderId);
        app->sendUnicast(msgPauseF, tempLeaderId);
    }
}

void AssistedOvertake::onOvertakeFinishAck(const OvertakeFinishAck *ack) {
    if (overtakeState == OvertakeState::L_WAIT_POSITION) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::LEADER);
        overtakeState = OvertakeState::IDLE;
    }
}

void AssistedOvertake::onFailedTransmissionAttempt(const ManeuverMessage *mm) {
    throw cRuntimeError(
            "Impossible to send this packet: %s. Maximum number of unicast retries reached",
            mm->getName());
}

void AssistedOvertake::handleOvertakeResponse(const OvertakeResponse *msg) {
    if (app->getPlatoonRole() != PlatoonRole::OVERTAKER)
        return;
    if (overtakeState != OvertakeState::M_WAIT_REPLY)
        return;
    if (msg->getPlatoonId() != targetPlatoonData->platoonId)
        return;
    if (msg->getVehicleId() != targetPlatoonData->platoonLeader)
        return;

    // evaluate permission
    if (msg->getPermitted()) {
        std::cout << positionHelper->getId()
                << " received OvertakeResponse (allowed to overtake)"
                << " -time:(" << simTime() << ") \n";
        ;

        overtakeState = OvertakeState::M_OT;
        plexeTraciVehicle->changeLaneRelative(1, 1);

    } else {
        std::cout << positionHelper->getId()
                << " received OvertakeResponse (not allowed to overtake)"
                << " -time:(" << simTime() << ") \n";
        ;
        // abort maneuver
        overtakeState = OvertakeState::IDLE;
        app->setPlatoonRole(PlatoonRole::NONE);
        app->setInManeuver(false, nullptr);
    }
}

void AssistedOvertake::handlePauseOrder(const PauseOrder *msg) {
    if (app->getPlatoonRole() == PlatoonRole::OVERTAKER) {
        overtakerPause();
    }
    if (app->getPlatoonRole() == PlatoonRole::FOLLOWER) {
        followerOpenGap();
    }
    // cosa fare con gli altri

}

//leader lancia messaggio in caso di emergenza
void AssistedOvertake::abortManeuver() {
    if (app->getPlatoonRole() == PlatoonRole::LEADER) {

        PauseOrder *msgPauseM = createPauseOrder(positionHelper->getId(),
                positionHelper->getExternalId(), positionHelper->getPlatoonId(),
                overtakerData->overtakerId);

        app->sendUnicast(msgPauseM, overtakerData->overtakerId);

        std::cout << positionHelper->getId() << " invio ordine pausa overtake"
                << " -time:(" << simTime() << ") \n";

        overtakeState = OvertakeState::L_WAIT_JOIN;
    }
}

void AssistedOvertake::overtakerPause() {
    if (app->getPlatoonRole() == PlatoonRole::OVERTAKER) {
        overtakeState == OvertakeState::M_WAIT_GAP;
        std::cout << positionHelper->getId()
                << " ricevuto ordine pausa overtake" << " -time:(" << simTime()
                << "), aspetta gap \n";
    }
    //per adesso torno in coda

    plexeTraciVehicle->setCruiseControlDesiredSpeed(100.0 / 3.6);

    veins::TraCICoord traciPosition =
            mobility->getManager()->getConnection()->omnet2traci(
                    mobility->getPositionAt(simTime()));

    std::cout << positionHelper->getId() << " rallenta do posizione a leader "
            << " -time:(" << simTime() << ") \n";
    PositionAck *ack = createPositionAck(positionHelper->getId(),
            positionHelper->getExternalId(), targetPlatoonData->platoonId,
            targetPlatoonData->platoonLeader, traciPosition.x);

    app->sendUnicast(ack, targetPlatoonData->platoonLeader);

    //inPause = true;

    //plexeTraciVehicle->changeLaneRelative(-1, 1);
    //plexeTraciVehicle->setCruiseControlDesiredSpeed(100.0 / 3.6);
}

void AssistedOvertake::followerOpenGap() {
    if (app->getPlatoonRole() == PlatoonRole::FOLLOWER) {
        overtakeState == OvertakeState::F_OPEN_GAP;
        std::cout << positionHelper->getId()
                << " ricevuto ordine pausa overtake" << " -time:(" << simTime()
                << "), apre gap \n";

        plexeTraciVehicle->setCACCConstantSpacing(10);

        /*
         OpenAck *ack = createOpenAck(positionHelper->getId(),
         // positionHelper->getExternalId(), targetPlatoonData->platoonId,
         0);

         app->sendUnicast(ack, 0);
         */
    }
}
void AssistedOvertake::changeLane() {
    if (app->getPlatoonRole() == PlatoonRole::OVERTAKER) {
        plexeTraciVehicle->changeLaneRelative(-1, 1);

    }

}

} // namespace plexe

