/*
  Astronomy Desk — Waveshare ESP32-S3-Touch-LCD-7, 800x480.
  Clock, date, weather, moon, tonight list, star map, planets, comets,
  asteroids, meteor showers, launches, satellites, and space weather.
  This file is only for this board. Do not flash it on the 4 inch or the 4.3.
  Prefs name astro7. First boot may copy Wi-Fi and place from ultwx, then skywx.
  It does not clear those desks. About can check GitHub for a newer copy.
  Downloads pause on Wi-Fi, place, the star map, the satellite page, and the
  aurora map. Those pages draw lines, so a download is not allowed to run on top.
*/

#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Wire.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <TJpg_Decoder.h>
#include <FS.h>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "iss_world.h"
#include "stars.h"
#include "rocks.h"
#include "sats.h"
#include "photos.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PW 800
#define PH 480
#define TOUCH_SDA 8
#define TOUCH_SCL 9
#define TOUCH_INT 4
#define GT911_ADDR 0x5D
#define CH422_WR_SET 0x24
#define CH422_WR_IO 0x38
#define EXIO_TP_RST 1
#define EXIO_LCD_BL 2
#define EXIO_LCD_RST 3
#define EXIO_SD_CS 4
#define EXIO_USB_SEL 5
#define ASTRO_VER "1.0"
#define ASTRO_GH_USER "stefi01"
#define ASTRO_GH_REPO "Astronomy-7-inch-display"
#define ASTRO_GH_BIN "astro_7in.bin"
#define PACK_CAP 8192
#define KW 80
#define CX 400
#define DEG 0.01745329252
#define SKY_CX 400
#define SKY_CY 220
#define SKY_R 158
#define MAP_Y0 48
#define MAP_H 292
#define NAME_CAP 96

enum {
  PG_HOME = 0, PG_SKY, PG_PLANETS, PG_MOON, PG_MORE, PG_SATS, PG_ROCKS,
  PG_SHOWERS, PG_LAUNCH, PG_WIFI, PG_PLACE, PG_SET, PG_ABOUT,
  PG_NIGHT, PG_BODY, PG_SPACE, PG_METEOR, PG_CRAFT, PG_COMETS, PG_SCOPES,
  PG_DARK, PG_CLOUD, PG_NEO, PG_SOLAR, PG_FIND, PG_CARD
};
enum { NET_NONE = 0, NET_PACK, NET_GEO, NET_SATS, NET_LAUNCH, NET_SPACE, NET_OTA, NET_CREW };

struct PlaceHit { char label[40]; float lat, lon; };
struct ZoneRow { const char *iana; const char *posix; };
struct Launch {
  char day[12];
  char when[8];
  char rocket[28];
  char mission[40];
  char pad[28];
  char agency[16];
  char status[24];
  char payload[96];
  char going[24];
  char owner[28];
};
struct IssTle {
  double epochJd, n, e, inc, raan, argp, m0;
  bool ok;
};
struct NowBody {
  int az, el;
  double distAu;
  double extra;
  bool ok;
  bool glare;
};
struct SatLive {
  IssTle tle;
  float lat, lon, alt;
  int az, el;
  bool ok, vis;
  char when[64];
  char pass[2][64];
  int nPass;
  float trkLat[72];
  float trkLon[72];
  int nTrk;
};
struct Shower {
  const char *name;
  int month, day, span, zhr, speed;
  int ra100, dec10;
  const char *parent;
  const char *fact;
};
struct ViewWin {
  char span[48];
  char high[24];
  int az, el;
  bool ok;
};
struct BodyFact {
  const char *made;
  const char *air;
  const char *moons;
  int km;
  const char *more;
  const char *dayLen;
  const char *yearLen;
  const char *temps;
};
struct DsoMark {
  const char *name;
  const char *note;
  float raH, dec;
  int ly;
  const char *count;
};
struct StarWide {
  const char *n;
  int suns10;
};
struct CraftFact {
  const char *what;
  const char *size;
  const char *made;
  const char *note;
};
struct RocketFact {
  const char *key;
  const char *tall;
  const char *filled;
  const char *speed;
  const char *stage1;
  const char *stage2;
};
struct PlanetOrb {
  const char *name;
  double L0, Ld, a;
};

static const ZoneRow ZONES[] = {
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Detroit", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Kentucky/Louisville", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Kentucky/Monticello", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Indianapolis", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Marengo", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Petersburg", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Vevay", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Vincennes", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Winamac", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Knox", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Indiana/Tell_City", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Menominee", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/North_Dakota/Beulah", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/North_Dakota/Center", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/North_Dakota/New_Salem", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Boise", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Phoenix", "MST7"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Juneau", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Sitka", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Yakutat", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Nome", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Metlakatla", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"America/Adak", "HST10HDT,M3.2.0,M11.1.0"},
    {"Pacific/Honolulu", "HST10"},
    {"America/Puerto_Rico", "AST4"},
    {"America/Virgin", "AST4"},
    {"America/St_Thomas", "AST4"},
    {"Pacific/Guam", "ChST-10"},
    {"Pacific/Saipan", "ChST-10"},
    {"Pacific/Pago_Pago", "SST11"},
};
static const char *MON[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char *WDAY[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const PlanetOrb PLANETS[] = {
    {"Mercury", 252.25, 4.09233, 0.387}, {"Venus", 181.98, 1.60213, 0.723},
    {"Mars", 355.43, 0.52402, 1.524},    {"Jupiter", 34.35, 0.08309, 5.203},
    {"Saturn", 50.08, 0.03346, 9.537},   {"Uranus", 314.05, 0.01173, 19.22},
    {"Neptune", 304.35, 0.00602, 30.07},
};
static const Shower SHOWERS[] = {
    {"Quadrantids", 1, 3, 2, 80, 41, 1533, 495, "2003 EH1", "dormant comet, about 4 km across"},
    {"Lyrids", 4, 22, 2, 18, 47, 1808, 333, "comet Thatcher", "C/1861 G1, seen in 1861"},
    {"Eta Aquariids", 5, 6, 3, 50, 66, 2233, -10, "comet dust", "small grains. the comet is on the Comets page"},
    {"Perseids", 8, 12, 3, 100, 59, 320, 580, "Swift-Tuttle", "nucleus 26 km across"},
    {"Orionids", 10, 21, 3, 20, 66, 633, 155, "comet dust", "same dust trail as the May shower"},
    {"Leonids", 11, 17, 2, 15, 71, 1013, 216, "Tempel-Tuttle", "nucleus about 3.6 km across"},
    {"Geminids", 12, 14, 3, 150, 35, 747, 322, "3200 Phaethon", "an asteroid, about 6 km across"},
    {"Ursids", 12, 22, 2, 10, 33, 1447, 750, "8P/Tuttle", "dust along that comet's orbit"},
};
static const BodyFact SUN_FACT = {"hydrogen and helium", "hot gas, no hard surface", "none. the planets orbit it", 1392700,
                                  "about 865,000 miles across", "about 25 days at the equator",
                                  "the planets orbit it", "surface about 10,000 F"};
static const BodyFact PLANET_FACT[] = {
    {"iron core and basalt crust", "almost none", "0 moons", 4879, "the smallest planet",
     "about 4,224 hours", "about 88 days", "day about 800 F, night about -290 F"},
    {"iron core and basalt crust", "thick carbon dioxide", "0 moons", 12104, "the thickest air of the inner planets",
     "about 2,808 hours", "about 225 days", "about 860 F day and night"},
    {"iron core, basalt crust, and water ice", "thin carbon dioxide", "2 moons, Phobos and Deimos", 6792,
     "Phobos and Deimos are tiny. their material is unknown", "24 hours 37 min",
     "about 687 days", "day about 70 F, night about -100 F"},
    {"hydrogen and helium", "hydrogen and helium",
     "4 big moons: Io, Europa, Ganymede, Callisto. smaller moons are not listed", 142984,
     "Io is sulfur and silicate. Europa, Ganymede, and Callisto are water ice and silicate.", "9 hours 56 min",
     "about 4,380 days", "cloud tops about -160 F, day and night"},
    {"hydrogen and helium", "hydrogen and helium",
     "7 big moons: Titan, Rhea, Iapetus, Dione, Tethys, Enceladus, Mimas. smaller moons are not listed", 120536,
     "Titan is water ice and silicate. The rings are water ice.", "10 hours 40 min",
     "about 10,585 days", "cloud tops about -220 F, day and night"},
    {"hydrogen, helium, water, methane, and ammonia", "hydrogen, helium, methane",
     "5 big moons: Miranda, Ariel, Umbriel, Titania, Oberon", 51118,
     "the large moons are water ice and silicate", "17 hours", "about 30,660 days",
     "cloud tops about -320 F, day and night"},
    {"hydrogen, helium, water, methane, and ammonia", "hydrogen, helium, methane",
     "1 big moon, Triton. smaller moons are not listed", 49528,
     "Triton is nitrogen ice and water ice, and orbits backward",
     "16 hours", "about 60,225 days", "cloud tops about -330 F, day and night"},
};
struct ScopeFact {
  const char *name;
  const char *where;
  const char *mirror;
  const char *glass;
  const char *launched;
  const char *svc1;
  const char *svc2;
  const char *svc3;
  const char *svc4;
  int satIx;
};
static const ScopeFact SCOPES[] = {
    {"Hubble", "low orbit, about 320 miles. the height on this page is live", "mirror 7.9 feet (2.4 m)",
     "no single magnification. detail about 0.05 arcsec", "launched 24 Apr 1990 on space shuttle Discovery",
     "last serviced May 2009 by space shuttle Atlantis. new camera, gyros, and batteries",
     "Dec 1993, space shuttle Endeavour fixed the blurry mirror",
     "Feb 1997, space shuttle Discovery added a spectrograph and an infrared camera",
     "Dec 1999, space shuttle Discovery replaced the gyros. Mar 2002, space shuttle Columbia added a camera", 2},
    {"James Webb", "about 930,000 miles, Sun-Earth balance point", "mirror 21 feet (6.5 m), 18 gold segments",
     "no single magnification. it sees infrared", "launched 25 Dec 2021 on an Ariane 5 rocket",
     "never serviced after launch. no crew can reach it", "", "", "", -1},
    {"Chandra", "about 10,000 to 83,000 miles. distance changes", "nested X-ray mirrors, outer about 4 feet",
     "no single magnification. it sees X-rays", "launched 23 Jul 1999 on space shuttle Columbia",
     "never serviced after launch", "", "", "", -1},
    {"Euclid", "about 1 million miles, Sun-Earth balance point", "mirror 3.9 feet (1.2 m)",
     "no single magnification. visible and infrared survey", "launched 1 Jul 2023 on a SpaceX Falcon 9",
     "never serviced after launch. no crew can reach it", "", "", "", -1},
    {"TESS", "high orbit, about 67,000 to 230,000 miles", "four cameras, each about a 4 inch lens",
     "no single magnification. it watches stars for planets", "launched 18 Apr 2018 on a SpaceX Falcon 9",
     "never serviced after launch", "", "", "", -1},
};
static const int SCOPE_N = (int)(sizeof(SCOPES) / sizeof(SCOPES[0]));
static const DsoMark DEEP[] = {
    {"M45", "cluster", 3.792f, 24.12f, 444, "about 1,000 stars"},
    {"M42", "Orion nebula", 5.588f, -5.39f, 1344, "thousands of young stars"},
    {"M44", "cluster", 8.668f, 19.98f, 577, "about 1,000 stars"},
    {"M8", "Lagoon nebula", 18.062f, -24.38f, 4100, "a star-forming cloud"},
    {"M7", "cluster", 17.898f, -34.79f, 980, "about 80 stars"},
    {"M13", "cluster", 16.695f, 36.46f, 22200, "hundreds of thousands of stars"},
    {"M31", "Andromeda galaxy", 0.712f, 41.27f, 2500000, "about a trillion stars"},
    {"M33", "Triangulum galaxy", 1.565f, 30.66f, 2730000, "about 40 billion stars"},
    {"M81", "Bode galaxy", 9.927f, 69.07f, 12000000, "about 250 billion stars"},
    {"M51", "Whirlpool galaxy", 13.498f, 47.20f, 31000000, "a large spiral galaxy"},
    {"LMC", "Large Magellanic Cloud", 5.392f, -69.76f, 163000, "about 30 billion stars"},
    {"SMC", "Small Magellanic Cloud", 0.877f, -72.83f, 200000, "about 3 billion stars"},
};
static const StarWide STAR_WIDE[] = {
    {"Sirius", 17},     {"Canopus", 710},  {"Arcturus", 250}, {"Vega", 24},
    {"Capella", 120},   {"Rigel", 790},    {"Procyon", 20},   {"Betelgeuse", 7600},
    {"Altair", 18},     {"Aldebaran", 440}, {"Spica", 74},    {"Antares", 6800},
    {"Pollux", 90},     {"Fomalhaut", 18}, {"Deneb", 2000},  {"Regulus", 42},
    {"Polaris", 460},   {"Achernar", 100}, {"Algol", 27},    {"Bellatrix", 60},
};
static const int WIDE_N = (int)(sizeof(STAR_WIDE) / sizeof(STAR_WIDE[0]));
static const CraftFact CRAFT_FACT[] = {
    {"crewed station", "about 357 feet long", "aluminum",
     "about 90 minutes a loop, near 17,500 mph"},
    {"crewed station", "about 180 feet long", "aluminum", "China's station, usually three people"},
    {"space telescope", "about 43 feet long", "glass, mirror 7.9 feet", "launched in 1990"},
};
static const RocketFact ROCKET_FACT[] = {
    {"Falcon Heavy", "vehicle height about 230 feet", "filled weight about 3,130,000 lb",
     "about 17,500 mph in a low orbit",
     "1st stage, three cores, about 2.6 million lb of kerosene and oxygen",
     "2nd stage about 204,000 lb of kerosene and oxygen"},
    {"Falcon 9", "vehicle height about 230 feet", "filled weight about 1,210,000 lb",
     "about 17,500 mph in a low orbit",
     "1st stage about 872,000 lb of kerosene and oxygen", "2nd stage about 204,000 lb of kerosene and oxygen"},
    {"Electron", "vehicle height about 59 feet", "filled weight about 28,700 lb",
     "about 17,500 mph in a low orbit",
     "1st stage kerosene and oxygen, nine engines", "2nd stage kerosene and oxygen, one engine"},
    {"Starship", "vehicle height about 400 feet with the booster", "filled weight about 11 million lb",
     "about 17,500 mph in a low orbit",
     "booster about 7.5 million lb of methane and oxygen", "ship about 2.6 million lb of methane and oxygen"},
    {"SLS", "vehicle height about 322 feet", "filled weight about 5.8 million lb on the first model",
     "about 17,500 mph in a low orbit, faster toward the Moon",
     "core stage hydrogen and oxygen, plus two solid boosters", "upper stage hydrogen and oxygen"},
    {"Atlas", "vehicle height about 190 feet. the model changes it", "filled weight depends on the model",
     "about 17,500 mph in a low orbit",
     "1st stage kerosene and oxygen. pounds depend on the model", "2nd stage hydrogen and oxygen"},
    {"Vulcan", "vehicle height about 202 feet", "filled weight unknown",
     "about 17,500 mph in a low orbit",
     "1st stage methane and oxygen, plus solid boosters", "2nd stage hydrogen and oxygen"},
    {"Soyuz", "vehicle height about 152 feet", "filled weight about 690,000 lb",
     "about 17,500 mph in a low orbit",
     "boosters and core use kerosene and oxygen", "upper stage kerosene and oxygen"},
    {"Ariane", "vehicle height about 170 to 200 feet, by model", "filled weight depends on the model",
     "about 17,500 mph in a low orbit",
     "stage fuel depends on the model", "stage fuel depends on the model"},
    {"Long March", "vehicle height depends on the model", "filled weight depends on the model",
     "about 17,500 mph in a low orbit",
     "stage fuel depends on the model", "stage fuel depends on the model"},
    {"New Glenn", "vehicle height about 320 feet", "filled weight unknown",
     "about 17,500 mph in a low orbit",
     "1st stage methane and oxygen", "2nd stage hydrogen and oxygen"},
    {"H3", "vehicle height about 207 feet", "filled weight depends on the model",
     "about 17,500 mph in a low orbit",
     "1st stage hydrogen and oxygen, plus solid boosters", "2nd stage hydrogen and oxygen"},
};
static const int STAR_N = (int)(sizeof(STARS) / sizeof(STARS[0]));
static const int NAME_N = (int)(sizeof(STAR_NAME) / sizeof(STAR_NAME[0]));
static const int ROCK_N = (int)(sizeof(ROCKS) / sizeof(ROCKS[0]));
static const int SAT_N = (int)(sizeof(SATS) / sizeof(SATS[0]));
static const int SHOWER_N = (int)(sizeof(SHOWERS) / sizeof(SHOWERS[0]));
static const int DEEP_N = (int)(sizeof(DEEP) / sizeof(DEEP[0]));
static const int PLANET_N = (int)(sizeof(PLANETS) / sizeof(PLANETS[0]));
static const uint16_t SAT_COL[3] = {RGB565(255, 196, 60), RGB565(70, 170, 210), RGB565(255, 80, 160)};

Arduino_ESP32RGBPanel *rgbBus = nullptr;
uint8_t ch422Io = 0;
Arduino_RGB_Display *gfx = nullptr;
Preferences prefs;
SemaphoreHandle_t i2cLock = nullptr;
uint16_t *worldPx = nullptr;
bool worldReady = false;
bool gfxOk = false;
bool sdOk = false;

uint16_t COL_BG, COL_DIM, COL_MID, COL_HOT, COL_TXT, COL_WHT, COL_UP, COL_DN, COL_AMB, COL_SEA, COL_PINK;

volatile int page = PG_HOME;
volatile int placeGen = 0;
volatile bool netHold = false;
volatile bool netBusy = false;
volatile int netPend = NET_NONE;
volatile bool homeDirty = false;
volatile bool skyDirty = false;
volatile bool satsDirty = false;
volatile bool launchDirty = false;
volatile bool placeDirty = false;
volatile bool spaceDirty = false;
volatile bool cloudDirty = false;
volatile bool neoDirty = false;
volatile bool solarDirty = false;
volatile bool satBusy = false;

int backTo = PG_HOME;
int blLevel = 2;
bool dark = true;
bool useC = false;
bool wifiOk = false;
bool placeSet = false;
bool placeTyping = true;
float placeLat = 0, placeLon = 0;
char placeName[40] = "";
char placeQuery[42] = "";
char placeTz[56] = "";
char placeMsg[28] = "";
bool placeShift = false, placeSym = false;
PlaceHit hits[4];
int nHits = 0;

bool wxOk = false;
bool wxDay = true;
int wxCode = 0, wxTemp = 0;
int wxCloud = -1;
int auroraKp = -1;
int windSpeed = -1;
int windPair = -1;
int windDens10 = -1;
float xrayFlux[8];
int xrayN = 0;
struct SunRegion {
  char num[8];
  char loc[12];
  char mag[20];
};
SunRegion sunRegions[8];
int nSunRegions = -1;
struct ClosePass {
  char des[20];
  char when[24];
  char dist[16];
  char spd[16];
};
ClosePass closePass[5];
int nClose = -1;
char darkEve[16] = "";
char darkMorn[16] = "";
int darkNow = 0;
int windBt = -1;
int windBz = -999;
int flareProb = -1;
int radProb = -1;
int kpNext[8];
int kpNextN = 0;
char flareNow[8] = "";
char flareMax[8] = "";
char flareAt[20] = "";
char stormNow[16] = "";
char stormDay[16] = "";
char sunUp[16] = "";
char sunDown[16] = "";
uint32_t lastSun = 0;
uint32_t lastBody = 0;
int shownKey = -999;
uint32_t lastSkyDraw = 0;
uint32_t lastSatDraw = 0;
uint32_t satMoveAt = 0;
uint32_t satHeavyAt = 0;
uint32_t lastTap = 0;

bool sunOk = false;
float sunLat = 0, sunLon = 0;
NowBody sunNow, moonNow;
NowBody planetNow[7];
NowBody rockNow[8];
SatLive satLive[4];

Launch launches[4];
int nLaunch = 0;
bool launchOk = false;
uint32_t lastLaunch = 0;
uint32_t lastTle = 0;
uint32_t nextPackAt = 0;
uint32_t nextSatAt = 0;
uint32_t nextLaunchAt = 0;
uint32_t nextSpaceAt = 0;
char issCrew[8][22];
char issNation[8][20];
int nIssCrew = -1;
uint32_t nextCrewAt = 0;
volatile bool crewDirty = false;
int meteorPick = 0;
int bodyPick = 0;
int nightPage = 0;
int rockPage = 0;
int factKind = 0;
int factId = 0;
char findQ[20] = "";
uint8_t findSrc[4];
int findId[4];
int nFind = 0;
int nFindMore = 0;
uint8_t cardSrc = 0;
int cardId = 0;
int cardY = 56;
char otaMsg[48] = "";
char otaLatest[12] = "";
bool otaNewer = false;
volatile bool otaBusy = false;
volatile int otaKind = 0;
volatile int otaPct = -1;
volatile bool otaDirty = false;
uint32_t lastWifiTry = 0;

int pickKind = 0;
int pickId = -1;

char wifiSsid[64] = "";
char wifiPass[64] = "";
char wifiScan[16][33];
char wifiMsg[28] = "";
int nWifiScan = 0, wifiScanTop = 0, wifiFocus = 0;
bool wifiShift = false, wifiSym = false, wifiScanBusy = false, wifiScanWait = false;

static char netBuf[PACK_CAP];
volatile int blDuty = 10;
TaskHandle_t blTask = nullptr;

void cols() {
  if (dark) {
    COL_BG = RGB565(12, 18, 28);
    COL_DIM = RGB565(24, 34, 48);
    COL_MID = RGB565(150, 168, 186);
    COL_HOT = RGB565(255, 196, 60);
    COL_TXT = RGB565(236, 240, 244);
    COL_WHT = RGB565(255, 255, 255);
    COL_UP = RGB565(70, 200, 120);
    COL_DN = RGB565(220, 64, 64);
    COL_AMB = RGB565(240, 160, 40);
    COL_SEA = RGB565(70, 170, 210);
    COL_PINK = RGB565(255, 80, 160);
  } else {
    COL_BG = RGB565(236, 232, 220);
    COL_DIM = RGB565(214, 208, 192);
    COL_MID = RGB565(80, 84, 96);
    COL_HOT = RGB565(20, 60, 120);
    COL_TXT = RGB565(20, 24, 28);
    COL_WHT = RGB565(255, 255, 255);
    COL_UP = RGB565(20, 120, 70);
    COL_DN = RGB565(170, 40, 40);
    COL_AMB = RGB565(160, 90, 10);
    COL_SEA = RGB565(20, 90, 140);
    COL_PINK = RGB565(180, 30, 90);
  }
}

void centre(const char *s, int cx, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(cx - (int)w / 2 - x1, y);
  gfx->print(s);
}

bool hit(int x, int y, int x0, int y0, int x1, int y1) {
  return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}
bool hitBox(int x, int y, int bx, int by, int bw, int bh) {
  return hit(x, y, bx, by, bx + bw, by + bh);
}
void chip(int x, int y, int w, int h, const char *t, bool on) {
  gfx->fillRoundRect(x, y, w, h, 6, on ? COL_HOT : COL_DIM);
  gfx->setTextColor(on ? COL_BG : COL_TXT, on ? COL_HOT : COL_DIM);
  gfx->setTextSize(2);
  centre(t, x + w / 2, y + h / 2 - 8);
}

void applyPlaceTz() {
  if (!placeTz[0]) return;
  setenv("TZ", placeTz, 1);
  tzset();
}

struct I2cHold {
  I2cHold() {
    if (i2cLock) xSemaphoreTake(i2cLock, portMAX_DELAY);
  }
  ~I2cHold() {
    if (i2cLock) xSemaphoreGive(i2cLock);
  }
};

void ch422Write(uint8_t addr, uint8_t val) {
  I2cHold hold;
  Wire.beginTransmission(addr);
  Wire.write(val);
  Wire.endTransmission();
}
void ch422Flush() { ch422Write(CH422_WR_IO, ch422Io); }
void ch422Bit(int bit, bool on) {
  I2cHold hold;
  if (on) ch422Io |= (uint8_t)(1u << bit);
  else ch422Io &= (uint8_t)~(1u << bit);
  Wire.beginTransmission(CH422_WR_IO);
  Wire.write(ch422Io);
  Wire.endTransmission();
}
void boardIoBegin() {
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  ch422Write(CH422_WR_SET, 0x01);
  delay(2);
  ch422Io = (uint8_t)((1u << EXIO_TP_RST) | (1u << EXIO_LCD_RST) | (1u << EXIO_SD_CS));
  ch422Flush();
  pinMode(TOUCH_INT, OUTPUT);
  digitalWrite(TOUCH_INT, LOW);
  ch422Bit(EXIO_TP_RST, false);
  delay(10);
  ch422Bit(EXIO_TP_RST, true);
  delay(50);
  pinMode(TOUCH_INT, INPUT);
}
void sdMount() {
  ch422Bit(EXIO_SD_CS, true);
  SD_MMC.setPins(12, 11, 13);
  sdOk = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT);
}
void blPwm(void *) {
  for (;;) {
    int duty = blDuty;
    if (duty >= 10) {
      ch422Bit(EXIO_LCD_BL, true);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (duty < 1) duty = 1;
    ch422Bit(EXIO_LCD_BL, true);
    vTaskDelay(pdMS_TO_TICKS(duty));
    if (blDuty >= 10) continue;
    ch422Bit(EXIO_LCD_BL, false);
    vTaskDelay(pdMS_TO_TICKS(10 - duty));
  }
}
void applyBl() {
  if (!i2cLock) i2cLock = xSemaphoreCreateMutex();
  if (blLevel <= 0) blDuty = 3;
  else if (blLevel == 1) blDuty = 6;
  else blDuty = 10;
  if (blDuty >= 10) ch422Bit(EXIO_LCD_BL, true);
  if (!blTask) xTaskCreatePinnedToCore(blPwm, "blpwm", 4096, nullptr, 2, &blTask, 0);
}

void savePrefs() {
  prefs.begin("astro7", false);
  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPass);
  prefs.putBool("set", placeSet);
  prefs.putFloat("lat", placeLat);
  prefs.putFloat("lon", placeLon);
  prefs.putString("name", placeName);
  prefs.putString("tzp", placeTz);
  prefs.putInt("bl", blLevel);
  prefs.putBool("c", useC);
  prefs.putBool("dk", dark);
  prefs.end();
}
void loadPrefs() {
  prefs.begin("astro7", true);
  strlcpy(wifiSsid, prefs.getString("ssid", "").c_str(), sizeof(wifiSsid));
  strlcpy(wifiPass, prefs.getString("pass", "").c_str(), sizeof(wifiPass));
  placeSet = prefs.getBool("set", false);
  placeLat = prefs.getFloat("lat", 0);
  placeLon = prefs.getFloat("lon", 0);
  strlcpy(placeName, prefs.getString("name", "").c_str(), sizeof(placeName));
  strlcpy(placeTz, prefs.getString("tzp", "").c_str(), sizeof(placeTz));
  blLevel = prefs.getInt("bl", 2);
  useC = prefs.getBool("c", false);
  dark = prefs.getBool("dk", true);
  prefs.end();
  if (!placeSet || !placeName[0]) placeSet = false;
  if (blLevel < 0 || blLevel > 2) blLevel = 2;
  applyPlaceTz();
  cols();
}
void borrowOne(const char *ns) {
  if (!prefs.begin(ns, true)) return;
  if (!wifiSsid[0]) {
    String ssid = prefs.getString("ssid", "");
    if (ssid.length()) {
      strlcpy(wifiSsid, ssid.c_str(), sizeof(wifiSsid));
      strlcpy(wifiPass, prefs.getString("pass", "").c_str(), sizeof(wifiPass));
    }
  }
  if (!placeSet) {
    String name = prefs.getString("name", "");
    if (prefs.getBool("set", false) && name.length()) {
      placeSet = true;
      placeLat = prefs.getFloat("lat", 0);
      placeLon = prefs.getFloat("lon", 0);
      strlcpy(placeName, name.c_str(), sizeof(placeName));
      strlcpy(placeTz, prefs.getString("tzp", "").c_str(), sizeof(placeTz));
    }
  }
  prefs.end();
}
bool borrowDesks() {
  bool had = wifiSsid[0] || placeSet;
  borrowOne("ultwx");
  borrowOne("skywx");
  return (wifiSsid[0] || placeSet) && !had;
}
void factoryReset() {
  prefs.begin("astro7", false);
  prefs.clear();
  prefs.end();
  prefs.begin("astro7x", false);
  prefs.putBool("noborrow", true);
  prefs.end();
}

bool clockReady() {
  time_t now = time(nullptr);
  return now > 1700000000;
}
bool liveClock(time_t *out) {
  time_t now = time(nullptr);
  if (now <= 1700000000) return false;
  if (out) *out = now;
  return true;
}
int clockKey() {
  if (!placeTz[0] || !clockReady()) return -1;
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_yday * 1440 + t.tm_hour * 60 + t.tm_min;
}
void fmtHm(int h24, int mi, char *out, int n) {
  int h = h24 % 12;
  if (h == 0) h = 12;
  snprintf(out, n, "%d:%02d %s", h, mi, h24 >= 12 ? "PM" : "AM");
}
int iround(float v) { return (int)(v + (v >= 0.f ? 0.5f : -0.5f)); }
int showT(int f) {
  if (!useC) return f;
  return iround((f - 32) * 5.f / 9.f);
}
const char *tempUnit() { return useC ? "C" : "F"; }
const char *codeWord(int c, bool daytime) {
  if (c >= 95) return "Storm";
  if ((c >= 71 && c <= 77) || c == 85 || c == 86) return "Snow";
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82)) return "Rain";
  if (c == 45 || c == 48) return "Fog";
  if (c == 0 || c == 1) return daytime ? "Clear" : "Clear";
  if (c == 2) return "Cloud";
  if (c == 3) return "Gray";
  return "Cloud";
}
const char *lookWord(int az) {
  static const char *d[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return d[((az + 22) / 45) & 7];
}

void drawBack(const char *title) {
  chip(8, 8, 140, 36, "< BACK", false);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre(title, CX, 16);
}
void drawFoot() {
  const char *lab[] = {"SKY", "PLANETS", "MOON", "MORE"};
  for (int i = 0; i < 4; i++) chip(8 + i * 198, 414, 190, 52, lab[i], false);
}
int footAt(int x, int y) {
  if (y < 414 || y > 466) return -1;
  int i = (x - 8) / 198;
  if (i < 0 || i > 3) return -1;
  if (x > 8 + i * 198 + 190) return -1;
  return i;
}
void drawMoonPhase(int cx, int cy, int r, float phase) {
  const uint16_t lit = RGB565(236, 232, 214);
  const uint16_t shade = RGB565(28, 32, 42);
  gfx->fillCircle(cx, cy, r, shade);
  if (phase >= 0 && phase <= 1) {
    float k = cosf(phase * 2.0f * (float)M_PI);
    for (int dy = -r; dy <= r; dy++) {
      int span = r * r - dy * dy;
      if (span <= 0) continue;
      int xe = (int)sqrtf((float)span);
      int x1, x2;
      if (phase < 0.5f) {
        x1 = (int)(k * (float)xe);
        x2 = xe;
      } else {
        x1 = -xe;
        x2 = (int)(-k * (float)xe);
      }
      if (x2 > x1) gfx->drawFastHLine(cx + x1, cy + dy, x2 - x1, lit);
    }
  }
  gfx->drawCircle(cx, cy, r, COL_MID);
}

void wrap360(double &a) {
  if (a != a || a > 1.0e7 || a < -1.0e7) {
    a = 0;
    return;
  }
  a = fmod(a, 360.0);
  if (a < 0) a += 360.0;
}
double unixToJd(time_t t) { return 2440587.5 + (double)t / 86400.0; }

void sunEcl(double jd, double *Ls, double *R, double *eps) {
  double T = (jd - 2451545.0) / 36525.0;
  double L = 280.460 + 36000.770 * T;
  double M = 357.528 + 35999.050 * T;
  wrap360(L);
  wrap360(M);
  double Mr = M * DEG;
  *Ls = L + 1.915 * sin(Mr) + 0.020 * sin(2 * Mr);
  wrap360(*Ls);
  *R = 1.00014 - 0.01671 * cos(Mr) - 0.00014 * cos(2 * Mr);
  *eps = 23.439 - 0.013 * T;
}
void planetRaDec(double L, double er, double *ra, double *dec) {
  double lr = L * DEG;
  *ra = atan2(cos(er) * sin(lr), cos(lr));
  *dec = asin(sin(er) * sin(lr));
}
void azelFromRaDec(double raRad, double dec, time_t t, int *az, int *el) {
  double jd = unixToJd(t);
  double gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0);
  wrap360(gmst);
  double lst = (gmst + placeLon) * DEG;
  double ha = lst - raRad;
  double lat = placeLat * DEG;
  double alt = asin(sin(lat) * sin(dec) + cos(lat) * cos(dec) * cos(ha));
  double azi = atan2(-cos(dec) * sin(ha), sin(dec) * cos(lat) - cos(dec) * sin(lat) * cos(ha));
  *el = (int)(alt * 57.2957795);
  int a = (int)(azi * 57.2957795);
  if (a < 0) a += 360;
  *az = a;
}
void sunAlt(time_t t, float *altOut) {
  double Ls, R, eps;
  sunEcl(unixToJd(t), &Ls, &R, &eps);
  double ra, dec;
  planetRaDec(Ls, eps * DEG, &ra, &dec);
  int az, el;
  azelFromRaDec(ra, dec, t, &az, &el);
  if (altOut) *altOut = (float)el;
}
void updateSunPos() {
  time_t t = 0;
  if (!liveClock(&t)) {
    sunOk = false;
    return;
  }
  double Ls, R, eps;
  sunEcl(unixToJd(t), &Ls, &R, &eps);
  double ra, dec;
  planetRaDec(Ls, eps * DEG, &ra, &dec);
  double jd = unixToJd(t);
  double gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0);
  wrap360(gmst);
  double raDeg = ra * 57.295779513;
  if (raDeg < 0) raDeg += 360.0;
  double gha = gmst - raDeg;
  wrap360(gha);
  sunLon = (float)(-gha);
  if (sunLon < -180.0f) sunLon += 360.0f;
  if (sunLon > 180.0f) sunLon -= 360.0f;
  sunLat = (float)(dec * 57.295779513);
  sunOk = true;
}
void drawSunMark(int x, int y) {
  gfx->fillCircle(x, y, 7, RGB565(255, 210, 40));
  gfx->drawCircle(x, y, 11, RGB565(255, 150, 20));
  for (int a = 0; a < 8; a++) {
    float r = a * 0.785398f;
    gfx->drawLine(x + (int)(sinf(r) * 13), y - (int)(cosf(r) * 13), x + (int)(sinf(r) * 20),
                  y - (int)(cosf(r) * 20), RGB565(255, 196, 60));
  }
}
bool dayLit(float lat, float lon) {
  float sls = sinf(sunLat * (float)DEG);
  float cls = cosf(sunLat * (float)DEG);
  float cd = cosf((lon - sunLon) * (float)DEG);
  float alt = sinf(lat * (float)DEG) * sls + cosf(lat * (float)DEG) * cls * cd;
  return alt >= 0;
}

