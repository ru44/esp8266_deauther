/* This software is licensed under the MIT License: https://github.com/spacehuhntech/esp8266_deauther */

#include "DisplayUI.h"

#include "settings.h"

#include <ESP8266WiFi.h> // WiFi.macAddress() + ESP.* for the System Info screen
#include <time.h>        // configTime()/localtime_r() for the NTP clock sync

// ===== adjustable ===== //
void DisplayUI::configInit() {
    // initialize display
    display.init();

    /*
       In case of a compiler (conversion char/uint8_t) error,
       make sure to have version 4 of the display library installed
       https://github.com/ThingPulse/esp8266-oled-ssd1306/releases/tag/4.0.0
     */
    display.setFont(DejaVu_Sans_Mono_12);

    display.setContrast(255);

    if (FLIP_DIPLAY) display.flipScreenVertically();

    display.clear();
    display.display();
}

void DisplayUI::configOn() {
    display.displayOn();
}

void DisplayUI::configOff() {
    display.displayOff();
}

void DisplayUI::updatePrefix() {
    display.clear();
}

void DisplayUI::updateSuffix() {
    display.display();
}

void DisplayUI::drawString(int x, int y, String str) {
    display.drawString(x, y, replaceUtf8(str, String(QUESTIONMARK)));
}

void DisplayUI::drawString(int row, String str) {
    drawString(0, row * lineHeight, str);
}

void DisplayUI::drawLine(int x1, int y1, int x2, int y2) {
    display.drawLine(x1, y1, x2, y2);
}

// ====================== //


DisplayUI::DisplayUI() {}

DisplayUI::~DisplayUI() {}


