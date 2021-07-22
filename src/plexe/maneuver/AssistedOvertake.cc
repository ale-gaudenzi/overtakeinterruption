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
            LOG << positionHelper->getId()
                       << " cannot begin the maneuver because already involved in another one\n";
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
        LOG << "maneuver initialized";
        return true;
    } else {
        LOG << "invalid state";
        return false;
    }
}

void AssistedOvertake::startManeuver(const void *parameters) {
    if (initializeOvertakeManeuver(parameters)) {
        // send overtake request to leader
        LOG << positionHelper->getId()
                   << " sending OvertakePlatoonRequesto to platoon with id "
                   << targetPlatoonData->platoonId << " (leader id "
                   << targetPlatoonData->platoonLeader << ")\n";
        OvertakeRequest *req = createOvertakeRequest(positionHelper->getId(),
                positionHelper->getExternalId(), targetPlatoonData->platoonId,
                targetPlatoonData->platoonLeader);
        app->sendUnicast(req, targetPlatoonData->platoonLeader);
    }
}

void AssistedOvertake::onPlatoonBeacon(const PlatooningBeacon *pb) {
    if (overtakeState == OvertakeState::M_OT) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::OVERTAKER);

        if (pb->getVehicleId() == targetPlatoonData->platoonLeader) { // solo se pb viene da leader perche mi dava errore massimum unicast retries

            veins::TraCICoord traciPosition =
                    mobility->getManager()->getConnection()->omnet2traci(
                            mobility->getPositionAt(simTime()));

            PositionAck *ack = createPositionAck(positionHelper->getId(),
                    positionHelper->getExternalId(),
                    targetPlatoonData->platoonId,
                    targetPlatoonData->platoonLeader, traciPosition.x);

            app->sendUnicast(ack, targetPlatoonData->platoonLeader);

            double leaderPosition = pb->getPositionX();

            double distance = leaderPosition - traciPosition.x;

            if (distance < -10) {
                std::cout << positionHelper->getId()
                        << "\n finishing overtake and returning to main lane \n";
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

    if (overtakeState == OvertakeState::L_WAIT_POSITION) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::LEADER);

        double overtakerPosition = ack->getPosition();

        for (int i = 1; i < 7; i++) {
            if (carPositions[i] != 0 && carPositions[i] < overtakerPosition
                    && carPositions[i - 1] >= overtakerPosition) {
                std::cout << " Nuovo leader temporaneo: " << i;
                tempLeaderId = i;
            } else if (i == 1 && carPositions[i] < overtakerPosition) {
                tempLeaderId = 0; // il temp leader è il leader stesso, che rallenta e fa passare senza modificare la formazione del platoon
            }
        }
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

bool AssistedOvertake::processOvertakeRequest(const OvertakeRequest *msg) {
    if (msg->getPlatoonId() != positionHelper->getPlatoonId())
        return false;

    if (app->getPlatoonRole() != PlatoonRole::LEADER
            && app->getPlatoonRole() != PlatoonRole::NONE)
        return false;

    bool permission = app->isOvertakeAllowed();

    // send response to the overtaker
    std::cout << positionHelper->getId()
            << " sending OvertakeResponse to vehicle with id "
            << msg->getVehicleId() << " (permission to overtake: "
            << (permission ? "permitted" : "not permitted") << ")\n";

    OvertakeResponse *response = createOvertakeResponse(positionHelper->getId(),
            positionHelper->getExternalId(), msg->getPlatoonId(),
            msg->getVehicleId(), permission);
    app->sendUnicast(response, msg->getVehicleId());

    if (!permission)
        return false;

    app->setInManeuver(true, this);
    app->setPlatoonRole(PlatoonRole::LEADER);

    overtakerData.reset(new OvertakerData());
    overtakerData->from(msg);

    overtakeState = OvertakeState::L_WAIT_POSITION;
    std::cout << " responso positivo";
    // carPositions[7] = {0};
    return true;
}

void AssistedOvertake::handleOvertakeRequest(const OvertakeRequest *msg) {
    if (processOvertakeRequest(msg)) {
        // LOG << positionHelper->getId() << " sending MoveToPosition to vehicle with id " << msg->getVehicleId() << "\n";
        // MoveToPosition* mtp = createMoveToPosition(positionHelper->getId(), positionHelper->getExternalId(), positionHelper->getPlatoonId(), joinerData->joinerId, positionHelper->getPlatoonSpeed(), positionHelper->getPlatoonLane(), joinerData->newFormation);
        // app->sendUnicast(mtp, overtakerData->overtakerId);
    }
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
        LOG << positionHelper->getId()
                   << " received OvertakeResponse (allowed to overtake)\n";

        overtakeState = OvertakeState::M_OT;
        plexeTraciVehicle->changeLaneRelative(1, 1);

    } else {
        LOG << positionHelper->getId()
                   << " received OvertakeResponse (not allowed to overtake)\n";
        // abort maneuver
        overtakeState = OvertakeState::IDLE;
        app->setPlatoonRole(PlatoonRole::NONE);
        app->setInManeuver(false, nullptr);
    }
}

void AssistedOvertake::pauseOvertake() {
    if (overtakeState == OvertakeState::L_WAIT_POSITION) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::LEADER);

        PauseOvertakeOrder *order = createPauseOvertakeOrder(
                positionHelper->getId(), positionHelper->getExternalId(),
                msg->getPlatoonId(), msg->getVehicleId(), tempLeaderId);
        send(order, "out");

        overtakeState = OvertakeState::L_WAIT_JOIN;
    }
}

void AssistedOvertake::handlePauseOvertakeOrder(const PauseOvertakeOrder *msg) {
    if (overtakeState == OvertakeState::M_OT) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::OVERTAKER);
        overtakeState == OvertakeState::M_WAIT_GAP;


    } else if (overtakeState == OvertakeState::IDLE
            && app->getPlatoonRole() == PlatoonRole::FOLLOWER) {



        if (msg->getTempLeader == positionHelper->getId()) { //diventare leader temporaneo
            overtakeState == OvertakeState::F_OPEN_GAP;
            plexeTraciVehicle->setActiveController(ACC);
            double distance = 5; //dopo mettere quella giusta
            double relSpeed;

            while (distance < 20) { //togliere magic number
                plexeTraciVehicle->getRadarMeasurements(distance, relSpeed);
                plexeTraciVehicle->setCruiseControlDesiredSpeed(80.0 / 3.6); // togliere magic number
                // qualcosa tipo wait?
            }

            plexeTraciVehicle->setCruiseControlDesiredSpeed(100.0 / 3.6); // togliere magic number
            OpenGapAck *ack = createOpenGapAck(positionHelper->getId(),
                    positionHelper->getExternalId(), msg->getPlatoonId(),
                    msg->getVehicleId());
            app->sendUnicast(ack, overtakerId);

        } else if (msg->getTempLeader < positionHelper->getId()) { //seguire nuovo leader
            overtakeState == OvertakeState::V_FOLLOWF;


        } else { //continuare a seguire il leader solito

        }
    }
}

} // namespace plexe