float moonFrac() {
  time_t now;
  if (!liveClock(&now)) return -1;
  double days = ((double)now - 947182440.0) / 86400.0;
  double syn = 29.53058867;
  return (float)(days / syn - floor(days / syn));
}
const char *moonName(float *phaseOut, int *pctOut) {
  float p = moonFrac();
  if (p < 0) return nullptr;
  if (phaseOut) *phaseOut = p;
  int pct = (int)(50.0 * (1.0 - cos(2.0 * M_PI * p)) + 0.5);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  if (pctOut) *pctOut = pct;
  if (p < 0.03f || p > 0.97f) return "new";
  if (p < 0.22f) return "crescent";
  if (p < 0.28f) return "first quarter";
  if (p < 0.47f) return "mostly full";
  if (p < 0.53f) return "full";
  if (p < 0.72f) return "mostly full";
  if (p < 0.78f) return "last quarter";
  return "crescent";
}

void fmtRange(double au, char *out, int n) {
  if (au <= 0 || au != au) {
    out[0] = 0;
    return;
  }
  double miles = au * 92955807.0;
  double lm = au * 8.3167;
  if (au < 0.01)
    snprintf(out, n, "%.0f miles   %.1f light sec", miles, au * 499.0);
  else if (lm < 90)
    snprintf(out, n, "%.2f AU   %.0f million miles   %.0f light min", au, miles / 1.0e6, lm);
  else
    snprintf(out, n, "%.1f AU   %.0f million miles   %.1f light hr", au, miles / 1.0e6, lm / 60.0);
}
static int orbitMph(double aAu) {
  if (!(aAu > 0.05)) return 0;
  double mph = 29.785 / sqrt(aAu) * 2236.94;
  int r = (int)(mph + 50.0);
  return (r / 100) * 100;
}
static void fmtMph(int mph, char *out, int n) {
  if (mph < 1000 || n < 8) {
    if (n > 0) out[0] = 0;
    return;
  }
  snprintf(out, n, "about %d,%03d mph", mph / 1000, mph % 1000);
}
void fmtLy(int ly, char *out, int n) {
  if (ly <= 0) {
    out[0] = 0;
    return;
  }
  if (ly >= 1000000) snprintf(out, n, "%.1f million light years", ly / 1000000.0);
  else if (ly >= 10000) snprintf(out, n, "%d thousand light years", (ly + 500) / 1000);
  else snprintf(out, n, "%d light years", ly);
}
void fmtMag10(int mag10, char *out, int n) {
  int neg = mag10 < 0;
  int a = neg ? -mag10 : mag10;
  snprintf(out, n, "%s%d.%d", neg ? "-" : "", a / 10, a % 10);
}

bool skyXY(int az, int el, int *x, int *y) {
  if (el < 0) return false;
  float r = (90.0f - (float)el) / 90.0f * (float)SKY_R;
  float a = (float)az * (float)DEG;
  *x = SKY_CX + (int)(r * sinf(a));
  *y = SKY_CY - (int)(r * cosf(a));
  return true;
}

void rockPlace(const Rock &rk, time_t now, int *az, int *el, double *distAu, double *helio, double *mag,
              double *raOut = nullptr, double *decOut = nullptr) {
  double jd = unixToJd(now);
  double Ls, R, eps;
  sunEcl(jd, &Ls, &R, &eps);
  double n = 0.9856076686 / pow(rk.a, 1.5);
  double Mdeg = rk.ma + n * (jd - rk.epochJd);
  wrap360(Mdeg);
  double M = Mdeg * DEG;
  double e = rk.e;
  double E = M;
  for (int k = 0; k < 12; k++) E = M + e * sin(E);
  double nu = 2.0 * atan2(sqrt(1.0 + e) * sin(E / 2.0), sqrt(1.0 - e) * cos(E / 2.0));
  double r = rk.a * (1.0 - e * cos(E));
  double u = rk.w * DEG + nu;
  double om = rk.om * DEG;
  double inc = rk.inc * DEG;
  double xh = r * cos(u);
  double yh = r * sin(u);
  double x = xh * cos(om) - yh * cos(inc) * sin(om);
  double y = xh * sin(om) + yh * cos(inc) * cos(om);
  double z = yh * sin(inc);
  double xE = -R * cos(Ls * DEG);
  double yE = -R * sin(Ls * DEG);
  double dx = x - xE;
  double dy = y - yE;
  double dz = z;
  double er = eps * DEG;
  double eqx = dx;
  double eqy = dy * cos(er) - dz * sin(er);
  double eqz = dy * sin(er) + dz * cos(er);
  double ra = atan2(eqy, eqx);
  double dec = atan2(eqz, sqrt(eqx * eqx + eqy * eqy));
  double dist = sqrt(dx * dx + dy * dy + dz * dz);
  azelFromRaDec(ra, dec, now, az, el);
  if (raOut) *raOut = ra;
  if (decOut) *decOut = dec;
  if (distAu) *distAu = dist;
  if (helio) *helio = r;
  if (mag) {
    if (rk.H < 40.0f && r > 0.05 && dist > 0.01)
      *mag = rk.H + 5.0 * log10(r * dist);
    else
      *mag = 99;
  }
}

void fillNow() {
  time_t now;
  if (!liveClock(&now) || !placeSet) return;
  if (lastBody && millis() - lastBody < 20000UL) return;
  double jd = unixToJd(now);
  double T = (jd - 2451545.0) / 36525.0;
  double Ls, R, eps;
  sunEcl(jd, &Ls, &R, &eps);
  double ra, dec;
  planetRaDec(Ls, eps * DEG, &ra, &dec);
  azelFromRaDec(ra, dec, now, &sunNow.az, &sunNow.el);
  sunNow.distAu = R;
  sunNow.ok = true;
  sunNow.glare = false;
  double Lp = 218.316 + 481267.881 * T;
  double Mp = 134.963 + 477198.868 * T;
  wrap360(Lp);
  wrap360(Mp);
  double mlam = Lp + 6.289 * sin(Mp * DEG);
  planetRaDec(mlam, eps * DEG, &ra, &dec);
  azelFromRaDec(ra, dec, now, &moonNow.az, &moonNow.el);
  moonNow.distAu = (385000.56 * (1.0 - 0.0549 * cos(Mp * DEG))) / 149597870.7;
  moonNow.ok = true;
  moonNow.glare = false;
  for (int i = 0; i < PLANET_N && i < 7; i++) {
    double L = PLANETS[i].L0 + PLANETS[i].Ld * (jd - 2451545.0);
    wrap360(L);
    double lr = L * DEG, sr = Ls * DEG;
    double x = PLANETS[i].a * cos(lr) + R * cos(sr);
    double y = PLANETS[i].a * sin(lr) + R * sin(sr);
    double lam = atan2(y, x) * 57.2957795;
    if (lam < 0) lam += 360.0;
    double elong = fabs(lam - Ls);
    if (elong > 180) elong = 360 - elong;
    planetNow[i].glare = elong < 8.0;
    planetNow[i].distAu = sqrt(x * x + y * y);
    planetRaDec(lam, eps * DEG, &ra, &dec);
    azelFromRaDec(ra, dec, now, &planetNow[i].az, &planetNow[i].el);
    planetNow[i].ok = true;
  }
  for (int i = 0; i < ROCK_N && i < 8; i++) {
    double mag = 99, helio = 0;
    rockPlace(ROCKS[i], now, &rockNow[i].az, &rockNow[i].el, &rockNow[i].distAu, &helio, &mag);
    rockNow[i].extra = mag;
    rockNow[i].ok = true;
    rockNow[i].glare = false;
    (void)helio;
  }
  lastBody = millis();
}

void skyHours(double ra, double dec, time_t now, char *hrs, int n) {
  char rs[12] = "", ss[12] = "";
  int prev = -99;
  for (int m = 0; m <= 24 * 60; m += 20) {
    if ((m & 255) == 0) delay(1);
    int az, el;
    azelFromRaDec(ra, dec, now + (time_t)m * 60, &az, &el);
    struct tm ti;
    time_t t = now + (time_t)m * 60;
    localtime_r(&t, &ti);
    char hm[16];
    fmtHm(ti.tm_hour, ti.tm_min, hm, sizeof(hm));
    if (prev < 0 && el >= 0 && !rs[0]) strlcpy(rs, hm, sizeof(rs));
    if (prev >= 0 && el < 0 && !ss[0]) strlcpy(ss, hm, sizeof(ss));
    prev = el;
  }
  if (rs[0] && ss[0]) snprintf(hrs, n, "up %s   down %s", rs, ss);
  else if (rs[0]) snprintf(hrs, n, "up %s", rs);
  else strlcpy(hrs, "up through the night", n);
}
void computeSunTimes() {
  sunUp[0] = sunDown[0] = 0;
  time_t now;
  if (!liveClock(&now) || !placeSet || !placeTz[0]) return;
  struct tm ti;
  localtime_r(&now, &ti);
  ti.tm_hour = 0;
  ti.tm_min = 0;
  ti.tm_sec = 0;
  time_t t0 = mktime(&ti);
  int prev = -99;
  for (int m = 0; m <= 24 * 60; m += 10) {
    if ((m & 127) == 0) delay(1);
    float alt = -99;
    sunAlt(t0 + (time_t)m * 60, &alt);
    struct tm t;
    time_t tt = t0 + (time_t)m * 60;
    localtime_r(&tt, &t);
    char hm[16];
    fmtHm(t.tm_hour, t.tm_min, hm, sizeof(hm));
    int up = alt >= 0 ? 1 : -1;
    if (prev < 0 && up > 0 && !sunUp[0]) strlcpy(sunUp, hm, sizeof(sunUp));
    if (prev > 0 && up < 0 && !sunDown[0]) strlcpy(sunDown, hm, sizeof(sunDown));
    prev = up;
  }
  lastSun = millis();
}
void computeDark() {
  darkEve[0] = darkMorn[0] = 0;
  darkNow = 0;
  time_t now;
  if (!liveClock(&now) || !placeSet || !placeTz[0]) return;
  float nowAlt = 0;
  sunAlt(now, &nowAlt);
  darkNow = nowAlt < -18.0f ? 1 : 2;
  struct tm ti;
  localtime_r(&now, &ti);
  ti.tm_hour = 0;
  ti.tm_min = 0;
  ti.tm_sec = 0;
  time_t t0 = mktime(&ti);
  int prev = -99;
  for (int m = 0; m <= 24 * 60; m += 10) {
    if ((m & 127) == 0) delay(1);
    float alt = -99;
    sunAlt(t0 + (time_t)m * 60, &alt);
    struct tm t;
    time_t tt = t0 + (time_t)m * 60;
    localtime_r(&tt, &t);
    char hm[16];
    fmtHm(t.tm_hour, t.tm_min, hm, sizeof(hm));
    int down = alt < -18.0f ? 1 : -1;
    if (prev > 0 && down < 0 && !darkMorn[0]) strlcpy(darkMorn, hm, sizeof(darkMorn));
    if (prev < 0 && down > 0 && !darkEve[0]) strlcpy(darkEve, hm, sizeof(darkEve));
    prev = down;
  }
}

int showerDelta(const Shower &s, const struct tm &nowTm) {
  struct tm p = nowTm;
  p.tm_mon = s.month - 1;
  p.tm_mday = s.day;
  p.tm_hour = 12;
  p.tm_min = 0;
  p.tm_sec = 0;
  mktime(&p);
  int d = p.tm_yday - nowTm.tm_yday;
  if (d < -180) d += 365;
  if (d > 180) d -= 365;
  return d;
}
void meteorView(const Shower &s, time_t now, ViewWin *v) {
  v->ok = false;
  v->az = 0;
  v->el = -99;
  v->span[0] = 0;
  v->high[0] = 0;
  if (!placeSet || !placeTz[0]) {
    strlcpy(v->span, "set a place", sizeof(v->span));
    return;
  }
  struct tm ti;
  localtime_r(&now, &ti);
  ti.tm_hour = 16;
  ti.tm_min = 0;
  ti.tm_sec = 0;
  time_t t0 = mktime(&ti);
  double ra = (s.ra100 / 100.0) * 15.0 * DEG;
  double dec = (s.dec10 / 10.0) * DEG;
  time_t riseT = 0, setT = 0, bestT = 0;
  int bestEl = -99, bestAz = 0;
  for (int m = 0; m <= 16 * 60; m += 15) {
    time_t t = t0 + (time_t)m * 60;
    int az, el;
    azelFromRaDec(ra, dec, t, &az, &el);
    float sun = 0;
    sunAlt(t, &sun);
    if (sun < -12.0f && el >= 20) {
      if (!riseT) riseT = t;
      setT = t;
      if (el > bestEl) {
        bestEl = el;
        bestAz = az;
        bestT = t;
      }
    }
  }
  if (!riseT || !bestT) {
    strlcpy(v->span, "radiant stays low here", sizeof(v->span));
    return;
  }
  struct tm ta, tb, th;
  localtime_r(&riseT, &ta);
  localtime_r(&setT, &tb);
  localtime_r(&bestT, &th);
  char ha[16], hb[16], hh[16];
  fmtHm(ta.tm_hour, ta.tm_min, ha, sizeof(ha));
  fmtHm(tb.tm_hour, tb.tm_min, hb, sizeof(hb));
  fmtHm(th.tm_hour, th.tm_min, hh, sizeof(hh));
  snprintf(v->span, sizeof(v->span), "%s to %s", ha, hb);
  strlcpy(v->high, hh, sizeof(v->high));
  v->az = bestAz;
  v->el = bestEl;
  v->ok = true;
}
void showerLine(char *out, int n) {
  time_t now;
  if (!liveClock(&now) || !placeTz[0]) {
    strlcpy(out, "meteor showers wait for the clock", n);
    return;
  }
  struct tm t;
  localtime_r(&now, &t);
  int best = -1, bestD = 400;
  for (int i = 0; i < SHOWER_N; i++) {
    int d = showerDelta(SHOWERS[i], t);
    if (d >= -SHOWERS[i].span && d <= SHOWERS[i].span) {
      ViewWin w;
      meteorView(SHOWERS[i], now, &w);
      snprintf(out, n, "meteor shower  %s  %s", SHOWERS[i].name, w.span);
      return;
    }
    if (d > 0 && d < bestD) {
      bestD = d;
      best = i;
    }
  }
  if (best < 0) {
    strlcpy(out, "no meteor shower date", n);
    return;
  }
  snprintf(out, n, "next meteor shower  %s  %s %d", SHOWERS[best].name, MON[SHOWERS[best].month - 1],
           SHOWERS[best].day);
}
bool bodySky(int which, time_t now, double *ra, double *dec) {
  double jd = unixToJd(now);
  double T = (jd - 2451545.0) / 36525.0;
  double Ls, R, eps;
  sunEcl(jd, &Ls, &R, &eps);
  if (which == 0) {
    planetRaDec(Ls, eps * DEG, ra, dec);
    return true;
  }
  if (which < 0) {
    double Lp = 218.316 + 481267.881 * T;
    double Mp = 134.963 + 477198.868 * T;
    wrap360(Lp);
    wrap360(Mp);
    planetRaDec(Lp + 6.289 * sin(Mp * DEG), eps * DEG, ra, dec);
    return true;
  }
  int i = which - 1;
  if (i < 0 || i >= PLANET_N) return false;
  double L = PLANETS[i].L0 + PLANETS[i].Ld * (jd - 2451545.0);
  wrap360(L);
  double x = PLANETS[i].a * cos(L * DEG) + R * cos(Ls * DEG);
  double y = PLANETS[i].a * sin(L * DEG) + R * sin(Ls * DEG);
  double lam = atan2(y, x) * 57.2957795;
  if (lam < 0) lam += 360.0;
  planetRaDec(lam, eps * DEG, ra, dec);
  return true;
}
void phaseWhen(float nowPhase, float target, char *out, int n) {
  float diff = fabsf(nowPhase - target);
  if (target < 0.02f && nowPhase > 0.97f) diff = 1.0f - nowPhase;
  if (diff < 0.03f) {
    strlcpy(out, "tonight", n);
    return;
  }
  float ahead = target - nowPhase;
  if (ahead < 0) ahead += 1.0f;
  int days = (int)(ahead * 29.530588f + 0.5f);
  if (days < 1) days = 1;
  time_t t = time(nullptr) + (time_t)days * 86400;
  struct tm tm;
  localtime_r(&t, &tm);
  snprintf(out, n, "%s %s %d", WDAY[tm.tm_wday], MON[tm.tm_mon], tm.tm_mday);
}

bool satPosAt(const IssTle *tle, time_t t, float *lat, float *lon, float *altKm) {
  if (!tle || !tle->ok) return false;
  double dt = unixToJd(t) - tle->epochJd;
  double n = tle->n * 2.0 * M_PI;
  double M = tle->m0 * DEG + n * dt;
  double e = tle->e;
  double E = M;
  for (int i = 0; i < 7; i++) E = M + e * sin(E);
  double nu = 2 * atan2(sqrt(1 + e) * sin(E / 2), sqrt(1 - e) * cos(E / 2));
  double ns = tle->n * 2.0 * M_PI / 86400.0;
  double a = pow(398600.4418 / (ns * ns), 1.0 / 3.0);
  double r = a * (1 - e * cos(E));
  double inc = tle->inc * DEG;
  double cosi = cos(inc);
  double raanDot = -1.5 * ns * 1.08263e-3 * pow(6378.137 / a, 2) * cosi / pow(1 - e * e, 2);
  double raan = tle->raan * DEG + raanDot * dt * 86400.0;
  double arg = tle->argp * DEG + nu;
  double x = r * cos(arg), y = r * sin(arg);
  double ecix = x * cos(raan) - y * cosi * sin(raan);
  double eciy = x * sin(raan) + y * cosi * cos(raan);
  double eciz = y * sin(inc);
  double gmst = 280.46061837 + 360.98564736629 * (unixToJd(t) - 2451545.0);
  wrap360(gmst);
  double g = gmst * DEG;
  double ex = ecix * cos(g) + eciy * sin(g);
  double ey = -ecix * sin(g) + eciy * cos(g);
  *lon = (float)(atan2(ey, ex) * 57.2957795);
  *lat = (float)(atan2(eciz, sqrt(ex * ex + ey * ey)) * 57.2957795);
  *altKm = (float)(sqrt(ex * ex + ey * ey + eciz * eciz) - 6371.0);
  return true;
}
void llToAzEl(float lat, float lon, float altKm, int *az, int *el) {
  float dLat = (lat - placeLat) * 0.0174533f;
  float dLon = (lon - placeLon) * 0.0174533f;
  float rlat = placeLat * 0.0174533f;
  float a = sinf(dLat / 2) * sinf(dLat / 2) +
            cosf(rlat) * cosf(lat * 0.0174533f) * sinf(dLon / 2) * sinf(dLon / 2);
  float c = 2 * atan2f(sqrtf(a), sqrtf(1 - a));
  float elev = atan2f((6371.0f + altKm) * cosf(c) - 6371.0f, (6371.0f + altKm) * sinf(c));
  *el = (int)(elev * 57.2958f + (elev >= 0 ? 0.5f : -0.5f));
  float yy = sinf(dLon) * cosf(lat * 0.0174533f);
  float xx = cosf(rlat) * sinf(lat * 0.0174533f) - sinf(rlat) * cosf(lat * 0.0174533f) * cosf(dLon);
  int brg = (int)(atan2f(yy, xx) * 57.2958f);
  if (brg < 0) brg += 360;
  *az = brg;
}

bool tleToJd(const char *l1, double *jd) {
  if (!l1 || (int)strlen(l1) < 32) return false;
  int yy = (l1[18] - '0') * 10 + (l1[19] - '0');
  if (yy < 0 || yy > 99) return false;
  int year = (yy >= 57) ? 1900 + yy : 2000 + yy;
  double day = atof(l1 + 20);
  if (day < 1 || day >= 367) return false;
  int doy = (int)day;
  double frac = day - doy;
  static const int mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
  int mo = 1;
  int d = doy;
  for (int i = 0; i < 12 && d > mdays[i] + (i == 1 && leap); i++) {
    d -= mdays[i] + (i == 1 && leap);
    mo++;
  }
  int sec = (int)(frac * 86400.0 + 0.5);
  if (sec >= 86400) sec = 86399;
  int y = year, m = mo, h = sec / 3600, mi = (sec / 60) % 60, se = sec % 60;
  if (m <= 2) {
    y--;
    m += 12;
  }
  int A = y / 100;
  int B = 2 - A + A / 4;
  *jd = floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5 +
        (h + mi / 60.0 + se / 3600.0) / 24.0;
  return true;
}
bool parseTle(const char *l1, const char *l2, IssTle *t) {
  if (!l1 || !l2 || l1[0] != '1' || l2[0] != '2') return false;
  if ((int)strlen(l2) < 63) return false;
  double jd = 0;
  if (!tleToJd(l1, &jd)) return false;
  char b[16];
  memcpy(b, l2 + 8, 8);
  b[8] = 0;
  double inc = atof(b);
  memcpy(b, l2 + 17, 8);
  b[8] = 0;
  double raan = atof(b);
  memcpy(b, l2 + 26, 7);
  b[7] = 0;
  double ecc = atof(b) * 1.0e-7;
  memcpy(b, l2 + 34, 8);
  b[8] = 0;
  double argp = atof(b);
  memcpy(b, l2 + 43, 8);
  b[8] = 0;
  double m0 = atof(b);
  memcpy(b, l2 + 52, 11);
  b[11] = 0;
  double nn = atof(b);
  if (nn < 1.0) return false;
  t->epochJd = jd;
  t->n = nn;
  t->e = ecc;
  t->inc = inc;
  t->raan = raan;
  t->argp = argp;
  t->m0 = m0;
  t->ok = true;
  return true;
}
void seedTles() {
  int n = SAT_N < 4 ? SAT_N : 4;
  for (int i = 0; i < n; i++) parseTle(SATS[i].l1, SATS[i].l2, &satLive[i].tle);
}

void buildSatPass(int idx, time_t now) {
  SatLive &s = satLive[idx];
  s.nPass = 0;
  s.when[0] = 0;
  s.vis = false;
  if (!s.tle.ok || !placeSet) return;
  float la, lo, al;
  if (satPosAt(&s.tle, now, &la, &lo, &al)) {
    s.lat = la;
    s.lon = lo;
    s.alt = al;
    llToAzEl(la, lo, al, &s.az, &s.el);
    float sun = 0;
    sunAlt(now, &sun);
    s.vis = s.el >= 10 && sun < -4.0f;
    s.ok = true;
  }
  bool needGap = true;
  for (int step = 2; step < 48 * 30 && s.nPass < 2; step++) {
    time_t t = now + (time_t)step * 120;
    if (!satPosAt(&s.tle, t, &la, &lo, &al)) break;
    int az, el;
    llToAzEl(la, lo, al, &az, &el);
    float sun = 0;
    sunAlt(t, &sun);
    bool vis = el >= 10 && sun < -4.0f;
    if (!vis) needGap = false;
    if (vis && !needGap) {
      struct tm tm;
      localtime_r(&t, &tm);
      char hm[16];
      fmtHm(tm.tm_hour, tm.tm_min, hm, sizeof(hm));
      snprintf(s.pass[s.nPass], 64, "%s %s  LOOK %s  %d deg", WDAY[tm.tm_wday], hm, lookWord(az), el);
      if (!s.nPass) snprintf(s.when, sizeof(s.when), "%s %s  LOOK %s", WDAY[tm.tm_wday], hm, lookWord(az));
      s.nPass++;
      needGap = true;
      step += 15;
    }
  }
}
void buildSatTrack(int idx, time_t now) {
  SatLive &s = satLive[idx];
  s.nTrk = 0;
  if (!s.tle.ok || s.tle.n < 1) return;
  double period = 86400.0 / s.tle.n;
  for (int i = 0; i < 72; i++) {
    float la, lo, al;
    if (!satPosAt(&s.tle, now + (time_t)(period * i / 72.0), &la, &lo, &al)) break;
    s.trkLat[s.nTrk] = la;
    s.trkLon[s.nTrk] = lo;
    s.nTrk++;
  }
}
bool ensureSats(bool forceHeavy) {
  if (satBusy || !placeSet || !liveClock(nullptr)) return false;
  uint32_t ms = millis();
  bool move = forceHeavy || !satMoveAt || ms - satMoveAt > 60000UL;
  bool heavy = forceHeavy || !satHeavyAt || ms - satHeavyAt > 600000UL;
  if (!move && !heavy) return false;
  time_t now;
  if (!liveClock(&now)) return false;
  int n = SAT_N < 4 ? SAT_N : 4;
  for (int i = 0; i < n; i++) {
    if (!satLive[i].tle.ok) continue;
    if (heavy) {
      buildSatPass(i, now);
      buildSatTrack(i, now);
    } else {
      float la, lo, al;
      if (satPosAt(&satLive[i].tle, now, &la, &lo, &al)) {
        satLive[i].lat = la;
        satLive[i].lon = lo;
        satLive[i].alt = al;
        llToAzEl(la, lo, al, &satLive[i].az, &satLive[i].el);
        float sun = 0;
        sunAlt(now, &sun);
        satLive[i].vis = satLive[i].el >= 10 && sun < -4.0f;
        satLive[i].ok = true;
      }
    }
  }
  satMoveAt = ms;
  if (heavy) satHeavyAt = ms;
  return true;
}

uint16_t dim565(uint16_t c) {
  uint16_t r = (c >> 11) & 31;
  uint16_t g = (c >> 5) & 63;
  uint16_t b = c & 31;
  return (uint16_t)((r / 2) << 11 | (g / 2) << 5 | (b / 2));
}
bool jpgToRam(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
  if (!worldPx || !bitmap) return 1;
  for (int row = 0; row < h; row++) {
    int yy = y + row;
    if (yy < 0 || yy >= 119) continue;
    for (int col = 0; col < w; col++) {
      int xx = x + col;
      if (xx < 0 || xx >= 320) continue;
      worldPx[yy * 320 + xx] = bitmap[row * w + col];
    }
  }
  return 1;
}
bool buildWorld() {
  worldPx = (uint16_t *)ps_malloc((size_t)320 * 119 * 2);
  if (!worldPx) return false;
  for (int i = 0; i < 320 * 119; i++) worldPx[i] = 1;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(jpgToRam);
  TJpgDec.setSwapBytes(false);
  bool ok = TJpgDec.drawJpg(0, 0, ISS_WORLD_JPG, ISS_WORLD_JPG_LEN) == 0;
  int filled = 0;
  for (int i = 0; i < 320 * 119; i += 17)
    if (worldPx[i] != 1) filled++;
  if (!ok || filled < 80) {
    for (int i = 0; i < 320 * 119; i++) worldPx[i] = 1;
    TJpgDec.setSwapBytes(true);
    ok = TJpgDec.drawJpg(0, 0, ISS_WORLD_JPG, ISS_WORLD_JPG_LEN) == 0;
    TJpgDec.setSwapBytes(false);
  }
  worldReady = ok;
  return worldReady;
}
int mapX(float lon) { return (int)((lon + 180.0f) * (PW / 360.0f)); }
int mapYAt(float lat, int y0, int h) { return y0 + (int)((72.0f - lat) * ((float)h / 134.0f)); }
int mapY(float lat) { return mapYAt(lat, MAP_Y0, MAP_H); }

