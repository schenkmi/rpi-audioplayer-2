/**
 * OHUPnPPlayer (OpenHome/UPnP/DLNA Player Daemon)
 *
 * Copyright (c) 2025-2026, Schenk Engineering
 * All Rights Reserved
 *
 * Author: Michael Schenk
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * OEMs, ISVs, VARs and other distributors that combine and distribute
 * commercially licensed software with Schenk Engineering software
 * and do not wish to distribute the source code for the commercially
 * licensed software under version 2, or (at your option) any later
 * version, of the GNU General Public License (the "GPL") must enter
 * into a commercial license agreement with Schenk Engineering.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file LICENSE.txt. If not, write to
 * the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 * http://www.gnu.org/licenses/gpl-2.0.html
 */
#include <assert.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>


/* Platinum/Neptune UPnP SDK includes */
#include <Neptune.h>
#include <PltUPnP.h>

/* local includes */
#include "MyLogger.h"
#include "MyOHPlaylist.h"
#include "MyUPnPRenderer.h"

#if defined(MPD_RENDERER)
#include "MPDRenderer.h"
#endif
#if defined(DUMMY_RENDERER)
#include "DummyRenderer.h"
#endif
#if defined(FFMPEGD_RENDERER)
#include "FFMPEGDRenderer.h"
#endif

#define PROGRAM "OHUPnPPlayer"
#define PROGRAMVERSION "1.0.3"
#define PROGRAMDATE "04.01.2026"
#define UPNP_CFG_VERSION 2

#define UPNP_CFG_DIR "ohupnpplayer"
#define UPNP_CFG_FILENAME "ohupnpcfg.xml"

// global-ish options (kept for compatibility)
int _mylogger_log_level_ = 0;
static volatile sig_atomic_t quit = 0;
static int opt_daemon = 0;
static int opt_proxy_controller_log_level = 0;
static int opt_log_level = 0;

/* UPnP DMR */
std::string m_cfgUUIDRenderer;
std::string m_cfgFriendlyNameRenderer;
bool m_useDMR;

/* OH Playlist */
std::string m_cfgUUIDOHPlaylist;
std::string m_cfgFriendlyNameOHPlaylist;

/* Build config directory path from HOME (or use std::filesystem when available) */
static std::string get_config_path() {
  const char* home = std::getenv("HOME");
  if (!home) {
    ML_LOG_ERROR("HOME environment variable not set\n");
    return "";
  }
  std::string dir = std::string(home) + "/" + UPNP_CFG_DIR;
  return dir;
}

/**
 * Save configuration to xml file (best-effort).
 */
bool saveCfg() {
  NPT_XmlElementNode* root;
  NPT_XmlElementNode* child;

  root = new NPT_XmlElementNode("UPnPConfiguration");
  assert(root != NULL);

  child = new NPT_XmlElementNode("Version");
  assert(child != NULL);
  child->AddText(NPT_String::FromInteger(UPNP_CFG_VERSION));
  root->AddChild(child);

  child = new NPT_XmlElementNode("UUIDRenderer");
  assert(child != NULL);
  child->AddText(m_cfgUUIDRenderer.c_str());
  root->AddChild(child);

  child = new NPT_XmlElementNode("FriendlyNameRenderer");
  assert(child != NULL);
  child->AddText(m_cfgFriendlyNameRenderer.c_str());
  root->AddChild(child);

  child = new NPT_XmlElementNode("UUIDOHPlaylist");
  assert(child != NULL);
  child->AddText(m_cfgUUIDOHPlaylist.c_str());
  root->AddChild(child);

  child = new NPT_XmlElementNode("FriendlyNameOHPlaylist");
  assert(child != NULL);
  child->AddText(m_cfgFriendlyNameOHPlaylist.c_str());
  root->AddChild(child);

  std::string dir = get_config_path();
  if (dir.empty()) {
    ML_LOG_ERROR("No config path available, cannot save cfg\n");
    delete root;
    return false;
  }
  std::string fqfn = dir + "/" + UPNP_CFG_FILENAME;

  ML_LOG_INFO("writing cfg %s\n", fqfn.c_str());

  /* write file */
  NPT_XmlWriter writer(2);
  NPT_File output(fqfn.c_str());
  NPT_Result r = output.Open(NPT_FILE_OPEN_MODE_WRITE | NPT_FILE_OPEN_MODE_CREATE | NPT_FILE_OPEN_MODE_TRUNCATE);
  if (NPT_FAILED(r)) {
    ML_LOG_ERROR("Failed to open config file '%s' for writing: %d\n", fqfn.c_str(), r);
    delete root;
    return false;
  }
  NPT_OutputStreamReference output_stream_ref;
  output.GetOutputStream(output_stream_ref);
  writer.Serialize(*root, *output_stream_ref);

  // delete the tree
  delete root;

  return true;
}