void DisplayUI::setup() {
    configInit();
    setupButtons();
    buttonTime = currentTime;

#ifdef RTC_DS3231
    bool h12;
    bool PM_time;
    clock.setClockMode(false);
    clockHour   = clock.getHour(h12, PM_time);
    clockMinute = clock.getMinute();
#else // ifdef RTC_DS3231
    clockHour   = random(12);
    clockMinute = random(60);
#endif // ifdef RTC_DS3231

    // ===== MENUS ===== //

    // MAIN MENU
    createMenu(&mainMenu, NULL, [this]() {
        addMenuNode(&mainMenu, D_SCAN, &scanMenu);          /// SCAN
        addMenuNode(&mainMenu, D_SHOW, &showMenu);          // SHOW
        addMenuNode(&mainMenu, D_ATTACK, &attackMenu);      // ATTACK
        addMenuNode(&mainMenu, D_PACKET_MONITOR, [this]() { // PACKET MONITOR
            scan.start(SCAN_MODE_SNIFFER, 0, SCAN_MODE_OFF, 0, false, wifi_channel);
            mode = DISPLAY_MODE::PACKETMONITOR;
        });
        addMenuNode(&mainMenu, D_CLOCK, &clockMenu); // CLOCK
        addMenuNode(&mainMenu, D_TOOLS, &toolsMenu); // TOOLS (extras submenu)

#ifdef HIGHLIGHT_LED
        addMenuNode(&mainMenu, D_LED, [this]() {     // LED
            highlightLED = !highlightLED;
            digitalWrite(HIGHLIGHT_LED, highlightLED);
        });
#endif // ifdef HIGHLIGHT_LED
    });

    // SCAN MENU
    createMenu(&scanMenu, &mainMenu, [this]() {
        addMenuNode(&scanMenu, D_SCAN_APST, [this]() { // SCAN AP + ST
            scan.start(SCAN_MODE_ALL, 15000, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::LOADSCAN;
        });
        addMenuNode(&scanMenu, D_SCAN_AP, [this]() { // SCAN AP
            scan.start(SCAN_MODE_APS, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::LOADSCAN;
        });
        addMenuNode(&scanMenu, D_SCAN_ST, [this]() { // SCAN ST
            scan.start(SCAN_MODE_STATIONS, 30000, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::LOADSCAN;
        });
    });

    // SHOW MENU
    createMenu(&showMenu, &mainMenu, [this]() {
        addMenuNode(&showMenu, [this]() { // Accesspoints 0 [0]
            return leftRight(str(D_ACCESSPOINTS), (String)accesspoints.count(), maxLen - 1);
        }, &apListMenu);
        addMenuNode(&showMenu, [this]() { // Stations 0 [0]
            return leftRight(str(D_STATIONS), (String)stations.count(), maxLen - 1);
        }, &stationListMenu);
        addMenuNode(&showMenu, [this]() { // Names 0 [0]
            return leftRight(str(D_NAMES), (String)names.count(), maxLen - 1);
        }, &nameListMenu);
        addMenuNode(&showMenu, [this]() { // SSIDs 0
            return leftRight(str(D_SSIDS), (String)ssids.count(), maxLen - 1);
        }, &ssidListMenu);
    });

    // AP LIST MENU
    createMenu(&apListMenu, &showMenu, [this]() {
        // add APs to list
        int c = accesspoints.count();

        for (int i = 0; i < c; i++) {
            addMenuNode(&apListMenu, [i]() {
                return b2a(accesspoints.getSelected(i)) + accesspoints.getSSID(i);
            }, [this, i]() {
                accesspoints.getSelected(i) ? accesspoints.deselect(i) : accesspoints.select(i);
            }, [this, i]() {
                selectedID = i;
                changeMenu(&apMenu);
            });
        }
        addMenuNode(&apListMenu, D_SELECT_ALL, [this]() { // SELECT ALL
            accesspoints.selectAll();
            changeMenu(&apListMenu);
        });
        addMenuNode(&apListMenu, D_DESELECT_ALL, [this]() { // DESELECT ALL
            accesspoints.deselectAll();
            changeMenu(&apListMenu);
        });
        addMenuNode(&apListMenu, D_REMOVE_ALL, [this]() { // REMOVE ALL
            accesspoints.removeAll();
            goBack();
        });
    });

    // STATION LIST MENU
    createMenu(&stationListMenu, &showMenu, [this]() {
        // add stations to list
        int c = stations.count();

        for (int i = 0; i < c; i++) {
            addMenuNode(&stationListMenu, [i]() {
                return b2a(stations.getSelected(i)) +
                (stations.hasName(i) ? stations.getNameStr(i) : stations.getMacVendorStr(i));
            }, [this, i]() {
                stations.getSelected(i) ? stations.deselect(i) : stations.select(i);
            }, [this, i]() {
                selectedID = i;
                changeMenu(&stationMenu);
            });
        }

        addMenuNode(&stationListMenu, D_SELECT_ALL, [this]() { // SELECT ALL
            stations.selectAll();
            changeMenu(&stationListMenu);
        });
        addMenuNode(&stationListMenu, D_DESELECT_ALL, [this]() { // DESELECT ALL
            stations.deselectAll();
            changeMenu(&stationListMenu);
        });
        addMenuNode(&stationListMenu, D_REMOVE_ALL, [this]() { // REMOVE ALL
            stations.removeAll();
            goBack();
        });
    });

    // NAME LIST MENU
    createMenu(&nameListMenu, &showMenu, [this]() {
        // add device names to list
        int c = names.count();

        for (int i = 0; i < c; i++) {
            addMenuNode(&nameListMenu, [i]() {
                return names.getSelectedStr(i) + names.getName(i);
            }, [this, i]() {
                names.getSelected(i) ? names.deselect(i) : names.select(i);
            }, [this, i]() {
                selectedID = i;
                changeMenu(&nameMenu);
            });
        }
        addMenuNode(&nameListMenu, D_SELECT_ALL, [this]() { // SELECT ALL
            names.selectAll();
            changeMenu(&nameListMenu);
        });
        addMenuNode(&nameListMenu, D_DESELECT_ALL, [this]() { // DESELECT ALL
            names.deselectAll();
            changeMenu(&nameListMenu);
        });
        addMenuNode(&nameListMenu, D_REMOVE_ALL, [this]() { // REMOVE ALL
            names.removeAll();
            goBack();
        });
    });

    // SSID LIST MENU
    createMenu(&ssidListMenu, &showMenu, [this]() {
        addMenuNode(&ssidListMenu, D_CLONE_APS, [this]() { // CLONE APs
            ssids.cloneSelected(true);
            changeMenu(&ssidListMenu);
            ssids.save(false);
        });
        addMenuNode(&ssidListMenu, [this]() {
            return b2a(ssids.getRandom()) + str(D_RANDOM_MODE); // *RANDOM MODE
        }, [this]() {
            if (ssids.getRandom()) ssids.disableRandom();
            else ssids.enableRandom(10);
            changeMenu(&ssidListMenu);
        });

        // add ssids to list
        int c = ssids.count();

        for (int i = 0; i < c; i++) {
            addMenuNode(&ssidListMenu, [i]() {
                return ssids.getName(i).substring(0, ssids.getLen(i));
            }, [this, i]() {
                selectedID = i;
                changeMenu(&ssidMenu);
            }, [this, i]() {
                ssids.remove(i);
                changeMenu(&ssidListMenu);
                ssidListMenu.selected = i;
            });
        }

        addMenuNode(&ssidListMenu, D_REMOVE_ALL, [this]() { // REMOVE ALL
            ssids.removeAll();
            goBack();
        });
    });

    // AP MENU
    createMenu(&apMenu, &apListMenu, [this]() {
        addMenuNode(&apMenu, [this]() {
            return accesspoints.getSelectedStr(selectedID)  + accesspoints.getSSID(selectedID); // *<ssid>
        }, [this]() {
            accesspoints.getSelected(selectedID) ? accesspoints.deselect(selectedID) : accesspoints.select(selectedID);
        });
        addMenuNode(&apMenu, [this]() {
            return str(D_ENCRYPTION) + accesspoints.getEncStr(selectedID);
        }, NULL);                                                                          // Encryption: -/WPA2
        addMenuNode(&apMenu, [this]() {
            return str(D_RSSI) + (String)accesspoints.getRSSI(selectedID);
        }, NULL);                                                                          // RSSI: -90
        addMenuNode(&apMenu, [this]() {
            return str(D_CHANNEL) + (String)accesspoints.getCh(selectedID);
        }, NULL);                                                                          // Channel: 11
        addMenuNode(&apMenu, [this]() {
            return accesspoints.getMacStr(selectedID);
        }, NULL);                                                                          // 00:11:22:00:11:22
        addMenuNode(&apMenu, [this]() {
            return str(D_VENDOR) + accesspoints.getVendorStr(selectedID);
        }, NULL);                                                                          // Vendor: INTEL
        addMenuNode(&apMenu, [this]() {
            return accesspoints.getSelected(selectedID) ? str(D_DESELECT) : str(D_SELECT); // SELECT/DESELECT
        }, [this]() {
            accesspoints.getSelected(selectedID) ? accesspoints.deselect(selectedID) : accesspoints.select(selectedID);
        });
        addMenuNode(&apMenu, D_CLONE, [this]() { // CLONE
            ssids.add(accesspoints.getSSID(selectedID), accesspoints.getEnc(selectedID) != ENC_TYPE_NONE, 60, true);
            changeMenu(&showMenu);
            ssids.save(false);
        });
        addMenuNode(&apMenu, D_REMOVE, [this]() { // REMOVE
            accesspoints.remove(selectedID);
            apListMenu.list->remove(apListMenu.selected);
            goBack();
        });
    });

    // STATION MENU
    createMenu(&stationMenu, &stationListMenu, [this]() {
        addMenuNode(&stationMenu, [this]() {
            return stations.getSelectedStr(selectedID) +
            (stations.hasName(selectedID) ? stations.getNameStr(selectedID) : stations.getMacVendorStr(selectedID)); // <station
            // name>
        }, [this]() {
            stations.getSelected(selectedID) ? stations.deselect(selectedID) : stations.select(selectedID);
        });
        addMenuNode(&stationMenu, [this]() {
            return stations.getMacStr(selectedID);
        }, NULL);                                             // 00:11:22:00:11:22
        addMenuNode(&stationMenu, [this]() {
            return str(D_VENDOR) + stations.getVendorStr(selectedID);
        }, NULL);                                             // Vendor: INTEL
        addMenuNode(&stationMenu, [this]() {
            return str(D_AP) + stations.getAPStr(selectedID); // AP: someAP
        }, [this]() {
            int apID = accesspoints.find(stations.getAP(selectedID));

            if (apID >= 0) {
                selectedID = apID;
                changeMenu(&apMenu);
            }
        });
        addMenuNode(&stationMenu, [this]() {
            return str(D_PKTS) + String(*stations.getPkts(selectedID));
        }, NULL);                                                                      // Pkts: 12
        addMenuNode(&stationMenu, [this]() {
            return str(D_CHANNEL) + String(stations.getCh(selectedID));
        }, NULL);                                                                      // Channel: 11
        addMenuNode(&stationMenu, [this]() {
            return str(D_SEEN) + stations.getTimeStr(selectedID);
        }, NULL);                                                                      // Seen: <1min

        addMenuNode(&stationMenu, [this]() {
            return stations.getSelected(selectedID) ? str(D_DESELECT) : str(D_SELECT); // SELECT/DESELECT
        }, [this]() {
            stations.getSelected(selectedID) ? stations.deselect(selectedID) : stations.select(selectedID);
        });
        addMenuNode(&stationMenu, D_REMOVE, [this]() { // REMOVE
            stations.remove(selectedID);
            stationListMenu.list->remove(stationListMenu.selected);
            goBack();
        });
    });

    // NAME MENU
    createMenu(&nameMenu, &nameListMenu, [this]() {
        addMenuNode(&nameMenu, [this]() {
            return names.getSelectedStr(selectedID) + names.getName(selectedID); // <station name>
        }, [this]() {
            names.getSelected(selectedID) ? names.deselect(selectedID) : names.select(selectedID);
        });
        addMenuNode(&nameMenu, [this]() {
            return names.getMacStr(selectedID);
        }, NULL);                                                                   // 00:11:22:00:11:22
        addMenuNode(&nameMenu, [this]() {
            return str(D_VENDOR) + names.getVendorStr(selectedID);
        }, NULL);                                                                   // Vendor: INTEL
        addMenuNode(&nameMenu, [this]() {
            return str(D_AP) + names.getBssidStr(selectedID);
        }, NULL);                                                                   // AP: 00:11:22:00:11:22
        addMenuNode(&nameMenu, [this]() {
            return str(D_CHANNEL) + (String)names.getCh(selectedID);
        }, NULL);                                                                   // Channel: 11

        addMenuNode(&nameMenu, [this]() {
            return names.getSelected(selectedID) ? str(D_DESELECT) : str(D_SELECT); // SELECT/DESELECT
        }, [this]() {
            names.getSelected(selectedID) ? names.deselect(selectedID) : names.select(selectedID);
        });
        addMenuNode(&nameMenu, D_REMOVE, [this]() { // REMOVE
            names.remove(selectedID);
            nameListMenu.list->remove(nameListMenu.selected);
            goBack();
        });
    });

    // SSID MENU
    createMenu(&ssidMenu, &ssidListMenu, [this]() {
        addMenuNode(&ssidMenu, [this]() {
            return ssids.getName(selectedID).substring(0, ssids.getLen(selectedID));
        }, NULL);                                                   // SSID
        addMenuNode(&ssidMenu, [this]() {
            return str(D_ENCRYPTION) + ssids.getEncStr(selectedID); // WPA2
        }, [this]() {
            ssids.setWPA2(selectedID, !ssids.getWPA2(selectedID));
        });
        addMenuNode(&ssidMenu, D_REMOVE, [this]() { // REMOVE
            ssids.remove(selectedID);
            ssidListMenu.list->remove(ssidListMenu.selected);
            goBack();
        });
    });

    // ATTACK MENU
    createMenu(&attackMenu, &mainMenu, [this]() {
        addMenuNode(&attackMenu, [this]() { // *DEAUTH 0/0
            if (attack.isRunning()) return leftRight(b2a(deauthSelected) + str(D_DEAUTH),
                                                     (String)attack.getDeauthPkts() + SLASH +
                                                     (String)attack.getDeauthMaxPkts(), maxLen - 1);
            else return leftRight(b2a(deauthSelected) + str(D_DEAUTH), (String)scan.countSelected(), maxLen - 1);
        }, [this]() { // deauth
            deauthSelected = !deauthSelected;

            if (attack.isRunning()) {
                attack.start(beaconSelected, deauthSelected, false, probeSelected, true,
                             settings::getAttackSettings().timeout * 1000);
            }
        });
        addMenuNode(&attackMenu, [this]() { // *BEACON 0/0
            if (attack.isRunning()) return leftRight(b2a(beaconSelected) + str(D_BEACON),
                                                     (String)attack.getBeaconPkts() + SLASH +
                                                     (String)attack.getBeaconMaxPkts(), maxLen - 1);
            else return leftRight(b2a(beaconSelected) + str(D_BEACON), (String)ssids.count(), maxLen - 1);
        }, [this]() { // beacon
            beaconSelected = !beaconSelected;

            if (attack.isRunning()) {
                attack.start(beaconSelected, deauthSelected, false, probeSelected, true,
                             settings::getAttackSettings().timeout * 1000);
            }
        });
        addMenuNode(&attackMenu, [this]() { // *PROBE 0/0
            if (attack.isRunning()) return leftRight(b2a(probeSelected) + str(D_PROBE),
                                                     (String)attack.getProbePkts() + SLASH +
                                                     (String)attack.getProbeMaxPkts(), maxLen - 1);
            else return leftRight(b2a(probeSelected) + str(D_PROBE), (String)ssids.count(), maxLen - 1);
        }, [this]() { // probe
            probeSelected = !probeSelected;

            if (attack.isRunning()) {
                attack.start(beaconSelected, deauthSelected, false, probeSelected, true,
                             settings::getAttackSettings().timeout * 1000);
            }
        });
        addMenuNode(&attackMenu, [this]() { // START
            return leftRight(str(attack.isRunning() ? D_STOP_ATTACK : D_START_ATTACK),
                             attack.getPacketRate() > 0 ? (String)attack.getPacketRate() : String(), maxLen - 1);
        }, [this]() {
            if (attack.isRunning()) attack.stop();
            else attack.start(beaconSelected, deauthSelected, false, probeSelected, true,
                              settings::getAttackSettings().timeout * 1000);
        });
    });

    // CLOCK MENU
    createMenu(&clockMenu, &mainMenu, [this]() {
        addMenuNode(&clockMenu, D_CLOCK_DISPLAY, [this]() { // CLOCK
            mode = DISPLAY_MODE::CLOCK_DISPLAY;
            display.setFont(ArialMT_Plain_24);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
        });
        addMenuNode(&clockMenu, D_CLOCK_SET, [this]() { // CLOCK SET TIME
            mode = DISPLAY_MODE::CLOCK;
            display.setFont(ArialMT_Plain_24);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
        });
        addMenuNode(&clockMenu, D_NTP_SYNC, [this]() { // NTP SYNC (real time over Wi-Fi)
            syncNTP();
            mode = DISPLAY_MODE::CLOCK_DISPLAY;
            display.setFont(ArialMT_Plain_24);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
        });
    });

    // TOOLS MENU (extras)
    createMenu(&toolsMenu, &mainMenu, [this]() {
        addMenuNode(&toolsMenu, D_CHANALYZER, [this]() { // Wi-Fi channel analyzer
            scan.start(SCAN_MODE_APS, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::CHANNELS;
        });
        addMenuNode(&toolsMenu, D_SIGNAL, [this]() {     // signal (RSSI) meter
            rssiIdx = 0;
            scan.start(SCAN_MODE_APS, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::SIGNALMETER;
        });
        addMenuNode(&toolsMenu, D_STOPWATCH, [this]() {  // stopwatch
            swRunning = false;
            swAccum   = 0;
            swStart   = millis();
            mode      = DISPLAY_MODE::STOPWATCH;
        });
        addMenuNode(&toolsMenu, D_TIMER, [this]() {      // countdown timer
            tmrRunning = false;
            tmrRemain  = 60000;
            mode       = DISPLAY_MODE::COUNTDOWN;
        });
        addMenuNode(&toolsMenu, D_SNAKE, [this]() {      // snake game
            gameReset();
            mode = DISPLAY_MODE::SNAKE;
        });
        addMenuNode(&toolsMenu, D_SYSINFO, [this]() {    // system info
            mode = DISPLAY_MODE::SYSINFO;
        });
        addMenuNode(&toolsMenu, D_WIFI, [this]() {       // Wi-Fi connect
            if (strlen(WIFI_STA_SSID) > 0) {
                WiFi.persistent(false);
                WiFi.mode(WIFI_STA);
                WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
            }
            wifiTime = millis();
            mode     = DISPLAY_MODE::WIFICONNECT;
        });
        addMenuNode(&toolsMenu, D_FUN, [this]() {        // fun fake-hacker show
            funFrame  = 0;
            funTarget = "";
            scan.start(SCAN_MODE_APS, 0, SCAN_MODE_OFF, 0, true, wifi_channel);
            mode = DISPLAY_MODE::FUNMODE;
        });
    });

    // ===================== //

    // set current menu to main menu
    changeMenu(&mainMenu);
    enabled   = true;
    startTime = currentTime;
}

#ifdef HIGHLIGHT_LED
void DisplayUI::setupLED() {
    pinMode(HIGHLIGHT_LED, OUTPUT);
    digitalWrite(HIGHLIGHT_LED, HIGH);
    highlightLED = true;
}

#endif // ifdef HIGHLIGHT_LED

void DisplayUI::update(bool force) {
    if (!enabled) return;

    up->update();
    down->update();
    a->update();
    b->update();

    draw(force);

    uint32_t timeout = settings::getDisplaySettings().timeout * 1000;

    if (currentTime > timeout) {
        if (!tempOff) {
            if (buttonTime < currentTime - timeout) off();
        } else {
            if (buttonTime > currentTime - timeout) on();
        }
    }
}

void DisplayUI::on() {
    if (enabled) {
        configOn();
        tempOff    = false;
        buttonTime = currentTime; // update a button time to keep display on
        prntln(D_MSG_DISPLAY_ON);
    } else {
        prntln(D_ERROR_NOT_ENABLED);
    }
}

void DisplayUI::off() {
    if (enabled) {
        configOff();
        tempOff = true;
        prntln(D_MSG_DISPLAY_OFF);
    } else {
        prntln(D_ERROR_NOT_ENABLED);
    }
}

void DisplayUI::setupButtons() {
    up   = new ButtonPullup(BUTTON_UP);
    down = new ButtonPullup(BUTTON_DOWN);
    a    = new ButtonPullup(BUTTON_A);
    b    = new ButtonPullup(BUTTON_B);

    // === BUTTON UP === //
    up->setOnClicked([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;

        if (!tempOff) {
            if (mode == DISPLAY_MODE::MENU) {                 // when in menu, go up or down with cursor
                if (currentMenu->selected > 0) currentMenu->selected--;
                else currentMenu->selected = currentMenu->list->size() - 1;
            } else if (mode == DISPLAY_MODE::PACKETMONITOR) { // when in packet monitor, change channel
                scan.setChannel(wifi_channel + 1);
            } else if (mode == DISPLAY_MODE::CLOCK) {         // when in clock, change time
                setTime(clockHour, clockMinute + 1, clockSecond);
            } else if (mode == DISPLAY_MODE::SIGNALMETER) {   // previous AP
                if (rssiIdx > 0) rssiIdx--;
            } else if (mode == DISPLAY_MODE::STOPWATCH) {     // start / stop
                if (swRunning) { swAccum += millis() - swStart; swRunning = false; }
                else { swStart = millis(); swRunning = true; }
            } else if (mode == DISPLAY_MODE::COUNTDOWN) {     // +1 min (when stopped)
                if (!tmrRunning) { tmrRemain += 60000; if (tmrRemain > 600000) tmrRemain = 60000; }
            } else if (mode == DISPLAY_MODE::SNAKE) {         // turn left / restart
                if (snDead) gameReset();
                else if (!snTurned) { int8_t o = snDx; snDx = snDy; snDy = -o; snTurned = true; }
            }
        }
    });

    up->setOnHolding([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (!tempOff) {
            if (mode == DISPLAY_MODE::MENU) {                 // when in menu, go up or down with cursor
                if (currentMenu->selected > 0) currentMenu->selected--;
                else currentMenu->selected = currentMenu->list->size() - 1;
            } else if (mode == DISPLAY_MODE::PACKETMONITOR) { // when in packet monitor, change channel
                scan.setChannel(wifi_channel + 1);
            } else if (mode == DISPLAY_MODE::CLOCK) {         // when in clock, change time
                setTime(clockHour, clockMinute + 10, clockSecond);
            }
        }
    }, buttonDelay);

    // === BUTTON DOWN === //
    down->setOnClicked([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (!tempOff) {
            if (mode == DISPLAY_MODE::MENU) {                 // when in menu, go up or down with cursor
                if (currentMenu->selected < currentMenu->list->size() - 1) currentMenu->selected++;
                else currentMenu->selected = 0;
            } else if (mode == DISPLAY_MODE::PACKETMONITOR) { // when in packet monitor, change channel
                scan.setChannel(wifi_channel - 1);
            } else if (mode == DISPLAY_MODE::CLOCK) {         // when in clock, change time
                setTime(clockHour, clockMinute - 1, clockSecond);
            } else if (mode == DISPLAY_MODE::SIGNALMETER) {   // next AP
                rssiIdx++; // clamped in drawSignal()
            } else if (mode == DISPLAY_MODE::STOPWATCH) {     // reset
                swAccum = 0; swRunning = false; swStart = millis();
            } else if (mode == DISPLAY_MODE::COUNTDOWN) {     // start / pause
                if (tmrRunning) { tmrRemain = (tmrEnd > millis()) ? (tmrEnd - millis()) : 0; tmrRunning = false; }
                else { if (tmrRemain == 0) tmrRemain = 60000; tmrEnd = millis() + tmrRemain; tmrRunning = true; }
            } else if (mode == DISPLAY_MODE::SNAKE) {         // turn right / restart
                if (snDead) gameReset();
                else if (!snTurned) { int8_t o = snDx; snDx = -snDy; snDy = o; snTurned = true; }
            }
        }
    });

    down->setOnHolding([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (!tempOff) {
            if (mode == DISPLAY_MODE::MENU) {                 // when in menu, go up or down with cursor
                if (currentMenu->selected < currentMenu->list->size() - 1) currentMenu->selected++;
                else currentMenu->selected = 0;
            } else if (mode == DISPLAY_MODE::PACKETMONITOR) { // when in packet monitor, change channel
                scan.setChannel(wifi_channel - 1);
            }

            else if (mode == DISPLAY_MODE::CLOCK) {           // when in clock, change time
                setTime(clockHour, clockMinute - 10, clockSecond);
            }
        }
    }, buttonDelay);

    // === BUTTON A === //
    a->setOnClicked([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (!tempOff) {
            switch (mode) {
                case DISPLAY_MODE::MENU:

                    if (currentMenu->list->get(currentMenu->selected).click) {
                        currentMenu->list->get(currentMenu->selected).click();
                    }
                    break;

                case DISPLAY_MODE::PACKETMONITOR:
                case DISPLAY_MODE::LOADSCAN:
                    scan.stop();
                    mode = DISPLAY_MODE::MENU;
                    break;

                case DISPLAY_MODE::CLOCK:
                case DISPLAY_MODE::CLOCK_DISPLAY:
                    mode = DISPLAY_MODE::MENU;
                    display.setFont(DejaVu_Sans_Mono_12);
                    display.setTextAlignment(TEXT_ALIGN_LEFT);
                    break;

                case DISPLAY_MODE::SYSINFO:
                    mode = DISPLAY_MODE::MENU;
                    display.setFont(DejaVu_Sans_Mono_12);
                    display.setTextAlignment(TEXT_ALIGN_LEFT);
                    break;

                case DISPLAY_MODE::FUNMODE:
                    scan.stop(); // stop the show's AP scan
                    mode = DISPLAY_MODE::MENU;
                    display.setFont(DejaVu_Sans_Mono_12);
                    display.setTextAlignment(TEXT_ALIGN_LEFT);
                    break;

                case DISPLAY_MODE::WIFICONNECT:
                    WiFi.disconnect(true); // leave the network on exit
                    mode = DISPLAY_MODE::MENU;
                    display.setFont(DejaVu_Sans_Mono_12);
                    display.setTextAlignment(TEXT_ALIGN_LEFT);
                    break;

                case DISPLAY_MODE::CHANNELS:
                case DISPLAY_MODE::SIGNALMETER:
                    scan.stop();
                    mode = DISPLAY_MODE::MENU;
                    break;

                case DISPLAY_MODE::STOPWATCH:
                case DISPLAY_MODE::COUNTDOWN:
                case DISPLAY_MODE::SNAKE:
                    mode = DISPLAY_MODE::MENU;
                    break;
            }
        }
    });

    a->setOnHolding([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (!tempOff) {
            if (mode == DISPLAY_MODE::MENU) {
                if (currentMenu->list->get(currentMenu->selected).hold) {
                    currentMenu->list->get(currentMenu->selected).hold();
                }
            }
        }
    }, 800);

    // === BUTTON B === //
    b->setOnClicked([this]() {
        scrollCounter = 0;
        scrollTime    = currentTime;
        buttonTime    = currentTime;
        if (!tempOff) {
            switch (mode) {
                case DISPLAY_MODE::MENU:
                    goBack();
                    break;

                case DISPLAY_MODE::PACKETMONITOR:
                case DISPLAY_MODE::LOADSCAN:
                    scan.stop();
                    mode = DISPLAY_MODE::MENU;
                    break;

                case DISPLAY_MODE::CLOCK:
                    mode = DISPLAY_MODE::MENU;
                    display.setFont(DejaVu_Sans_Mono_12);
                    display.setTextAlignment(TEXT_ALIGN_LEFT);
                    break;
            }
        }
    });
}

String DisplayUI::getChannel() {
    String ch = String(wifi_channel);

    if (ch.length() < 2) ch = ' ' + ch;
    return ch;
}

void DisplayUI::draw(bool force) {
    if (force || ((currentTime - drawTime > drawInterval) && currentMenu)) {
        drawTime = currentTime;

        updatePrefix();

#ifdef RTC_DS3231
        bool h12;
        bool PM_time;
        clockHour   = clock.getHour(h12, PM_time);
        clockMinute = clock.getMinute();
        clockSecond = clock.getSecond();
#else // ifdef RTC_DS3231
        if (currentTime - clockTime >= 1000) {
            setTime(clockHour, clockMinute, ++clockSecond);
            clockTime += 1000;
        }
#endif // ifdef RTC_DS3231

        switch (mode) {
            case DISPLAY_MODE::BUTTON_TEST:
                drawButtonTest();
                break;

            case DISPLAY_MODE::MENU:
                drawMenu();
                break;

            case DISPLAY_MODE::LOADSCAN:
                drawLoadingScan();
                break;

            case DISPLAY_MODE::PACKETMONITOR:
                drawPacketMonitor();
                break;

            case DISPLAY_MODE::INTRO:
                if (!scan.isScanning() && (currentTime - startTime >= screenIntroTime)) {
                    mode = DISPLAY_MODE::MENU;
                }
                drawIntro();
                break;
            case DISPLAY_MODE::CLOCK:
            case DISPLAY_MODE::CLOCK_DISPLAY:
                drawClock();
                break;
            case DISPLAY_MODE::SYSINFO:
                drawSysInfo();
                break;
            case DISPLAY_MODE::FUNMODE:
                drawFun();
                break;
            case DISPLAY_MODE::WIFICONNECT:
                drawWifi();
                break;
            case DISPLAY_MODE::CHANNELS:
                drawChannels();
                break;
            case DISPLAY_MODE::SIGNALMETER:
                drawSignal();
                break;
            case DISPLAY_MODE::STOPWATCH:
                drawStopwatch();
                break;
            case DISPLAY_MODE::COUNTDOWN:
                drawTimer();
                break;
            case DISPLAY_MODE::SNAKE:
                drawGame();
                break;
            case DISPLAY_MODE::RESETTING:
                drawResetting();
                break;
        }

        updateSuffix();
    }
}

void DisplayUI::drawButtonTest() {
    drawString(0, str(D_UP) + b2s(up->read()));
    drawString(1, str(D_DOWN) + b2s(down->read()));
    drawString(2, str(D_A) + b2s(a->read()));
    drawString(3, str(D_B) + b2s(b->read()));
}

void DisplayUI::drawMenu() {
    String tmp;
    int    tmpLen;
    int    row = (currentMenu->selected / 5) * 5;

    // correct selected if it's off
    if (currentMenu->selected < 0) currentMenu->selected = 0;
    else if (currentMenu->selected >= currentMenu->list->size()) currentMenu->selected = currentMenu->list->size() - 1;

    // draw menu entries
    for (int i = row; i < currentMenu->list->size() && i < row + 5; i++) {
        tmp    = currentMenu->list->get(i).getStr();
        tmpLen = tmp.length();

        // horizontal scrolling
        if ((currentMenu->selected == i) && (tmpLen >= maxLen)) {
            tmp = tmp + tmp;
            tmp = tmp.substring(scrollCounter, scrollCounter + maxLen - 1);

            if (((scrollCounter > 0) && (scrollTime < currentTime - scrollSpeed)) || ((scrollCounter == 0) && (scrollTime < currentTime - scrollSpeed * 4))) {
                scrollTime = currentTime;
                scrollCounter++;
            }

            if (scrollCounter > tmpLen) scrollCounter = 0;
        }

        tmp = (currentMenu->selected == i ? CURSOR : SPACE) + tmp;
        drawString(0, (i - row) * 12, tmp);
    }
}

void DisplayUI::drawLoadingScan() {
    String percentage;

    if (scan.isScanning()) {
        percentage = String(scan.getPercentage()) + '%';
    } else {
        percentage = str(DSP_SCAN_DONE);
    }

    drawString(0, leftRight(str(DSP_SCAN_FOR), scan.getMode(), maxLen));
    drawString(1, leftRight(str(DSP_APS), String(accesspoints.count()), maxLen));
    drawString(2, leftRight(str(DSP_STS), String(stations.count()), maxLen));
    drawString(3, leftRight(str(DSP_PKTS), String(scan.getPacketRate()) + str(DSP_S), maxLen));
    drawString(4, center(percentage, maxLen));
}

void DisplayUI::drawPacketMonitor() {
    double scale = scan.getScaleFactor(sreenHeight - lineHeight - 2);

    String headline = leftRight(str(D_CH) + getChannel() + String(' ') + String('[') + String(scan.deauths) + String(']'), String(scan.getPacketRate()) + str(D_PKTS), maxLen);

    drawString(0, 0, headline);

    if (scan.getMaxPacket() > 0) {
        int i = 0;
        int x = 0;
        int y = 0;

        while (i < SCAN_PACKET_LIST_SIZE && x < screenWidth) {
            y = (sreenHeight-1) - (scan.getPackets(i) * scale);
            i++;

            // Serial.printf("%d,%d -> %d,%d\n", x, (sreenHeight-1), x, y);
            drawLine(x, (sreenHeight-1), x, y);
            x++;

            // Serial.printf("%d,%d -> %d,%d\n", x, (sreenHeight-1), x, y);
            drawLine(x, (sreenHeight-1), x, y);
            x++;
        }
        // Serial.println("---------");
    }
}

void DisplayUI::drawIntro() {
    String ver = String(DEAUTHER_VERSION);
    if (strlen(BUILD_TAG) > 0) ver += " " + String(BUILD_TAG);

    drawString(0, center(str(D_INTRO_0), maxLen));
    drawString(1, center(str(D_INTRO_1), maxLen));
    drawString(2, center(str(D_INTRO_2), maxLen));
    drawString(3, center(ver, maxLen));
    if (scan.isScanning()) {
        if (currentTime - startTime >= screenIntroTime+4500) drawString(4, left(str(D_SCANNING_3), maxLen));
        else if (currentTime - startTime >= screenIntroTime+3000) drawString(4, left(str(D_SCANNING_2), maxLen));
        else if (currentTime - startTime >= screenIntroTime+1500) drawString(4, left(str(D_SCANNING_1), maxLen));
        else if (currentTime - startTime >= screenIntroTime) drawString(4, left(str(D_SCANNING_0), maxLen));
    }
}

void DisplayUI::drawClock() {
    String clockTime = String(clockHour);

    clockTime += ':';
    if (clockMinute < 10) clockTime += '0';
    clockTime += String(clockMinute);

    display.drawString(64, 20, clockTime);
}

void DisplayUI::drawResetting() {
    drawString(2, center(str(D_RESETTING), maxLen));
}

// ===== System Info screen ===== //
// Read-only status page: firmware version + build tag, MAC, free RAM, uptime.
// Press the A button to return to the menu.
void DisplayUI::drawSysInfo() {
    uint32_t up = millis() / 1000;
    char upbuf[10];

    snprintf(upbuf, sizeof(upbuf), "%02u:%02u:%02u",
             (unsigned)(up / 3600), (unsigned)((up / 60) % 60), (unsigned)(up % 60));

    String ver = String(DEAUTHER_VERSION);
    if (strlen(BUILD_TAG) > 0) ver += " " + String(BUILD_TAG);

    drawString(0, leftRight(String(F("Ver")), ver, maxLen));
    drawString(1, WiFi.macAddress());
    drawString(2, leftRight(String(F("RAM")), String(ESP.getFreeHeap() / 1024.0, 1) + "K", maxLen));
    drawString(3, leftRight(String(F("Up")), String(upbuf), maxLen));

#ifdef BATTERY_MONITOR
    int   raw   = analogRead(A0);
    float volts = raw * (BATTERY_ADC_SCALE);
    drawString(4, leftRight(String(F("Bat")), String(raw) + " " + String(volts, 1) + "V", maxLen));
#else
    drawString(4, leftRight(String(F("Bat")), String(F("n/a")), maxLen));
#endif
}

// ===== Fun mode: scripted fake-hacker "show" ===== //
// A cinematic sequence that LOOKS like a real attack but isn't: it scans and
// lists REAL nearby Wi-Fi (passive, for authenticity), then plays theatrical
// "wifite / deauth / access" phases. The deauth phase is a VISUAL SIMULATION -
// it transmits nothing. (Real, deliberate attacks live in the ATTACK menu.)
// Press the A button to exit. Phases (frames @ ~10 fps, 320-frame loop):
//   boot -> scan -> handshake -> deauth(10s sim) -> clients -> exploit -> root
void DisplayUI::drawFun() {
    funFrame++;

    // Plays once then HOLDS on the final root-terminal frame (no looping).
    const uint32_t END = 320;
    uint32_t t   = (funFrame < END) ? funFrame : (END - 1);
    String   cur = ((funFrame / 4) % 2) ? String("_") : String(" "); // blinking cursor

    // lock a real target once, at the scan -> handshake transition
    if (t == 80) {
        if (accesspoints.count() > 0) {
            funTarget   = accesspoints.getSSID(0);
            funTargetCh = accesspoints.getCh(0);
        } else {
            funTarget   = "FreeWiFi";
            funTargetCh = 6;
        }
        if (funTarget.length() == 0) funTarget = "<hidden>";
    }

    String tgt = funTarget.length() ? funTarget : String("target");
    if (tgt.length() > 12) tgt = tgt.substring(0, 12);

    if (t < 20) {
        // boot: typed command
        String cmd = "wifite --kill";
        int    n   = (int)((t * cmd.length()) / 18);
        if (n > (int)cmd.length()) n = cmd.length();
        drawString(0, "root@kali:~#");
        drawString(1, "# " + cmd.substring(0, n) + cur);
    } else if (t < 80) {
        // scan real Wi-Fi
        drawString(0, "scanning WLANs" + String("...").substring(0, (t / 5) % 4));
        int cnt = accesspoints.count();
        drawString(1, leftRight("found APs", String(cnt), maxLen));
        int shown = 0;
        for (int i = cnt - 1; (i >= 0) && (shown < 3); i--, shown++) {
            String s = accesspoints.getSSID(i);
            if (s.length() == 0) s = "<hidden>";
            if (s.length() > 11) s = s.substring(0, 11);
            drawString(2 + shown, "ch" + String(accesspoints.getCh(i)) + " " + s);
        }
    } else if (t < 110) {
        // wifite-style handshake capture
        int pct = (int)(((t - 80) * 100) / 30);
        drawString(0, "wifite: " + tgt);
        drawString(1, "ch " + String(funTargetCh));
        drawString(2, leftRight("WPA handshake", String(pct) + "%", maxLen));
        drawString(3, "capturing EAPOL" + cur);
    } else if (t < 210) {
        // deauth - SIMULATION ONLY (no frames transmitted), 10 seconds
        uint32_t p    = t - 110;             // 0..99
        int      left = 10 - (int)(p / 10);  // 10..0
        int      pps  = 160 + (int)random(80);
        int      bars = (int)(p / 6);
        if (bars > 16) bars = 16;
        String bar = "[";
        for (int i = 0; i < 16; i++) bar += (i < bars) ? '#' : ' ';
        bar += "]";
        drawString(0, "deauth > " + tgt);
        drawString(1, leftRight("ch " + String(funTargetCh), String(left) + "s", maxLen));
        drawString(2, leftRight("pkts/s", String(pps), maxLen));
        drawString(3, bar);
        drawString(4, "clients dropping" + cur);
    } else if (t < 250) {
        // listing devices
        uint32_t p    = t - 210;
        int      rows = 1 + (int)(p / 10);
        if (rows > 4) rows = 4;
        drawString(0, "clients on " + tgt);
        for (int i = 0; i < rows; i++) {
            int  s = i * 37 + (int)(funFrame / 50);
            char mac[18];
            snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                     0x1A, 0x2B, (s * 7) & 0xFF, (s * 13) & 0xFF, (s * 29) & 0xFF, (s * 53) & 0xFF);
            drawString(1 + i, String(mac));
        }
    } else if (t < 280) {
        // gaining access
        int pct = (int)(((t - 250) * 100) / 30);
        drawString(0, "exploit > " + tgt);
        drawString(1, leftRight("privesc", String(pct) + "%", maxLen));
        drawString(2, "CVE-2024-" + String(1000 + (int)(funFrame % 9000)));
        drawString(3, "bypassing auth" + cur);
    } else {
        // root terminal
        drawString(0, "root@" + tgt.substring(0, 8) + ":~#");
        drawString(1, "id");
        drawString(2, "uid=0(root)");
        drawString(3, "[+] root access");
        drawString(4, "    granted" + cur);
    }
}

// ===== Wi-Fi connect screen ===== //
// Joins the credentials.h network as a client and shows live status. The menu
// kicked off WiFi.begin(); here we just poll it (non-blocking). Exit = button A.
void DisplayUI::drawWifi() {
    drawString(0, "WiFi connect");

    if (strlen(WIFI_STA_SSID) == 0) {
        drawString(2, "no network set");
        drawString(3, "edit credentials.h");
        return;
    }

    String ssid = String(WIFI_STA_SSID);
    if (ssid.length() > 16) ssid = ssid.substring(0, 16);
    drawString(1, ssid);

    wl_status_t st = WiFi.status();

    if (st == WL_CONNECTED) {
        drawString(2, "connected");
        drawString(3, WiFi.localIP().toString());
        drawString(4, leftRight("RSSI", String(WiFi.RSSI()) + "dBm", maxLen));
    } else if (millis() - wifiTime > 16000) {
        drawString(2, "failed");
        drawString(3, "status " + String((int)st));
        drawString(4, "A = back");
    } else {
        drawString(2, "connecting" + String("...").substring(0, (millis() / 400) % 4));
    }
}

// ===== Wi-Fi channel analyzer ===== //
// Bar graph of how many APs sit on each channel (1-13). Pick the least-busy one
// for your own router. Needs the AP scan (started on entry). A = exit.
void DisplayUI::drawChannels() {
    int chan[15] = { 0 };
    int cnt      = accesspoints.count();

    for (int i = 0; i < cnt; i++) {
        uint8_t c = accesspoints.getCh(i);
        if ((c >= 1) && (c <= 14)) chan[c]++;
    }

    int maxC = 1;
    int best = 1;
    for (int c = 1; c <= 13; c++) {
        if (chan[c] > maxC) maxC = chan[c];
        if (chan[c] < chan[best]) best = c;
    }

    drawString(0, leftRight("APs " + String(cnt), "best ch" + String(best), maxLen));

    const int baseY = 62;
    const int top   = 16;
    for (int c = 1; c <= 13; c++) {
        int x = 3 + (c - 1) * 9;
        int h = (chan[c] == 0) ? 0 : (2 + (chan[c] * (baseY - top)) / maxC);
        if (h > baseY - top) h = baseY - top;
        if (h > 0) display.fillRect(x, baseY - h, 6, h);
    }
}

// ===== Signal (RSSI) meter ===== //
// UP/DOWN pick an AP, big live dBm + bar. Walk around to find dead zones. A = exit.
void DisplayUI::drawSignal() {
    int cnt = accesspoints.count();

    drawString(0, "Signal meter");

    if (cnt == 0) {
        drawString(2, "scanning...");
        return;
    }

    if (rssiIdx >= cnt) rssiIdx = cnt - 1;
    if (rssiIdx < 0) rssiIdx = 0;

    String s = accesspoints.getSSID(rssiIdx);
    if (s.length() == 0) s = "<hidden>";
    if (s.length() > 12) s = s.substring(0, 12);

    int rssi = accesspoints.getRSSI(rssiIdx);

    drawString(1, leftRight(s, String(rssiIdx + 1) + "/" + String(cnt), maxLen));
    drawString(2, leftRight("ch " + String(accesspoints.getCh(rssiIdx)), String(rssi) + "dBm", maxLen));

    int pct = rssi + 90; // -90..-30 -> 0..60
    if (pct < 0) pct = 0;
    if (pct > 60) pct = 60;
    int w = (pct * 124) / 60;
    display.drawRect(2, 46, 124, 12);
    if (w > 0) display.fillRect(2, 46, w, 12);
}

// ===== Stopwatch ===== //  UP = start/stop, DOWN = reset, A = exit.
void DisplayUI::drawStopwatch() {
    uint32_t ms    = swRunning ? (millis() - swStart + swAccum) : swAccum;
    uint32_t tenth = ms / 100;
    char     buf[12];
    snprintf(buf, sizeof(buf), "%02u:%02u.%u",
             (unsigned)(tenth / 600), (unsigned)((tenth / 10) % 60), (unsigned)(tenth % 10));

    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 16, buf);
    display.setFont(DejaVu_Sans_Mono_12);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    drawString(4, swRunning ? "UP=stop DOWN=rst" : "UP=start DOWN=rst");
}

// ===== Countdown timer ===== //  UP = +1min, DOWN = start/pause, A = exit.
void DisplayUI::drawTimer() {
    long remain;
    bool expired = false;

    if (tmrRunning) {
        remain = (long)tmrEnd - (long)millis();
        if (remain <= 0) { remain = 0; expired = true; }
    } else {
        remain = (long)tmrRemain;
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%02ld:%02ld", (remain / 1000) / 60, (remain / 1000) % 60);

    bool blink = (millis() / 250) % 2;

    display.setFont(ArialMT_Plain_24);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    if (!expired || blink) display.drawString(64, 14, buf);
    display.setFont(DejaVu_Sans_Mono_12);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    if (expired) {
        drawString(4, "DONE  A=back");
#ifdef HIGHLIGHT_LED
        digitalWrite(HIGHLIGHT_LED, blink ? HIGH : LOW); // flash the LED
#endif
    } else {
        drawString(4, tmrRunning ? "DOWN=pause" : "UP=+1m DOWN=start");
    }
}

// ===== Snake ===== //  UP/DOWN = turn, A = exit. Pure fun.
void DisplayUI::gameSpawnFood() {
    for (int tries = 0; tries < 200; tries++) {
        uint8_t fx = random(GW);
        uint8_t fy = random(GH);
        bool    on = false;
        for (int i = 0; i < snLen; i++) {
            if ((snX[i] == fx) && (snY[i] == fy)) { on = true; break; }
        }
        if (!on) { foodX = fx; foodY = fy; return; }
    }
    foodX = 0; foodY = 0;
}

void DisplayUI::gameReset() {
    randomSeed(micros());
    snLen    = 3;
    snDx     = 1; snDy = 0;
    snX[0]   = GW / 2;     snY[0] = GH / 2;
    snX[1]   = snX[0] - 1; snY[1] = snY[0];
    snX[2]   = snX[0] - 2; snY[2] = snY[0];
    snScore  = 0;
    snDead   = false;
    snTurned = false;
    snTick   = millis();
    gameSpawnFood();
}

void DisplayUI::drawGame() {
    if (!snDead && (millis() - snTick >= 160)) {
        snTick   = millis();
        snTurned = false;

        int nx = snX[0] + snDx;
        int ny = snY[0] + snDy;

        if ((nx < 0) || (nx >= GW) || (ny < 0) || (ny >= GH)) {
            snDead = true;
        } else {
            for (int i = 0; i < snLen; i++) {
                if ((snX[i] == nx) && (snY[i] == ny)) { snDead = true; break; }
            }
            if (!snDead) {
                bool grew   = (nx == foodX) && (ny == foodY);
                int  newLen = grew ? snLen + 1 : snLen;
                if (newLen > SNAKE_MAX) newLen = SNAKE_MAX;
                for (int i = newLen - 1; i > 0; i--) { snX[i] = snX[i - 1]; snY[i] = snY[i - 1]; }
                snX[0] = nx; snY[0] = ny;
                snLen  = newLen;
                if (grew) { snScore++; gameSpawnFood(); }
            }
        }
    }

    if (snDead) {
        drawString(1, center(String(F("GAME OVER")), maxLen));
        drawString(2, center("score " + String(snScore), maxLen));
        drawString(4, center(String(F("UP/DOWN = retry")), maxLen));
        return;
    }

    const int cell = 4;
    display.fillRect(foodX * cell, foodY * cell, cell - 1, cell - 1);
    for (int i = 0; i < snLen; i++) display.fillRect(snX[i] * cell, snY[i] * cell, cell - 1, cell - 1);
}

void DisplayUI::clearMenu(Menu* menu) {
    while (menu->list->size() > 0) {
        menu->list->remove(0);
    }
}

void DisplayUI::changeMenu(Menu* menu) {
    if (menu) {
        // only open list menu if it has nodes
        if (((menu == &apListMenu) && (accesspoints.count() == 0)) ||
            ((menu == &stationListMenu) && (stations.count() == 0)) ||
            ((menu == &nameListMenu) && (names.count() == 0))) {
            return;
        }

        if (currentMenu) clearMenu(currentMenu);
        currentMenu           = menu;
        currentMenu->selected = 0;
        buttonTime            = currentTime;

        if (selectedID < 0) selectedID = 0;

        if (currentMenu->parentMenu) {
            addMenuNode(currentMenu, D_BACK, currentMenu->parentMenu); // add [BACK]
            currentMenu->selected = 1;
        }

        if (currentMenu->build) currentMenu->build();
    }
}

void DisplayUI::goBack() {
    if (currentMenu->parentMenu) changeMenu(currentMenu->parentMenu);
}

void DisplayUI::createMenu(Menu* menu, Menu* parent, std::function<void()>build) {
    menu->list       = new SimpleList<MenuNode>;
    menu->parentMenu = parent;
    menu->selected   = 0;
    menu->build      = build;
}

void DisplayUI::addMenuNode(Menu* menu, std::function<String()>getStr, std::function<void()>click,
                            std::function<void()>hold) {
    menu->list->add(MenuNode{ getStr, click, hold });
}

void DisplayUI::addMenuNode(Menu* menu, std::function<String()>getStr, std::function<void()>click) {
    addMenuNode(menu, getStr, click, NULL);
}

void DisplayUI::addMenuNode(Menu* menu, std::function<String()>getStr, Menu* next) {
    addMenuNode(menu, getStr, [this, next]() {
        changeMenu(next);
    });
}

void DisplayUI::addMenuNode(Menu* menu, const char* ptr, std::function<void()>click) {
    addMenuNode(menu, [ptr]() {
        return str(ptr);
    }, click);
}

void DisplayUI::addMenuNode(Menu* menu, const char* ptr, Menu* next) {
    addMenuNode(menu, [ptr]() {
        return str(ptr);
    }, next);
}

void DisplayUI::setTime(int h, int m, int s) {
    if (s >= 60) {
        s = 0;
        m++;
    }

    if (m >= 60) {
        m = 0;
        h++;
    }

    if (h >= 24) {
        h = 0;
    }

    if (s < 0) {
        s = 59;
        m--;
    }

    if (m < 0) {
        m = 59;
        h--;
    }

    if (h < 0) {
        h = 23;
    }

    clockHour   = h;
    clockMinute = m;
    clockSecond = s;

#ifdef RTC_DS3231
    clock.setHour(clockHour);
    clock.setMinute(clockMinute);
    clock.setSecond(clockSecond);
#endif // ifdef RTC_DS3231
}

// Briefly join the Wi-Fi network from credentials.h, fetch time over NTP, and
// seed the (software) clock with real local time. User-triggered from the clock
// menu; it blocks for up to ~16s and temporarily takes the radio off whatever
// it was doing, so don't run it mid-attack. Credentials stay local (git-ignored).
bool DisplayUI::syncNTP() {
    if (strlen(WIFI_STA_SSID) == 0) {
        Serial.println("NTP: no Wi-Fi configured (credentials.h)");
        return false;
    }

    // we're about to block, so show a status line now
    display.clear();
    drawString(2, center(String(F("NTP: connecting")), maxLen));
    display.display();

    Serial.printf("NTP: connecting to \"%s\"...\n", WIFI_STA_SSID);

    WiFiMode_t prevMode = WiFi.getMode();
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);

    uint32_t t0      = millis();
    bool     retried = false;
    while ((WiFi.status() != WL_CONNECTED) && (millis() - t0 < 15000)) {
        delay(200);
        if (!retried && (millis() - t0 > 7000)) { // one re-kick for flaky association
            WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
            retried = true;
        }
    }

    bool ok = false;

    Serial.printf("NTP: WiFi.status=%d (3=connected)\n", WiFi.status());

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("NTP: got IP ");
        Serial.print(WiFi.localIP());
        Serial.printf(" RSSI %d dBm\n", WiFi.RSSI());

        configTime(NTP_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov");

        time_t   now = 0;
        uint32_t t1  = millis();
        while ((now < 100000) && (millis() - t1 < 10000)) {
            now = time(nullptr);
            delay(200);
        }

        Serial.printf("NTP: epoch=%ld\n", (long)now);

        if (now > 100000) {
            struct tm tm;
            localtime_r(&now, &tm);
            setTime(tm.tm_hour, tm.tm_min, tm.tm_sec);
            clockTime = currentTime; // realign the software seconds counter
            ok = true;
            Serial.printf("NTP: time set to %02d:%02d:%02d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
        }

        // Fallback: many managed networks block UDP/123 but allow HTTP. Read the
        // server's "Date:" header (GMT) and apply the local offset.
        if (!ok) {
            Serial.println("NTP: SNTP blocked, trying HTTP Date header...");
            WiFiClient client;
            if (client.connect("www.google.com", 80)) {
                client.print(F("HEAD / HTTP/1.1\r\nHost: www.google.com\r\nConnection: close\r\n\r\n"));
                uint32_t t2 = millis();
                while (client.connected() && (millis() - t2 < 6000)) {
                    String line = client.readStringUntil('\n');
                    // "Date: Sat, 11 Oct 2026 09:45:00 GMT"
                    if ((line.length() >= 31) &&
                        (line.startsWith("Date:") || line.startsWith("date:"))) {
                        int h = line.substring(23, 25).toInt() + (NTP_OFFSET_SEC / 3600);
                        int m = line.substring(26, 28).toInt();
                        int s = line.substring(29, 31).toInt();
                        setTime(h, m, s);
                        clockTime = currentTime;
                        ok = true;
                        Serial.printf("NTP(HTTP): GMT %s -> local %02d:%02d:%02d\n",
                                      line.substring(23, 31).c_str(), clockHour, clockMinute, clockSecond);
                        break;
                    }
                    if (line == "\r") break; // end of headers
                }
                client.stop();
            } else {
                Serial.println("NTP: HTTP connect failed");
            }
        }
    }

    WiFi.disconnect(true);
    WiFi.mode(prevMode);

    Serial.printf("NTP sync: %s\n", ok ? "OK" : "FAILED");
    return ok;
}
