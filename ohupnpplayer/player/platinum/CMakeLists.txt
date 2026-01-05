cmake_minimum_required(VERSION 3.15...4.1)

project(
        platinum-upnp
        VERSION 1.0
        LANGUAGES CXX)

# <EDIT> LIST ALL HEADERS AND SOURCES.
set(PLATINUM_HEADERS
		Source/Core/PltAction.h
		Source/Core/PltArgument.h
		Source/Core/PltConstants.h
		Source/Core/PltCtrlPoint.h
		Source/Core/PltCtrlPointTask.h
		Source/Core/PltDatagramStream.h
		Source/Core/PltDeviceData.h
		Source/Core/PltDeviceHost.h
		Source/Core/PltEvent.h
		Source/Core/PltHttp.h
		Source/Core/PltHttpClientTask.h
		Source/Core/PltHttpServer.h
		Source/Core/PltHttpServerTask.h
		Source/Core/PltMimeType.h
		Source/Core/PltProtocolInfo.h
		Source/Core/PltService.h
		Source/Core/PltSsdp.h
		Source/Core/PltStateVariable.h
		Source/Core/PltTaskManager.h
		Source/Core/PltThreadTask.h
		Source/Core/PltUPnP.h
		Source/Devices/MediaConnect/PltMediaConnect.h
		Source/Devices/MediaRenderer/PltMediaRenderer.h
		Source/Devices/MediaRenderer/PltMediaController.h
		Source/Devices/MediaServer/PltDidl.h
		Source/Devices/MediaServer/PltFileMediaServer.h
		Source/Devices/MediaServer/PltMediaBrowser.h
		Source/Devices/MediaServer/PltMediaCache.h
		Source/Devices/MediaServer/PltMediaItem.h
		Source/Devices/MediaServer/PltMediaServer.h
		Source/Devices/MediaServer/PltSyncMediaBrowser.h
		Source/Extras/PltDownloader.h
		Source/Extras/PltFrameBuffer.h
		Source/Extras/PltFrameServer.h
		Source/Extras/PltFrameStream.h
		Source/Extras/PltRingBufferStream.h
		Source/Platinum/PltVersion.h
		ThirdParty/Neptune/Source/Core/Neptune.h
		ThirdParty/Neptune/Source/Core/NptBase64.h
		ThirdParty/Neptune/Source/Core/NptBufferedStreams.h
		ThirdParty/Neptune/Source/Core/NptCommon.h
		ThirdParty/Neptune/Source/Core/NptDataBuffer.h
		ThirdParty/Neptune/Source/Core/NptDebug.h
		ThirdParty/Neptune/Source/Core/NptFile.h
		ThirdParty/Neptune/Source/Core/NptHash.h
		ThirdParty/Neptune/Source/Core/NptHttp.h
		ThirdParty/Neptune/Source/Core/NptTls.h
		ThirdParty/Neptune/Source/Core/NptList.h
		ThirdParty/Neptune/Source/Core/NptMessaging.h
		ThirdParty/Neptune/Source/Core/NptNetwork.h
		ThirdParty/Neptune/Source/Core/NptQueue.h
		ThirdParty/Neptune/Source/Core/NptRingBuffer.h
		ThirdParty/Neptune/Source/Core/NptSimpleMessageQueue.h
		ThirdParty/Neptune/Source/Core/NptSockets.h
		ThirdParty/Neptune/Source/Core/NptStreams.h
		ThirdParty/Neptune/Source/Core/NptStrings.h
		ThirdParty/Neptune/Source/Core/NptSystem.h
		ThirdParty/Neptune/Source/Core/NptThreads.h
		ThirdParty/Neptune/Source/Core/NptTime.h
		ThirdParty/Neptune/Source/Core/NptUri.h
		ThirdParty/Neptune/Source/Core/NptUtils.h
		ThirdParty/Neptune/Source/Core/NptXml.h
		ThirdParty/Neptune/Source/Core/NptLogging.h
		ThirdParty/Neptune/Source/Core/NptResults.h
		ThirdParty/Neptune/Source/Core/NptAutomaticCleaner.h
)