/**
 * Load configuration (if available).
 */
static bool loadCfg() {
  NPT_InputStreamReference stream;
  NPT_Result result;
  int cfgVersion = 0;

  /* reset defaults */
  m_cfgUUIDRenderer = "";
  m_cfgFriendlyNameRenderer = "Albis DMR";
  m_cfgUUIDOHPlaylist = "";
  m_cfgFriendlyNameOHPlaylist = "Albis OHPlaylist";

  std::string dir = get_config_path();
  if (dir.empty()) {
    ML_LOG_INFO("No config directory, using defaults\n");
    return false;
  }
  std::string fqfn = dir + "/" + UPNP_CFG_FILENAME;

  ML_LOG_INFO("loading cfg %s\n", fqfn.c_str());

  // open the input file
  NPT_File input(fqfn.c_str());
  result = input.Open(NPT_FILE_OPEN_MODE_READ);
  if (NPT_FAILED(result)) {
    NPT_Debug("Cannot open input '%s' (%d)\n", fqfn.c_str(), result);
    return false;
  }
  result = input.GetInputStream(stream);
  if (NPT_FAILED(result)) {
    NPT_Debug("Cannot get input stream for '%s' (%d)\n", fqfn.c_str(), result);
    return false;
  }

  // parse the buffer
  NPT_XmlParser parser;
  NPT_XmlNode* tree = NULL;
  result = parser.Parse(*stream, tree);
  if (NPT_FAILED(result) || tree == NULL) {
    NPT_Debug("Cannot parse input %s (%d)\n", fqfn.c_str(), result);
    if (tree) delete tree;
    return false;
  }

  NPT_XmlElementNode* root = tree->AsElementNode();
  if (root) {
    NPT_XmlElementNode* elem;
    const NPT_String* text;

    if ((elem = root->GetChild("Version", NPT_XML_ANY_NAMESPACE))) {
      if ((text = elem->GetText())) {
        text->ToInteger32(cfgVersion);
        ML_LOG_INFO("Version %d\n", cfgVersion);
        if (cfgVersion != UPNP_CFG_VERSION) {
          ML_LOG_ERROR("UPnP cfg version is different (found %d expected %d)\n", cfgVersion, UPNP_CFG_VERSION);
          delete tree;
          return false;
        }
      }
    }

    if ((elem = root->GetChild("UUIDRenderer", NPT_XML_ANY_NAMESPACE))) {
      m_cfgUUIDRenderer = elem->GetText() ? elem->GetText()->GetChars() : "";
      ML_LOG_INFO("m_cfgUUIDRenderer [%s]\n", m_cfgUUIDRenderer.c_str());
    }

    if ((elem = root->GetChild("FriendlyNameRenderer", NPT_XML_ANY_NAMESPACE))) {
      m_cfgFriendlyNameRenderer = elem->GetText() ? elem->GetText()->GetChars() : "";
      ML_LOG_INFO("m_cfgFriendlyNameRenderer [%s]\n", m_cfgFriendlyNameRenderer.c_str());
    }

    if ((elem = root->GetChild("UUIDOHPlaylist", NPT_XML_ANY_NAMESPACE))) {
      m_cfgUUIDOHPlaylist = elem->GetText() ? elem->GetText()->GetChars() : "";
      ML_LOG_INFO("m_cfgUUIDOHPlaylist [%s]\n", m_cfgUUIDOHPlaylist.c_str());
    }

    if ((elem = root->GetChild("FriendlyNameOHPlaylist", NPT_XML_ANY_NAMESPACE))) {
      m_cfgFriendlyNameOHPlaylist = elem->GetText() ? elem->GetText()->GetChars() : "";
      ML_LOG_INFO("m_cfgFriendlyNameOHPlaylist [%s]\n", m_cfgFriendlyNameOHPlaylist.c_str());
    }
  }

#if 0
    // dump the tree
    NPT_XmlWriter writer(2);
    NPT_File output(NPT_FILE_STANDARD_OUTPUT);
    output.Open(NPT_FILE_OPEN_MODE_WRITE);
    NPT_OutputStreamReference output_stream_ref;
    output.GetOutputStream(output_stream_ref);
    writer.Serialize(*tree, *output_stream_ref);
#endif

  // delete the tree
  delete tree;

  return true;
}

/**
 * Print help and exit.
 */
static void display_help(void) {
  printf("Usage: " PROGRAM " [OPTION] PARAM\n" PROGRAM
         " daemon.\n"
         "\n"
         "      -l, --log <LEVEL>\n"
         "          Set logging to level <LEVEL>\n"
         "      -d, --daemon\n"
         "          Run as daemon\n"
         "      -p, --plog\n"
         "          Set proxy controller level\n"
         "      -v, --version\n"
         "          Display the actual version of the " PROGRAM "\n\n");
  exit(0);
}

