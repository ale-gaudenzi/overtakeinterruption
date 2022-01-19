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
        OvertakeManeuver(app), overtakeState(OvertakeState::IDLE), checkDistance(
                new cMessage("checkDistance")), checkEmergency(
                new cMessage("checkEmergency")) {
}

bool AssistedOvertake::initializeOvertakeManeuver(const void *parameters) {
    delete checkDistance;
    checkDistance = nullptr;
    delete checkEmergency;
    checkEmergency = nullptr;

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

    emergency = false;

    relativePosition = 7;

    app->scheduleAt(simTime() + 0.5, checkEmergency);

}

void AssistedOvertake::onPlatoonBeacon(const PlatooningBeacon *pb) {
    if (overtakeState == OvertakeState::M_OT) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::OVERTAKER);

        veins::TraCICoord traciPosition =
                mobility->getManager()->getConnection()->omnet2traci(
                        mobility->getPositionAt(simTime()));

        if (pb->getVehicleId() == targetPlatoonData->platoonLeader) { // solo se pb viene da leader perche mi dava errore massimum unicast retries

            PositionAck *ack = createPositionAck(positionHelper->getId(),
                    positionHelper->getExternalId(),
                    targetPlatoonData->platoonId,
                    targetPlatoonData->platoonLeader, traciPosition.x);

            app->sendUnicast(ack, targetPlatoonData->platoonLeader);

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

        if (pb->getVehicleId() == 6) { //cambiare in platoondimension
            if (inPause == true) {
                lastPosition = pb->getPositionX();
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

    if (overtakeState == OvertakeState::L_WAIT_POSITION) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::LEADER);

        double overtakerPosition = ack->getPosition();

        for (int i = 1; i <= 6; i++) {
            if (carPositions[i] < overtakerPosition
                    && carPositions[i - 1] >= overtakerPosition) {
                std::cout << "Posizione relativa: " << i << " -time:("
                        << simTime() << ") \n";
                relativePosition = i;
            } else if (i == 6 && carPositions[i] > overtakerPosition) {
                relativePosition = i + 1;
            }
        }
    }
}

void AssistedOvertake::onOvertakeFinishAck(const OvertakeFinishAck *ack) {
    if (overtakeState == OvertakeState::L_WAIT_POSITION) {
        ASSERT(app->getPlatoonRole() == PlatoonRole::LEADER);
        overtakeState = OvertakeState::IDLE;
        plexeTraciVehicle->setCruiseControlDesiredSpeed(100.0 / 3.6);
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
        plexeTraciVehicle->setCruiseControlDesiredSpeed(130.0 / 3.6);

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
        if (msg->getTail()) {
            overtakerToTail();
        } else {
            overtakerPause();
        }
    }
    if (app->getPlatoonRole() == PlatoonRole::FOLLOWER) {
        oId = msg->getOvertakerId();
        followerOpenGap();
    }
}

//leader lancia messaggio in caso di emergenza
void AssistedOvertake::abortManeuver() {
    if (app->getPlatoonRole() == PlatoonRole::LEADER) {
        overtakeState = OvertakeState::L_WAIT_JOIN;
        if (relativePosition <= 3) {
            plexeTraciVehicle->setCruiseControlDesiredSpeed(70.0 / 3.6);
        } else if (relativePosition > 3 && relativePosition < 7) {

            tempLeaderId = relativePosition - pOffset;

            std::cout << positionHelper->getId()
                                << " temp leader aggiustato" << tempLeaderId << " -time:(" << simTime()
                                << ") \n";

            PauseOrder *msgPauseM = createPauseOrder(positionHelper->getId(),
                    positionHelper->getExternalId(),
                    positionHelper->getPlatoonId(), overtakerData->overtakerId,
                    overtakerData->overtakerId, false);

            app->sendUnicast(msgPauseM, overtakerData->overtakerId);

            PauseOrder *msgPauseF = createPauseOrder(positionHelper->getId(),
                    positionHelper->getExternalId(),
                    positionHelper->getPlatoonId(), tempLeaderId,
                    overtakerData->overtakerId, false);

            app->sendUnicast(msgPauseF, tempLeaderId);

            std::cout << positionHelper->getId()
                    << " invio ordine pausa overtake" << " -time:(" << simTime()
                    << ") \n";
        } else {
            PauseOrder *msgPauseTail = createPauseOrder(positionHelper->getId(),
                    positionHelper->getExternalId(),
                    positionHelper->getPlatoonId(), overtakerData->overtakerId,
                    overtakerData->overtakerId, true);

            app->sendUnicast(msgPauseTail, overtakerData->overtakerId);
        }
    }
}

void AssistedOvertake::restartManeuver() {
    if (app->getPlatoonRole() == PlatoonRole::LEADER) {

        OvertakeRestart *restartM = createOvertakeRestart(
                positionHelper->getId(), positionHelper->getExternalId(),
                positionHelper->getPlatoonId(), overtakerData->overtakerId);

        app->sendUnicast(restartM, overtakerData->overtakerId);

        OvertakeRestart *restartF = createOvertakeRestart(
                positionHelper->getId(), positionHelper->getExternalId(),
                positionHelper->getPlatoonId(), tempLeaderId);

        app->sendUnicast(restartF, tempLeaderId);

        overtakeState = OvertakeState::L_WAIT_POSITION;
    }
}

