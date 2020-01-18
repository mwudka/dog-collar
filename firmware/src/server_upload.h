#ifndef SERVER_UPLOAD_H_
#define SERVER_UPLOAD_H_

#include <sys/time.h>
#include "vendor/croaring/roaring.h"

int report_activity_history(time_t start_timestamp, roaring_bitmap_t* activity);

#endif 