void drawWorldBand(int y0, int h) {
  if (!worldReady || !worldPx || h < 8) return;
  uint16_t row[800];
  for (int j = 0; j < h; j++) {
    int sy = j * 119 / h;
    if (sy > 118) sy = 118;
    float lat = 72.0f - (sy * 134.0f / 119.0f);
    for (int i = 0; i < PW; i++) {
      int sx = i * 320 / PW;
      if (sx > 319) sx = 319;
      uint16_t c = worldPx[sy * 320 + sx];
      if (sunOk && !dayLit(lat, (i * 360.0f / (float)PW) - 180.0f)) c = dim565(c);
      row[i] = c;
    }
    gfx->draw16bitRGBBitmap(0, y0 + j, row, PW, 1);
    if ((j & 15) == 15) delay(0);
  }
}
void drawWorldPhoto() { drawWorldBand(MAP_Y0, MAP_H); }

float gmlatOf(float lat, float lon) {
  float plat = 80.7f * (float)DEG;
  float plon = -72.7f * (float)DEG;
  float la = lat * (float)DEG;
  float lo = lon * (float)DEG;
  float s = sinf(la) * sinf(plat) + cosf(la) * cosf(plat) * cosf(lo - plon);
  if (s > 1) s = 1;
  if (s < -1) s = -1;
  return asinf(s) / (float)DEG;
}
float auroraEdge(int kp) {
  static const float edge[] = {66.5f, 64.5f, 62.4f, 60.4f, 58.3f, 56.3f, 54.2f, 52.2f, 50.1f, 48.1f};
  if (kp < 0) kp = 0;
  if (kp > 9) kp = 9;
  return edge[kp];
}
void destDeg(float lat1, float lon1, float dist, float bear, float *lat, float *lon) {
  float d = dist * (float)DEG;
  float br = bear * (float)DEG;
  float p1 = lat1 * (float)DEG;
  float l1 = lon1 * (float)DEG;
  float la = asinf(sinf(p1) * cosf(d) + cosf(p1) * sinf(d) * cosf(br));
  float lo = l1 + atan2f(sinf(br) * sinf(d) * cosf(p1), cosf(d) - sinf(p1) * sinf(la));
  *lat = la / (float)DEG;
  *lon = lo / (float)DEG;
  if (*lon > 180) *lon -= 360;
  if (*lon < -180) *lon += 360;
}
void drawAuroraOval(int y0, int h, float edge) {
  float ang = 90.0f - edge;
  if (ang < 8) ang = 8;
  float poles[2][2] = {{80.7f, -72.7f}, {-80.7f, 107.3f}};
  for (int p = 0; p < 2; p++) {
    for (int band = 0; band < 2; band++) {
      float use = ang - band * 7.0f;
      if (use < 6) continue;
      int px = -999, py = -999;
      for (int b = 0; b <= 360; b += 4) {
        float la, lo;
        destDeg(poles[p][0], poles[p][1], use, (float)b, &la, &lo);
        if (la > 72 || la < -62) {
          px = -999;
          continue;
        }
        int x = mapX(lo);
        int y = mapYAt(la, y0, h);
        if (px > -900 && abs(x - px) < 180) gfx->drawLine(px, py, x, y, RGB565(255, 210, 70));
        px = x;
        py = y;
      }
    }
  }
}

void drawHome();
void drawSky();
void drawPlanets();
void drawMoon();
void drawMore();
void drawSats();
void drawRocks();
void drawComets();
void drawScopes();
void drawDark();
void drawClouds();
void drawNeo();
void drawSolar();
void drawShowers();
void drawLaunch();
void drawWifi();
void drawPlace();
void drawSet();
void drawAbout();
void askOta(int kind);
void drawHomeLower();

void drawClock() {
  gfx->fillRect(80, 48, 640, 52, COL_BG);
  gfx->setTextColor(COL_TXT, COL_BG);
  if (!placeSet || !placeTz[0]) {
    gfx->setTextSize(3);
    centre("set a place", CX, 58);
    return;
  }
  if (!clockReady()) {
    gfx->setTextSize(3);
    centre("waiting for the clock", CX, 58);
    return;
  }
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char line[24];
  fmtHm(t.tm_hour, t.tm_min, line, sizeof(line));
  gfx->setTextSize(6);
  centre(line, CX, 50);
}
void drawDateLine() {
  gfx->fillRect(80, 104, 640, 26, COL_BG);
  gfx->setTextSize(2);
  if (!placeSet || !placeTz[0] || !clockReady()) {
    gfx->setTextColor(COL_MID, COL_BG);
    centre("date follows the place", CX, 108);
    return;
  }
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  char line[40];
  snprintf(line, sizeof(line), "%s  %s %d", WDAY[t.tm_wday], MON[t.tm_mon], t.tm_mday);
  gfx->setTextColor(COL_MID, COL_BG);
  centre(line, CX, 108);
}
void drawHomeLower() {
  gfx->fillRect(0, 284, PW, 126, COL_BG);
  gfx->setTextSize(2);
  char line[80];
  gfx->setTextColor(COL_TXT, COL_BG);
  if (auroraKp < 0) strlcpy(line, "aurora  --", sizeof(line));
  else {
    const char *word = "quiet";
    if (auroraKp >= 7) word = "severe";
    else if (auroraKp >= 5) word = "active";
    else if (auroraKp >= 4) word = "unsettled";
    snprintf(line, sizeof(line), "aurora  Kp %d  %s", auroraKp, word);
    if (windSpeed > 0) {
      char extra[24];
      snprintf(extra, sizeof(extra), "   wind %d", windSpeed);
      strlcat(line, extra, sizeof(line));
    }
  }
  gfx->setCursor(16, 292);
  gfx->print(line);
  if (satLive[0].vis)
    snprintf(line, sizeof(line), "ISS is up  LOOK %s  %d miles", lookWord(satLive[0].az),
             (int)(satLive[0].alt * 0.621371f));
  else if (satLive[0].when[0])
    snprintf(line, sizeof(line), "ISS  %s", satLive[0].when);
  else
    snprintf(line, sizeof(line), "ISS  %s", wifiOk ? "waiting for a pass" : "join wifi");
  gfx->setTextColor(satLive[0].vis ? COL_HOT : COL_MID, COL_BG);
  gfx->setCursor(16, 324);
  gfx->print(line);
  showerLine(line, sizeof(line));
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setCursor(16, 364);
  gfx->print(line);
}
void drawHome() {
  page = PG_HOME;
  netHold = false;
  gfx->fillScreen(COL_BG);
  chip(648, 8, 140, 36, "WIFI", false);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  gfx->setCursor(16, 16);
  gfx->print(placeSet && placeName[0] ? placeName : "tap to set place");
  drawClock();
  drawDateLine();
  gfx->fillRoundRect(12, 140, 380, 136, 8, COL_DIM);
  gfx->fillRoundRect(408, 140, 380, 136, 8, COL_DIM);
  float phase = 0;
  int pct = 0;
  const char *mn = moonName(&phase, &pct);
  if (mn) drawMoonPhase(78, 208, 40, phase);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_DIM);
  gfx->setCursor(140, 158);
  gfx->print(mn ? mn : "moon");
  char line[64];
  if (mn) {
    snprintf(line, sizeof(line), "%d%% lit", pct);
    gfx->setTextColor(COL_HOT, COL_DIM);
    gfx->setCursor(140, 186);
    gfx->print(line);
  }
  fillNow();
  gfx->setTextColor(COL_MID, COL_DIM);
  gfx->setCursor(140, 214);
  if (moonNow.ok) {
    gfx->setTextSize(1);
    snprintf(line, sizeof(line), "distance from Earth  %.0f miles", moonNow.distAu * 92955807.0);
    gfx->print(line);
  } else
    gfx->print("tap for the moon");
  if (!lastSun || millis() - lastSun > 1800000UL) computeSunTimes();
  gfx->setTextColor(COL_TXT, COL_DIM);
  gfx->setCursor(424, 156);
  if (wxOk) {
    snprintf(line, sizeof(line), "%d %s", showT(wxTemp), tempUnit());
    gfx->setTextSize(3);
    gfx->print(line);
    gfx->setTextSize(2);
    gfx->setCursor(424, 196);
    gfx->print(codeWord(wxCode, wxDay));
  } else {
    gfx->setTextSize(2);
    gfx->print(wifiOk ? "sky waiting" : "no wifi");
  }
  gfx->setTextColor(COL_MID, COL_DIM);
  gfx->setTextSize(1);
  gfx->setCursor(424, 224);
  snprintf(line, sizeof(line), "sun rise %s   sun set %s", sunUp[0] ? sunUp : "--", sunDown[0] ? sunDown : "--");
  gfx->print(line);
  drawHomeLower();
  drawFoot();
  shownKey = clockKey();
}

int labN = 0;
int labX[40], labY[40];
void labelReset() { labN = 0; }
bool labelOk(int x, int y) {
  if (y < 50 || y > 392 || x < 8 || x > 790) return false;
  for (int i = 0; i < labN; i++) {
    int dx = x - labX[i], dy = y - labY[i];
    if (dx * dx + dy * dy < 42 * 42) return false;
  }
  if (labN < 40) {
    labX[labN] = x;
    labY[labN] = y;
    labN++;
  }
  return true;
}
void skyLabel(int x, int y, const char *s, uint16_t c) {
  if (!labelOk(x, y - 14)) return;
  gfx->setTextSize(1);
  gfx->setTextColor(c, RGB565(8, 14, 28));
  gfx->setCursor(x + 6, y - 14);
  gfx->print(s);
}

void drawSkyDetail() {
  gfx->fillRect(0, 400, PW, 80, COL_BG);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  char a[96], b[96];
  strlcpy(a, "tap a star, planet, or the moon", sizeof(a));
  b[0] = 0;
  time_t nowt = 0;
  bool timed = liveClock(&nowt) && placeSet;
  if (pickKind == 1 && pickId >= 0 && pickId < NAME_N) {
    char ly[32] = "";
    fmtLy(STAR_NAME[pickId].ly, ly, sizeof(ly));
    int mag10 = 99, az = 0, el = -99;
    double ra = 0, dec = 0;
    bool got = false;
    if (timed) {
      for (int s = 0; s < STAR_N; s++) {
        if (STARS[s].name != (int8_t)pickId) continue;
        mag10 = STARS[s].mag;
        ra = (STARS[s].ra / 100.0) * 15.0 * DEG;
        dec = (STARS[s].dec / 10.0) * DEG;
        azelFromRaDec(ra, dec, nowt, &az, &el);
        got = true;
        break;
      }
    }
    if (mag10 < 90) {
      char mag[12];
      fmtMag10(mag10, mag, sizeof(mag));
      snprintf(a, sizeof(a), "%s  mag %s  LOOK %s  %d deg", STAR_NAME[pickId].n, mag, lookWord(az), el);
    } else
      snprintf(a, sizeof(a), "%s", STAR_NAME[pickId].n);
    if (got) {
      char hrs[48], wide[40] = "";
      skyHours(ra, dec, nowt, hrs, sizeof(hrs));
      for (int w = 0; w < WIDE_N; w++) {
        if (strcmp(STAR_WIDE[w].n, STAR_NAME[pickId].n)) continue;
        int s10 = STAR_WIDE[w].suns10;
        if (s10 >= 100)
          snprintf(wide, sizeof(wide), "about %d suns across", s10 / 10);
        else if (s10 >= 10)
          snprintf(wide, sizeof(wide), "about %d.%d suns across", s10 / 10, s10 % 10);
        break;
      }
      if (wide[0])
        snprintf(b, sizeof(b), "%s   %s   %s", ly, wide, hrs);
      else
        snprintf(b, sizeof(b), "%s   %s", ly, hrs);
    } else
      strlcpy(b, ly, sizeof(b));
  } else if (pickKind == 2 && pickId >= 0 && pickId < PLANET_N && planetNow[pickId].ok) {
    snprintf(a, sizeof(a), "%s  LOOK %s  %d deg", PLANETS[pickId].name, lookWord(planetNow[pickId].az),
             planetNow[pickId].el);
    fmtRange(planetNow[pickId].distAu, b, sizeof(b));
    if (timed) {
      double ra, dec;
      char hrs[48];
      if (bodySky(pickId + 1, nowt, &ra, &dec)) {
        skyHours(ra, dec, nowt, hrs, sizeof(hrs));
        strlcat(b, "   ", sizeof(b));
        strlcat(b, hrs, sizeof(b));
      }
    }
  } else if (pickKind == 3 && moonNow.ok) {
    float phase = 0;
    int pct = 0;
    const char *mn = moonName(&phase, &pct);
    snprintf(a, sizeof(a), "Moon  %s  %d%%  LOOK %s  %d deg", mn ? mn : "moon", pct, lookWord(moonNow.az),
             moonNow.el);
    fmtRange(moonNow.distAu, b, sizeof(b));
    if (timed) {
      double ra, dec;
      char hrs[48];
      if (bodySky(-1, nowt, &ra, &dec)) {
        skyHours(ra, dec, nowt, hrs, sizeof(hrs));
        strlcat(b, "   ", sizeof(b));
        strlcat(b, hrs, sizeof(b));
      }
    }
  } else if (pickKind == 4 && sunNow.ok) {
    snprintf(a, sizeof(a), "Sun  LOOK %s  %d deg", lookWord(sunNow.az), sunNow.el);
    fmtRange(sunNow.distAu, b, sizeof(b));
    if (timed) {
      double ra, dec;
      char hrs[48];
      if (bodySky(0, nowt, &ra, &dec)) {
        skyHours(ra, dec, nowt, hrs, sizeof(hrs));
        strlcat(b, "   ", sizeof(b));
        strlcat(b, hrs, sizeof(b));
      }
    }
  } else if (pickKind == 5 && pickId >= 0 && pickId < ROCK_N && rockNow[pickId].ok) {
    snprintf(a, sizeof(a), "%s  %s  LOOK %s  %d deg", ROCKS[pickId].name, ROCKS[pickId].kind,
             lookWord(rockNow[pickId].az), rockNow[pickId].el);
    fmtRange(rockNow[pickId].distAu, b, sizeof(b));
  } else if (pickKind == 6 && pickId >= 0 && pickId < DEEP_N) {
    snprintf(a, sizeof(a), "%s  %s", DEEP[pickId].name, DEEP[pickId].note);
    fmtLy(DEEP[pickId].ly, b, sizeof(b));
    if (timed) {
      double ra = DEEP[pickId].raH * 15.0 * DEG;
      double dec = DEEP[pickId].dec * DEG;
      int az, el;
      azelFromRaDec(ra, dec, nowt, &az, &el);
      char hrs[48];
      skyHours(ra, dec, nowt, hrs, sizeof(hrs));
      snprintf(a, sizeof(a), "%s  LOOK %s  %d deg", DEEP[pickId].name, lookWord(az), el);
      snprintf(b, sizeof(b), "%s   %s   %s", DEEP[pickId].note, DEEP[pickId].count, hrs);
    }
  }
  gfx->setCursor(16, 412);
  gfx->print(a);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(16, 444);
  gfx->print(b[0] ? b : "north is up. the rim is the horizon");
}

void drawSky() {
  page = PG_SKY;
  netHold = true;
  netPend = NET_NONE;
  if (netBusy) {
    skyDirty = true;
    gfx->fillScreen(COL_BG);
    drawBack("STAR MAP");
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(2);
    centre("one moment", CX, 220);
    return;
  }
  fillNow();
  time_t now = 0;
  bool timed = liveClock(&now);
  gfx->fillScreen(COL_BG);
  drawBack("STAR MAP");
  chip(640, 8, 148, 36, "TONIGHT", false);
  gfx->fillCircle(SKY_CX, SKY_CY, SKY_R, RGB565(8, 14, 28));
  gfx->drawCircle(SKY_CX, SKY_CY, SKY_R, COL_MID);
  int nameX[NAME_CAP], nameY[NAME_CAP], nameElv[NAME_CAP], nameMag[NAME_CAP];
  uint8_t nameOn[NAME_CAP];
  for (int i = 0; i < NAME_CAP; i++) {
    nameOn[i] = 0;
    nameMag[i] = 99;
  }
  if (timed && placeSet) {
    for (int s = 0; s < STAR_N; s++) {
      int nm = STARS[s].name;
      if (nm < 0 || nm >= NAME_N || nm >= NAME_CAP) continue;
      double ra = (STARS[s].ra / 100.0) * 15.0 * DEG;
      double dec = (STARS[s].dec / 10.0) * DEG;
      int az, el, x, y;
      azelFromRaDec(ra, dec, now, &az, &el);
      if (!skyXY(az, el, &x, &y)) continue;
      nameX[nm] = x;
      nameY[nm] = y;
      nameElv[nm] = el;
      nameOn[nm] = 1;
      if (STARS[s].mag < nameMag[nm]) nameMag[nm] = STARS[s].mag;
      if ((s & 63) == 0) delay(0);
    }
    for (int i = 0; i + 1 < (int)sizeof(STAR_LINE); i += 2) {
      int a = STAR_LINE[i], b = STAR_LINE[i + 1];
      if (a == 255 || b == 255) break;
      if (a < 0 || b < 0 || a >= NAME_N || b >= NAME_N || a >= NAME_CAP || b >= NAME_CAP) continue;
      if (!nameOn[a] || !nameOn[b]) continue;
      int dx = nameX[a] - nameX[b], dy = nameY[a] - nameY[b];
      if (dx * dx + dy * dy < 300 * 300)
        gfx->drawLine(nameX[a], nameY[a], nameX[b], nameY[b], RGB565(70, 90, 120));
    }
    for (int s = 0; s < STAR_N; s++) {
      int nm = STARS[s].name;
      if (nm < 0 || nm >= NAME_N) continue;
      double ra = (STARS[s].ra / 100.0) * 15.0 * DEG;
      double dec = (STARS[s].dec / 10.0) * DEG;
      int az, el, x, y;
      azelFromRaDec(ra, dec, now, &az, &el);
      if (!skyXY(az, el, &x, &y)) continue;
      int r = 3;
      if (STARS[s].mag <= 0) r = 4;
      else if (STARS[s].mag > 20) r = 2;
      gfx->fillCircle(x, y, r, STARS[s].mag <= 20 ? RGB565(255, 255, 255) : RGB565(186, 198, 214));
      if ((s & 63) == 0) delay(0);
    }
  }
  labelReset();
  gfx->setTextSize(2);
  gfx->setTextColor(RGB565(200, 210, 220), RGB565(8, 14, 28));
  const char *comp[4] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; i++) {
    int x, y;
    if (!skyXY(i * 90, 8, &x, &y)) continue;
    centre(comp[i], x, y - 8);
  }
  if (timed && placeSet) {
    for (int i = 0; i < DEEP_N; i++) {
      double ra = DEEP[i].raH * 15.0 * DEG;
      double dec = DEEP[i].dec * DEG;
      int az, el;
      azelFromRaDec(ra, dec, now, &az, &el);
      int x, y;
      if (!skyXY(az, el, &x, &y)) continue;
      gfx->drawCircle(x, y, 5, RGB565(120, 190, 220));
      skyLabel(x, y, DEEP[i].name, RGB565(120, 190, 220));
    }
    for (int i = 0; i < ROCK_N && i < 8; i++) {
      if (!rockNow[i].ok) continue;
      int x, y;
      if (!skyXY(rockNow[i].az, rockNow[i].el, &x, &y)) continue;
      gfx->fillRect(x - 2, y - 2, 5, 5, RGB565(240, 170, 60));
      if (rockNow[i].el >= 0) skyLabel(x, y, ROCKS[i].name, RGB565(240, 170, 60));
    }
    for (int i = 0; i < PLANET_N && i < 7; i++) {
      if (!planetNow[i].ok || planetNow[i].glare) continue;
      int x, y;
      if (!skyXY(planetNow[i].az, planetNow[i].el, &x, &y)) continue;
      gfx->fillCircle(x, y, 4, RGB565(255, 196, 60));
      skyLabel(x, y, PLANETS[i].name, RGB565(255, 196, 60));
    }
    if (moonNow.ok) {
      int x, y;
      if (skyXY(moonNow.az, moonNow.el, &x, &y)) {
        float phase = 0;
        moonName(&phase, nullptr);
        drawMoonPhase(x, y, 8, phase);
        skyLabel(x, y, "Moon", RGB565(236, 240, 244));
      }
    }
    static const struct {
      const char *n;
      uint8_t at;
    } CON_NAME[] = {
        {"Orion", 30},     {"Ursa Major", 5}, {"Cassiopeia", 44}, {"Cygnus", 49},
        {"Leo", 61},       {"Gemini", 23},    {"Pegasus", 41},    {"Scorpius", 22},
        {"Taurus", 20},    {"Crux", 52},      {"Auriga", 15},     {"Perseus", 64},
        {"Andromeda", 38}, {"Canis Major", 11}, {"Draco", 78},
    };
    for (int i = 0; i < (int)(sizeof(CON_NAME) / sizeof(CON_NAME[0])); i++) {
      int at = CON_NAME[i].at;
      if (at < 0 || at >= NAME_N || !nameOn[at]) continue;
      skyLabel(nameX[at], nameY[at] + 16, CON_NAME[i].n, RGB565(120, 160, 200));
    }
    for (int i = 0; i < NAME_N && i < NAME_CAP; i++) {
      if (!nameOn[i] || nameElv[i] < 8 || nameMag[i] > 25) continue;
      skyLabel(nameX[i], nameY[i], STAR_NAME[i].n, RGB565(220, 226, 232));
    }
  } else {
    gfx->setTextColor(RGB565(200, 210, 220), RGB565(8, 14, 28));
    gfx->setTextSize(2);
    centre(placeSet ? "waiting for the clock" : "set a place", CX, SKY_CY - 8);
  }
  drawSkyDetail();
  lastSkyDraw = millis();
  skyDirty = false;
}

void skyPick(int x, int y) {
  if (!liveClock(nullptr) || !placeSet) return;
  fillNow();
  time_t now = time(nullptr);
  int bestD = 28 * 28;
  int kind = 0, id = -1;
  if (moonNow.ok) {
    int sx, sy;
    if (skyXY(moonNow.az, moonNow.el, &sx, &sy)) {
      int d = (x - sx) * (x - sx) + (y - sy) * (y - sy);
      if (d < bestD) {
        bestD = d;
        kind = 3;
        id = 0;
      }
    }
  }
  for (int i = 0; i < PLANET_N; i++) {
    if (!planetNow[i].ok) continue;
    int sx, sy;
    if (!skyXY(planetNow[i].az, planetNow[i].el, &sx, &sy)) continue;
    int d = (x - sx) * (x - sx) + (y - sy) * (y - sy);
    if (d < bestD) {
      bestD = d;
      kind = 2;
      id = i;
    }
  }
  for (int i = 0; i < ROCK_N; i++) {
    if (!rockNow[i].ok) continue;
    int sx, sy;
    if (!skyXY(rockNow[i].az, rockNow[i].el, &sx, &sy)) continue;
    int d = (x - sx) * (x - sx) + (y - sy) * (y - sy);
    if (d < bestD) {
      bestD = d;
      kind = 5;
      id = i;
    }
  }
  for (int i = 0; i < DEEP_N; i++) {
    double ra = DEEP[i].raH * 15.0 * DEG;
    double dec = DEEP[i].dec * DEG;
    int az, el, sx, sy;
    azelFromRaDec(ra, dec, now, &az, &el);
    if (!skyXY(az, el, &sx, &sy)) continue;
    int d = (x - sx) * (x - sx) + (y - sy) * (y - sy);
    if (d < bestD) {
      bestD = d;
      kind = 6;
      id = i;
    }
  }
  int starBest = 26 * 26;
  int starId = -1;
  for (int s = 0; s < STAR_N; s++) {
    if (STARS[s].name < 0 || STARS[s].mag > 30) continue;
    double ra = (STARS[s].ra / 100.0) * 15.0 * DEG;
    double dec = (STARS[s].dec / 10.0) * DEG;
    int az, el, sx, sy;
    azelFromRaDec(ra, dec, now, &az, &el);
    if (!skyXY(az, el, &sx, &sy)) continue;
    int d = (x - sx) * (x - sx) + (y - sy) * (y - sy);
    if (d < starBest) {
      starBest = d;
      starId = STARS[s].name;
    }
  }
  if (starId >= 0 && starBest < bestD) {
    kind = 1;
    id = starId;
  }
  int dx = x - SKY_CX;
  int dy = y - SKY_CY;
  if (dx * dx + dy * dy > SKY_R * SKY_R) return;
  pickKind = kind;
  pickId = id;
  drawSkyDetail();
}

static int fitX, fitY, fitW, fitH, fitSW, fitSH;
static int sdSkipSy, sdSkipDy;

bool sdPlot(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *data) {
  if (fitSW < 1 || fitSH < 1 || fitW < 1 || fitH < 1 || !data) return true;
  uint16_t buf[232];
  for (uint16_t row = 0; row < h; row++) {
    int sy = y + (int)row;
    int dy0 = fitY + (int)((int32_t)sy * fitH / fitSH);
    int dy1 = fitY + (int)((int32_t)(sy + 1) * fitH / fitSH);
    if (dy1 <= dy0) dy1 = dy0 + 1;
    if (dy0 < fitY) dy0 = fitY;
    if (dy1 > fitY + fitH) dy1 = fitY + fitH;
    if (dy0 >= dy1) continue;
    if (sy != sdSkipSy && dy0 == sdSkipDy && (dy1 - dy0) == 1) continue;
    sdSkipSy = sy;
    sdSkipDy = dy0;
    int lastDx = -1;
    int runX = -1;
    int n = 0;
    for (uint16_t col = 0; col < w; col++) {
      int sx = x + (int)col;
      int dx0 = fitX + (int)((int32_t)sx * fitW / fitSW);
      int dx1 = fitX + (int)((int32_t)(sx + 1) * fitW / fitSW);
      if (dx1 <= dx0) {
        if (dx0 == lastDx) continue;
        dx1 = dx0 + 1;
      }
      if (dx0 < fitX) dx0 = fitX;
      if (dx1 > fitX + fitW) dx1 = fitX + fitW;
      if (dx0 >= dx1) continue;
      lastDx = dx1 - 1;
      uint16_t c = data[(int)row * w + col];
      if (runX >= 0 && (dx0 != runX + n || n + (dx1 - dx0) > 232)) {
        for (int yy = dy0; yy < dy1; yy++) gfx->draw16bitRGBBitmap(runX, yy, buf, n, 1);
        n = 0;
        runX = -1;
      }
      if (runX < 0) runX = dx0;
      while (dx0 < dx1 && n < 232) buf[n++] = c, dx0++;
    }
    if (n > 0)
      for (int yy = dy0; yy < dy1; yy++) gfx->draw16bitRGBBitmap(runX, yy, buf, n, 1);
    if ((sy & 7) == 0) delay(0);
  }
  return true;
}

bool picPath(const char *name, const char **dir, const char **file) {
  if (!name || !name[0]) return false;
  for (int i = 0; i < PIC_N; i++) {
    if (!strcmp(PICS[i].name, name)) {
      *dir = PICS[i].dir;
      *file = PICS[i].file;
      return true;
    }
  }
  for (int i = 0; i < NAME_N; i++) {
    if (!strcmp(STAR_NAME[i].n, name)) {
      *dir = "stars";
      *file = STAR_NAME[i].n;
      return true;
    }
  }
  for (int i = 0; i < (int)(sizeof(ROCKET_FACT) / sizeof(ROCKET_FACT[0])); i++) {
    if (strstr(name, ROCKET_FACT[i].key)) {
      *dir = "rockets";
      *file = ROCKET_FACT[i].key;
      return true;
    }
  }
  for (int i = 0; i < PIC_N; i++) {
    if (PICS[i].src != PIC_LIB || strcmp(PICS[i].kind, "rocket")) continue;
    if (strstr(name, PICS[i].name) || (PICS[i].also[0] && strstr(name, PICS[i].also))) {
      *dir = PICS[i].dir;
      *file = PICS[i].file;
      return true;
    }
  }
  return false;
}

void drawSdPic(int x, int y, int w, int h, const char *dir, const char *file) {
  if (!sdOk || !gfx || !dir || !file || !file[0] || w < 2 || h < 2) return;
  char path[84];
  snprintf(path, sizeof(path), "/%s/%s.jpg", dir, file);
  if (!SD_MMC.exists(path)) {
    snprintf(path, sizeof(path), "/catalogue/%s/%s.jpg", dir, file);
    if (!SD_MMC.exists(path)) return;
  }
  File pic = SD_MMC.open(path, "r");
  if (!pic) return;
  size_t sz = pic.size();
  uint8_t *jpg = nullptr;
  if (sz > 32 && sz <= 1200000) jpg = (uint8_t *)ps_malloc(sz);
  bool inRam = false;
  if (jpg) {
    inRam = pic.read(jpg, sz) == sz;
    if (!inRam) {
      free(jpg);
      jpg = nullptr;
    }
  }
  pic.close();
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(sdPlot);
  uint16_t iw = 0, ih = 0;
  JRESULT jr = inRam ? TJpgDec.getJpgSize(&iw, &ih, jpg, (uint32_t)sz)
                     : TJpgDec.getFsJpgSize(&iw, &ih, path, SD_MMC);
  if (jr != JDR_OK || iw < 2 || ih < 2) {
    if (jpg) free(jpg);
    TJpgDec.setJpgScale(1);
    return;
  }
  uint8_t scale = 1;
  while (scale < 8 && (iw / (uint16_t)(scale * 2)) >= (uint16_t)w) scale = (uint8_t)(scale * 2);
  TJpgDec.setJpgScale(scale);
  fitSW = (int)iw / scale;
  fitSH = (int)ih / scale;
  if (fitSW < 1) fitSW = 1;
  if (fitSH < 1) fitSH = 1;
  fitX = x;
  fitY = y;
  fitW = w;
  fitH = h;
  sdSkipSy = -9999;
  sdSkipDy = -9999;
  if (inRam) TJpgDec.drawJpg(0, 0, jpg, (uint32_t)sz);
  else TJpgDec.drawFsJpg(0, 0, path, SD_MMC);
  if (jpg) free(jpg);
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
}

void drawNamedPic(int x, int y, int w, int h, const char *name) {
  const char *dir = nullptr;
  const char *file = nullptr;
  if (picPath(name, &dir, &file)) drawSdPic(x, y, w, h, dir, file);
}

