#include <RemoteDebug.h>        //https://github.com/JoaoLopesF/RemoteDebug

#define HOST_NAME  "MouseTrap"
// Instance of RemoteDebug
RemoteDebug Debug;

void remoteDebugSetup (void) {
#if 0
// Register host name in mDNS
  if (MDNS.begin(HOST_NAME)) {
    Serial.print("* MDNS responder started. Hostname -> ");
    Serial.println(HOST_NAME);
  }
  MDNS.addService("telnet", "tcp", 23);// Telnet server RemoteDebug
#endif

	Debug.begin(HOST_NAME); // Initialize the WiFi server

  Debug.setResetCmdEnabled(true); // Enable the reset command

	Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
	Debug.showColors(true); // Colors

	// RemoteDebug handle
	
  // Project commands
//  String helpCmd = "bench1 - Benchmark 1\n";
//  helpCmd.concat("bench2 - Benchmark 2");

//  Debug.setHelpProjectsCmds(helpCmd);
}