set(PLATINUM_SOURCES
		Source/Core/PltAction.cpp
		Source/Core/PltArgument.cpp
		Source/Core/PltConstants.cpp
		Source/Core/PltCtrlPoint.cpp
		Source/Core/PltCtrlPointTask.cpp
		Source/Core/PltDatagramStream.cpp
		Source/Core/PltDeviceData.cpp
		Source/Core/PltDeviceHost.cpp
		Source/Core/PltEvent.cpp
		Source/Core/PltHttp.cpp
		Source/Core/PltHttpClientTask.cpp
		Source/Core/PltHttpServer.cpp
		Source/Core/PltHttpServerTask.cpp
		Source/Core/PltIconsData.cpp
		Source/Core/PltMimeType.cpp
		Source/Core/PltProtocolInfo.cpp
		Source/Core/PltService.cpp
		Source/Core/PltSsdp.cpp
		Source/Core/PltStateVariable.cpp
		Source/Core/PltTaskManager.cpp
		Source/Core/PltThreadTask.cpp
		Source/Core/PltUPnP.cpp
		Source/Devices/MediaConnect/X_MS_MediaReceiverRegistrarSCPD.cpp
		Source/Devices/MediaConnect/PltMediaConnect.cpp
		Source/Devices/MediaRenderer/PltMediaRenderer.cpp
		Source/Devices/MediaRenderer/PltMediaController.cpp
		Source/Devices/MediaRenderer/AVTransportSCPD.cpp
		Source/Devices/MediaRenderer/RdrConnectionManagerSCPD.cpp
		Source/Devices/MediaRenderer/RenderingControlSCPD.cpp
		Source/Devices/MediaServer/ConnectionManagerSCPD.cpp
		Source/Devices/MediaServer/ContentDirectorySCPD.cpp
		Source/Devices/MediaServer/ContentDirectorywSearchSCPD.cpp
		Source/Devices/MediaServer/PltDidl.cpp
		Source/Devices/MediaServer/PltFileMediaServer.cpp
		Source/Devices/MediaServer/PltMediaBrowser.cpp
		Source/Devices/MediaServer/PltMediaCache.cpp
		Source/Devices/MediaServer/PltMediaItem.cpp
		Source/Devices/MediaServer/PltMediaServer.cpp
		Source/Devices/MediaServer/PltSyncMediaBrowser.cpp
		Source/Extras/PltDownloader.cpp
		Source/Extras/PltFrameBuffer.cpp
		Source/Extras/PltFrameServer.cpp
		Source/Extras/PltFrameStream.cpp
		Source/Extras/PltRingBufferStream.cpp
		ThirdParty/Neptune/Source/Core/Neptune.cpp
		ThirdParty/Neptune/Source/Core/NptBase64.cpp
		ThirdParty/Neptune/Source/Core/NptBufferedStreams.cpp
		ThirdParty/Neptune/Source/Core/NptCommon.cpp
		ThirdParty/Neptune/Source/Core/NptDataBuffer.cpp
		ThirdParty/Neptune/Source/Core/NptDebug.cpp
		ThirdParty/Neptune/Source/Core/NptFile.cpp
		ThirdParty/Neptune/Source/Core/NptHash.cpp
		ThirdParty/Neptune/Source/Core/NptHttp.cpp
		ThirdParty/Neptune/Source/Core/NptTls.cpp
		ThirdParty/Neptune/Source/Core/NptList.cpp
		ThirdParty/Neptune/Source/Core/NptMessaging.cpp
		ThirdParty/Neptune/Source/Core/NptNetwork.cpp
		ThirdParty/Neptune/Source/Core/NptQueue.cpp
		ThirdParty/Neptune/Source/Core/NptRingBuffer.cpp
		ThirdParty/Neptune/Source/Core/NptSimpleMessageQueue.cpp
		ThirdParty/Neptune/Source/Core/NptSockets.cpp
		ThirdParty/Neptune/Source/Core/NptStreams.cpp
		ThirdParty/Neptune/Source/Core/NptStrings.cpp
		ThirdParty/Neptune/Source/Core/NptSystem.cpp
		ThirdParty/Neptune/Source/Core/NptThreads.cpp
		ThirdParty/Neptune/Source/Core/NptTime.cpp
		ThirdParty/Neptune/Source/Core/NptUri.cpp
		ThirdParty/Neptune/Source/Core/NptUtils.cpp
		ThirdParty/Neptune/Source/Core/NptXml.cpp
		ThirdParty/Neptune/Source/Core/NptLogging.cpp
		ThirdParty/Neptune/Source/Core/NptResults.cpp
		ThirdParty/Neptune/Source/Core/NptAutomaticCleaner.cpp
		ThirdParty/Neptune/Source/System/Bsd/NptBsdSockets.cpp
		ThirdParty/Neptune/Source/System/Bsd/NptBsdNetwork.cpp
		ThirdParty/Neptune/Source/System/Bsd/NptBsdResolver.cpp
		ThirdParty/Neptune/Source/System/Posix/NptPosixSystem.cpp
		ThirdParty/Neptune/Source/System/Posix/NptSelectableMessageQueue.cpp
		ThirdParty/Neptune/Source/System/Posix/NptPosixQueue.cpp
		ThirdParty/Neptune/Source/System/Posix/NptPosixThreads.cpp
		ThirdParty/Neptune/Source/System/Posix/NptPosixFile.cpp
		ThirdParty/Neptune/Source/System/Posix/NptPosixTime.cpp
		ThirdParty/Neptune/Source/System/Posix/NptPosixEnvironment.cpp
		ThirdParty/Neptune/Source/System/StdC/NptStdcDebug.cpp
		ThirdParty/Neptune/Source/System/StdC/NptStdcEnvironment.cpp
		ThirdParty/Neptune/Source/System/StdC/NptStdcConsole.cpp
		ThirdParty/Neptune/Source/System/StdC/NptStdcFile.cpp
		ThirdParty/Neptune/Source/System/Null/NptNullAutoreleasePool.cpp
		albisOH/AOHInfoSCPD.cpp
		albisOH/AOHPlaylistSCPD.cpp
		albisOH/AOHProductSCPD.cpp
		albisOH/AOHReceiverSCPD.cpp
		albisOH/AOHTimeSCPD.cpp
		albisOH/AOHVolumeSCPD.cpp
		albisOH/AOHCredentialsSCPD.cpp
		albisOH/PltOHPlaylist.cpp

)


set(SOURCES
		${PLATINUM_SOURCES}
)

set(HEADERS
		${PLATINUM_HEADERS}
)


include_directories(ThirdParty/Neptune/Source/Core)
include_directories(Source/Platinum)
include_directories(Source/Core)
include_directories(Source/Devices/MediaServer)
include_directories(Source/Devices/MediaRenderer)
include_directories(Source/Devices/MediaConnect)
include_directories(Source/Extras)

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
target_include_directories(platinum-upnp PUBLIC Source/Core albisOH ThirdParty/Neptune/Source/Core Source/Platinum Source/Devices/MediaServer Source/Devices/MediaRenderer)

set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER "${PLATINUM_HEADERS}")
install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION examples/bin
        LIBRARY DESTINATION examples/lib)
