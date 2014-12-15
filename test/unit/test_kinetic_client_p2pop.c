
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
*/

#include "kinetic_client.h"
#include "kinetic_types.h"
#include "kinetic_types_internal.h"
#include "mock_kinetic_connection.h"
#include "mock_kinetic_controller.h"
#include "mock_kinetic_operation.h"

#include "kinetic_logger.h"
#include "kinetic_proto.h"
#include "protobuf-c/protobuf-c.h"
#include "byte_array.h"
#include "unity.h"
#include "unity_helper.h"

static KineticSessionHandle DummyHandle = 12;

void setUp(void)
{
    KineticLogger_Init("stdout", 3);
}

void tearDown(void)
{
    KineticLogger_Close();
}

void test_KineticClient_P2POperation_should_execute_a_p2p_operation(void)
{
    LOG_LOCATION;

    KineticOperation operation;
    KineticP2P_Operation p2pOp;

    KineticController_CreateOperation_ExpectAndReturn(DummyHandle, &operation);
    KineticOperation_BuildP2POperation_Expect(&operation, &p2pOp);
    KineticController_ExecuteOperation_ExpectAndReturn(&operation, NULL, KINETIC_STATUS_SUCCESS);

    KineticStatus status = KineticClient_P2POperation(DummyHandle, &p2pOp, NULL);

    TEST_ASSERT_EQUAL_KineticStatus(KINETIC_STATUS_SUCCESS, status);
}
