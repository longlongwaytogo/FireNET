cmake_minimum_required (VERSION 3.6.0)
project (CryFireNetCore VERSION 1.0 LANGUAGES CXX)

# HOT RELOAD
if (${CMAKE_GENERATOR} MATCHES "^Visual Studio")
	ADD_DEFINITIONS("/ZI")
endif()

option(OPTION_PCH "Enable Precompiled headers" ON)

# CryEngine headers
find_package (CryCommon REQUIRED)
find_package (CryAction REQUIRED)

# BOOST 
set(BOOST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../3rd/boost)
set(BOOST_INCLUDEDIR ${CMAKE_CURRENT_SOURCE_DIR}/../../3rd/boost/boost)
set(BOOST_LIBRARYDIR ${CMAKE_CURRENT_SOURCE_DIR}/../../3rd/boost/lib)
set(Boost_NO_SYSTEM_PATHS        ON)
set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
find_package(Boost 1.63.0 COMPONENTS date_time system regex REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
else()
  message("Boost NOT INCLUDED!")
endif()

# OpenSsl
set(OPENSSL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../3rd/openssl)
find_package(OPENSSL REQUIRED)
if(OPENSSL_FOUND)
  include_directories(${OPENSSL_INCLUDE_DIR})
else()
  message("OpenSsl NOT INCLUDED!")
endif()

# Main filter
set (SourceGroup_PluginMain
	"StdAfx.cpp"
	"StdAfx.h"
	"Main/Plugin.cpp"
	"Main/Plugin.h"
	"Main/PluginEnv.h"
)

# Flow nodes
set (SourceGroup_FlowNodes
	"FlowNodes/FlowNodeFireNET.cpp"
)

# Network
set (SourceGroup_Network
	"Network/ReadQueue.cpp"
	"Network/ReadQueue.h"
	"Network/NetworkThread.cpp"
	"Network/NetworkThread.h"
	"Network/TcpClient.cpp"
	"Network/TcpClient.h"
	"Network/TcpPacket.cpp"
	"Network/TcpPacket.h"
)

# FireNet spawn point
set(SourceGroup_FireNetSpawnPoint
	"Entities/FireNetSpawnPoint.cpp"
	"Entities/FireNetSpawnPoint.h"
)

# FireNet player
set (SourceGroup_FireNetPlayer
	"../../Common/Actors/FireNetPlayer.cpp"
	"../../Common/Actors/FireNetPlayer.h"
	"../../Common/Actors/ISimpleActor.h"
)

# FireNet player animations
set(SourceGroup_FireNetPlayerAnimations
	"../../Common/Actors/Animations/FireNetPlayerAnimations.cpp"
	"../../Common/Actors/Animations/FireNetPlayerAnimations.h"
)

# FireNet player input
set(SourceGroup_FireNetPlayerInput
	"../../Common/Actors/Input/FireNetPlayerInput.cpp"
	"../../Common/Actors/Input/FireNetPlayerInput.h"
)

# FireNet player movement
set(SourceGroup_FireNetPlayerMovement
	"../../Common/Actors/Movement/FireNetPlayerMovement.cpp"
	"../../Common/Actors/Movement/FireNetPlayerMovement.h"
)

source_group("Main" FILES ${SourceGroup_PluginMain})
source_group("FlowNodes" FILES ${SourceGroup_FlowNodes})
source_group("Network" FILES ${SourceGroup_Network})
source_group("Entities\\FireNetSpawnPoint" FILES ${SourceGroup_FireNetSpawnPoint})
source_group("Entities\\FireNetPlayer" FILES ${SourceGroup_FireNetPlayer})
source_group("Entities\\FireNetPlayer\\Animations" FILES ${SourceGroup_FireNetPlayerAnimations})
source_group("Entities\\FireNetPlayer\\Input" FILES ${SourceGroup_FireNetPlayerInput})
source_group("Entities\\FireNetPlayer\\Movement" FILES ${SourceGroup_FireNetPlayerMovement})

set (SOURCE
	${SourceGroup_PluginMain}
	${SourceGroup_FlowNodes}
	${SourceGroup_Network}
	${SourceGroup_FireNetSpawnPoint}
	${SourceGroup_FireNetPlayer}
	${SourceGroup_FireNetPlayerAnimations}
	${SourceGroup_FireNetPlayerInput}
	${SourceGroup_FireNetPlayerMovement}
)

if(OPTION_PCH AND MSVC)
	set (PRECOMPILED_SOURCE ${SOURCE})
	list(REMOVE_ITEM PRECOMPILED_SOURCE "StdAfx.cpp")

	set (PRECOMPILED_BINARY ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pch)
	set_source_files_properties (${PRECOMPILED_SOURCE} PROPERTIES COMPILE_FLAGS "/Yu\"StdAfx.h\" /Fp\"${PRECOMPILED_BINARY}\"" OBJECT_DEPENDS "${PRECOMPILED_BINARY}")
	set_source_files_properties (StdAfx.cpp PROPERTIES COMPILE_FLAGS "/Yc\"StdAfx.h\" /Fp\"${PRECOMPILED_BINARY}\"" OBJECT_OUTPUTS "${PRECOMPILED_BINARY}")
endif (OPTION_PCH AND MSVC)

add_library(${PROJECT_NAME} MODULE ${SOURCE})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../Common)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../includes/FireNet)
target_link_libraries(${PROJECT_NAME} PRIVATE CryCommon CryAction ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

if (WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE Ntdll)
endif(WIN32)