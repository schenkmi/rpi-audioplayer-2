OpenHome extension based on platinum inspired by https://github.com/medoc92/upmpdcli

Build TextToHeader utility used to convert XML files:

[schemic0@simonpc62 platinum]$ cd PlatinumKit-1-0-5-13_0ab854/Platinum/Source/Tools/TextToHeader/
[schemic0@simonpc62 TextToHeader]$ g++ TextToHeader.cpp -o  TextToHeader

Build the SCPD headers:
[schemic0@simonpc62 albisOH]$ ./TextToHeader -v AOH_InfoSCPD OHInfo.xml AOHInfoSCPD.cpp

[schemic0@simonpc62 albisOH]$ ./TextToHeader -v AOH_PlaylistSCPD OHPlaylist.xml AOHPlaylistSCPD.cpp
[schemic0@simonpc62 albisOH]$ ./TextToHeader -v AOH_ProductSCPD OHProduct.xml AOHProductSCPD.cpp
[schemic0@simonpc62 albisOH]$ ./TextToHeader -v AOH_ReceiverSCPD OHReceiver.xml AOHReceiverSCPD.cpp
[schemic0@simonpc62 albisOH]$ ./TextToHeader -v AOH_TimeSCPD OHTime.xml AOHTimeSCPD.cpp
[schemic0@simonpc62 albisOH]$ ./TextToHeader -v AOH_VolumeSCPD OHVolume.xml AOHVolumeSCPD.cpp

[schemic0@simonpc62 albisOH]$ ./TextToHeader -v AOH_CredentialsSCPD OHCredentials.xml AOHCredentialsSCPD.cpp