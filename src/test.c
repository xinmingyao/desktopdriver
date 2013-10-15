#include <stdio.h>
#include <assert.h>
#include "desktop_driver.h"
int main(int argc, const char *argv[])
{
    struct desktop_driver * driver = alloc_driver();
    int r = start_driver(driver);
	if(r!=0){
		printf("start driver error: %d \n",r);
	}
    struct driver_rect_list * list = get_rect_list(driver,0);
    assert(list);
    printf("position:%d\n", list->position);

    struct driver_rect_list * list1 = get_rect_list(driver,0);
    assert(list1->position > list->position);
    stop_free_driver(driver);
    free_rect_list(list);
    free_rect_list(list1);
    return 0;
}
