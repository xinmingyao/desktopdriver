#include <windows.h>
#include "DisplayEsc.h"

#define DRIVER_STRING "Mirage Driver"
#define DRIVER_STRING_ALT "DemoForge Mirage Driver"
#define MINI_PORT_NAME "dfmirage"
#define MINIPORT_REGISTRY_PATH	"SYSTEM\\CurrentControlSet\\Hardware Profiles\\Current\\System\\CurrentControlSet\\Services"
struct  desktop_driver {
    char name[30];
    struct GETCHANGESBUF  driver_buf;
    RECT  desktop_rect;
    DISPLAY_DEVICE  display_dev;
};

struct  driver_rect {
    LONG left;
    LONG right;
    LONG top;
    LONG bottom;
    struct driver_rect * next;
};

struct driver_rect_list {
    int  position;
    char  * buf;
    struct driver_rect  * head;
    struct driver_rect  *tail;
};
struct desktop_driver * start_driver();
int stop_driver(struct desktop_driver * driver);
int get_current_count(struct desktop_driver * driver_recter);
struct driver_rect_list * get_rect_list(struct desktop_driver * driver,int position);