void drawPlanets() {
  page = PG_PLANETS;
  netHold = false;
  fillNow();
  gfx->fillScreen(COL_BG);
  drawBack("PLANETS");
  gfx->setTextSize(2);
  int y = 46;
  char line[120], dist[96];
  if (!sunNow.ok) {
    gfx->setTextColor(COL_MID, COL_BG);
    centre(placeSet ? "waiting for the clock" : "set a place", CX, 220);
    return;
  }
  for (int i = 0; i < PLANET_N; i++) {
    uint16_t row = (i % 2) ? COL_BG : COL_DIM;
    gfx->fillRect(12, y, 776, 58, row);
    fmtRange(planetNow[i].distAu, dist, sizeof(dist));
    char spd[28];
    fmtMph(orbitMph(PLANETS[i].a), spd, sizeof(spd));
    const char *st = planetNow[i].glare ? "in the glare" : (planetNow[i].el >= 0 ? "up" : "set");
    snprintf(line, sizeof(line), "%s  LOOK %s  %s  %d deg", PLANETS[i].name, lookWord(planetNow[i].az), st,
             planetNow[i].el);
    gfx->setTextSize(2);
    gfx->setTextColor(COL_TXT, row);
    gfx->setCursor(20, y + 1);
    gfx->print(line);
    gfx->setTextSize(1);
    gfx->setTextColor(COL_MID, row);
    gfx->setCursor(20, y + 18);
    snprintf(line, sizeof(line), "distance from Earth  %s", dist);
    gfx->print(line);
    gfx->setCursor(20, y + 28);
    if (spd[0])
      snprintf(line, sizeof(line), "made of %s. speed %s", PLANET_FACT[i].made, spd);
    else
      snprintf(line, sizeof(line), "made of %s. speed unknown", PLANET_FACT[i].made);
    gfx->print(line);
    gfx->setCursor(20, y + 38);
    snprintf(line, sizeof(line), "day length %s. year length %s", PLANET_FACT[i].dayLen, PLANET_FACT[i].yearLen);
    gfx->print(line);
    gfx->setCursor(20, y + 48);
    gfx->print(PLANET_FACT[i].moons);
    drawNamedPic(728, y + 6, 46, 46, PLANETS[i].name);
    y += 60;
  }
  gfx->setTextSize(1);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(16, 468);
  gfx->print("tap a row for the air and the temperatures");
}

void drawMoon() {
  page = PG_MOON;
  netHold = false;
  fillNow();
  gfx->fillScreen(COL_BG);
  drawBack("MOON");
  float phase = 0;
  int pct = 0;
  const char *mn = moonName(&phase, &pct);
  if (mn) drawMoonPhase(150, 150, 52, phase);
  gfx->setTextSize(3);
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setCursor(250, 78);
  gfx->print(mn ? mn : "waiting");
  gfx->setTextSize(2);
  char line[96];
  if (mn) {
    snprintf(line, sizeof(line), "%d%% lit", pct);
    gfx->setTextColor(COL_HOT, COL_BG);
    gfx->setCursor(250, 118);
    gfx->print(line);
    if (!strcmp(mn, "new")) {
      gfx->setCursor(250, 146);
      gfx->print("no moon to see");
    } else if (!strcmp(mn, "full")) {
      gfx->setCursor(250, 146);
      gfx->print("full moon");
    }
  }
  gfx->setTextColor(COL_TXT, COL_BG);
  if (mn && placeTz[0]) {
    char when[32];
    phaseWhen(phase, 0.5f, when, sizeof(when));
    snprintf(line, sizeof(line), "full moon   %s", when);
    gfx->setCursor(250, 186);
    gfx->print(line);
    phaseWhen(phase, 0.0f, when, sizeof(when));
    snprintf(line, sizeof(line), "no visible moon   %s", when);
    gfx->setCursor(250, 214);
    gfx->print(line);
  }
  if (moonNow.ok) {
    snprintf(line, sizeof(line), "%s  LOOK %s  %d deg", moonNow.el >= 0 ? "up" : "below the horizon",
             lookWord(moonNow.az), moonNow.el);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(24, 280);
    gfx->print(line);
    fmtRange(moonNow.distAu, line, sizeof(line));
    gfx->setCursor(24, 312);
    char earth[140];
    snprintf(earth, sizeof(earth), "distance from Earth  %s", line);
    gfx->print(earth);
    time_t now;
    if (liveClock(&now)) {
      double ra, dec;
      char hrs[48];
      if (bodySky(-1, now, &ra, &dec)) {
        skyHours(ra, dec, now, hrs, sizeof(hrs));
        if (!strncmp(hrs, "up ", 3)) {
          char *down = strstr(hrs, "   down ");
          if (down) {
            char rise[16], setb[16];
            int rl = (int)(down - (hrs + 3));
            if (rl < 0) rl = 0;
            if (rl > 15) rl = 15;
            memcpy(rise, hrs + 3, (size_t)rl);
            rise[rl] = 0;
            strlcpy(setb, down + 8, sizeof(setb));
            snprintf(hrs, sizeof(hrs), "moon rise %s   moon set %s", rise, setb);
          } else if (strcmp(hrs, "up through the night")) {
            char rise[16];
            strlcpy(rise, hrs + 3, sizeof(rise));
            snprintf(hrs, sizeof(hrs), "moon rise %s", rise);
          } else
            strlcpy(hrs, "moon is up through the night", sizeof(hrs));
        }
        gfx->setCursor(24, 344);
        gfx->print(hrs);
      }
    }
  }
  gfx->setTextSize(1);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(24, 392);
  gfx->print("highlands are anorthosite, dark plains are basalt, small iron core, about 2,160 miles across, almost no air");
  gfx->setCursor(24, 424);
  gfx->print("distance is approximate");
  drawNamedPic(620, 56, 160, 160, "Moon");
}

void drawMore() {
  page = PG_MORE;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("MORE");
  chip(16, 48, 376, 48, "TONIGHT", false);
  chip(408, 48, 376, 48, "SPACE WEATHER", false);
  chip(16, 104, 376, 48, "ISS", false);
  chip(408, 104, 376, 48, "ASTEROIDS", false);
  chip(16, 160, 376, 48, "COMETS", false);
  chip(408, 160, 376, 48, "METEOR SHOWERS", false);
  chip(16, 216, 376, 48, "LAUNCHES", false);
  chip(408, 216, 376, 48, "TELESCOPES", false);
  chip(16, 272, 376, 48, "SETUP", false);
  chip(408, 272, 376, 48, "ABOUT", false);
  chip(16, 328, 376, 48, "DARK SKY", false);
  chip(408, 328, 376, 48, "CLOUDS", false);
  chip(16, 384, 376, 48, "CLOSE PASSES", false);
  chip(408, 384, 376, 48, "SOLAR", false);
  chip(16, 432, 768, 40, "SEARCH", false);
}

void drawDark() {
  page = PG_DARK;
  netHold = false;
  fillNow();
  computeDark();
  gfx->fillScreen(COL_BG);
  drawBack("DARK SKY");
  float phase = 0;
  int pct = 0;
  const char *mn = moonName(&phase, &pct);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  if (mn) {
    drawMoonPhase(70, 120, 36, phase);
    char line[64];
    snprintf(line, sizeof(line), "%s    %d%% lit", mn, pct);
    gfx->setCursor(130, 88);
    gfx->print(line);
    time_t now;
    char hrs[48];
    hrs[0] = 0;
    double ra, dec;
    if (moonNow.ok && liveClock(&now) && bodySky(-1, now, &ra, &dec)) {
      skyHours(ra, dec, now, hrs, sizeof(hrs));
      if (!strncmp(hrs, "up ", 3)) {
        char *down = strstr(hrs, "   down ");
        if (down) {
          char rise[16], setb[16];
          int rl = (int)(down - (hrs + 3));
          if (rl < 0) rl = 0;
          if (rl > 15) rl = 15;
          memcpy(rise, hrs + 3, (size_t)rl);
          rise[rl] = 0;
          strlcpy(setb, down + 8, sizeof(setb));
          snprintf(hrs, sizeof(hrs), "moon rise %s   moon set %s", rise, setb);
        } else if (strcmp(hrs, "up through the night")) {
          char rise[16];
          strlcpy(rise, hrs + 3, sizeof(rise));
          snprintf(hrs, sizeof(hrs), "moon rise %s", rise);
        } else
          strlcpy(hrs, "moon is up through the night", sizeof(hrs));
      }
      gfx->setCursor(130, 124);
      gfx->print(hrs);
    }
  } else {
    gfx->setCursor(24, 100);
    gfx->print("moon waiting");
  }
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setCursor(24, 200);
  gfx->print("astronomical twilight");
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setCursor(24, 240);
  if (!placeSet || !placeTz[0])
    gfx->print("set a place");
  else if (!darkNow)
    gfx->print("waiting for the clock");
  else if (darkNow == 1 && !darkEve[0] && !darkMorn[0])
    gfx->print("the sky stays dark today");
  else if (darkNow == 2 && !darkEve[0] && !darkMorn[0])
    gfx->print("the sun stays too high for a dark sky today");
  else if (darkNow == 1)
    gfx->print("the sky is dark now");
  else
    gfx->print("the sky is not dark now");
  char line[64];
  gfx->setCursor(24, 280);
  if (darkEve[0]) {
    snprintf(line, sizeof(line), "dark after %s", darkEve);
    gfx->print(line);
  }
  gfx->setCursor(24, 316);
  if (darkMorn[0]) {
    snprintf(line, sizeof(line), "dark until %s", darkMorn);
    gfx->print(line);
  }
  gfx->setTextSize(1);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(24, 420);
  gfx->print("astronomical twilight is the sun 18 degrees below the horizon");
}

void drawClouds() {
  page = PG_CLOUD;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("CLOUDS");
  gfx->setTextSize(3);
  gfx->setTextColor(COL_TXT, COL_BG);
  if (wxCloud >= 0) {
    char line[32];
    snprintf(line, sizeof(line), "cloud cover  %d%%", wxCloud);
    centre(line, CX, 200);
  } else {
    centre(wifiOk ? "cloud cover waiting" : "join wifi", CX, 200);
  }
  cloudDirty = false;
}

void drawNeo() {
  page = PG_NEO;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("CLOSE PASSES");
  gfx->setTextSize(2);
  if (nClose < 0) {
    gfx->setTextColor(COL_MID, COL_BG);
    centre(wifiOk ? "waiting" : "join wifi", CX, 220);
    neoDirty = false;
    return;
  }
  if (!nClose) {
    gfx->setTextColor(COL_MID, COL_BG);
    centre("none on the list", CX, 220);
    neoDirty = false;
    return;
  }
  int y = 56;
  for (int i = 0; i < nClose && i < 5; i++) {
    gfx->setTextColor(COL_HOT, COL_BG);
    gfx->setCursor(24, y);
    gfx->print(closePass[i].des);
    gfx->setTextColor(COL_TXT, COL_BG);
    gfx->setCursor(24, y + 28);
    char line[80];
    snprintf(line, sizeof(line), "%s    %s    %s", closePass[i].when, closePass[i].dist, closePass[i].spd);
    gfx->print(line);
    y += 72;
  }
  gfx->setTextSize(1);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(24, 440);
  gfx->print("size is not in this list");
  neoDirty = false;
}

void drawSolar() {
  page = PG_SOLAR;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("SOLAR");
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setCursor(16, 48);
  gfx->print("X-ray flux");
  if (xrayN <= 0) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(180, 48);
    gfx->print(wifiOk ? "waiting" : "join wifi");
  } else {
    int base = 168;
    int maxH = 80;
    gfx->setTextSize(1);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(16, 78);
    gfx->print("X");
    gfx->setCursor(16, 98);
    gfx->print("M");
    gfx->setCursor(16, 118);
    gfx->print("C");
    gfx->setCursor(16, 138);
    gfx->print("B");
    gfx->setCursor(16, 158);
    gfx->print("A");
    for (int i = 0; i < xrayN && i < 8; i++) {
      float f = xrayFlux[i];
      if (f < 1e-9f) f = 1e-9f;
      float lg = log10f(f);
      if (lg < -8.0f) lg = -8.0f;
      if (lg > -3.0f) lg = -3.0f;
      int h = (int)((lg + 8.0f) / 5.0f * (float)maxH);
      if (h < 2) h = 2;
      char cls = 'A';
      uint16_t col = COL_MID;
      if (f >= 1e-4f) {
        cls = 'X';
        col = COL_HOT;
      } else if (f >= 1e-5f) {
        cls = 'M';
        col = COL_HOT;
      } else if (f >= 1e-6f) {
        cls = 'C';
        col = COL_AMB;
      } else if (f >= 1e-7f) {
        cls = 'B';
        col = COL_TXT;
      }
      int x = 48 + i * 52;
      gfx->fillRect(x, base - h, 36, h, col);
      gfx->setTextSize(2);
      gfx->setTextColor(COL_TXT, COL_BG);
      gfx->setCursor(x + 10, base + 4);
      gfx->print(cls);
    }
  }
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setCursor(16, 214);
  if (nSunRegions < 0)
    gfx->print(wifiOk ? "sunspot regions  waiting" : "sunspot regions  join wifi");
  else if (!nSunRegions)
    gfx->print("sunspot regions  0");
  else {
    char line[40];
    snprintf(line, sizeof(line), "sunspot regions  %d", nSunRegions);
    gfx->print(line);
    int shown = nSunRegions < 4 ? nSunRegions : 4;
    for (int i = 0; i < shown; i++) {
      char row[48];
      snprintf(row, sizeof(row), "%s   %s   %s", sunRegions[i].num, sunRegions[i].loc, sunRegions[i].mag);
      gfx->setCursor(24, 246 + i * 28);
      gfx->print(row);
    }
  }
  int spd = windPair > 0 ? windPair : windSpeed;
  char line[64];
  gfx->setCursor(16, 372);
  if (spd > 0)
    snprintf(line, sizeof(line), "solar wind speed  %d km/s", spd);
  else
    snprintf(line, sizeof(line), "solar wind speed  %s", wifiOk ? "waiting" : "join wifi");
  gfx->print(line);
  gfx->setCursor(16, 408);
  if (windDens10 >= 0)
    snprintf(line, sizeof(line), "proton density  %d.%d per cm3", windDens10 / 10, windDens10 % 10);
  else
    snprintf(line, sizeof(line), "proton density  %s", wifiOk ? "waiting" : "join wifi");
  gfx->print(line);
  solarDirty = false;
}

void drawStationGraphic(int x, int y, int which) {
  if (which == 0) {
    gfx->fillRect(x - 20, y - 14, 8, 28, RGB565(30, 70, 150));
    gfx->fillRect(x + 12, y - 14, 8, 28, RGB565(30, 70, 150));
    gfx->fillRect(x - 16, y - 2, 32, 4, RGB565(210, 214, 220));
    gfx->fillRect(x - 8, y - 6, 16, 12, RGB565(240, 240, 245));
  } else {
    gfx->fillRect(x - 16, y - 10, 6, 20, RGB565(30, 70, 150));
    gfx->fillRect(x + 10, y - 10, 6, 20, RGB565(30, 70, 150));
    gfx->fillRoundRect(x - 8, y - 4, 16, 8, 3, RGB565(230, 232, 236));
  }
  gfx->fillCircle(x, y, 2, COL_WHT);
  gfx->setTextSize(1);
  gfx->setTextColor(COL_WHT, RGB565(8, 14, 28));
  gfx->setCursor(x + 18, y - 4);
  gfx->print(SATS[which].name);
}

static int satMph(int ix) {
  if (ix < 0 || ix >= SAT_N || !satLive[ix].ok || !satLive[ix].tle.ok) return 0;
  double n = satLive[ix].tle.n;
  if (n < 10.0 || n > 20.0) return 0;
  double alt = satLive[ix].alt > 50 ? satLive[ix].alt : 420.0;
  double period = 86400.0 / n;
  double mph = (2.0 * 3.14159265 * (6371.0 + alt) * 1000.0 / period) * 2.23694;
  int r = (int)(mph + 50.0);
  return (r / 100) * 100;
}

void drawSats() {
  page = PG_SATS;
  netHold = true;
  netPend = NET_NONE;
  if (netBusy) {
    satsDirty = true;
    gfx->fillScreen(COL_BG);
    drawBack("ISS");
    gfx->setTextSize(2);
    gfx->setTextColor(COL_MID, COL_BG);
    centre("one moment", CX, 220);
    return;
  }
  ensureSats(satHeavyAt == 0);
  updateSunPos();
  gfx->fillScreen(COL_BG);
  drawBack("ISS");
  if (worldReady) drawWorldPhoto();
  else {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(2);
    centre("world picture did not open", CX, 180);
  }
  if (sunOk) drawSunMark(mapX(sunLon), mapY(sunLat));
  int n = SAT_N < 2 ? SAT_N : 2;
  for (int i = 0; i < n; i++) {
    uint16_t c = SAT_COL[i];
    int px = -999, py = -999;
    for (int k = 0; k < satLive[i].nTrk; k++) {
      int x = mapX(satLive[i].trkLon[k]);
      int y = mapY(satLive[i].trkLat[k]);
      if (px > -900 && abs(x - px) < 200) gfx->drawLine(px, py, x, y, c);
      px = x;
      py = y;
    }
    if (satLive[i].ok) {
      int x = mapX(satLive[i].lon);
      int y = mapY(satLive[i].lat);
      drawStationGraphic(x, y, i);
    }
  }
  if (placeSet) gfx->fillCircle(mapX(placeLon), mapY(placeLat), 5, COL_PINK);
  gfx->fillRect(0, MAP_Y0 + MAP_H, PW, PH - (MAP_Y0 + MAP_H), COL_BG);
  gfx->setTextSize(2);
  int y = MAP_Y0 + MAP_H + 6;
  for (int i = 0; i < n; i++) {
    char line[96];
    int miles = (int)(satLive[i].alt * 0.621371f);
    int mph = satMph(i);
    char spd[36] = "";
    if (mph) snprintf(spd, sizeof(spd), "  speed about %d mph", mph);
    if (satLive[i].vis)
      snprintf(line, sizeof(line), "%s  up now  LOOK %s  %d miles%s", SATS[i].name, lookWord(satLive[i].az), miles, spd);
    else if (satLive[i].when[0])
      snprintf(line, sizeof(line), "%s  %s%s", SATS[i].name, satLive[i].when, spd);
    else
      snprintf(line, sizeof(line), "%s  %s%s", SATS[i].name, wifiOk ? "no pass soon" : "join wifi", spd);
    gfx->setTextColor(SAT_COL[i], COL_BG);
    gfx->setCursor(12, y);
    gfx->print(line);
    y += 18;
    gfx->setTextSize(1);
    if (satLive[i].nPass) {
      for (int p = 0; p < satLive[i].nPass && p < 2; p++) {
        gfx->setCursor(28, y);
        gfx->print(satLive[i].pass[p]);
        y += 12;
      }
    }
    gfx->setTextSize(2);
    y += 4;
  }
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  gfx->setCursor(12, y);
  gfx->print("pink dot is your place. tap a line for facts");
  gfx->setCursor(12, y + 14);
  if (nIssCrew > 0) {
    char crew[40];
    snprintf(crew, sizeof(crew), "ISS onboard crew - %d", nIssCrew);
    gfx->print(crew);
  } else
    gfx->print("ISS onboard crew waiting");
  lastSatDraw = millis();
  satsDirty = false;
  crewDirty = false;
}

static const char *asteroidMade(const char *name) {
  if (!strcmp(name, "Ceres")) return "water ice, clay, and salts";
  if (!strcmp(name, "Vesta")) return "basalt crust and an iron core";
  if (!strcmp(name, "Pallas")) return "unknown";
  if (!strcmp(name, "Juno")) return "olivine and pyroxene";
  return "unknown";
}

void drawRocks() {
  page = PG_ROCKS;
  netHold = false;
  fillNow();
  gfx->fillScreen(COL_BG);
  drawBack("ASTEROIDS");
  chip(600, 8, 188, 36, rockPage ? "ASTEROIDS" : "SHOWERS", false);
  gfx->setTextSize(2);
  if (!rockNow[0].ok) {
    gfx->setTextColor(COL_MID, COL_BG);
    centre(placeSet ? "waiting for the clock" : "set a place", CX, 220);
    return;
  }
  if (rockPage) {
    time_t now = 0;
    struct tm t;
    bool timed = liveClock(&now) && placeTz[0];
    if (timed) localtime_r(&now, &t);
    int y = 52;
    int shown = 0;
    for (int s = 0; s < SHOWER_N; s++) {
      if (y + 48 > 468) break;
      bool on = false;
      if (timed) {
        int d = showerDelta(SHOWERS[s], t);
        on = d >= -SHOWERS[s].span && d <= SHOWERS[s].span;
      }
      uint16_t row = on ? COL_HOT : ((shown % 2) ? COL_BG : COL_DIM);
      gfx->fillRect(12, y, 776, 48, row);
      char line[96];
      snprintf(line, sizeof(line), "peaks %s %d   %s   %d an hour", MON[SHOWERS[s].month - 1], SHOWERS[s].day,
               SHOWERS[s].name, SHOWERS[s].zhr);
      gfx->setTextColor(on ? COL_BG : COL_TXT, row);
      gfx->setCursor(20, y + 4);
      gfx->print(line);
      ViewWin w;
      if (timed) meteorView(SHOWERS[s], now, &w);
      else {
        w.ok = false;
        strlcpy(w.span, "waiting for the clock", sizeof(w.span));
      }
      if (w.ok)
        snprintf(line, sizeof(line), "%s   %s   LOOK %s  %d deg", SHOWERS[s].parent, w.span, lookWord(w.az), w.el);
      else
        snprintf(line, sizeof(line), "%s   %s", SHOWERS[s].parent, w.span);
      gfx->setTextColor(on ? COL_BG : COL_MID, row);
      gfx->setCursor(20, y + 26);
      gfx->print(line);
      drawNamedPic(732, y + 4, 40, 40, SHOWERS[s].name);
      y += 50;
      shown++;
    }
    return;
  }
  int y = 52;
  for (int i = 0; i < ROCK_N; i++) {
    if (strcmp(ROCKS[i].kind, "asteroid")) continue;
    uint16_t row = (i % 2) ? COL_BG : COL_DIM;
    gfx->fillRect(12, y, 776, 66, row);
    char line[80], dist[96];
    fmtRange(rockNow[i].distAu, dist, sizeof(dist));
    char spd[28] = "";
    if (!strcmp(ROCKS[i].kind, "asteroid")) fmtMph(orbitMph(ROCKS[i].a), spd, sizeof(spd));
    if (spd[0])
      snprintf(line, sizeof(line), "%s  %s  LOOK %s  %s %d deg   speed %s", ROCKS[i].name, ROCKS[i].kind,
               lookWord(rockNow[i].az), rockNow[i].el >= 0 ? "up" : "set", rockNow[i].el, spd);
    else
      snprintf(line, sizeof(line), "%s  %s  LOOK %s  %s %d deg", ROCKS[i].name, ROCKS[i].kind, lookWord(rockNow[i].az),
               rockNow[i].el >= 0 ? "up" : "set", rockNow[i].el);
    gfx->setTextSize(2);
    gfx->setTextColor(COL_TXT, row);
    gfx->setCursor(20, y + 4);
    gfx->print(line);
    gfx->setTextSize(1);
    gfx->setTextColor(COL_MID, row);
    gfx->setCursor(20, y + 28);
    char earth[140];
    snprintf(earth, sizeof(earth), "distance from Earth  %s", dist);
    if (ROCKS[i].H < 40 && rockNow[i].extra < 40) {
      char extra[24];
      snprintf(extra, sizeof(extra), "   about mag %.0f", rockNow[i].extra);
      strlcat(earth, extra, sizeof(earth));
    } else if (ROCKS[i].note[0]) {
      strlcat(earth, "   ", sizeof(earth));
      strlcat(earth, ROCKS[i].note, sizeof(earth));
    }
    gfx->print(earth);
    snprintf(earth, sizeof(earth), "made of %s", asteroidMade(ROCKS[i].name));
    gfx->setCursor(20, y + 42);
    gfx->print(earth);
    drawNamedPic(724, y + 8, 48, 48, ROCKS[i].name);
    y += 70;
  }
  gfx->setTextSize(2);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(16, 452);
  gfx->print("orbits are stepped on this board. distances are approximate");
}

static const char *nearCon(double ra, double dec) {
  static const struct {
    const char *n;
    uint8_t at;
  } tab[] = {
      {"Orion", 30},     {"Ursa Major", 5}, {"Cassiopeia", 44}, {"Cygnus", 49},
      {"Leo", 61},       {"Gemini", 23},    {"Pegasus", 41},    {"Scorpius", 22},
      {"Taurus", 20},    {"Crux", 52},      {"Auriga", 15},     {"Perseus", 64},
      {"Andromeda", 38}, {"Canis Major", 11}, {"Draco", 78},
  };
  const char *best = nullptr;
  double bestA = 40.0 * DEG;
  for (int i = 0; i < (int)(sizeof(tab) / sizeof(tab[0])); i++) {
    for (int s = 0; s < STAR_N; s++) {
      if (STARS[s].name != tab[i].at) continue;
      double sra = (STARS[s].ra / 100.0) * 15.0 * DEG;
      double sdec = (STARS[s].dec / 10.0) * DEG;
      double cd = sin(dec) * sin(sdec) + cos(dec) * cos(sdec) * cos(ra - sra);
      if (cd > 1) cd = 1;
      if (cd < -1) cd = -1;
      double ang = acos(cd);
      if (ang < bestA) {
        bestA = ang;
        best = tab[i].n;
      }
      break;
    }
  }
  return best;
}

void drawComets() {
  page = PG_COMETS;
  netHold = false;
  fillNow();
  gfx->fillScreen(COL_BG);
  drawBack("COMETS");
  if (!rockNow[0].ok) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(2);
    centre(placeSet ? "waiting for the clock" : "set a place", CX, 220);
    return;
  }
  int y = 56;
  time_t now = 0;
  liveClock(&now);
  for (int i = 0; i < ROCK_N; i++) {
    if (strcmp(ROCKS[i].kind, "comet")) continue;
    uint16_t row = (i % 2) ? COL_BG : COL_DIM;
    gfx->fillRect(12, y, 776, 76, row);
    char line[96], dist[96], vis[96], earth[140];
    snprintf(line, sizeof(line), "%s  comet  LOOK %s  %s %d deg", ROCKS[i].name, lookWord(rockNow[i].az),
             rockNow[i].el >= 0 ? "up" : "set", rockNow[i].el);
    gfx->setTextSize(2);
    gfx->setTextColor(COL_TXT, row);
    gfx->setCursor(20, y + 2);
    gfx->print(line);
    fmtRange(rockNow[i].distAu, dist, sizeof(dist));
    gfx->setTextColor(COL_MID, row);
    gfx->setTextSize(1);
    gfx->setCursor(20, y + 22);
    snprintf(earth, sizeof(earth), "distance from Earth  %s", dist);
    gfx->print(earth);
    gfx->setCursor(20, y + 36);
    gfx->print(ROCKS[i].note);
    const char *see = "needs a telescope";
    const char *con = nullptr;
    if (rockNow[i].el >= 0 && now) {
      double ra = 0, dec = 0;
      int az = 0, el = 0;
      rockPlace(ROCKS[i], now, &az, &el, nullptr, nullptr, nullptr, &ra, &dec);
      con = nearCon(ra, dec);
    }
    if (con)
      snprintf(vis, sizeof(vis), "near %s   %s", con, see);
    else
      snprintf(vis, sizeof(vis), "%s", see);
    gfx->setCursor(20, y + 52);
    gfx->print(vis);
    drawNamedPic(716, y + 12, 52, 52, ROCKS[i].name);
    y += 84;
  }
  gfx->setTextSize(2);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(16, 452);
  gfx->print("A comet's speed changes along its orbit.");
}

void drawScopes() {
  page = PG_SCOPES;
  netHold = false;
  ensureSats(false);
  gfx->fillScreen(COL_BG);
  drawBack("TELESCOPES");
  gfx->setTextSize(2);
  int y = 52;
  for (int i = 0; i < SCOPE_N; i++) {
    uint16_t row = (i % 2) ? COL_BG : COL_DIM;
    gfx->fillRect(12, y, 776, 70, row);
    gfx->setTextSize(2);
    gfx->setTextColor(COL_HOT, row);
    gfx->setCursor(20, y + 1);
    gfx->print(SCOPES[i].name);
    gfx->setTextColor(COL_TXT, row);
    gfx->setTextSize(1);
    gfx->setCursor(20, y + 18);
    gfx->print(SCOPES[i].mirror);
    char where[140];
    snprintf(where, sizeof(where), "distance from Earth  %s", SCOPES[i].where);
    gfx->setCursor(20, y + 30);
    gfx->print(where);
    gfx->setCursor(20, y + 42);
    gfx->print(SCOPES[i].launched);
    gfx->setCursor(20, y + 54);
    gfx->print(SCOPES[i].svc1);
    drawNamedPic(720, y + 10, 50, 50, SCOPES[i].name);
    y += 74;
  }
  gfx->setTextSize(2);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(16, 448);
  gfx->print("tap a row. there is no single magnification number");
}

void drawShowers() {
  page = PG_SHOWERS;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("METEOR SHOWERS");
  time_t now = 0;
  struct tm t;
  bool timed = liveClock(&now) && placeTz[0];
  if (timed) localtime_r(&now, &t);
  gfx->setTextSize(2);
  int y = 50;
  for (int i = 0; i < SHOWER_N; i++) {
    bool on = false;
    if (timed) {
      int d = showerDelta(SHOWERS[i], t);
      on = d >= -SHOWERS[i].span && d <= SHOWERS[i].span;
    }
    uint16_t row = on ? COL_HOT : ((i % 2) ? COL_BG : COL_DIM);
    gfx->fillRect(12, y, 776, 48, row);
    char line[96];
    snprintf(line, sizeof(line), "peaks %s %d   %s   %d an hour", MON[SHOWERS[i].month - 1], SHOWERS[i].day,
             SHOWERS[i].name, SHOWERS[i].zhr);
    gfx->setTextColor(on ? COL_BG : COL_TXT, row);
    gfx->setCursor(20, y + 4);
    gfx->print(line);
    ViewWin w;
    if (timed) meteorView(SHOWERS[i], now, &w);
    else {
      w.ok = false;
      strlcpy(w.span, "waiting for the clock", sizeof(w.span));
    }
    if (w.ok)
      snprintf(line, sizeof(line), "%s   %s   LOOK %s  %d deg", SHOWERS[i].parent, w.span, lookWord(w.az), w.el);
    else
      snprintf(line, sizeof(line), "%s   %s", SHOWERS[i].parent, w.span);
    gfx->setTextColor(on ? COL_BG : COL_MID, row);
    gfx->setCursor(20, y + 26);
    gfx->print(line);
    drawNamedPic(732, y + 4, 40, 40, SHOWERS[i].name);
    y += 50;
  }
}

static void payloadMeasure(const Launch &L, char *out, int n) {
  if (L.mission[0] && strstr(L.mission, "Starlink")) {
    strlcpy(out, "one satellite is about 1,760 lb. size unknown", n);
    return;
  }
  if (strstr(L.rocket, "Falcon") && (L.going[0] || strstr(L.mission, "Dragon") || strstr(L.mission, "Crew"))) {
    strlcpy(out, "payload about 27 feet tall and 13 feet across. weight changes with the cargo", n);
    return;
  }
  strlcpy(out, "payload weight unknown. payload size unknown", n);
}

