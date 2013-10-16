#include <windows.h>


struct desktop_driver;



struct driver_rect_list {
    int  position;
    char  * buf;
    struct driver_rect  * head;
    struct driver_rect  *tail;
};
struct  driver_rect {
    LONG left;
    LONG right;
    LONG top;
    LONG bottom;
    struct driver_rect * next;
};

struct  desktop_driver;
struct  driver_rect;
struct driver_rect_list;
struct desktop_driver *  alloc_driver();
int start_driver(struct desktop_driver *);
int stop_free_driver(struct desktop_driver * driver);
struct driver_rect_list * get_rect_list(struct desktop_driver * driver,int position);
int free_rect_list(struct driver_rect_list * );


