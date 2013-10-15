#include <stdio.h>
#include <assert.h>
#include "desktop_driver.h"
int main(int argc, const char *argv[])
{
    struct desktop_driver * driver = start_driver();
    if(driver == NULL){
        printf("start driver error\n");
        exit(0);
    }
    struct driver_rect_list * list = get_rect_list(driver,0);
    assert(list);
    printf("position:%d\n", list->position);

    struct driver_rect_list * list1 = get_rect_list(driver,0);
    assert(list1->position > list->position);
    stop_driver(driver);
    free(list);
    free(list1);
    return 0;
}