void AssistedOvertake::handleOvertakeRestart(const OvertakeRestart *msg) {
    if (app->getPlatoonRole() == PlatoonRole::OVERTAKER) {
        overtakeState = OvertakeState::M_OT;
        plexeTraciVehicle->changeLaneRelative(1, 1);
        plexeTraciVehicle->setCruiseControlDesiredSpeed(130.0 / 3.6);
    }
    if (app->getPlatoonRole() == PlatoonRole::FOLLOWER) {
        plexeTraciVehicle->setCACCConstantSpacing(5);
    }
}

void AssistedOvertake::overtakerPause() {
    if (app->getPlatoonRole() == PlatoonRole::OVERTAKER) {
        overtakeState == OvertakeState::M_WAIT_GAP;
        std::cout << positionHelper->getId()
                << " ricevuto ordine pausa overtake" << " -time:(" << simTime()
                << "), aspetta gap \n";
    }
    plexeTraciVehicle->setCruiseControlDesiredSpeed(100.0 / 3.6);
    std::cout << positionHelper->getId() << " rallenta " << " -time:("
            << simTime() << ") \n";
}

void AssistedOvertake::overtakerToTail() {
    if (app->getPlatoonRole() == PlatoonRole::OVERTAKER) {
        overtakeState == OvertakeState::M_WAIT_DANGER_END;

        std::cout << positionHelper->getId()
                << " ricevuto ordine rientro in coda" << " -time:(" << simTime()
                << "), \n";
    }
    plexeTraciVehicle->setCruiseControlDesiredSpeed(80.0 / 3.6);
    std::cout << positionHelper->getId() << " rallenta " << " -time:("
            << simTime() << ") \n";

}

void AssistedOvertake::followerOpenGap() {
    if (app->getPlatoonRole() == PlatoonRole::FOLLOWER) {
        plexeTraciVehicle->setCACCConstantSpacing(gap);

        app->scheduleAt(simTime() + 0.5, checkDistance);

        overtakeState == OvertakeState::F_OPEN_GAP;
    }
}

void AssistedOvertake::handleOpenGapAck(const OpenGapAck *msg) {
    if (app->getPlatoonRole() == PlatoonRole::OVERTAKER) {
        plexeTraciVehicle->changeLaneRelative(-1, 1);

        JoinAck *joinAck = createJoinAck(positionHelper->getId(),
                positionHelper->getExternalId(), targetPlatoonData->platoonId,
                targetPlatoonData->platoonLeader);
        app->sendUnicast(joinAck, targetPlatoonData->platoonLeader);

        std::cout << positionHelper->getId() << " corsia joinata" << " -time:("
                << simTime() << "), \n";
        overtakeState == OvertakeState::M_WAIT_REPLY;

    }
}

void AssistedOvertake::handleJoinAck(const JoinAck *msg) {
    if (app->getPlatoonRole() == PlatoonRole::LEADER) {
        overtakeState = OvertakeState::L_WAIT_DANGER_END;
        std::cout << positionHelper->getId() << " LEADER aspetto fine pericolo"
                << " -time:(" << simTime() << "), \n";
    }
}

bool AssistedOvertake::handleSelfMsg(cMessage *msg) {
    if (msg == checkDistance) {
        double distance, relativeSpeed;
        plexeTraciVehicle->getRadarMeasurements(distance, relativeSpeed);
        if (distance + 1 >= gap) {
            OpenGapAck *openGapAck = createOpenGapAck(positionHelper->getId(),
                    positionHelper->getExternalId(),
                    positionHelper->getPlatoonId(), oId);
            app->sendUnicast(openGapAck, oId);
        } else {
            app->scheduleAt(simTime() + 0.5, checkDistance);
        }
        return true;
    }

    else if (msg == checkEmergency) {
        // al posto di guardare la variabile si guarderà il radar
        if (emergency && overtakeState == OvertakeState::L_WAIT_POSITION) {
            abortManeuver();
            std::cout << positionHelper->getId() << " RILEVATA EMERGENZA"
                    << " -time:(" << simTime() << "), ABORT MANEUVER \n";

        } else if (!emergency
                && overtakeState == OvertakeState::L_WAIT_DANGER_END) {
            restartManeuver();
            std::cout << positionHelper->getId() << " EMERGENZA FINITA "
                    << " -time:(" << simTime() << "), RESTART MANEUVER \n";
        } else {
            std::cout << positionHelper->getId()
                    << " RISCHEDULO CHECKEMERGENCY " << " -time:(" << simTime()
                    << "),\n";

        }
        app->scheduleAt(simTime() + 0.5, checkEmergency);

        return true;
    }

    else {

        return false;
    }
}

void AssistedOvertake::changeLane() {
    if (app->getPlatoonRole() == PlatoonRole::OVERTAKER) {
        plexeTraciVehicle->changeLaneRelative(-1, 1);
    }
}

void AssistedOvertake::fakeEmergencyStart() {
    emergency = true;
}

void AssistedOvertake::fakeEmergencyFinish() {
    emergency = false;
}

} // namespace plexe