/**
 * Print version and exit.
 */
static void display_version(void) {
  printf(PROGRAM " " PROGRAMVERSION " " PROGRAMDATE
                 "\n"
                 "\n"
                 "Copyright (C) 2025-2026 Schenk Engineering.\n"
                 "\n" PROGRAM
                 " comes with NO WARRANTY\n"
                 "to the extent permitted by law.\n"
                 "\n");

  exit(0);
}

/**
 * Read a single character from stdin (non-canonical).
 * Returns 0 on success, -1 on error.
 */
static int get_one_character(char* c) {
  struct termios tmbuf, tmsave;

  if (tcgetattr(0, &tmbuf)) {
    return -1;
  }

  tmsave = tmbuf; // copy current settings

  tmbuf.c_lflag &= ~ICANON;
  tmbuf.c_cc[VMIN] = 1;
  tmbuf.c_cc[VTIME] = 0;

  if (tcsetattr(0, TCSANOW, &tmbuf)) {
    return -1;
  }

  ssize_t r = read(0, c, 1);
  if (r != 1) {
    // restore
    tcsetattr(0, TCSANOW, &tmsave);
    return -1;
  }

  if (tcsetattr(0, TCSANOW, &tmsave)) {
    return -1;
  }

  return 0;
}

/**
 * Process command line options.
 */
int process_options(int argc, char* argv[]) {
  int error = 0;

  for (;;) {
    int option_index = 0;
    static const char* short_options = "p:l:hvd";
    static const struct option long_options[] = {
        {"plog", required_argument, 0, 'p'},
        {"log", required_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"signal", no_argument, 0, 's'},
        {"daemon", no_argument, 0, 'd'},
        {0, 0, 0, 0},
    };

    int c = getopt_long(argc, argv, short_options, long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 0:
        switch (option_index) {
          default:
            break;
        }
        break;
      case 'h':
        display_help();
        break;
      case 'v':
        display_version();
        break;
      case 'l':
        opt_log_level = atoi(optarg);
        break;
      case 'd':
        opt_daemon = 1;
        break;
      case 'p':
        opt_proxy_controller_log_level = atoi(optarg);
        break;
      case '?':
      default:
        error = 1;
        break;
    }
  }

  if (error) {
    display_help();
  }

  return optind;
}

/**
 * Create config directory. Uses std::filesystem when available.
 */
static int create_cfg_directory() {
  std::string dir = get_config_path();
  if (dir.empty()) {
    ML_LOG_ERROR("Cannot determine config directory\n");
    return 1;
  }

  int ret = mkdir(dir.c_str(), 0755); // rwxr-xr-x
  if (ret == 0) {
    ML_LOG_INFO("Directory created: %s\n", dir.c_str());
  } else if (errno == EEXIST) {
    ML_LOG_INFO("Directory already exists: %s\n", dir.c_str());
  } else {
    ML_LOG_ERROR("Filesystem error creating %s: %s\n", dir.c_str(), ::strerror(errno));
    return 1;
  }

  return 0;
}

/**
 * Signal handler -> set quit flag.
 */
static void handle_sig_int(int /*signal*/) {
  quit = 1;
}

/**
 * Install a simple signal handler if requested.
 */
static void setup_signal_handlers() {
  struct sigaction sa;
  sa.sa_handler = handle_sig_int;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    ML_LOG_ERROR("Failed to install SIGINT handler: %s\n", strerror(errno));
  }
  if (sigaction(SIGTERM, &sa, NULL) == -1) {
    ML_LOG_ERROR("Failed to install SIGTERM handler: %s\n", strerror(errno));
  }
}

/**
 * main
 */