void drawLaunchCard(int y, const Launch &L) {
  gfx->fillRoundRect(16, y, PW - 32, 92, 6, COL_DIM);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_HOT, COL_DIM);
  gfx->setCursor(28, y + 6);
  gfx->print(L.rocket[0] ? L.rocket : "----");
  char line[180];
  snprintf(line, sizeof(line), "%s    %s", L.day[0] ? L.day : "----", L.when[0] ? L.when : "--:--");
  gfx->setTextColor(COL_TXT, COL_DIM);
  gfx->setCursor(420, y + 6);
  gfx->print(line);
  gfx->setCursor(28, y + 30);
  gfx->print(L.mission[0] ? L.mission : "mission not listed");
  gfx->setTextSize(1);
  gfx->setTextColor(COL_TXT, COL_DIM);
  snprintf(line, sizeof(line), "%s", L.payload[0] ? L.payload : "unknown payload");
  gfx->setCursor(28, y + 48);
  gfx->print(line);
  const RocketFact *rf = nullptr;
  for (int i = 0; i < (int)(sizeof(ROCKET_FACT) / sizeof(ROCKET_FACT[0])); i++) {
    if (L.rocket[0] && strstr(L.rocket, ROCKET_FACT[i].key)) {
      rf = &ROCKET_FACT[i];
      break;
    }
  }
  if (rf)
    snprintf(line, sizeof(line), "%s   %s   planned orbit height unknown", rf->tall, rf->filled);
  else
    strlcpy(line, "vehicle height and filled weight are not stored for this rocket. planned orbit height unknown", sizeof(line));
  gfx->setCursor(28, y + 60);
  gfx->print(line);
  payloadMeasure(L, line, sizeof(line));
  gfx->setCursor(28, y + 72);
  gfx->print(line);
  if (L.going[0]) {
    if (!strcmp(L.going, "not listed"))
      strlcpy(line, "crew flight, destination unknown", sizeof(line));
    else
      snprintf(line, sizeof(line), "crew flight, going to %s", L.going);
  } else
    snprintf(line, sizeof(line), "%s", L.pad[0] ? L.pad : "");
  gfx->setTextColor(COL_MID, COL_DIM);
  gfx->setCursor(28, y + 84);
  if (line[0]) gfx->print(line);
  drawNamedPic(720, y + 20, 52, 52, L.rocket);
}
void drawLaunch() {
  page = PG_LAUNCH;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("LAUNCHES");
  if (!wifiOk || !launchOk) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(2);
    centre(liveClock(nullptr) ? "waiting for live launches" : "waiting for the clock", CX, 220);
    return;
  }
  if (!nLaunch) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(2);
    centre("no upcoming in the live list", CX, 220);
    return;
  }
  int y = 56;
  for (int i = 0; i < nLaunch && i < 4; i++) {
    drawLaunchCard(y, launches[i]);
    y += 100;
  }
}

void drawSet() {
  page = PG_SET;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("SETUP");
  chip(652, 8, 140, 36, "MORE", false);
  chip(16, 64, 180, 52, "DARK", dark);
  chip(208, 64, 180, 52, "LIGHT", !dark);
  chip(16, 132, 160, 52, "DIM", blLevel == 0);
  chip(188, 132, 160, 52, "MID", blLevel == 1);
  chip(360, 132, 180, 52, "BRIGHT", blLevel == 2);
  chip(16, 200, 160, 52, "F", !useC);
  chip(188, 200, 160, 52, "C", useC);
  chip(16, 268, 250, 52, "ABOUT", false);
  chip(280, 268, 250, 52, "WIFI", false);
  chip(544, 268, 240, 52, "PLACE", false);
  chip(16, 336, 360, 52, "FACTORY RESET", false);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(2);
  gfx->setCursor(16, 408);
  gfx->print("clears this desk only");
}
void drawAbout() {
  page = PG_ABOUT;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("ABOUT");
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setCursor(24, 70);
  gfx->print("Astronomy Desk");
  char line[40];
  snprintf(line, sizeof(line), "version  %s", ASTRO_VER);
  gfx->setCursor(24, 102);
  gfx->print(line);
  chip(170, 160, 460, 52, "CHECK FOR UPDATES", false);
  chip(170, 236, 460, 52, "INSTALL", otaNewer);
  gfx->setTextColor(COL_MID, COL_BG);
  centre(otaMsg[0] ? otaMsg : "checks GitHub for a newer copy", CX, 320);
  if (otaPct >= 0) {
    int w = (720 * (otaPct < 0 ? 0 : (otaPct > 100 ? 100 : otaPct))) / 100;
    gfx->drawRect(40, 360, 720, 22, COL_DIM);
    if (w > 0) gfx->fillRect(40, 360, w, 22, COL_HOT);
  }
  gfx->setCursor(24, 420);
  gfx->print("Distances are approximate.");
}

void drawOtaStatus() {
  if (page != PG_ABOUT) return;
  gfx->fillRect(0, 300, PW, 90, COL_BG);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_MID, COL_BG);
  centre(otaMsg[0] ? otaMsg : "checks GitHub for a newer copy", CX, 320);
  if (otaPct >= 0) {
    int pct = otaPct;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    int w = (720 * pct) / 100;
    gfx->drawRect(40, 360, 720, 22, COL_DIM);
    if (w > 0) gfx->fillRect(40, 360, w, 22, COL_HOT);
  }
}

void shortPlace(const char *display, char *out, int n) {
  char buf[120];
  strlcpy(buf, display, sizeof(buf));
  char *parts[6];
  int np = 0;
  parts[np++] = buf;
  for (char *p = buf; *p && np < 6; p++) {
    if (*p == ',') {
      *p = 0;
      char *q = p + 1;
      while (*q == ' ') q++;
      parts[np++] = q;
      p = q - 1;
    }
  }
  for (int i = 0; i < np; i++) {
    int len = (int)strlen(parts[i]);
    while (len > 0 && parts[i][len - 1] == ' ') parts[i][--len] = 0;
  }
  if (np >= 4) snprintf(out, n, "%s, %s", parts[0], parts[2]);
  else if (np >= 2) snprintf(out, n, "%s, %s", parts[0], parts[np - 2]);
  else strlcpy(out, display, n);
}
void drawKey(int x, int y, int w, int h, const char *t) {
  gfx->fillRoundRect(x, y, w, h, 4, COL_DIM);
  gfx->setTextColor(COL_TXT, COL_DIM);
  gfx->setTextSize(2);
  centre(t, x + w / 2, y + h / 2 - 8);
}
void drawRow(const char *s, int y) {
  int n = (int)strlen(s);
  for (int i = 0; i < n; i++) {
    char t[2] = {s[i], 0};
    drawKey(i * KW + 2, y, KW - 6, 40, t);
  }
}
void drawKeyboard(bool shift, bool sym) {
  const char *r0 = "1234567890";
  const char *r1 = sym ? "!@#$%^&*()" : (shift ? "QWERTYUIOP" : "qwertyuiop");
  const char *r2 = sym ? "-_=+[]{};'" : (shift ? "ASDFGHJKL" : "asdfghjkl");
  const char *r3 = sym ? ",.<>/?\\|~`" : (shift ? "ZXCVBNM.@-" : "zxcvbnm.@-");
  drawRow(r0, 200);
  drawRow(r1, 248);
  drawRow(r2, 296);
  drawRow(r3, 344);
  drawKey(722, 296, 70, 40, "DEL");
  chip(8, 392, 120, 36, shift ? "ABC" : "abc", shift);
  chip(140, 392, 400, 36, "SPACE", false);
  chip(552, 392, 120, 36, sym ? "ABC" : "!#", sym);
}
void drawPlace() {
  page = PG_PLACE;
  netHold = true;
  netPend = NET_NONE;
  gfx->fillScreen(COL_BG);
  chip(8, 8, 140, 36, "< BACK", false);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  if (!placeTyping && nHits > 0) {
    centre("TAP A PLACE", CX, 16);
    for (int i = 0; i < nHits; i++) chip(24, 70 + i * 78, 752, 66, hits[i].label, false);
    return;
  }
  centre("downloads pause here", CX, 16);
  chip(660, 8, 132, 36, "SEARCH", false);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(16, 70);
  gfx->print("CITY OR ZIP");
  gfx->fillRoundRect(120, 100, 664, 40, 6, COL_HOT);
  gfx->setTextColor(COL_BG, COL_HOT);
  gfx->setCursor(132, 110);
  gfx->print(placeQuery[0] ? placeQuery : "type a US place");
  if (placeMsg[0]) {
    gfx->setTextColor(COL_HOT, COL_BG);
    gfx->setCursor(16, 156);
    gfx->print(placeMsg);
  }
  drawKeyboard(placeShift, placeSym);
}
void drawWifiField(int focus, const char *raw, const char *empty, int y) {
  bool on = wifiFocus == focus;
  gfx->fillRoundRect(120, y, 664, 40, 6, on ? COL_HOT : COL_DIM);
  gfx->setTextColor(on ? COL_BG : COL_TXT, on ? COL_HOT : COL_DIM);
  gfx->setTextSize(2);
  gfx->setCursor(132, y + 10);
  gfx->print(raw[0] ? raw : empty);
}
void startWifiScan() {
  nWifiScan = 0;
  wifiScanTop = 0;
  wifiScanBusy = true;
  wifiScanWait = false;
  strlcpy(wifiMsg, "scanning", sizeof(wifiMsg));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  WiFi.scanDelete();
  WiFi.scanNetworks(true, false);
}
void drawWifi() {
  page = PG_WIFI;
  netHold = true;
  netPend = NET_NONE;
  gfx->fillScreen(COL_BG);
  chip(8, 8, 140, 36, "< BACK", false);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("WIFI", CX, 16);
  chip(660, 8, 132, 36, "SCAN", wifiScanBusy);
  chip(8, 52, 56, 36, "<", false);
  chip(736, 52, 56, 36, ">", false);
  for (int i = 0; i < 2; i++) {
    int idx = wifiScanTop + i;
    if (idx >= nWifiScan) break;
    char shown[18];
    strlcpy(shown, wifiScan[idx], sizeof(shown));
    chip(72 + i * 328, 52, 320, 36, shown, !strcmp(wifiScan[idx], wifiSsid));
  }
  if (wifiMsg[0] && nWifiScan == 0) {
    gfx->setTextColor(COL_HOT, COL_BG);
    gfx->setTextSize(2);
    centre(wifiMsg, CX, 56);
  }
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(2);
  gfx->setCursor(16, 110);
  gfx->print("SSID");
  drawWifiField(0, wifiSsid, "tap net or type", 100);
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(16, 158);
  gfx->print("PASS");
  drawWifiField(1, wifiPass, "type password", 148);
  drawKeyboard(wifiShift, wifiSym);
  chip(8, 436, 252, 36, "SAVE", true);
  chip(274, 436, 252, 36, "RESET WIFI", false);
  chip(540, 436, 252, 36, "FACTORY", false);
}
void openWifi() {
  wifiFocus = wifiSsid[0] ? 1 : 0;
  wifiMsg[0] = 0;
  if (!netBusy) startWifiScan();
  else wifiScanWait = true;
  drawWifi();
}
void wifiAppend(char c) {
  char *d = wifiFocus ? wifiPass : wifiSsid;
  int n = (int)strlen(d);
  if (n < 62) {
    d[n] = c;
    d[n + 1] = 0;
  }
}
void wifiDel() {
  char *d = wifiFocus ? wifiPass : wifiSsid;
  int n = (int)strlen(d);
  if (n) d[n - 1] = 0;
}
void placeAppend(char c) {
  int n = (int)strlen(placeQuery);
  if (n < 40) {
    placeQuery[n] = c;
    placeQuery[n + 1] = 0;
  }
}
void placeDel() {
  int n = (int)strlen(placeQuery);
  if (n) placeQuery[n - 1] = 0;
}
bool hitRow(int x, int y, const char *s, int y0) {
  if (y < y0 || y >= y0 + 44) return false;
  int i = x / KW;
  int n = (int)strlen(s);
  if (i < 0 || i >= n) return false;
  if (page == PG_PLACE) {
    placeAppend(s[i]);
    if (placeShift && s[i] >= 'A' && s[i] <= 'Z') placeShift = false;
  } else {
    wifiAppend(s[i]);
    if (wifiShift && s[i] >= 'A' && s[i] <= 'Z') wifiShift = false;
  }
  return true;
}
void leaveQuiet() {
  if (backTo == PG_SET) drawSet();
  else if (backTo == PG_MORE) drawMore();
  else drawHome();
}
void askGeo();
void choosePlace(int i);
void pollWifiScan() {
  if (wifiScanWait && !netBusy && page == PG_WIFI) startWifiScan();
  if (!wifiScanBusy) return;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  wifiScanBusy = false;
  if (n < 0) n = 0;
  nWifiScan = 0;
  for (int i = 0; i < n && nWifiScan < 16; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    strlcpy(wifiScan[nWifiScan], s.c_str(), 33);
    nWifiScan++;
  }
  WiFi.scanDelete();
  wifiScanTop = 0;
  if (nWifiScan) wifiMsg[0] = 0;
  else strlcpy(wifiMsg, "none found  type name", sizeof(wifiMsg));
  if (page == PG_WIFI) drawWifi();
}
void applyWifiSave() {
  if (!wifiSsid[0]) {
    strlcpy(wifiMsg, "need wifi name", sizeof(wifiMsg));
    drawWifi();
    return;
  }
  savePrefs();
  gfx->fillScreen(COL_BG);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("saved", CX, 160);
  centre(wifiSsid, CX, 200);
  centre("linking", CX, 240);
  delay(600);
  ESP.restart();
}
void clearWifi() {
  wifiSsid[0] = wifiPass[0] = 0;
  wifiOk = false;
  savePrefs();
}
void askGeo() {
  if (!placeQuery[0]) {
    strlcpy(placeMsg, "type a place", sizeof(placeMsg));
    drawPlace();
    return;
  }
  if (!wifiOk) {
    strlcpy(placeMsg, "join wifi", sizeof(placeMsg));
    drawPlace();
    return;
  }
  strlcpy(placeMsg, "searching", sizeof(placeMsg));
  drawPlace();
  netPend = NET_GEO;
}
void choosePlace(int i) {
  if (i < 0 || i >= nHits) return;
  placeGen++;
  strlcpy(placeName, hits[i].label, sizeof(placeName));
  placeLat = hits[i].lat;
  placeLon = hits[i].lon;
  placeSet = true;
  placeTz[0] = 0;
  wxOk = false;
  lastBody = 0;
  lastSun = 0;
  sunUp[0] = sunDown[0] = 0;
  satMoveAt = satHeavyAt = 0;
  savePrefs();
  netHold = false;
  nextPackAt = millis() + 600000UL;
  nextSatAt = millis();
  nextSpaceAt = millis();
  netPend = NET_PACK;
  drawHome();
}

void drawNightRow(int y, const char *name, const char *kind, int az, int el, const char *size, const char *dist,
                  const char *hrs) {
  char line[140];
  snprintf(line, sizeof(line), "%s   %s   %s", name, kind, (size && size[0]) ? size : "size unknown");
  gfx->setTextSize(2);
  gfx->setTextColor(el >= 0 ? COL_TXT : COL_MID, COL_BG);
  gfx->setCursor(16, y);
  gfx->print(line);
  gfx->setTextSize(1);
  gfx->setTextColor(COL_MID, COL_BG);
  snprintf(line, sizeof(line), "LOOK %s   %d deg   %s", lookWord(az), el, hrs ? hrs : "");
  gfx->setCursor(16, y + 18);
  gfx->print(line);
  if (dist && dist[0])
    snprintf(line, sizeof(line), "distance from Earth  %s", dist);
  else
    strlcpy(line, "distance from Earth unknown", sizeof(line));
  gfx->setCursor(16, y + 32);
  gfx->print(line);
  drawNamedPic(752, y + 4, 40, 40, name);
}
void drawTonight() {
  page = PG_NIGHT;
  netHold = false;
  fillNow();
  gfx->fillScreen(COL_BG);
  drawBack("TONIGHT");
  chip(620, 8, 168, 36, nightPage ? "PLANETS" : "STARS", false);
  time_t now = 0;
  if (!liveClock(&now) || !placeSet || !sunNow.ok) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(2);
    centre(placeSet ? "waiting for the clock" : "set a place", CX, 220);
    return;
  }
  int y = 52;
  char hrs[48], facts[128], spd[28];
  if (!nightPage) {
    double ra, dec;
    if (moonNow.ok && bodySky(-1, now, &ra, &dec) && y < 440) {
      skyHours(ra, dec, now, hrs, sizeof(hrs));
      fmtRange(moonNow.distAu, facts, sizeof(facts));
      strlcat(facts, "   speed about 2,300 mph", sizeof(facts));
      drawNightRow(y, "Moon", "moon", moonNow.az, moonNow.el, "about 2,160 miles across", facts, hrs);
      y += 48;
    }
    if (bodySky(0, now, &ra, &dec) && y < 440) {
      skyHours(ra, dec, now, hrs, sizeof(hrs));
      fmtRange(sunNow.distAu, facts, sizeof(facts));
      strlcat(facts, "   speed about 490,000 mph", sizeof(facts));
      drawNightRow(y, "Sun", "star", sunNow.az, sunNow.el, "about 865,000 miles across", facts, hrs);
      y += 48;
    }
    for (int i = 0; i < PLANET_N && y < 440; i++) {
      if (!planetNow[i].ok || !bodySky(i + 1, now, &ra, &dec)) continue;
      skyHours(ra, dec, now, hrs, sizeof(hrs));
      fmtRange(planetNow[i].distAu, facts, sizeof(facts));
      fmtMph(orbitMph(PLANETS[i].a), spd, sizeof(spd));
      int miles = (int)(PLANET_FACT[i].km * 0.621371f + 0.5f);
      char wide[80];
      if (miles >= 100000)
        snprintf(wide, sizeof(wide), "about %d thousand miles across", miles / 1000);
      else
        snprintf(wide, sizeof(wide), "about %d miles across", miles);
      if (spd[0]) {
        strlcat(facts, "   ", sizeof(facts));
        strlcat(facts, spd, sizeof(facts));
      }
      drawNightRow(y, PLANETS[i].name, "planet", planetNow[i].az, planetNow[i].el, wide, facts, hrs);
      y += 48;
    }
    return;
  }
  int id[5], magv[5], azs[5], els[5], nPick = 0;
  double ras[5], decs[5];
  for (int s = 0; s < STAR_N; s++) {
    int nm = STARS[s].name;
    if (nm < 0 || nm >= NAME_N || STARS[s].mag > 25) continue;
    double ra = (STARS[s].ra / 100.0) * 15.0 * DEG;
    double dec = (STARS[s].dec / 10.0) * DEG;
    int az, el;
    azelFromRaDec(ra, dec, now, &az, &el);
    if (el < 10) continue;
    int slot;
    if (nPick < 5) slot = nPick++;
    else {
      slot = 0;
      for (int k = 1; k < 5; k++)
        if (magv[k] > magv[slot]) slot = k;
      if (STARS[s].mag >= magv[slot]) continue;
    }
    id[slot] = nm;
    magv[slot] = STARS[s].mag;
    azs[slot] = az;
    els[slot] = el;
    ras[slot] = ra;
    decs[slot] = dec;
    if ((s & 63) == 0) delay(0);
  }
  for (int i = 0; i < nPick && y < 300; i++) {
    int best = 0;
    for (int k = 1; k < nPick; k++)
      if (magv[k] < magv[best]) best = k;
    skyHours(ras[best], decs[best], now, hrs, sizeof(hrs));
    fmtLy(STAR_NAME[id[best]].ly, facts, sizeof(facts));
    char wide[96] = "diameter unknown";
    for (int w = 0; w < WIDE_N; w++) {
      if (strcmp(STAR_WIDE[w].n, STAR_NAME[id[best]].n)) continue;
      int s10 = STAR_WIDE[w].suns10;
      if (s10 >= 100 && s10 % 10 == 0)
        snprintf(wide, sizeof(wide), "diameter about %d times as wide as the sun", s10 / 10);
      else if (s10 >= 10)
        snprintf(wide, sizeof(wide), "diameter about %d.%d times as wide as the sun", s10 / 10, s10 % 10);
      break;
    }
    drawNightRow(y, STAR_NAME[id[best]].n, "star", azs[best], els[best], wide, facts, hrs);
    magv[best] = 99;
    y += 48;
  }
  for (int i = 0; i < ROCK_N && y < 400; i++) {
    if (!rockNow[i].ok || rockNow[i].el < 8) continue;
    fmtRange(rockNow[i].distAu, facts, sizeof(facts));
    char wide[48];
    int miles = 0;
    if (!strcmp(ROCKS[i].name, "Ceres")) miles = 590;
    else if (!strcmp(ROCKS[i].name, "Vesta")) miles = 326;
    else if (!strcmp(ROCKS[i].name, "Pallas")) miles = 318;
    else if (!strcmp(ROCKS[i].name, "Juno")) miles = 160;
    if (miles)
      snprintf(wide, sizeof(wide), "about %d miles across", miles);
    else
      strlcpy(wide, "size unknown", sizeof(wide));
    if (!strcmp(ROCKS[i].kind, "asteroid")) {
      fmtMph(orbitMph(ROCKS[i].a), spd, sizeof(spd));
      if (spd[0]) {
        strlcat(facts, "   ", sizeof(facts));
        strlcat(facts, spd, sizeof(facts));
      }
    }
    const char *extra = strcmp(ROCKS[i].kind, "comet") ? ROCKS[i].note : "";
    drawNightRow(y, ROCKS[i].name, ROCKS[i].kind, rockNow[i].az, rockNow[i].el, wide, facts, extra);
    y += 48;
  }
  for (int i = 0; i < DEEP_N && y < 440; i++) {
    double ra = DEEP[i].raH * 15.0 * DEG;
    double dec = DEEP[i].dec * DEG;
    int az, el;
    azelFromRaDec(ra, dec, now, &az, &el);
    if (el < 8) continue;
    skyHours(ra, dec, now, hrs, sizeof(hrs));
    fmtLy(DEEP[i].ly, facts, sizeof(facts));
    const char *wide = DEEP[i].count[0] ? DEEP[i].count : "size unknown";
    const char *kind = "deep sky";
    if (strstr(DEEP[i].note, "galaxy") || strstr(DEEP[i].note, "Magellanic")) kind = "galaxy";
    else if (strstr(DEEP[i].note, "nebula")) kind = "nebula";
    else if (strstr(DEEP[i].note, "cluster")) kind = "cluster";
    drawNightRow(y, DEEP[i].name, kind, az, el, wide, facts, hrs);
    y += 48;
  }
}
void drawBody() {
  page = PG_BODY;
  netHold = false;
  fillNow();
  gfx->fillScreen(COL_BG);
  const BodyFact *f = &SUN_FACT;
  const char *name = "SUN";
  NowBody *nb = &sunNow;
  int which = 0;
  if (bodyPick > 0 && bodyPick <= PLANET_N) {
    f = &PLANET_FACT[bodyPick - 1];
    name = PLANETS[bodyPick - 1].name;
    nb = &planetNow[bodyPick - 1];
    which = bodyPick;
  }
  drawBack(name);
  chip(652, 8, 140, 36, "MORE", false);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  char line[110];
  int miles = (int)(f->km * 0.621371f + 0.5f);
  if (miles >= 100000)
    snprintf(line, sizeof(line), "about %d thousand miles across", miles / 1000);
  else
    snprintf(line, sizeof(line), "about %d miles across", miles);
  gfx->setCursor(16, 52);
  gfx->print(line);
  snprintf(line, sizeof(line), "made of %s", f->made);
  gfx->setCursor(16, 74);
  gfx->print(line);
  snprintf(line, sizeof(line), "air   %s", f->air);
  gfx->setCursor(16, 96);
  gfx->print(line);
  snprintf(line, sizeof(line), "moons   %s", f->moons);
  gfx->setCursor(16, 118);
  gfx->print(line);
  snprintf(line, sizeof(line), "day length  %s", f->dayLen);
  gfx->setCursor(16, 140);
  gfx->print(line);
  snprintf(line, sizeof(line), "year length  %s", f->yearLen);
  gfx->setCursor(16, 162);
  gfx->print(line);
  gfx->setCursor(16, 184);
  gfx->print(f->temps);
  if (which > 0) {
    char spd[28];
    fmtMph(orbitMph(PLANETS[which - 1].a), spd, sizeof(spd));
    if (spd[0]) snprintf(line, sizeof(line), "speed  %s", spd);
    else
      strlcpy(line, "speed  unknown", sizeof(line));
  } else
    strlcpy(line, "speed  about 490,000 mph", sizeof(line));
  gfx->setCursor(16, 206);
  gfx->print(line);
  if (f->more && f->more[0]) {
    gfx->setTextSize(1);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(16, 228);
    gfx->print(f->more);
    gfx->setTextSize(2);
  }
  if (nb->ok) {
    snprintf(line, sizeof(line), "LOOK %s   %s   %d deg", lookWord(nb->az), nb->el >= 0 ? "up" : "set", nb->el);
    gfx->setTextColor(COL_HOT, COL_BG);
    gfx->setCursor(16, 258);
    gfx->print(line);
    fmtRange(nb->distAu, line, sizeof(line));
    gfx->setTextColor(COL_TXT, COL_BG);
    gfx->setCursor(16, 282);
    char earth[140];
    snprintf(earth, sizeof(earth), "distance from Earth  %s", line);
    gfx->print(earth);
    time_t now;
    if (liveClock(&now)) {
      double ra, dec;
      char hrs[48];
      if (bodySky(which, now, &ra, &dec)) {
        skyHours(ra, dec, now, hrs, sizeof(hrs));
        gfx->setCursor(16, 306);
        gfx->print(hrs);
      }
    }
  }
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setCursor(16, 400);
  gfx->print("Day, year, temperatures, and speed are rounded.");
  gfx->setCursor(16, 424);
  gfx->print("The distance is worked out here and is approximate.");
  drawNamedPic(590, 56, 190, 190, which > 0 ? name : "Sun");
}
void drawMeteor() {
  page = PG_METEOR;
  netHold = false;
  if (meteorPick < 0 || meteorPick >= SHOWER_N) meteorPick = 0;
  const Shower &s = SHOWERS[meteorPick];
  gfx->fillScreen(COL_BG);
  drawBack("METEOR SHOWER");
  gfx->setTextSize(3);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setCursor(24, 60);
  gfx->print(s.name);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  char line[96];
  snprintf(line, sizeof(line), "peaks %s %d, give or take %d nights", MON[s.month - 1], s.day, s.span);
  gfx->setCursor(24, 108);
  gfx->print(line);
  snprintf(line, sizeof(line), "about %d an hour at the peak, under a dark sky", s.zhr);
  gfx->setCursor(24, 144);
  gfx->print(line);
  snprintf(line, sizeof(line), "from %s", s.parent);
  gfx->setCursor(24, 188);
  gfx->print(line);
  gfx->setCursor(24, 216);
  gfx->print(s.fact);
  gfx->setCursor(24, 252);
  gfx->print("dust and gravel along that orbit");
  gfx->setCursor(24, 280);
  gfx->print("there is no fixed count of pieces");
  snprintf(line, sizeof(line), "hits the air at %d km/s", s.speed);
  gfx->setCursor(24, 316);
  gfx->print(line);
  time_t now;
  if (liveClock(&now) && placeTz[0]) {
    ViewWin w;
    meteorView(s, now, &w);
    gfx->setTextColor(COL_HOT, COL_BG);
    if (w.ok) {
      snprintf(line, sizeof(line), "LOOK %s   %d deg at %s", lookWord(w.az), w.el, w.high);
      gfx->setCursor(24, 360);
      gfx->print(line);
      snprintf(line, sizeof(line), "dark window  %s", w.span);
      gfx->setTextColor(COL_TXT, COL_BG);
      gfx->setCursor(24, 396);
      gfx->print(line);
    } else {
      gfx->setCursor(24, 360);
      gfx->print(w.span);
    }
  }
  drawNamedPic(600, 56, 180, 180, s.name);
}
void drawSpace() {
  page = PG_SPACE;
  netHold = true;
  netPend = NET_NONE;
  if (netBusy) {
    spaceDirty = true;
    gfx->fillScreen(COL_BG);
    drawBack("SPACE WEATHER");
    gfx->setTextSize(2);
    gfx->setTextColor(COL_MID, COL_BG);
    centre("one moment", CX, 220);
    return;
  }
  updateSunPos();
  fillNow();
  gfx->fillScreen(COL_BG);
  drawBack("SPACE WEATHER");
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  char line[110];
  if (windSpeed > 0 && windBt >= 0 && windBz > -100)
    snprintf(line, sizeof(line), "solar wind  %d km/s    Bt %d    Bz %d", windSpeed, windBt, windBz);
  else if (windSpeed > 0)
    snprintf(line, sizeof(line), "solar wind  %d km/s", windSpeed);
  else
    strlcpy(line, "solar wind  waiting", sizeof(line));
  gfx->setCursor(16, 48);
  gfx->print(line);
  if (flareNow[0])
    snprintf(line, sizeof(line), "flare  now %s   peak %s  %s", flareNow, flareMax[0] ? flareMax : "--",
             flareAt[0] ? flareAt : "");
  else
    strlcpy(line, "flare  waiting", sizeof(line));
  gfx->setCursor(16, 70);
  gfx->print(line);
  snprintf(line, sizeof(line), "storms now  %s    today  %s", stormNow[0] ? stormNow : "--",
           stormDay[0] ? stormDay : "--");
  gfx->setCursor(16, 92);
  gfx->print(line);
  if (flareProb < 0 && radProb < 0)
    strlcpy(line, "flare chance  --", sizeof(line));
  else if (flareProb >= 0 && radProb >= 0)
    snprintf(line, sizeof(line), "flare chance %d%%    radiation %d%%", flareProb, radProb);
  else if (flareProb >= 0)
    snprintf(line, sizeof(line), "flare chance %d%%", flareProb);
  else
    snprintf(line, sizeof(line), "radiation %d%%", radProb);
  gfx->setCursor(16, 114);
  gfx->print(line);
  if (sunNow.ok)
    snprintf(line, sizeof(line), "Sun  LOOK %s  %s %d deg", lookWord(sunNow.az), sunNow.el >= 0 ? "up" : "set",
             sunNow.el);
  else
    strlcpy(line, "Sun  waiting for the clock", sizeof(line));
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setCursor(16, 136);
  gfx->print(line);
  if (kpNextN > 0) {
    gfx->setTextSize(1);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(16, 160);
    gfx->print("Kp storm number");
    gfx->setTextSize(2);
    for (int i = 0; i < kpNextN && i < 8; i++) {
      int bh = kpNext[i] * 4 / 10;
      if (bh < 2) bh = 2;
      int x = 150 + i * 78;
      gfx->fillRect(x, 184 - bh, 40, bh, COL_HOT);
      gfx->setTextSize(1);
      gfx->setTextColor(COL_TXT, COL_BG);
      char num[8];
      snprintf(num, sizeof(num), "%d.%d", kpNext[i] / 10, kpNext[i] % 10);
      gfx->setCursor(x, 186);
      gfx->print(num);
      gfx->setTextSize(2);
    }
  }
  int mapTop = 204;
  int mapH = 200;
  if (worldReady) drawWorldBand(mapTop, mapH);
  if (auroraKp >= 0) drawAuroraOval(mapTop, mapH, auroraEdge(auroraKp));
  if (placeSet) gfx->fillCircle(mapX(placeLon), mapYAt(placeLat, mapTop, mapH), 5, COL_PINK);
  int sx = mapX(sunLon);
  int sy = mapYAt(sunLat, mapTop, mapH);
  if (sunOk && sy >= mapTop && sy < mapTop + mapH) {
    drawSunMark(sx, sy);
    gfx->setTextSize(1);
    gfx->setTextColor(COL_HOT, RGB565(8, 14, 28));
    gfx->setCursor(sx + 22, sy - 4);
    gfx->print("Sun");
    gfx->setTextSize(2);
  }
  gfx->fillRect(0, 408, PW, 72, COL_BG);
  gfx->setTextSize(1);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setCursor(12, 412);
  gfx->print("Yellow lines across the map are the edge of the northern lights and the southern lights.");
  gfx->setCursor(12, 426);
  gfx->print("Kp is the aurora storm number. 0 quiet, 9 extreme. About 4 unsettled, 5 active, 7 severe.");
  gfx->setTextColor(COL_TXT, COL_BG);
  if (auroraKp < 0 || !placeSet)
    strlcpy(line, "The lines wait for a Kp number and a place.", sizeof(line));
  else {
    float g = gmlatOf(placeLat, placeLon);
    float edge = auroraEdge(auroraKp);
    float sun = 99;
    time_t nowt;
    if (liveClock(&nowt)) sunAlt(nowt, &sun);
    const char *look = placeLat >= 0 ? "north" : "south";
    if (fabsf(g) + 1.0f >= edge && sun < -6)
      snprintf(line, sizeof(line), "This place is inside that edge. Look %s if the sky is clear.", look);
    else if (fabsf(g) + 1.0f >= edge)
      snprintf(line, sizeof(line), "This place is inside that edge. Look %s after dark.", look);
    else
      snprintf(line, sizeof(line), "This place is outside the edge for Kp %d. Look %s only on a stronger night.",
               auroraKp, look);
  }
  gfx->setCursor(12, 448);
  gfx->print(line);
  spaceDirty = false;
}

