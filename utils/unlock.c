#include "include/utils/unlock.h"

#include <linux/types.h>

void perform_unlocking(struct list_head *head) {
    unlock_data *data ;
    list_for_each_entry(data, head, stack) {
        data->unlock(data) ;
    }
}