int main(int argc, char* argv[]) {
  PLT_DeviceHostReference m_upnpOHPlaylist; /* OpenHome Playlist */
  PLT_DeviceHostReference m_upnpDMR;
  std::string ipAddr = "";
  std::string friendlyName = "";

#if 0
  //NPT_LogManager::GetDefault().Configure("plist:.level=FINE;.handlers=ConsoleHandler;.ConsoleHandler.colors=off;.ConsoleHandler.filter=42");
#endif

  try {
    std::cout << "__cplusplus = " << __cplusplus << std::endl;
    printf(PROGRAM " " PROGRAMVERSION " " PROGRAMDATE "\n");

    process_options(argc, argv);
    _mylogger_log_level_ = opt_log_level;

    /* load configuration */
    if (create_cfg_directory() != 0) {
      ML_LOG_ERROR("Could not ensure config directory exists\n");
    }

    if (!loadCfg()) {
      // attempt to write default cfg and reload; ignore failures
      saveCfg();
      loadCfg();
    }

    setup_signal_handlers();

#if 1
    NPT_List<NPT_IpAddress> list;
    if (NPT_SUCCEEDED(PLT_UPnPMessageHelper::GetIPAddresses(list))) {
      ipAddr = "@" + std::string((*(list.GetFirstItem())).ToString().GetChars());
    } else {
      ML_LOG_INFO("No IP addresses returned by PLT_UPnPMessageHelper::GetIPAddresses\n");
    }
#endif

#if defined(MPD_RENDERER)
    auto renderer = std::make_shared<MPDRenderer>(opt_proxy_controller_log_level);
#endif
#if defined(DUMMY_RENDERER)
    auto renderer = std::make_shared<DummyRenderer>(opt_proxy_controller_log_level);
#endif
#if defined(FFMPEGD_RENDERER)
    auto renderer = std::make_shared<FFMPEGDRenderer>(opt_proxy_controller_log_level);
#endif

#if __cplusplus >= 201703L
    std::unique_ptr<PLT_UPnP> m_upnp = std::make_unique<PLT_UPnP>();
#else
    std::unique_ptr<PLT_UPnP> m_upnp(new PLT_UPnP());
#endif

    friendlyName = m_cfgFriendlyNameOHPlaylist + ipAddr;

    MyOHPlaylist* myOHPlaylist = new MyOHPlaylist(renderer,
                                                  friendlyName.empty() ? NULL : friendlyName.c_str(),
                                                  false /* do not show ip */,
                                                  m_cfgUUIDOHPlaylist.empty() ? NULL : m_cfgUUIDOHPlaylist.c_str(),
                                                  0 /*port*/);
    m_upnpOHPlaylist = myOHPlaylist;

    NPT_Result res = m_upnp->AddDevice(m_upnpOHPlaylist);
    if (NPT_FAILED(res)) {
      ML_LOG_ERROR("failed to add OHPlaylist device (result=%d)\n", res);
      // keep going — library may have its own recovery or another device may still be added.
    }

    friendlyName = m_cfgFriendlyNameRenderer + ipAddr;
    MyUPnPRenderer* myUPnPRenderer = new MyUPnPRenderer(renderer,
                                                        friendlyName.empty() ? NULL : friendlyName.c_str(),
                                                        false /* do not show ip */,
                                                        m_cfgUUIDRenderer.empty() ? NULL : m_cfgUUIDRenderer.c_str(),
                                                        0 /*port*/);
    m_upnpDMR = myUPnPRenderer;

    res = m_upnp->AddDevice(m_upnpDMR);
    if (NPT_FAILED(res)) {
      ML_LOG_ERROR("failed to add DMR device (result=%d)\n", res);
    }

    m_upnp->Start();

    if ((m_cfgUUIDOHPlaylist != std::string(myOHPlaylist->GetUUID())) ||
        (m_cfgUUIDRenderer != std::string(myUPnPRenderer->GetUUID()))) {
      m_cfgUUIDOHPlaylist = myOHPlaylist->GetUUID();
      m_cfgUUIDRenderer = myUPnPRenderer->GetUUID();
      ML_LOG_INFO("m_cfgUUIDRenderer [%s]\n", m_cfgUUIDRenderer.c_str());
      ML_LOG_INFO("m_cfgUUIDOHPlaylist [%s]\n", m_cfgUUIDOHPlaylist.c_str());
      saveCfg();
    }

    if (opt_daemon) {
      while (!quit) {
        sleep(1);
      }
    } else {
      char ch;
      while (!quit) {
        printf("\nEnter command : ");
        fflush(stdout);
        if (get_one_character(&ch) != 0) {
          // read failure — continue
          continue;
        }
        printf("\n");
        switch (ch) {
          case 'q':
            quit = 1;
            break;
          default:
            break;
        }
      }
    }

    m_upnp->Stop();

    if (!m_upnpOHPlaylist.IsNull()) {
      m_upnp->RemoveDevice(m_upnpOHPlaylist);
      m_upnpOHPlaylist = NULL;
    }

    if (!m_upnpDMR.IsNull()) {
      m_upnp->RemoveDevice(m_upnpDMR);
      m_upnpDMR = NULL;
    }

    // renderer.reset(); // renderer is local and will be destroyed automatically
    return 0;
  } catch (const std::exception& e) {
    ML_LOG_ERROR("Unhandled exception in main: %s\n", e.what());
    return 2;
  } catch (...) {
    ML_LOG_ERROR("Unhandled non-standard exception in main\n");
    return 3;
  }
}