void drawCraft() {
  page = PG_CRAFT;
  netHold = false;
  gfx->fillScreen(COL_BG);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  char line[96];
  if (factKind == 0) {
    int i = factId;
    if (i < 0 || i >= SAT_N) i = 0;
    drawBack(SATS[i].name);
    if (i < 3) {
      gfx->setCursor(24, 64);
      gfx->print(CRAFT_FACT[i].what);
      gfx->setCursor(24, 100);
      gfx->print(CRAFT_FACT[i].size);
      snprintf(line, sizeof(line), "made of %s", CRAFT_FACT[i].made);
      gfx->setCursor(24, 136);
      gfx->print(line);
      gfx->setTextColor(COL_MID, COL_BG);
      gfx->setCursor(24, 172);
      gfx->print(CRAFT_FACT[i].note);
    }
    gfx->setTextColor(COL_HOT, COL_BG);
    if (satLive[i].ok) {
      int miles = (int)(satLive[i].alt * 0.621371f);
      int mph = satMph(i);
      if (mph)
        snprintf(line, sizeof(line), "%s   LOOK %s   %d miles up   speed about %d mph", satLive[i].vis ? "up now" : "not up",
                 lookWord(satLive[i].az), miles, mph);
      else
        snprintf(line, sizeof(line), "%s   LOOK %s   %d miles up", satLive[i].vis ? "up now" : "not up",
                 lookWord(satLive[i].az), miles);
      gfx->setCursor(24, 220);
      gfx->print(line);
    }
    if (i == 0) {
      gfx->setTextSize(1);
      gfx->setTextColor(COL_TXT, COL_BG);
      gfx->setCursor(24, 250);
      if (nIssCrew > 0) {
        char crew[40];
        snprintf(crew, sizeof(crew), "ISS onboard crew - %d", nIssCrew);
        gfx->print(crew);
        char a[110] = "", b[110] = "";
        for (int c = 0; c < nIssCrew; c++) {
          char bit[44];
          if (issNation[c][0])
            snprintf(bit, sizeof(bit), "%s, %s", issCrew[c], issNation[c]);
          else
            snprintf(bit, sizeof(bit), "%s", issCrew[c]);
          char *dst = (strlen(a) + strlen(bit) + 2 < sizeof(a)) ? a : b;
          if (dst[0]) strlcat(dst, "   ", sizeof(a));
          strlcat(dst, bit, sizeof(a));
        }
        gfx->setCursor(24, 264);
        gfx->print(a);
        if (b[0]) {
          gfx->setCursor(24, 278);
          gfx->print(b);
        }
      } else
        gfx->print("ISS onboard crew waiting");
      gfx->setTextSize(2);
    }
    int py = (i == 0) ? 310 : 270;
    if (satLive[i].nPass) {
      gfx->setTextColor(COL_TXT, COL_BG);
      gfx->setCursor(24, py);
      gfx->print(satLive[i].pass[0]);
      if (satLive[i].nPass > 1) {
        gfx->setCursor(24, py + 32);
        gfx->print(satLive[i].pass[1]);
      }
    } else if (satLive[i].when[0]) {
      gfx->setCursor(24, py);
      gfx->print(satLive[i].when);
    }
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(24, 360);
    gfx->print("The path on the map is one orbit from the satellite file.");
    gfx->setCursor(24, 396);
    gfx->print("Size lines are rounded. Height is from the live path.");
    crewDirty = false;
    drawNamedPic(600, 64, 180, 180, SATS[i].name);
    return;
  }
  if (factKind == 2) {
    int i = factId;
    if (i < 0 || i >= SCOPE_N) i = 0;
    const ScopeFact &S = SCOPES[i];
    drawBack(S.name);
    chip(652, 8, 140, 36, "MORE", false);
    gfx->setTextSize(1);
    gfx->setTextColor(COL_TXT, COL_BG);
    gfx->setCursor(16, 56);
    gfx->print(S.launched);
    int sy = 78;
    if (S.svc1[0]) {
      gfx->setCursor(16, sy);
      gfx->print(S.svc1);
      sy += 22;
    }
    if (S.svc2[0]) {
      gfx->setCursor(16, sy);
      gfx->print(S.svc2);
      sy += 22;
    }
    if (S.svc3[0]) {
      gfx->setCursor(16, sy);
      gfx->print(S.svc3);
      sy += 22;
    }
    if (S.svc4[0]) {
      gfx->setCursor(16, sy);
      gfx->print(S.svc4);
      sy += 22;
    }
    gfx->setCursor(16, sy + 8);
    snprintf(line, sizeof(line), "distance from Earth  %s", S.where);
    gfx->print(line);
    gfx->setCursor(16, sy + 30);
    gfx->print(S.mirror);
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(16, sy + 52);
    gfx->print(S.glass);
    gfx->setTextColor(COL_TXT, COL_BG);
    if (S.satIx >= 0 && S.satIx < SAT_N && satLive[S.satIx].ok) {
      int miles = (int)(satLive[S.satIx].alt * 0.621371f);
      snprintf(line, sizeof(line), "live height  %d miles   LOOK %s", miles, lookWord(satLive[S.satIx].az));
      gfx->setTextColor(COL_HOT, COL_BG);
      gfx->setCursor(16, sy + 74);
      gfx->print(line);
    }
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setCursor(16, 430);
    gfx->print("Mirror size and dates are rounded. No single magnification.");
    drawNamedPic(600, 56, 180, 180, S.name);
    return;
  }
  drawBack("ROCKET");
  if (factId < 0 || factId >= nLaunch) {
    centre("no launch picked", CX, 220);
    return;
  }
  const Launch &L = launches[factId];
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setCursor(24, 60);
  gfx->print(L.rocket[0] ? L.rocket : "rocket");
  gfx->setTextColor(COL_TXT, COL_BG);
  snprintf(line, sizeof(line), "%s    %s", L.day, L.when);
  gfx->setCursor(24, 100);
  gfx->print(line);
  if (L.mission[0]) {
    gfx->setCursor(24, 132);
    gfx->print(L.mission);
  }
  gfx->setCursor(24, 168);
  gfx->print(L.pad[0] ? L.pad : "pad not listed");
  snprintf(line, sizeof(line), "%s%s%s", L.agency[0] ? L.agency : "", L.status[0] ? "   " : "", L.status);
  gfx->setCursor(24, 200);
  gfx->print(line);
  snprintf(line, sizeof(line), "%s", L.payload[0] ? L.payload : "unknown payload");
  gfx->setCursor(24, 224);
  gfx->print(line);
  if (L.going[0]) {
    if (!strcmp(L.going, "not listed"))
      strlcpy(line, "crew flight, destination unknown", sizeof(line));
    else
      snprintf(line, sizeof(line), "crew flight, going to %s", L.going);
    gfx->setTextColor(COL_HOT, COL_BG);
    gfx->setCursor(24, 248);
    gfx->print(line);
  }
  const RocketFact *rf = nullptr;
  for (int i = 0; i < (int)(sizeof(ROCKET_FACT) / sizeof(ROCKET_FACT[0])); i++) {
    if (strstr(L.rocket, ROCKET_FACT[i].key)) {
      rf = &ROCKET_FACT[i];
      break;
    }
  }
  gfx->setTextColor(COL_MID, COL_BG);
  gfx->setTextSize(1);
  if (rf) {
    gfx->setCursor(24, 270);
    gfx->print(rf->tall);
    gfx->setCursor(24, 286);
    gfx->print(rf->filled);
    gfx->setCursor(24, 302);
    gfx->print(rf->speed);
    gfx->setCursor(24, 318);
    gfx->print(rf->stage1);
    gfx->setCursor(24, 334);
    gfx->print(rf->stage2);
  } else {
    gfx->setCursor(24, 270);
    gfx->print("vehicle height and filled weight are not stored for this rocket");
  }
  if (L.owner[0])
    snprintf(line, sizeof(line), "owner  %s", L.owner);
  else
    strlcpy(line, "owner unknown", sizeof(line));
  gfx->setCursor(24, 358);
  gfx->print(line);
  payloadMeasure(L, line, sizeof(line));
  gfx->setCursor(24, 374);
  gfx->print(line);
  gfx->setCursor(24, 390);
  gfx->print("planned orbit height unknown");
  gfx->setTextSize(2);
  gfx->setCursor(24, 400);
  gfx->print("The time is for your place. Sizes are rounded.");
  drawNamedPic(600, 56, 180, 180, L.rocket);
}

char foldCh(char c) {
  if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
  return c;
}
bool foldHas(const char *hay, const char *needle) {
  if (!hay || !needle || !needle[0]) return false;
  for (int i = 0; hay[i]; i++) {
    int j = 0;
    while (needle[j] && hay[i + j] && foldCh(hay[i + j]) == foldCh(needle[j])) j++;
    if (!needle[j]) return true;
  }
  return false;
}
bool foldStart(const char *hay, const char *needle) {
  if (!hay || !needle || !needle[0]) return false;
  int j = 0;
  while (needle[j] && hay[j] && foldCh(hay[j]) == foldCh(needle[j])) j++;
  return !needle[j];
}
const PicRow *picBy(uint8_t src, int id) {
  for (int i = 0; i < PIC_N; i++) {
    if (PICS[i].src == src && PICS[i].id == id) return &PICS[i];
  }
  return nullptr;
}
void findAdd(uint8_t src, int id) {
  for (int i = 0; i < nFind; i++) {
    if (findSrc[i] == src && findId[i] == id) return;
  }
  if (nFind >= 4) {
    nFindMore = 1;
    return;
  }
  findSrc[nFind] = src;
  findId[nFind] = id;
  nFind++;
}
void findScan() {
  nFind = 0;
  nFindMore = 0;
  if (!findQ[0]) return;
  bool wide = strlen(findQ) >= 4;
  for (int pass = 1; pass <= 2; pass++) {
    for (int i = 0; i < PIC_N; i++) {
      bool ok = pass == 1 ? foldStart(PICS[i].name, findQ)
                          : (foldHas(PICS[i].name, findQ) ||
                             (wide && (foldHas(PICS[i].kind, findQ) || (PICS[i].also[0] && foldHas(PICS[i].also, findQ)))));
      if (ok) findAdd(PICS[i].src, PICS[i].id);
    }
    for (int i = 0; i < NAME_N; i++) {
      bool ok = pass == 1 ? foldStart(STAR_NAME[i].n, findQ)
                          : (foldHas(STAR_NAME[i].n, findQ) || (wide && strlen(findQ) == 4 && foldHas(findQ, "star")));
      if (ok) findAdd(PIC_STAR, i);
    }
  }
}
void findPush(char c) {
  int n = (int)strlen(findQ);
  if (n + 1 >= (int)sizeof(findQ)) return;
  findQ[n] = c;
  findQ[n + 1] = 0;
  findScan();
}
void findPop() {
  int n = (int)strlen(findQ);
  if (n) findQ[n - 1] = 0;
  findScan();
}
bool findHitKey(int x, int y, const char *s, int y0) {
  if (y < y0 || y >= y0 + 40) return false;
  int i = x / KW;
  int n = (int)strlen(s);
  if (i < 0 || i >= n) return false;
  findPush(s[i]);
  return true;
}
void cardLine(const char *s) {
  if (!s || !s[0] || cardY > 440) return;
  gfx->setTextSize(2);
  gfx->setTextColor(COL_TXT, COL_BG);
  gfx->setCursor(16, cardY);
  gfx->print(s);
  cardY += 22;
}
void cardPhoto() {
  if (cardSrc == PIC_STAR) {
    if (cardId >= 0 && cardId < NAME_N) drawSdPic(560, 52, 220, 220, "stars", STAR_NAME[cardId].n);
    return;
  }
  const PicRow *P = picBy(cardSrc, cardId);
  if (P) drawSdPic(560, 52, 220, 220, P->dir, P->file);
}
bool cardTxt(const char *dir, const char *file) {
  if (!sdOk || !dir || !file || !file[0]) return false;
  char path[84];
  snprintf(path, sizeof(path), "/%s/%s.txt", dir, file);
  if (!SD_MMC.exists(path)) {
    snprintf(path, sizeof(path), "/catalogue/%s/%s.txt", dir, file);
    if (!SD_MMC.exists(path)) return false;
  }
  File f = SD_MMC.open(path, "r");
  if (!f) return false;
  char line[120];
  int n = 0;
  int shown = 0;
  while (f.available() && cardY <= 440) {
    int c = f.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c == '\n') {
      line[n] = 0;
      if (n) {
        cardLine(line);
        shown++;
      }
      n = 0;
      continue;
    }
    if (n < (int)sizeof(line) - 1) line[n++] = (char)c;
  }
  if (n && cardY <= 440) {
    line[n] = 0;
    cardLine(line);
    shown++;
  }
  f.close();
  return shown > 0;
}
void drawFind() {
  page = PG_FIND;
  netHold = false;
  gfx->fillScreen(COL_BG);
  drawBack("SEARCH");
  chip(652, 8, 140, 36, "HOME", false);
  gfx->fillRoundRect(16, 52, 768, 40, 6, COL_HOT);
  gfx->setTextColor(COL_BG, COL_HOT);
  gfx->setTextSize(2);
  gfx->setCursor(28, 62);
  gfx->print(findQ[0] ? findQ : "type a name");
  if (!findQ[0]) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(2);
    gfx->setCursor(16, 108);
    gfx->print("planet, star, nebula, rocket, moon, telescope");
  } else if (!nFind) {
    gfx->setTextColor(COL_MID, COL_BG);
    gfx->setTextSize(2);
    gfx->setCursor(16, 108);
    gfx->print("nothing stored under that name");
  } else {
    for (int i = 0; i < nFind; i++) {
      char lab[48];
      const char *nm = "object";
      const char *kd = "";
      if (findSrc[i] == PIC_STAR && findId[i] >= 0 && findId[i] < NAME_N) {
        nm = STAR_NAME[findId[i]].n;
        kd = "star";
      } else {
        const PicRow *P = picBy(findSrc[i], findId[i]);
        if (P) {
          nm = P->name;
          kd = P->kind;
        }
      }
      snprintf(lab, sizeof(lab), "%s   %s", nm, kd);
      chip(16, 100 + i * 42, 768, 38, lab, false);
    }
    if (nFindMore) {
      gfx->setTextSize(1);
      gfx->setTextColor(COL_MID, COL_BG);
      gfx->setCursor(16, 268);
      gfx->print("more matches, keep typing");
    }
  }
  drawRow("1234567890", 284);
  drawRow("qwertyuiop", 320);
  drawRow("asdfghjkl", 356);
  drawRow("zxcvbnm", 392);
  drawKey(722, 392, 70, 40, "DEL");
  chip(8, 436, 144, 36, "CLEAR", false);
  chip(160, 436, 400, 36, "SPACE", false);
}
void drawCard() {
  page = PG_CARD;
  netHold = false;
  fillNow();
  gfx->fillScreen(COL_BG);
  const char *title = "FACTS";
  if (cardSrc == PIC_STAR && cardId >= 0 && cardId < NAME_N) title = STAR_NAME[cardId].n;
  else {
    const PicRow *P = picBy(cardSrc, cardId);
    if (P) title = P->name;
  }
  drawBack(title);
  chip(652, 8, 140, 36, "HOME", false);
  cardY = 56;
  char line[140];
  if (cardSrc == PIC_STAR && cardId >= 0 && cardId < NAME_N) {
    fmtLy(STAR_NAME[cardId].ly, line, sizeof(line));
    cardLine(line[0] ? line : "distance unknown");
    for (int w = 0; w < WIDE_N; w++) {
      if (strcmp(STAR_WIDE[w].n, STAR_NAME[cardId].n)) continue;
      int s10 = STAR_WIDE[w].suns10;
      if (s10 >= 100) snprintf(line, sizeof(line), "about %d suns across", s10 / 10);
      else if (s10 >= 10) snprintf(line, sizeof(line), "about %d.%d suns across", s10 / 10, s10 % 10);
      else line[0] = 0;
      cardLine(line);
      break;
    }
    time_t nowt = 0;
    if (liveClock(&nowt) && placeSet) {
      for (int s = 0; s < STAR_N; s++) {
        if (STARS[s].name != (int8_t)cardId) continue;
        char mag[12];
        fmtMag10(STARS[s].mag, mag, sizeof(mag));
        double ra = (STARS[s].ra / 100.0) * 15.0 * DEG;
        double dec = (STARS[s].dec / 10.0) * DEG;
        int az = 0, el = 0;
        azelFromRaDec(ra, dec, nowt, &az, &el);
        snprintf(line, sizeof(line), "mag %s   LOOK %s   %d deg", mag, lookWord(az), el);
        cardLine(line);
        char hrs[48];
        skyHours(ra, dec, nowt, hrs, sizeof(hrs));
        cardLine(hrs);
        break;
      }
    }
  } else if (cardSrc == PIC_PLANET && cardId >= 0 && cardId < PLANET_N) {
    const BodyFact *f = &PLANET_FACT[cardId];
    int miles = (int)(f->km * 0.621371f + 0.5f);
    if (miles >= 100000) snprintf(line, sizeof(line), "about %d thousand miles across", miles / 1000);
    else snprintf(line, sizeof(line), "about %d miles across", miles);
    cardLine(line);
    snprintf(line, sizeof(line), "made of %s", f->made);
    cardLine(line);
    snprintf(line, sizeof(line), "air   %s", f->air);
    cardLine(line);
    snprintf(line, sizeof(line), "moons   %s", f->moons);
    cardLine(line);
    snprintf(line, sizeof(line), "day length  %s", f->dayLen);
    cardLine(line);
    snprintf(line, sizeof(line), "year length  %s", f->yearLen);
    cardLine(line);
    cardLine(f->temps);
    if (f->more && f->more[0]) cardLine(f->more);
    char spd[28];
    fmtMph(orbitMph(PLANETS[cardId].a), spd, sizeof(spd));
    if (spd[0]) snprintf(line, sizeof(line), "speed  %s", spd);
    else strlcpy(line, "speed  unknown", sizeof(line));
    cardLine(line);
    if (planetNow[cardId].ok) {
      snprintf(line, sizeof(line), "LOOK %s   %s   %d deg", lookWord(planetNow[cardId].az),
               planetNow[cardId].el >= 0 ? "up" : "set", planetNow[cardId].el);
      cardLine(line);
      fmtRange(planetNow[cardId].distAu, line, sizeof(line));
      char earth[140];
      snprintf(earth, sizeof(earth), "distance from Earth  %s", line);
      cardLine(earth);
    }
    cardLine("Day, year, temperatures, and speed are rounded.");
    cardLine("The distance is worked out here and is approximate.");
  } else if (cardSrc == PIC_MOON) {
    float phase = 0;
    int pct = 0;
    const char *mn = moonName(&phase, &pct);
    if (mn) {
      snprintf(line, sizeof(line), "%s    %d%% lit", mn, pct);
      cardLine(line);
    }
    cardLine("highlands are anorthosite, dark plains are basalt");
    cardLine("small iron core, about 2,160 miles across, almost no air");
    if (moonNow.ok) {
      snprintf(line, sizeof(line), "LOOK %s   %s   %d deg", lookWord(moonNow.az), moonNow.el >= 0 ? "up" : "set",
               moonNow.el);
      cardLine(line);
      fmtRange(moonNow.distAu, line, sizeof(line));
      char earth[140];
      snprintf(earth, sizeof(earth), "distance from Earth  %s", line);
      cardLine(earth);
    }
  } else if (cardSrc == PIC_SUN) {
    const BodyFact *f = &SUN_FACT;
    int miles = (int)(f->km * 0.621371f + 0.5f);
    snprintf(line, sizeof(line), "about %d thousand miles across", miles / 1000);
    cardLine(line);
    snprintf(line, sizeof(line), "made of %s", f->made);
    cardLine(line);
    snprintf(line, sizeof(line), "air   %s", f->air);
    cardLine(line);
    snprintf(line, sizeof(line), "moons   %s", f->moons);
    cardLine(line);
    snprintf(line, sizeof(line), "day length  %s", f->dayLen);
    cardLine(line);
    snprintf(line, sizeof(line), "year length  %s", f->yearLen);
    cardLine(line);
    cardLine(f->temps);
    if (f->more && f->more[0]) cardLine(f->more);
    cardLine("speed  about 490,000 mph");
    if (sunNow.ok) {
      fmtRange(sunNow.distAu, line, sizeof(line));
      char earth[140];
      snprintf(earth, sizeof(earth), "distance from Earth  %s", line);
      cardLine(earth);
    }
    cardLine("Day, year, temperatures, and speed are rounded.");
    cardLine("The distance is worked out here and is approximate.");
  } else if (cardSrc == PIC_DEEP && cardId >= 0 && cardId < DEEP_N) {
    snprintf(line, sizeof(line), "%s", DEEP[cardId].note);
    cardLine(line);
    cardLine(DEEP[cardId].count);
    fmtLy(DEEP[cardId].ly, line, sizeof(line));
    if (line[0]) {
      char earth[140];
      snprintf(earth, sizeof(earth), "distance from Earth  %s", line);
      cardLine(earth);
    }
    time_t nowt = 0;
    if (liveClock(&nowt) && placeSet) {
      double ra = DEEP[cardId].raH * 15.0 * DEG;
      double dec = DEEP[cardId].dec * DEG;
      int az = 0, el = 0;
      azelFromRaDec(ra, dec, nowt, &az, &el);
      snprintf(line, sizeof(line), "LOOK %s   %d deg", lookWord(az), el);
      cardLine(line);
      char hrs[48];
      skyHours(ra, dec, nowt, hrs, sizeof(hrs));
      cardLine(hrs);
    }
  } else if (cardSrc == PIC_ROCK && cardId >= 0 && cardId < ROCK_N) {
    snprintf(line, sizeof(line), "%s", ROCKS[cardId].kind);
    cardLine(line);
    cardLine(ROCKS[cardId].note);
    if (!strcmp(ROCKS[cardId].kind, "asteroid")) {
      snprintf(line, sizeof(line), "made of %s", asteroidMade(ROCKS[cardId].name));
      cardLine(line);
      char spd[28];
      fmtMph(orbitMph(ROCKS[cardId].a), spd, sizeof(spd));
      if (spd[0]) {
        snprintf(line, sizeof(line), "speed %s", spd);
        cardLine(line);
      }
    } else
      cardLine("needs a telescope");
    if (rockNow[cardId].ok) {
      snprintf(line, sizeof(line), "LOOK %s   %s   %d deg", lookWord(rockNow[cardId].az),
               rockNow[cardId].el >= 0 ? "up" : "set", rockNow[cardId].el);
      cardLine(line);
      fmtRange(rockNow[cardId].distAu, line, sizeof(line));
      char earth[140];
      snprintf(earth, sizeof(earth), "distance from Earth  %s", line);
      cardLine(earth);
    }
  } else if (cardSrc == PIC_SHOWER && cardId >= 0 && cardId < SHOWER_N) {
    const Shower &s = SHOWERS[cardId];
    snprintf(line, sizeof(line), "peaks %s %d, give or take %d nights", MON[s.month - 1], s.day, s.span);
    cardLine(line);
    snprintf(line, sizeof(line), "about %d an hour at the peak, under a dark sky", s.zhr);
    cardLine(line);
    snprintf(line, sizeof(line), "from %s", s.parent);
    cardLine(line);
    cardLine(s.fact);
    snprintf(line, sizeof(line), "hits the air at %d km/s", s.speed);
    cardLine(line);
  } else if (cardSrc == PIC_SCOPE && cardId >= 0 && cardId < SCOPE_N) {
    const ScopeFact &S = SCOPES[cardId];
    cardLine(S.launched);
    if (S.svc1[0]) cardLine(S.svc1);
    if (S.svc2[0]) cardLine(S.svc2);
    snprintf(line, sizeof(line), "distance from Earth  %s", S.where);
    cardLine(line);
    cardLine(S.mirror);
    cardLine(S.glass);
  } else if (cardSrc == PIC_SAT && cardId >= 0 && cardId < SAT_N && cardId < 3) {
    cardLine(CRAFT_FACT[cardId].what);
    cardLine(CRAFT_FACT[cardId].size);
    snprintf(line, sizeof(line), "made of %s", CRAFT_FACT[cardId].made);
    cardLine(line);
    cardLine(CRAFT_FACT[cardId].note);
    if (satLive[cardId].ok) {
      int miles = (int)(satLive[cardId].alt * 0.621371f);
      int mph = satMph(cardId);
      if (mph)
        snprintf(line, sizeof(line), "%s   LOOK %s   %d miles up   speed about %d mph",
                 satLive[cardId].vis ? "up now" : "not up", lookWord(satLive[cardId].az), miles, mph);
      else
        snprintf(line, sizeof(line), "%s   LOOK %s   %d miles up", satLive[cardId].vis ? "up now" : "not up",
                 lookWord(satLive[cardId].az), miles);
      cardLine(line);
    }
  } else if (cardSrc == PIC_ROCKET && cardId >= 0 && cardId < (int)(sizeof(ROCKET_FACT) / sizeof(ROCKET_FACT[0]))) {
    const PicRow *Pr = picBy(cardSrc, cardId);
    if (!(Pr && cardTxt(Pr->dir, Pr->file))) {
      const RocketFact &rf = ROCKET_FACT[cardId];
      cardLine(rf.tall);
      cardLine(rf.filled);
      cardLine(rf.speed);
      cardLine(rf.stage1);
      cardLine(rf.stage2);
    }
  } else if (cardSrc == PIC_LIB) {
    const PicRow *P = picBy(cardSrc, cardId);
    if (P) cardLine(P->kind);
    if (!(P && cardTxt(P->dir, P->file)) && cardId >= 0 && cardId < (int)(sizeof(LIB_FACT) / sizeof(LIB_FACT[0]))) {
      const LibFact &L = LIB_FACT[cardId];
      if (L.a && L.a[0]) cardLine(L.a);
      if (L.b && L.b[0]) cardLine(L.b);
      if (L.c && L.c[0]) cardLine(L.c);
      if (L.d && L.d[0]) cardLine(L.d);
    }
  } else
    cardLine("not stored");
  if (!sdOk) cardLine("pictures need the sd card");
  cardPhoto();
}

void goSubBack() {
  if (page == PG_ABOUT) drawSet();
  else if (page == PG_METEOR) drawShowers();
  else if (page == PG_BODY) drawPlanets();
  else if (page == PG_NIGHT) {
    if (backTo == PG_SKY) drawSky();
    else if (backTo == PG_MORE) drawMore();
    else drawHome();
  } else if (page == PG_CRAFT) {
    if (factKind == 0) drawSats();
    else if (factKind == 2) drawScopes();
    else drawLaunch();
  } else if (page == PG_COMETS || page == PG_SCOPES || page == PG_DARK || page == PG_CLOUD || page == PG_NEO ||
             page == PG_SOLAR || page == PG_FIND)
    drawMore();
  else if (page == PG_CARD) drawFind();
  else if (page == PG_SPACE) {
    if (backTo == PG_MORE) drawMore();
    else drawHome();
  } else if (page == PG_SET || page == PG_SATS || page == PG_ROCKS || page == PG_SHOWERS || page == PG_LAUNCH)
    drawMore();
  else
    drawHome();
}

