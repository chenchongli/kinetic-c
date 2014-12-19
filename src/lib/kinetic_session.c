/*
* kinetic-c
* Copyright (C) 2014 Seagate Technology.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
*/

#include "kinetic_session.h"
#include "kinetic_types_internal.h"
#include "kinetic_controller.h"
#include "kinetic_socket.h"
#include "kinetic_pdu.h"
#include "kinetic_operation.h"
#include "kinetic_controller.h"
#include "kinetic_allocator.h"
#include "kinetic_logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

KineticStatus KineticSession_Create(KineticSession * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }

    session->connection = KineticAllocator_NewConnection();
    if (session->connection == NULL) {
        return KINETIC_STATUS_MEMORY_ERROR;
    }

    KINETIC_CONNECTION_INIT(session->connection);
    session->connection->session = session;
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Destroy(KineticSession * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    if (session->connection == NULL) {
        return KINETIC_STATUS_SESSION_INVALID;
    }
    pthread_mutex_destroy(&session->connection->writeMutex);
    KineticAllocator_FreeConnection(session->connection);
    session->connection = NULL;
    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Connect(KineticSession * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    KineticConnection* connection = session->connection;
    if (connection == NULL) {
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Establish the connection
    assert(session != NULL);
    assert(session->connection != NULL);
    assert(strlen(session->config.host) > 0);
    connection->connected = false;
    connection->socket = KineticSocket_Connect(
        session->config.host, session->config.port);
    connection->connected = (connection->socket >= 0);
    if (!connection->connected) {
        LOG0("Session connection failed!");
        connection->socket = KINETIC_SOCKET_DESCRIPTOR_INVALID;
        return KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Kick off the worker thread
    session->connection->thread.connection = connection;
    KineticController_Init(session);

    connection->session = session; // TODO: refactor out this duplicate pointer/reference to session

    // Wait for initial unsolicited status to be received in order to obtain connectionID
    const long maxWaitMicrosecs = 2000000;
    long microsecsWaiting = 0;
    struct timespec sleepDuration = {.tv_nsec = 500000};
    while(connection->connectionID == 0) {
        if (microsecsWaiting > maxWaitMicrosecs) {
            LOG0("Timed out waiting for connection ID from device!");
            return KINETIC_STATUS_SOCKET_TIMEOUT;
        }
        nanosleep(&sleepDuration, NULL);
        microsecsWaiting += (sleepDuration.tv_nsec / 1000);
    }

    return KINETIC_STATUS_SUCCESS;
}

KineticStatus KineticSession_Disconnect(KineticSession const * const session)
{
    if (session == NULL) {
        return KINETIC_STATUS_SESSION_EMPTY;
    }
    KineticConnection* connection = session->connection;
    if (connection == NULL || !session->connection->connected || connection->socket < 0) {
        return KINETIC_STATUS_CONNECTION_ERROR;
    }
    
    // Shutdown the worker thread
    KineticStatus status = KINETIC_STATUS_SUCCESS;
    connection->thread.abortRequested = true;
    LOG3("\nSent abort request to worker thread!\n");
    int pthreadStatus = pthread_join(connection->threadID, NULL);
    if (pthreadStatus != 0) {
        char errMsg[256];
        Kinetic_GetErrnoDescription(pthreadStatus, errMsg, sizeof(errMsg));
        LOGF0("Failed terminating worker thread w/error: %s", errMsg);
        status = KINETIC_STATUS_CONNECTION_ERROR;
    }

    // Close the connection
    KineticSocket_Close(connection->socket);
    connection->socket = KINETIC_HANDLE_INVALID;
    connection->connected = false;

    return status;
}

void KineticSession_IncrementSequence(KineticSession const * const session)
{
    assert(session != NULL);
    assert(session->connection != NULL);
    session->connection->sequence++;
}