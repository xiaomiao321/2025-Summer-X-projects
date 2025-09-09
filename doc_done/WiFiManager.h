#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

// Tries to connect to a saved WiFi network.
// If it fails, it starts an Access Point and a web server for configuration.
// Returns true if already connected, false if it starts the config portal.
bool connectWiFi_with_Manager();

#endif // WIFIMANAGER_H