void onTap(int x, int y) {
  if (page == PG_WIFI) {
    if (hit(x, y, 8, 8, 148, 44)) {
      leaveQuiet();
      return;
    }
    if (hit(x, y, 660, 8, 792, 44)) {
      if (!netBusy) startWifiScan();
      else wifiScanWait = true;
      drawWifi();
      return;
    }
    if (hit(x, y, 8, 52, 64, 88) && wifiScanTop > 0) {
      wifiScanTop -= 2;
      if (wifiScanTop < 0) wifiScanTop = 0;
      drawWifi();
      return;
    }
    if (hit(x, y, 736, 52, 792, 88) && wifiScanTop + 2 < nWifiScan) {
      wifiScanTop += 2;
      drawWifi();
      return;
    }
    for (int i = 0; i < 2; i++) {
      int idx = wifiScanTop + i;
      if (idx >= nWifiScan) break;
      if (hit(x, y, 72 + i * 328, 52, 392 + i * 328, 88)) {
        strlcpy(wifiSsid, wifiScan[idx], sizeof(wifiSsid));
        wifiFocus = 1;
        drawWifi();
        return;
      }
    }
    if (hit(x, y, 120, 100, 784, 140)) {
      wifiFocus = 0;
      drawWifi();
      return;
    }
    if (hit(x, y, 120, 148, 784, 188)) {
      wifiFocus = 1;
      drawWifi();
      return;
    }
    if (hit(x, y, 8, 436, 260, 472)) {
      applyWifiSave();
      return;
    }
    if (hit(x, y, 274, 436, 526, 472)) {
      clearWifi();
      strlcpy(wifiMsg, "wifi cleared", sizeof(wifiMsg));
      drawWifi();
      return;
    }
    if (hit(x, y, 540, 436, 792, 472)) {
      factoryReset();
      delay(200);
      ESP.restart();
      return;
    }
    if (hit(x, y, 8, 392, 128, 428)) {
      wifiShift = !wifiShift;
      wifiSym = false;
      drawWifi();
      return;
    }
    if (hit(x, y, 140, 392, 540, 428)) {
      wifiAppend(' ');
      drawWifi();
      return;
    }
    if (hit(x, y, 552, 392, 672, 428)) {
      wifiSym = !wifiSym;
      drawWifi();
      return;
    }
    if (hit(x, y, 722, 296, 792, 336)) {
      wifiDel();
      drawWifi();
      return;
    }
    const char *r0 = "1234567890";
    const char *r1 = wifiSym ? "!@#$%^&*()" : (wifiShift ? "QWERTYUIOP" : "qwertyuiop");
    const char *r2 = wifiSym ? "-_=+[]{};'" : (wifiShift ? "ASDFGHJKL" : "asdfghjkl");
    const char *r3 = wifiSym ? ",.<>/?\\|~`" : (wifiShift ? "ZXCVBNM.@-" : "zxcvbnm.@-");
    if (hitRow(x, y, r0, 200) || hitRow(x, y, r1, 248) || hitRow(x, y, r2, 296) || hitRow(x, y, r3, 344))
      drawWifi();
    return;
  }
  if (page == PG_PLACE) {
    if (hit(x, y, 8, 8, 148, 44)) {
      if (!placeTyping && nHits > 0) {
        placeTyping = true;
        placeMsg[0] = 0;
        drawPlace();
      } else
        leaveQuiet();
      return;
    }
    if (!placeTyping && nHits > 0) {
      for (int i = 0; i < nHits; i++) {
        if (hitBox(x, y, 24, 70 + i * 78, 752, 66)) {
          choosePlace(i);
          return;
        }
      }
      return;
    }
    if (hit(x, y, 660, 8, 792, 44)) {
      askGeo();
      return;
    }
    if (hit(x, y, 8, 392, 128, 428)) {
      placeShift = !placeShift;
      placeSym = false;
      drawPlace();
      return;
    }
    if (hit(x, y, 140, 392, 540, 428)) {
      placeAppend(' ');
      drawPlace();
      return;
    }
    if (hit(x, y, 552, 392, 672, 428)) {
      placeSym = !placeSym;
      drawPlace();
      return;
    }
    if (hit(x, y, 722, 296, 792, 336)) {
      placeDel();
      drawPlace();
      return;
    }
    const char *r0 = "1234567890";
    const char *r1 = placeSym ? "!@#$%^&*()" : (placeShift ? "QWERTYUIOP" : "qwertyuiop");
    const char *r2 = placeSym ? "-_=+[]{};'" : (placeShift ? "ASDFGHJKL" : "asdfghjkl");
    const char *r3 = placeSym ? ",.<>/?\\|~`" : (placeShift ? "ZXCVBNM.@-" : "zxcvbnm.@-");
    if (hitRow(x, y, r0, 200) || hitRow(x, y, r1, 248) || hitRow(x, y, r2, 296) || hitRow(x, y, r3, 344))
      drawPlace();
    return;
  }
  if (page != PG_HOME && page != PG_MORE && hit(x, y, 8, 8, 148, 44)) {
    goSubBack();
    return;
  }
  if ((page == PG_BODY || page == PG_SET || (page == PG_CRAFT && factKind == 2)) && hit(x, y, 652, 8, 792, 44)) {
    drawMore();
    return;
  }
  if (page == PG_FIND || page == PG_CARD) {
    if (hit(x, y, 652, 8, 792, 44)) {
      drawHome();
      return;
    }
    if (page == PG_CARD) return;
    for (int i = 0; i < nFind; i++) {
      if (hitBox(x, y, 16, 100 + i * 42, 768, 38)) {
        cardSrc = findSrc[i];
        cardId = findId[i];
        drawCard();
        return;
      }
    }
    if (hit(x, y, 722, 392, 792, 432)) {
      findPop();
      drawFind();
      return;
    }
    if (hit(x, y, 8, 436, 152, 472)) {
      findQ[0] = 0;
      findScan();
      drawFind();
      return;
    }
    if (hit(x, y, 160, 436, 560, 472)) {
      findPush(' ');
      drawFind();
      return;
    }
    if (findHitKey(x, y, "1234567890", 284) || findHitKey(x, y, "qwertyuiop", 320) ||
        findHitKey(x, y, "asdfghjkl", 356) || findHitKey(x, y, "zxcvbnm", 392))
      drawFind();
    return;
  }
  if (page == PG_MORE) {
    if (hit(x, y, 8, 8, 148, 44)) {
      drawHome();
      return;
    }
    if (hitBox(x, y, 16, 48, 376, 48)) {
      backTo = PG_MORE;
      nightPage = 0;
      drawTonight();
      return;
    }
    if (hitBox(x, y, 408, 48, 376, 48)) {
      backTo = PG_MORE;
      drawSpace();
      return;
    }
    if (hitBox(x, y, 16, 104, 376, 48)) {
      drawSats();
      return;
    }
    if (hitBox(x, y, 408, 104, 376, 48)) {
      rockPage = 0;
      drawRocks();
      return;
    }
    if (hitBox(x, y, 16, 160, 376, 48)) {
      drawComets();
      return;
    }
    if (hitBox(x, y, 408, 160, 376, 48)) {
      drawShowers();
      return;
    }
    if (hitBox(x, y, 16, 216, 376, 48)) {
      drawLaunch();
      return;
    }
    if (hitBox(x, y, 408, 216, 376, 48)) {
      drawScopes();
      return;
    }
    if (hitBox(x, y, 16, 272, 376, 48)) {
      drawSet();
      return;
    }
    if (hitBox(x, y, 408, 272, 376, 48)) {
      drawAbout();
      return;
    }
    if (hitBox(x, y, 16, 328, 376, 48)) {
      drawDark();
      return;
    }
    if (hitBox(x, y, 408, 328, 376, 48)) {
      drawClouds();
      return;
    }
    if (hitBox(x, y, 16, 384, 376, 48)) {
      drawNeo();
      return;
    }
    if (hitBox(x, y, 408, 384, 376, 48)) {
      drawSolar();
      return;
    }
    if (hitBox(x, y, 16, 432, 768, 40)) {
      findScan();
      drawFind();
      return;
    }
    return;
  }
  if (page == PG_SKY) {
    if (hit(x, y, 640, 8, 788, 44)) {
      backTo = PG_SKY;
      nightPage = 0;
      drawTonight();
      return;
    }
    skyPick(x, y);
    return;
  }
  if (page == PG_NIGHT) {
    if (hit(x, y, 620, 8, 788, 44)) {
      nightPage = nightPage ? 0 : 1;
      drawTonight();
    }
    return;
  }
  if (page == PG_PLANETS) {
    int i = (y - 46) / 60;
    if (i >= 0 && i < PLANET_N && y < 46 + i * 60 + 58) {
      bodyPick = i + 1;
      drawBody();
    }
    return;
  }
  if (page == PG_SCOPES) {
    int i = (y - 52) / 74;
    if (i >= 0 && i < SCOPE_N && y < 52 + i * 74 + 70) {
      factKind = 2;
      factId = i;
      drawCraft();
    }
    return;
  }
  if (page == PG_SHOWERS) {
    int i = (y - 50) / 50;
    if (i >= 0 && i < SHOWER_N && y < 450) {
      meteorPick = i;
      drawMeteor();
    }
    return;
  }
  if (page == PG_ROCKS) {
    if (hit(x, y, 600, 8, 788, 44)) {
      rockPage = rockPage ? 0 : 1;
      drawRocks();
    }
    return;
  }
  if (page == PG_SATS) {
    int y0 = MAP_Y0 + MAP_H + 6;
    int n = SAT_N < 2 ? SAT_N : 2;
    for (int i = 0; i < n; i++) {
      int h = 22;
      if (satLive[i].nPass) h += 12 * (satLive[i].nPass < 2 ? satLive[i].nPass : 2);
      if (y >= y0 && y < y0 + h) {
        factKind = 0;
        factId = i;
        drawCraft();
        return;
      }
      y0 += h;
    }
    return;
  }
  if (page == PG_LAUNCH) {
    int i = (y - 56) / 100;
    if (i >= 0 && i < nLaunch && y < 56 + i * 100 + 92) {
      factKind = 1;
      factId = i;
      drawCraft();
    }
    return;
  }
  if (page == PG_ABOUT) {
    if (hitBox(x, y, 170, 160, 460, 52)) askOta(1);
    else if (hitBox(x, y, 170, 236, 460, 52)) askOta(2);
    return;
  }
  if (page == PG_SET) {
    if (hitBox(x, y, 16, 64, 180, 52)) {
      dark = true;
      cols();
      savePrefs();
      drawSet();
    } else if (hitBox(x, y, 208, 64, 180, 52)) {
      dark = false;
      cols();
      savePrefs();
      drawSet();
    } else if (hitBox(x, y, 16, 132, 160, 52)) {
      blLevel = 0;
      applyBl();
      savePrefs();
      drawSet();
    } else if (hitBox(x, y, 188, 132, 160, 52)) {
      blLevel = 1;
      applyBl();
      savePrefs();
      drawSet();
    } else if (hitBox(x, y, 360, 132, 180, 52)) {
      blLevel = 2;
      applyBl();
      savePrefs();
      drawSet();
    } else if (hitBox(x, y, 16, 200, 160, 52)) {
      useC = false;
      savePrefs();
      drawSet();
    } else if (hitBox(x, y, 188, 200, 160, 52)) {
      useC = true;
      savePrefs();
      drawSet();
    } else if (hitBox(x, y, 16, 268, 250, 52))
      drawAbout();
    else if (hitBox(x, y, 280, 268, 250, 52)) {
      backTo = PG_SET;
      openWifi();
    } else if (hitBox(x, y, 544, 268, 240, 52)) {
      backTo = PG_SET;
      placeTyping = true;
      placeMsg[0] = 0;
      drawPlace();
    } else if (hitBox(x, y, 16, 336, 360, 52)) {
      factoryReset();
      delay(200);
      ESP.restart();
    }
    return;
  }
  if (page != PG_HOME) return;
  if (hit(x, y, 648, 8, 788, 44)) {
    backTo = PG_HOME;
    openWifi();
    return;
  }
  if (hit(x, y, 8, 8, 620, 44)) {
    backTo = PG_HOME;
    placeTyping = true;
    placeMsg[0] = 0;
    drawPlace();
    return;
  }
  if (hitBox(x, y, 12, 140, 380, 136)) {
    drawMoon();
    return;
  }
  if (hit(x, y, 8, 284, 790, 316)) {
    backTo = PG_HOME;
    drawSpace();
    return;
  }
  if (hit(x, y, 8, 316, 790, 356)) {
    drawSats();
    return;
  }
  if (hit(x, y, 8, 356, 790, 408)) {
    drawShowers();
    return;
  }
  int f = footAt(x, y);
  if (f < 0) return;
  if (f == 0) drawSky();
  else if (f == 1) drawPlanets();
  else if (f == 2) drawMoon();
  else drawMore();
}

int gt911Touches(int *xs, int *ys, int maxN) {
  I2cHold hold;
  Wire.beginTransmission(GT911_ADDR);
  Wire.write(0x81);
  Wire.write(0x4E);
  if (Wire.endTransmission(false) != 0) return 0;
  if (Wire.requestFrom((uint8_t)GT911_ADDR, (uint8_t)1) != 1) return 0;
  uint8_t st = Wire.read();
  if ((st & 0x80) == 0) return 0;
  int n = st & 0x0F;
  if (n == 0) {
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(0x81);
    Wire.write(0x4E);
    Wire.write(0);
    Wire.endTransmission();
    return 0;
  }
  if (n > maxN) n = maxN;
  Wire.beginTransmission(GT911_ADDR);
  Wire.write(0x81);
  Wire.write(0x50);
  if (Wire.endTransmission(false) != 0) return 0;
  uint8_t want = (uint8_t)(n * 8);
  if (Wire.requestFrom((uint8_t)GT911_ADDR, want) != want) {
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(0x81);
    Wire.write(0x4E);
    Wire.write(0);
    Wire.endTransmission();
    return 0;
  }
  uint8_t p[16];
  for (int i = 0; i < want; i++) p[i] = Wire.read();
  Wire.beginTransmission(GT911_ADDR);
  Wire.write(0x81);
  Wire.write(0x4E);
  Wire.write(0);
  Wire.endTransmission();
  for (int i = 0; i < n; i++) {
    int rx = p[i * 8] | (p[i * 8 + 1] << 8);
    int ry = p[i * 8 + 2] | (p[i * 8 + 3] << 8);
    xs[i] = constrain(rx, 0, PW - 1);
    ys[i] = constrain(ry, 0, PH - 1);
  }
  return n;
}

bool tapReady(int *x, int *y) {
  static bool held = false;
  int n = gt911Touches(x, y, 1);
  if (n != 1) {
    held = false;
    return false;
  }
  if (held || millis() - lastTap <= 180) return false;
  lastTap = millis();
  held = true;
  return true;
}

static void noteHttp(HTTPClient &http) {
  http.setTimeout(12000);
  http.setConnectTimeout(4000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setReuse(false);
  http.setUserAgent("AstroDesk/1.0 (desk display)");
}
static void urlEncode(const char *in, char *out, int n) {
  int j = 0;
  for (int i = 0; in[i] && j < n - 4; i++) {
    char c = in[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
      out[j++] = c;
    else if (c == ' ') {
      out[j++] = '%';
      out[j++] = '2';
      out[j++] = '0';
    } else if (c == ',') {
      out[j++] = '%';
      out[j++] = '2';
      out[j++] = 'C';
    }
  }
  out[j] = 0;
}
static bool copyQuoted(const char *p, const char *end, char *out, int n, const char **after) {
  while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
  if ((end - p) >= 4 && strncmp(p, "null", 4) == 0) {
    out[0] = 0;
    if (after) *after = p + 4;
    return true;
  }
  if (p >= end || *p != '"') return false;
  p++;
  int i = 0;
  while (p < end && *p != '"') {
    if (i < n - 1) out[i++] = *p;
    p++;
  }
  out[i] = 0;
  if (p >= end || *p != '"') return false;
  if (after) *after = p + 1;
  return true;
}
static bool jsonNumRange(const char *s, const char *end, const char *key, float *out) {
  if (!s || !end || !key) return false;
  int klen = (int)strlen(key);
  const char *p = s;
  while (p < end && (end - p) > klen) {
    if (!strncmp(p, key, klen)) {
      p += klen;
      while (p < end && *p == ' ') p++;
      if (p + 4 <= end && !strncmp(p, "null", 4)) return false;
      *out = (float)atof(p);
      return true;
    }
    p++;
  }
  return false;
}
static bool httpGetCap(const char *url, char *body, int cap, const char *accept, bool stopOnHold) {
  body[0] = 0;
  if (WiFi.status() != WL_CONNECTED || cap < 8) return false;
  if (page == PG_WIFI || (stopOnHold && netHold)) return false;
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  noteHttp(http);
  bool ok = false;
  if (http.begin(cli, url)) {
    if (accept) http.addHeader("Accept", accept);
    int code = http.GET();
    if (code == 200) {
      int got = 0;
      uint32_t t0 = millis();
      WiFiClient *stream = http.getStreamPtr();
      while (got < cap - 1 && millis() - t0 < 12000) {
        if (page == PG_WIFI || (stopOnHold && netHold)) break;
        int n = stream->available();
        if (n > 0) {
          if (got + n > cap - 1) n = cap - 1 - got;
          int r = stream->readBytes(body + got, n);
          if (r <= 0) break;
          got += r;
          continue;
        }
        if (!http.connected()) break;
        delay(2);
      }
      body[got] = 0;
      ok = got > 20 && page != PG_WIFI && !(stopOnHold && netHold);
    }
    http.end();
  }
  cli.stop();
  return ok;
}
static bool httpGetTail(const char *url, char *body, int cap, const char *accept, bool stopOnHold) {
  body[0] = 0;
  if (WiFi.status() != WL_CONNECTED || cap < 64) return false;
  if (page == PG_WIFI || (stopOnHold && netHold)) return false;
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  noteHttp(http);
  bool ok = false;
  if (http.begin(cli, url)) {
    char range[32];
    snprintf(range, sizeof(range), "bytes=-%d", cap - 1);
    http.addHeader("Range", range);
    if (accept) http.addHeader("Accept", accept);
    int code = http.GET();
    if (code == 206 || code == 200) {
      int got = 0;
      uint32_t t0 = millis();
      WiFiClient *stream = http.getStreamPtr();
      uint8_t chunk[256];
      while (millis() - t0 < 12000) {
        if (page == PG_WIFI || (stopOnHold && netHold)) break;
        int n = stream->available();
        if (n > 0) {
          if (n > (int)sizeof(chunk)) n = (int)sizeof(chunk);
          int r = stream->readBytes(chunk, n);
          if (r <= 0) break;
          if (got + r < cap - 1) {
            memcpy(body + got, chunk, (size_t)r);
            got += r;
          } else {
            int keep = cap - 1 - r;
            if (keep < 0) keep = 0;
            if (got > keep) {
              memmove(body, body + (got - keep), (size_t)keep);
              got = keep;
            }
            int put = r;
            if (got + put > cap - 1) put = cap - 1 - got;
            if (put > 0) {
              memcpy(body + got, chunk + (r - put), (size_t)put);
              got += put;
            }
          }
          continue;
        }
        if (!http.connected()) break;
        delay(2);
      }
      body[got] = 0;
      ok = got > 20 && page != PG_WIFI && !(stopOnHold && netHold);
    }
    http.end();
  }
  cli.stop();
  return ok;
}
static void adoptZone(const char *iana, int offsetSec) {
  char next[56];
  next[0] = 0;
  if (iana && iana[0]) {
    for (unsigned i = 0; i < sizeof(ZONES) / sizeof(ZONES[0]); i++) {
      if (!strcmp(ZONES[i].iana, iana)) {
        strlcpy(next, ZONES[i].posix, sizeof(next));
        break;
      }
    }
  }
  if (!next[0]) {
    int west = -offsetSec;
    int neg = west < 0;
    int a = neg ? -west : west;
    int h = a / 3600;
    int m = (a % 3600) / 60;
    if (m)
      snprintf(next, sizeof(next), "XXX%d:%02d", neg ? -h : h, m);
    else
      snprintf(next, sizeof(next), "XXX%d", neg ? -h : h);
  }
  if (!strcmp(next, placeTz)) return;
  strlcpy(placeTz, next, sizeof(placeTz));
  applyPlaceTz();
  savePrefs();
}
static bool pullMeteo(float lat, float lon, int gen) {
  char url[420];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,is_day,cloud_cover"
           "&temperature_unit=fahrenheit&timezone=auto&forecast_days=1",
           lat, lon);
  if (!httpGetCap(url, netBuf, PACK_CAP, "application/json", true)) return false;
  if (gen != placeGen || lat != placeLat || lon != placeLon) return false;
  char iana[48];
  iana[0] = 0;
  const char *tz = strstr(netBuf, "\"timezone\":\"");
  if (tz) copyQuoted(tz + 11, netBuf + strlen(netBuf), iana, sizeof(iana), nullptr);
  float off = 0, code = 0, day = 1, temp = 0;
  jsonNumRange(netBuf, netBuf + strlen(netBuf), "\"utc_offset_seconds\":", &off);
  adoptZone(iana, (int)off);
  const char *cur = strstr(netBuf, "\"current\":");
  if (!cur) return false;
  jsonNumRange(cur, netBuf + strlen(netBuf), "\"weather_code\":", &code);
  jsonNumRange(cur, netBuf + strlen(netBuf), "\"is_day\":", &day);
  jsonNumRange(cur, netBuf + strlen(netBuf), "\"temperature_2m\":", &temp);
  float cloud = -1;
  wxCloud = -1;
  if (jsonNumRange(cur, netBuf + strlen(netBuf), "\"cloud_cover\":", &cloud) && cloud >= 0 && cloud <= 100)
    wxCloud = iround(cloud);
  wxCode = (int)code;
  wxDay = day >= 0.5f;
  wxTemp = iround(temp);
  wxOk = true;
  return true;
}
static void pullAurora() {
  if (!httpGetCap("https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json", netBuf, PACK_CAP,
                  "application/json", true))
    return;
  const char *p = netBuf;
  const char *hitp = nullptr;
  while ((p = strstr(p, "],[\""))) {
    hitp = p;
    p += 4;
  }
  if (!hitp) return;
  const char *s = strchr(hitp, ',');
  if (!s) return;
  while (*s && (*s < '0' || *s > '9') && *s != '-') s++;
  float k = (float)atof(s);
  if (k >= 0 && k <= 9) auroraKp = (int)(k + 0.5f);
}
static bool pullPack(float lat, float lon) {
  int gen = placeGen;
  bool sky = pullMeteo(lat, lon, gen);
  if (gen != placeGen || netHold) return false;
  pullAurora();
  return sky;
}
static bool pullGeo() {
  char enc[140];
  urlEncode(placeQuery, enc, sizeof(enc));
  char url[240];
  snprintf(url, sizeof(url),
           "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=4&countrycodes=us&addressdetails=0",
           enc);
  nHits = 0;
  if (!httpGetCap(url, netBuf, PACK_CAP, "application/json", false)) {
    strlcpy(placeMsg, "search failed", sizeof(placeMsg));
    placeTyping = true;
    return true;
  }
  const char *p = netBuf;
  const char *end = netBuf + strlen(netBuf);
  while (nHits < 4) {
    const char *latk = strstr(p, "\"lat\":\"");
    if (!latk) break;
    float la = (float)atof(latk + 7);
    const char *lonk = strstr(latk, "\"lon\":\"");
    if (!lonk) break;
    float lo = (float)atof(lonk + 7);
    const char *nk = strstr(lonk, "\"display_name\":\"");
    if (!nk) break;
    char raw[110];
    const char *after = nullptr;
    if (!copyQuoted(nk + 15, end, raw, sizeof(raw), &after)) break;
    shortPlace(raw, hits[nHits].label, sizeof(hits[nHits].label));
    hits[nHits].lat = la;
    hits[nHits].lon = lo;
    nHits++;
    p = after ? after : nk + 15;
  }
  if (!nHits) strlcpy(placeMsg, "nothing in the US", sizeof(placeMsg));
  else placeMsg[0] = 0;
  placeTyping = nHits == 0;
  return true;
}
static int lineLen(const char *p) {
  int n = 0;
  while (p[n] && p[n] != '\n' && p[n] != '\r') n++;
  return n;
}
static bool pullOneTle(int cat, IssTle *tle) {
  char url[96];
  snprintf(url, sizeof(url), "https://celestrak.org/NORAD/elements/gp.php?CATNR=%d&FORMAT=TLE", cat);
  if (!httpGetCap(url, netBuf, PACK_CAP, "text/plain", true)) return false;
  const char *l1 = nullptr, *l2 = nullptr;
  for (char *p = netBuf; *p; p++) {
    if ((p == netBuf || p[-1] == '\n') && p[0] == '1' && p[1] == ' ') l1 = p;
    if ((p == netBuf || p[-1] == '\n') && p[0] == '2' && p[1] == ' ') l2 = p;
  }
  if (!l1 || !l2 || lineLen(l1) < 32 || lineLen(l2) < 63) return false;
  char a[80], b[80];
  int na = lineLen(l1);
  int nb = lineLen(l2);
  if (na > 78) na = 78;
  if (nb > 78) nb = 78;
  memcpy(a, l1, na);
  a[na] = 0;
  memcpy(b, l2, nb);
  b[nb] = 0;
  return parseTle(a, b, tle);
}
static void pullSats() {
  if (netHold) return;
  satBusy = true;
  int n = SAT_N < 4 ? SAT_N : 4;
  int got = 0;
  for (int i = 0; i < n; i++) {
    if (netHold) break;
    IssTle fresh;
    fresh.ok = false;
    if (pullOneTle(SATS[i].cat, &fresh)) {
      satLive[i].tle = fresh;
      got++;
    }
  }
  satBusy = false;
  if (got) {
    lastTle = millis();
    satHeavyAt = 0;
    satMoveAt = 0;
  }
  if (page == PG_SATS) satsDirty = true;
  if (page == PG_HOME) homeDirty = true;
}
time_t utcYmd(int y, int mo, int d, int h, int mi, int se) {
  if (mo <= 2) {
    y--;
    mo += 12;
  }
  int A = y / 100;
  int B = 2 - A + A / 4;
  double jd = floor(365.25 * (y + 4716)) + floor(30.6001 * (mo + 1)) + d + B - 1524.5 +
              (h + mi / 60.0 + se / 3600.0) / 24.0;
  return (time_t)((jd - 2440587.5) * 86400.0);
}
static bool launchJunk(const char *s) {
  return !s || !s[0] || !strcmp(s, "Minute") || !strcmp(s, "Second") || !strcmp(s, "Hour");
}
static void plainOwner(const char *mission, char *out, int n) {
  out[0] = 0;
  if (!mission || !mission[0]) return;
  if (strstr(mission, "Starlink")) strlcpy(out, "SpaceX", n);
  else if (strstr(mission, "OneWeb")) strlcpy(out, "OneWeb", n);
  else if (strstr(mission, "GPS") || strstr(mission, "Navstar")) strlcpy(out, "the US Space Force", n);
  else if (strstr(mission, "BeiDou")) strlcpy(out, "China", n);
  else if (strstr(mission, "Galileo")) strlcpy(out, "the European Union", n);
  else if (strstr(mission, "NOAA") || strstr(mission, "GOES")) strlcpy(out, "NOAA", n);
}
static void sayOwned(char *out, int n, const char *what, const char *who) {
  if (who && who[0]) snprintf(out, n, "%s, owned by %s", what, who);
  else snprintf(out, n, "%s, owner unknown", what);
}
static void plainPayload(const char *mtype, const char *mission, char *out, int n) {
  char who[28];
  plainOwner(mission, who, sizeof(who));
  if (mission && strstr(mission, "Starlink")) {
    snprintf(out, n, "Starlink, owned by SpaceX. satellite count is not in the live list");
    return;
  }
  if (!mtype || !mtype[0]) {
    if (mission && mission[0]) strlcpy(out, mission, n);
    else strlcpy(out, "unknown payload", n);
    return;
  }
  if (strstr(mtype, "Communication")) sayOwned(out, n, "communications satellite", who);
  else if (strstr(mtype, "Earth")) sayOwned(out, n, "satellite that photographs the Earth", who);
  else if (strstr(mtype, "Navigation")) sayOwned(out, n, "navigation satellite", who);
  else if (strstr(mtype, "Resupply") || strstr(mtype, "Cargo")) strlcpy(out, "a cargo ship", n);
  else if (strstr(mtype, "Astrophys") || !strcmp(mtype, "Science")) strlcpy(out, "a science satellite", n);
  else if (strstr(mtype, "Test")) strlcpy(out, "a test flight", n);
  else if (strstr(mtype, "Rideshare")) strlcpy(out, "several satellites on one rocket", n);
  else if (strstr(mtype, "Human") || strstr(mtype, "Tourism")) strlcpy(out, "a crew flight", n);
  else if (mission && mission[0]) strlcpy(out, mission, n);
  else strlcpy(out, "unknown payload", n);
}
static void pullLaunch() {
  lastLaunch = millis();
  if (!liveClock(nullptr) || netHold) return;
  char url[220];
  strlcpy(url, "https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=4&mode=list", sizeof(url));
  int got = 0;
  bool any = false;
  for (int page = 0; page < 3 && got < 4; page++) {
    if (!httpGetCap(url, netBuf, PACK_CAP, "application/json", true)) break;
    any = true;
    char nextUrl[220];
    nextUrl[0] = 0;
    const char *nx = strstr(netBuf, "\"next\":");
    if (nx) {
      const char *q = nx + 7;
      while (*q == ' ' || *q == '\t') q++;
      if (*q == '"') copyQuoted(q, netBuf + strlen(netBuf), nextUrl, sizeof(nextUrl), nullptr);
    }
    time_t now = time(nullptr);
    char *end = netBuf + strlen(netBuf);
    char *p = netBuf;
    while (got < 4 && (p = strstr(p, "\"net\":\""))) {
    if (p >= end) break;
    char *lim = strstr(p + 7, "\"net\":\"");
    if (!lim || lim > end) lim = end;
    char net[32] = "", name[64] = "", locn[48] = "", padn[40] = "", lsp[24] = "";
    copyQuoted(p + 6, lim, net, sizeof(net), nullptr);
    const char *status = nullptr;
    for (const char *q = p; q > netBuf && q + 9 > p - 800; q--) {
      if (!strncmp(q, "\"status\":", 9)) {
        status = q;
        break;
      }
    }
    const char *win = status ? (status - 500 > netBuf ? status - 500 : netBuf) : netBuf;
    const char *lastName = nullptr;
    if (status) {
      for (const char *q = win; q < status;) {
        const char *nk = strstr(q, "\"name\":\"");
        if (!nk || nk >= status) break;
        lastName = nk;
        q = nk + 8;
      }
    }
    if (lastName) copyQuoted(lastName + 7, status, name, sizeof(name), nullptr);
    const char *pk = strstr(p, "\"pad\":");
    if (pk && pk < lim) copyQuoted(pk + 6, lim, padn, sizeof(padn), nullptr);
    const char *lk = strstr(p, "\"location\":");
    if (lk && lk < lim) copyQuoted(lk + 11, lim, locn, sizeof(locn), nullptr);
    const char *ak = strstr(p, "\"lsp_name\":");
    if (ak && ak < lim) copyQuoted(ak + 11, lim, lsp, sizeof(lsp), nullptr);
    int y, mo, d, h, mi;
    float se = 0;
    if (sscanf(net, "%d-%d-%dT%d:%d:%f", &y, &mo, &d, &h, &mi, &se) >= 5) {
      time_t t = utcYmd(y, mo, d, h, mi, (int)se);
      if (t >= now - 600 && !launchJunk(name)) {
        struct tm loc;
        struct tm today;
        localtime_r(&t, &loc);
        localtime_r(&now, &today);
        Launch &L = launches[got];
        int dayDiff = (loc.tm_yday + loc.tm_year * 366) - (today.tm_yday + today.tm_year * 366);
        if (dayDiff <= 0)
          strlcpy(L.day, "TODAY", sizeof(L.day));
        else {
          static const char *moName[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                         "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
          snprintf(L.day, sizeof(L.day), "%d %s", loc.tm_mday, moName[loc.tm_mon]);
        }
        char hm[16];
        fmtHm(loc.tm_hour, loc.tm_min, hm, sizeof(hm));
        strlcpy(L.when, hm, sizeof(L.when));
        const char *bar = strstr(name, " | ");
        L.mission[0] = 0;
        L.status[0] = 0;
        if (bar) {
          int nn = (int)(bar - name);
          if (nn > 26) nn = 26;
          memcpy(L.rocket, name, nn);
          L.rocket[nn] = 0;
          strlcpy(L.mission, bar + 3, sizeof(L.mission));
        } else
          strlcpy(L.rocket, name, sizeof(L.rocket));
        if (status) {
          const char *sn = strstr(status, "\"name\":\"");
          if (sn && sn < p) copyQuoted(sn + 7, p, L.status, sizeof(L.status), nullptr);
        }
        if (locn[0])
          strlcpy(L.pad, locn, sizeof(L.pad));
        else
          strlcpy(L.pad, padn, sizeof(L.pad));
        strlcpy(L.agency, lsp, sizeof(L.agency));
        L.payload[0] = 0;
        L.going[0] = 0;
        L.owner[0] = 0;
        char mtype[28] = "";
        const char *mtk = strstr(p, "\"mission_type\":");
        if (mtk && mtk < lim) {
          const char *q = mtk + 15;
          while (q < lim && (*q == ' ' || *q == '\t')) q++;
          if (q < lim && *q == '"') copyQuoted(q, lim, mtype, sizeof(mtype), nullptr);
        }
        plainPayload(mtype, L.mission, L.payload, sizeof(L.payload));
        plainOwner(L.mission, L.owner, sizeof(L.owner));
        bool crew = strstr(mtype, "Human") || strstr(name, "Crew") || strstr(name, "crew") ||
                    strstr(name, "Shenzhou");
        if (strstr(name, "ISS"))
          strlcpy(L.going, "the ISS", sizeof(L.going));
        else if (strstr(name, "Tiangong") || strstr(name, "Shenzhou"))
          strlcpy(L.going, "Tiangong", sizeof(L.going));
        else if (strstr(name, "Artemis") || strstr(name, "Lunar") || strstr(name, "Moon"))
          strlcpy(L.going, "the Moon", sizeof(L.going));
        else if (crew)
          strlcpy(L.going, "not listed", sizeof(L.going));
        got++;
      }
    }
    p += 7;
    }
    if (!nextUrl[0] || !strcmp(nextUrl, url)) break;
    strlcpy(url, nextUrl, sizeof(url));
  }
  if (!any) return;
  nLaunch = got;
  launchOk = true;
}

static void takeQuote(const char *buf, const char *end, const char *key, char *out, int n) {
  out[0] = 0;
  const char *p = strstr(buf, key);
  if (!p || !end) return;
  p += strlen(key);
  while (p < end && (*p == ' ' || *p == '\t')) p++;
  if (p < end && *p == '"') copyQuoted(p, end, out, n, nullptr);
}
static void pullSolarMore() {
  if (netHold) return;
  if (httpGetTail("https://services.swpc.noaa.gov/json/goes/primary/xrays-6-hour.json", netBuf, 7000,
                  "application/json", true)) {
    float tmp[8];
    int n = 0;
    const char *p = netBuf;
    while ((p = strstr(p, "0.1-0.8nm"))) {
      const char *win = p - 220;
      if (win < netBuf) win = netBuf;
      const char *flux = nullptr;
      for (const char *q = win; q + 7 < p; q++) {
        if (!strncmp(q, "\"flux\":", 7)) flux = q;
      }
      float v = 0;
      if (flux && jsonNumRange(flux, p, "\"flux\":", &v) && v > 0 && v < 1.0f) {
        if (n < 8)
          tmp[n++] = v;
        else {
          memmove(tmp, tmp + 1, 7 * sizeof(float));
          tmp[7] = v;
        }
      }
      p += 10;
    }
    if (n > 0) {
      for (int i = 0; i < n; i++) xrayFlux[i] = tmp[i];
      xrayN = n;
    }
  }
  if (netHold) return;
  if (httpGetCap("https://services.swpc.noaa.gov/text/srs.txt", netBuf, PACK_CAP, "text/plain", true)) {
    const char *sec = strstr(netBuf, "Regions with Sunspots");
    if (sec) {
      const char *stop = strstr(sec, "\nIA.");
      if (!stop) stop = strstr(sec, "\nII.");
      if (!stop) stop = netBuf + strlen(netBuf);
      SunRegion tmp[8];
      int n = 0;
      const char *line = strchr(sec, '\n');
      while (line && line + 1 < stop && n < 8) {
        line++;
        while (*line == ' ' && line < stop) line++;
        const char *nl = strchr(line, '\n');
        if (!nl || nl > stop) nl = stop;
        if (line >= stop || *line < '0' || *line > '9') {
          if (nl >= stop) break;
          line = nl;
          continue;
        }
        int i = 0;
        char num[8];
        while (line[i] >= '0' && line[i] <= '9' && i < 7) {
          num[i] = line[i];
          i++;
        }
        num[i] = 0;
        if (i < 3) {
          if (nl >= stop) break;
          line = nl;
          continue;
        }
        while (line[i] == ' ') i++;
        int j = 0;
        char loc[12];
        while (line[i] && line[i] != ' ' && line[i] != '\n' && line[i] != '\r' && j < 11) loc[j++] = line[i++];
        loc[j] = 0;
        const char *eol = strchr(line, '\n');
        if (!eol || eol > stop) eol = stop;
        const char *magp = nullptr;
        for (const char *q = line; q + 4 <= eol; q++) {
          if (!strncmp(q, "Beta", 4) || (q + 5 <= eol && !strncmp(q, "Alpha", 5)) ||
              (q + 5 <= eol && !strncmp(q, "Gamma", 5))) {
            magp = q;
            break;
          }
        }
        char mag[20];
        mag[0] = 0;
        if (magp) {
          int k = 0;
          while (magp[k] && magp[k] != '\n' && magp[k] != '\r' && k < 19) {
            mag[k] = magp[k];
            k++;
          }
          mag[k] = 0;
        }
        if (num[0] && loc[0]) {
          strlcpy(tmp[n].num, num, sizeof(tmp[n].num));
          strlcpy(tmp[n].loc, loc, sizeof(tmp[n].loc));
          strlcpy(tmp[n].mag, mag, sizeof(tmp[n].mag));
          n++;
        }
        if (nl >= stop) break;
        line = nl;
      }
      for (int i = 0; i < n; i++) sunRegions[i] = tmp[i];
      nSunRegions = n;
    }
  }
  if (netHold) return;
  if (httpGetTail("https://services.swpc.noaa.gov/products/geospace/propagated-solar-wind-1-hour.json", netBuf, 4000,
                  "application/json", true)) {
    const char *last = nullptr;
    const char *p = netBuf;
    while ((p = strstr(p, "], [\""))) {
      last = p;
      p += 4;
    }
    if (last) {
      const char *q = strchr(last, '"');
      const char *e = q ? strchr(q + 1, '"') : nullptr;
      float sp = 0, den = 0;
      if (e && sscanf(e + 1, ", %f, %f", &sp, &den) == 2 && sp > 50 && sp < 2500 && den >= 0 && den < 200) {
        windPair = (int)(sp + 0.5f);
        windDens10 = (int)(den * 10.0f + 0.5f);
      }
    }
  }
}
static bool takeField(const char **p, const char *end, char *out, int n) {
  const char *q = *p;
  while (q < end && *q != '"') {
    if (*q == ']') return false;
    q++;
  }
  if (q >= end || *q != '"') return false;
  return copyQuoted(q, end, out, n, p);
}
static void pullClose() {
  if (netHold) return;
  time_t now;
  if (!liveClock(&now)) return;
  struct tm g;
  gmtime_r(&now, &g);
  char url[180];
  snprintf(url, sizeof(url),
           "https://ssd-api.jpl.nasa.gov/cad.api?dist-max=0.05&date-min=%04d-%02d-%02d&sort=date&limit=5",
           g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);
  if (!httpGetCap(url, netBuf, PACK_CAP, "application/json", true)) return;
  const char *data = strstr(netBuf, "\"data\":");
  if (!data) return;
  const char *end = netBuf + strlen(netBuf);
  ClosePass tmp[5];
  int n = 0;
  const char *p = data;
  while (n < 5) {
    const char *row = strstr(p, "[\"");
    if (!row) break;
    const char *q = row + 1;
    char des[20], skip[24], cd[24], dist[24], spd[24];
    if (!takeField(&q, end, des, sizeof(des))) break;
    if (!takeField(&q, end, skip, sizeof(skip))) break;
    if (!takeField(&q, end, skip, sizeof(skip))) break;
    if (!takeField(&q, end, cd, sizeof(cd))) break;
    if (!takeField(&q, end, dist, sizeof(dist))) break;
    if (!takeField(&q, end, skip, sizeof(skip))) break;
    if (!takeField(&q, end, skip, sizeof(skip))) break;
    if (!takeField(&q, end, spd, sizeof(spd))) break;
    float d = (float)atof(dist);
    float v = (float)atof(spd);
    if (des[0] && d > 0 && d < 1.0f && v > 0 && v < 200) {
      strlcpy(tmp[n].des, des, sizeof(tmp[n].des));
      strlcpy(tmp[n].when, cd, sizeof(tmp[n].when));
      snprintf(tmp[n].dist, sizeof(tmp[n].dist), "%.3f AU", d);
      snprintf(tmp[n].spd, sizeof(tmp[n].spd), "%.1f km/s", v);
      n++;
    }
    p = q;
  }
  if (n > 0) {
    for (int i = 0; i < n; i++) closePass[i] = tmp[i];
    nClose = n;
  } else if (strstr(netBuf, "\"count\":0") || strstr(netBuf, "\"data\":[]"))
    nClose = 0;
}
static void pullSpace() {
  if (netHold) return;
  const char *end;
  float v = -1;
  if (httpGetCap("https://services.swpc.noaa.gov/products/summary/solar-wind-speed.json", netBuf, PACK_CAP,
                 "application/json", true)) {
    end = netBuf + strlen(netBuf);
    if (jsonNumRange(netBuf, end, "\"proton_speed\":", &v) && v > 50 && v < 2500) windSpeed = (int)(v + 0.5f);
  }
  if (netHold) return;
  if (httpGetCap("https://services.swpc.noaa.gov/products/summary/solar-wind-mag-field.json", netBuf, PACK_CAP,
                 "application/json", true)) {
    end = netBuf + strlen(netBuf);
    float bt = -1, bz = -999;
    if (jsonNumRange(netBuf, end, "\"bt\":", &bt) && bt >= 0 && bt < 200) windBt = (int)(bt + 0.5f);
    if (jsonNumRange(netBuf, end, "\"bz_gsm\":", &bz) && bz > -100 && bz < 100)
      windBz = (int)(bz + (bz >= 0 ? 0.5f : -0.5f));
  }
  if (netHold) return;
  if (httpGetCap("https://services.swpc.noaa.gov/json/goes/primary/xray-flares-latest.json", netBuf, PACK_CAP,
                 "application/json", true)) {
    end = netBuf + strlen(netBuf);
    char nowc[8], maxc[8], when[24];
    takeQuote(netBuf, end, "\"current_class\":", nowc, sizeof(nowc));
    takeQuote(netBuf, end, "\"max_class\":", maxc, sizeof(maxc));
    takeQuote(netBuf, end, "\"max_time\":", when, sizeof(when));
    if (nowc[0]) strlcpy(flareNow, nowc, sizeof(flareNow));
    if (maxc[0]) strlcpy(flareMax, maxc, sizeof(flareMax));
    int y, mo, d, h, mi;
    if (sscanf(when, "%d-%d-%dT%d:%d", &y, &mo, &d, &h, &mi) >= 5 && liveClock(nullptr)) {
      time_t t = utcYmd(y, mo, d, h, mi, 0);
      struct tm loc;
      localtime_r(&t, &loc);
      fmtHm(loc.tm_hour, loc.tm_min, flareAt, sizeof(flareAt));
    }
  }
  if (netHold) return;
  if (httpGetCap("https://services.swpc.noaa.gov/products/noaa-scales.json", netBuf, PACK_CAP, "application/json",
                 true)) {
    end = netBuf + strlen(netBuf);
    const char *day0 = strstr(netBuf, "\"0\":");
    const char *day1 = strstr(netBuf, "\"1\":");
    const char *day2 = strstr(netBuf, "\"2\":");
    if (day0) {
      const char *stop = day1 ? day1 : end;
      const char *g = strstr(day0, "\"G\":");
      if (g && g < stop) {
        const char *tx = strstr(g, "\"Text\":");
        if (tx && tx < stop) {
          const char *q = strchr(tx + 7, '"');
          if (q && q < stop && q - tx < 20) copyQuoted(q, stop, stormNow, sizeof(stormNow), nullptr);
        }
      }
    }
    if (day1) {
      const char *stop = day2 ? day2 : end;
      const char *g = strstr(day1, "\"G\":");
      if (g && g < stop) {
        const char *tx = strstr(g, "\"Text\":");
        if (tx && tx < stop) {
          const char *q = strchr(tx + 7, '"');
          if (q && q < stop && q - tx < 20) copyQuoted(q, stop, stormDay, sizeof(stormDay), nullptr);
        }
      }
      const char *mp = strstr(day1, "\"MinorProb\":");
      if (mp && mp < stop) {
        const char *r = mp + 12;
        while (*r == ' ') r++;
        if (*r == '"') {
          char num[8];
          if (copyQuoted(r, stop, num, sizeof(num), nullptr) && num[0]) flareProb = atoi(num);
        }
      }
      const char *pr = mp ? strstr(mp + 12, "\"Prob\":") : nullptr;
      if (pr && pr < stop) {
        const char *r = pr + 7;
        while (*r == ' ') r++;
        if (*r == '"') {
          char num[8];
          if (copyQuoted(r, stop, num, sizeof(num), nullptr) && num[0]) radProb = atoi(num);
        }
      }
    }
  }
  if (netHold) return;
  if (!httpGetCap("https://services.swpc.noaa.gov/products/noaa-planetary-k-index-forecast.json", netBuf, PACK_CAP,
                  "application/json", true)) {
    if (netHold) return;
    pullSolarMore();
    if (netHold) return;
    pullClose();
    return;
  }
  end = netBuf + strlen(netBuf);
  kpNextN = 0;
  const char *p = netBuf;
  while (kpNextN < 8 && (p = strstr(p, "\"observed\":\""))) {
    char obs[16];
    if (!copyQuoted(p + 11, end, obs, sizeof(obs), nullptr)) break;
    if (strcmp(obs, "observed")) {
      const char *kp = nullptr;
      int back = 0;
      for (const char *q = p; q > netBuf && back < 60; q--, back++) {
        if (!strncmp(q, "\"kp\":", 5)) {
          kp = q + 5;
          break;
        }
      }
      if (kp) {
        float k = (float)atof(kp);
        if (k >= 0 && k <= 9.0f) kpNext[kpNextN++] = (int)(k * 10.0f + 0.5f);
      }
    }
    p += 12;
  }
  if (netHold) return;
  pullSolarMore();
  if (netHold) return;
  pullClose();
}

static int verPack(const char *s) {
  int a = 0, b = 0;
  if (!s) return 0;
  while (*s && (*s < '0' || *s > '9')) s++;
  while (*s >= '0' && *s <= '9') a = a * 10 + (*s++ - '0');
  if (*s == '.') s++;
  while (*s >= '0' && *s <= '9') b = b * 10 + (*s++ - '0');
  return a * 100 + b;
}
static void otaTrim(char *s) {
  if (!s) return;
  char *a = s;
  while (*a == ' ' || *a == '\n' || *a == '\r' || *a == '\t') a++;
  char *e = a + strlen(a);
  while (e > a && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r' || e[-1] == '\t')) e--;
  *e = 0;
  if (a != s) memmove(s, a, (size_t)(e - a) + 1);
}
static void otaCheck() {
  otaNewer = false;
  otaLatest[0] = 0;
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(otaMsg, "join wifi first", sizeof(otaMsg));
    return;
  }
  char url[180];
  snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/%s/main/ota/version.txt", ASTRO_GH_USER,
           ASTRO_GH_REPO);
  char body[48];
  if (!httpGetCap(url, body, sizeof(body), "text/plain", true)) {
    strlcpy(otaMsg, "could not reach github", sizeof(otaMsg));
    return;
  }
  otaTrim(body);
  if (!body[0] || strlen(body) > 8) {
    strlcpy(otaMsg, "bad version file", sizeof(otaMsg));
    return;
  }
  strlcpy(otaLatest, body, sizeof(otaLatest));
  if (verPack(otaLatest) > verPack(ASTRO_VER)) {
    otaNewer = true;
    snprintf(otaMsg, sizeof(otaMsg), "version  %s  tap INSTALL", otaLatest);
  } else
    strlcpy(otaMsg, "newest copy is already on this desk", sizeof(otaMsg));
}
static void otaFlash() {
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(otaMsg, "join wifi first", sizeof(otaMsg));
    return;
  }
  if (!otaNewer) {
    strlcpy(otaMsg, "check for updates first", sizeof(otaMsg));
    return;
  }
  if (!esp_ota_get_next_update_partition(NULL)) {
    strlcpy(otaMsg, "no update slot", sizeof(otaMsg));
    return;
  }
  strlcpy(otaMsg, "downloading  do not unplug", sizeof(otaMsg));
  otaPct = 0;
  otaDirty = true;
  char url[200];
  snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%s/%s/main/ota/%s", ASTRO_GH_USER, ASTRO_GH_REPO,
           ASTRO_GH_BIN);
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  http.setUserAgent("AstroDesk/1.0 (desk display)");
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(180000);
  if (!http.begin(cli, url)) {
    strlcpy(otaMsg, "download fail", sizeof(otaMsg));
    return;
  }
  int code = http.GET();
  int len = http.getSize();
  if (code != 200 || len < 100000) {
    http.end();
    strlcpy(otaMsg, code != 200 ? "download fail" : "file too small", sizeof(otaMsg));
    return;
  }
  if (!Update.begin((size_t)len)) {
    http.end();
    strlcpy(otaMsg, "flash begin fail", sizeof(otaMsg));
    return;
  }
  WiFiClient *stream = http.getStreamPtr();
  size_t got = 0;
  uint8_t buf[1024];
  while (got < (size_t)len) {
    if (netHold) {
      Update.abort();
      http.end();
      strlcpy(otaMsg, "update stopped", sizeof(otaMsg));
      return;
    }
    int n = stream->readBytes(buf, sizeof(buf));
    if (n <= 0) {
      if (!http.connected()) break;
      delay(2);
      continue;
    }
    if (Update.write(buf, (size_t)n) != (size_t)n) {
      Update.abort();
      http.end();
      strlcpy(otaMsg, "flash write fail", sizeof(otaMsg));
      return;
    }
    got += (size_t)n;
    otaPct = (int)((got * 100UL) / (size_t)len);
    otaDirty = true;
  }
  http.end();
  if (got != (size_t)len || !Update.end(true)) {
    strlcpy(otaMsg, "download fail", sizeof(otaMsg));
    return;
  }
  otaPct = 100;
  strlcpy(otaMsg, "done  rebooting", sizeof(otaMsg));
  otaDirty = true;
  delay(800);
  ESP.restart();
}
void askOta(int kind) {
  if (otaBusy || netBusy) return;
  otaKind = kind;
  otaPct = kind == 2 ? 0 : -1;
  strlcpy(otaMsg, kind == 1 ? "checking github" : "starting update", sizeof(otaMsg));
  netPend = NET_OTA;
  otaDirty = true;
}

static void pullCrew() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!httpGetCap("https://corquaid.github.io/international-space-station-APIs/JSON/people-in-space.json", netBuf,
                  PACK_CAP, "application/json", true))
    return;
  if (!strstr(netBuf, "\"people\"")) return;
  char nextNames[8][22];
  char nextNation[8][20];
  int n = 0;
  const char *p = netBuf;
  const char *end = netBuf + strlen(netBuf);
  while (n < 8 && (p = strstr(p, "\"name\""))) {
    const char *nx = strstr(p + 6, "\"name\"");
    const char *colon = strchr(p, ':');
    char name[22];
    if (!colon || colon >= end || (nx && colon > nx) ||
        !copyQuoted(colon + 1, end, name, sizeof(name), nullptr) || !name[0]) {
      p += 6;
      continue;
    }
    char nation[20] = "";
    const char *cy = strstr(p, "\"country\"");
    if (cy && (!nx || cy < nx)) {
      const char *cc = strchr(cy, ':');
      if (cc && (!nx || cc < nx)) copyQuoted(cc + 1, end, nation, sizeof(nation), nullptr);
    }
    const char *iss = strstr(p, "\"iss\"");
    bool onIss = false;
    if (iss && (!nx || iss < nx)) {
      const char *ic = strchr(iss, ':');
      if (ic && (!nx || ic < nx)) {
        const char *v = ic + 1;
        while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
        onIss = !strncmp(v, "true", 4);
      }
    }
    if (onIss) {
      strlcpy(nextNames[n], name, sizeof(nextNames[n]));
      strlcpy(nextNation[n], nation, sizeof(nextNation[n]));
      n++;
    }
    p += 6;
  }
  if (!n) return;
  for (int i = 0; i < n; i++) {
    strlcpy(issCrew[i], nextNames[i], sizeof(issCrew[i]));
    strlcpy(issNation[i], nextNation[i], sizeof(issNation[i]));
  }
  nIssCrew = n;
}

void netTask(void *) {
  for (;;) {
    if (netPend && wifiOk && page != PG_WIFI) {
      int job = netPend;
      if (netHold && job != NET_GEO) {
        netPend = NET_NONE;
      } else {
        netPend = NET_NONE;
        netBusy = true;
        bool drew = false;
        if (job == NET_PACK && placeSet) drew = pullPack(placeLat, placeLon);
        else if (job == NET_GEO) drew = pullGeo();
        else if (job == NET_SATS) pullSats();
        else if (job == NET_LAUNCH) pullLaunch();
        else if (job == NET_SPACE) pullSpace();
        else if (job == NET_CREW) pullCrew();
        else if (job == NET_OTA) {
          otaBusy = true;
          if (otaKind == 1) otaCheck();
          else if (otaKind == 2) otaFlash();
          otaBusy = false;
          otaKind = 0;
          if (page == PG_ABOUT) otaDirty = true;
        }
        netBusy = false;
        if (job == NET_PACK && drew && page == PG_HOME) homeDirty = true;
        if (job == NET_PACK && drew && page == PG_CLOUD) cloudDirty = true;
        if (job == NET_GEO && page == PG_PLACE) placeDirty = true;
        if (job == NET_LAUNCH && page == PG_LAUNCH) launchDirty = true;
        if (job == NET_CREW && (page == PG_SATS || page == PG_CRAFT)) crewDirty = true;
        if (job == NET_SPACE && page == PG_SPACE) spaceDirty = true;
        if (job == NET_SPACE && page == PG_HOME) homeDirty = true;
        if (job == NET_SPACE && page == PG_SOLAR) solarDirty = true;
        if (job == NET_SPACE && page == PG_NEO) neoDirty = true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

void serviceWifi() {
  if (wifiSsid[0] == 0) {
    wifiOk = false;
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiOk) {
      wifiOk = true;
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      applyPlaceTz();
      if (placeSet && !netHold && page != PG_WIFI) {
        nextPackAt = millis() + 600000UL;
        nextSatAt = millis();
        nextLaunchAt = millis();
        nextSpaceAt = millis();
        netPend = NET_PACK;
      }
    }
    return;
  }
  wifiOk = false;
  if (page == PG_WIFI) return;
  if (millis() - lastWifiTry < 30000UL) return;
  lastWifiTry = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(wifiSsid, wifiPass);
}
void serviceFetch() {
  if (otaBusy || netPend == NET_OTA) return;
  if (!wifiOk || !placeSet || netBusy || netPend || netHold) return;
  if (page == PG_WIFI || page == PG_PLACE || page == PG_SKY || page == PG_SATS || page == PG_SPACE) return;
  uint32_t now = millis();
  if ((int32_t)(now - nextPackAt) >= 0) {
    nextPackAt = now + 600000UL;
    netPend = NET_PACK;
    return;
  }
  if ((int32_t)(now - nextSpaceAt) >= 0) {
    nextSpaceAt = now + 600000UL;
    netPend = NET_SPACE;
    return;
  }
  if (!lastTle || now - lastTle > 21600000UL) {
    if ((int32_t)(now - nextSatAt) >= 0) {
      nextSatAt = now + 1800000UL;
      netPend = NET_SATS;
      return;
    }
  }
  if (liveClock(nullptr) && (int32_t)(now - nextLaunchAt) >= 0) {
    nextLaunchAt = now + 1800000UL;
    netPend = NET_LAUNCH;
    return;
  }
  if (liveClock(nullptr) && (int32_t)(now - nextCrewAt) >= 0) {
    nextCrewAt = now + 86400000UL;
    netPend = NET_CREW;
  }
}
void serviceClock() {
  if (netBusy || page != PG_HOME || homeDirty) return;
  int key = clockKey();
  if (key == shownKey) return;
  int prev = shownKey;
  shownKey = key;
  bool dayFlip = prev >= 0 && key >= 0 && prev / 1440 != key / 1440;
  if (dayFlip) {
    lastSun = 0;
    lastBody = 0;
    drawHome();
  } else {
    drawDateLine();
    drawClock();
  }
}
void connectWifi() {
  loadPrefs();
  bool noborrow = false;
  prefs.begin("astro7x", true);
  noborrow = prefs.getBool("noborrow", false);
  prefs.end();
  if (!noborrow && !wifiSsid[0] && !placeSet) {
    if (borrowDesks()) savePrefs();
  }
  applyPlaceTz();
  cols();
  applyBl();
  if (wifiSsid[0] == 0) {
    backTo = PG_HOME;
    openWifi();
    return;
  }
  gfx->fillScreen(COL_BG);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("linking", CX, 220);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(wifiSsid, wifiPass);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(50);
  wifiOk = WiFi.status() == WL_CONNECTED;
  lastWifiTry = millis();
  if (wifiOk) {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    applyPlaceTz();
  }
  if (!placeSet) {
    backTo = PG_HOME;
    placeTyping = true;
    drawPlace();
    return;
  }
  if (wifiOk) {
    nextPackAt = millis() + 600000UL;
    nextSatAt = millis();
    nextLaunchAt = millis();
    nextSpaceAt = millis();
    netPend = NET_PACK;
  }
  drawHome();
}

void setup() {
  Serial.begin(115200);
  boardIoBegin();
  rgbBus = new Arduino_ESP32RGBPanel(
      5, 3, 46, 7,
      1, 2, 42, 41, 40,
      39, 0, 45, 48, 47, 21,
      14, 38, 18, 17, 10,
      0, 8, 4, 8,
      0, 8, 4, 8,
      1, 12500000, false, 0, 0, 800 * 10);
  gfx = new Arduino_RGB_Display(PW, PH, rgbBus, 0, true);
  if (!gfx->begin()) {
    Serial.println("gfx begin failed");
    return;
  }
  gfxOk = true;
  gfx->setTextWrap(false);
  cols();
  gfx->fillScreen(COL_BG);
  gfx->setTextColor(COL_HOT, COL_BG);
  gfx->setTextSize(2);
  centre("opening the sky", CX, 220);
  sdMount();
  applyBl();
  seedTles();
  buildWorld();
  xTaskCreatePinnedToCore(netTask, "astro7net", 32768, nullptr, 1, nullptr, 0);
  connectWifi();
  lastTap = millis();
}

void loop() {
  if (!gfxOk) return;
  pollWifiScan();
  serviceWifi();
  if (page == PG_WIFI || page == PG_PLACE) {
    if (placeDirty && !netBusy && page == PG_PLACE) {
      placeDirty = false;
      drawPlace();
    }
    int x, y;
    if (tapReady(&x, &y)) onTap(x, y);
    delay(8);
    return;
  }
  serviceFetch();
  if (!netBusy && page == PG_HOME && !homeDirty && placeSet && placeTz[0] && liveClock(nullptr) &&
      (!lastSun || millis() - lastSun > 1800000UL))
    drawHome();
  bool moved = false;
  if (!netBusy && (page == PG_HOME || page == PG_SATS || page == PG_SCOPES ||
                   (page == PG_CRAFT && (factKind == 0 || factKind == 2))))
    moved = ensureSats(false);
  if (homeDirty && !netBusy && page == PG_HOME) {
    homeDirty = false;
    drawHome();
  } else if (moved && !netBusy && page == PG_HOME)
    drawHomeLower();
  if (skyDirty && !netBusy && page == PG_SKY) drawSky();
  if (satsDirty && !netBusy && page == PG_SATS) drawSats();
  if (crewDirty && !netBusy && page == PG_SATS) drawSats();
  if (crewDirty && !netBusy && page == PG_CRAFT && factKind == 0) drawCraft();
  if (launchDirty && !netBusy && page == PG_LAUNCH) {
    launchDirty = false;
    drawLaunch();
  }
  if (spaceDirty && !netBusy && page == PG_SPACE) drawSpace();
  if (cloudDirty && !netBusy && page == PG_CLOUD) drawClouds();
  if (neoDirty && !netBusy && page == PG_NEO) drawNeo();
  if (solarDirty && !netBusy && page == PG_SOLAR) drawSolar();
  if (page == PG_SKY && !netBusy && !otaBusy && !skyDirty && lastSkyDraw && millis() - lastSkyDraw > 60000UL) drawSky();
  if (page == PG_SATS && !netBusy && !otaBusy && !satsDirty && lastSatDraw && millis() - lastSatDraw > 60000UL) drawSats();
  if (!netBusy && !otaBusy && lastBody && millis() - lastBody > 60000UL) {
    if (page == PG_NIGHT || page == PG_PLANETS || page == PG_MOON || page == PG_BODY || page == PG_CRAFT ||
        page == PG_ROCKS || page == PG_COMETS || page == PG_SHOWERS || page == PG_SPACE || page == PG_METEOR ||
        page == PG_LAUNCH || page == PG_SCOPES || page == PG_DARK || page == PG_CLOUD || page == PG_NEO ||
        page == PG_SOLAR) {
      lastBody = 0;
      if (page == PG_NIGHT) drawTonight();
      else if (page == PG_PLANETS) drawPlanets();
      else if (page == PG_MOON) drawMoon();
      else if (page == PG_BODY) drawBody();
      else if (page == PG_CRAFT) drawCraft();
      else if (page == PG_ROCKS) drawRocks();
      else if (page == PG_COMETS) drawComets();
      else if (page == PG_SHOWERS) drawShowers();
      else if (page == PG_SPACE) drawSpace();
      else if (page == PG_METEOR) drawMeteor();
      else if (page == PG_LAUNCH) drawLaunch();
      else if (page == PG_DARK) drawDark();
      else if (page == PG_CLOUD) drawClouds();
      else if (page == PG_NEO) drawNeo();
      else if (page == PG_SOLAR) drawSolar();
      else drawScopes();
      if (!lastBody) lastBody = millis();
    }
  }
  if (page == PG_ABOUT && otaDirty) {
    static int shownOta = -2;
    if (!(otaBusy && otaPct == shownOta)) {
      shownOta = otaPct;
      if (otaBusy) drawOtaStatus();
      else drawAbout();
    }
    otaDirty = false;
  }
  serviceClock();
  int x, y;
  if (tapReady(&x, &y)) onTap(x, y);
  delay(8);
}
