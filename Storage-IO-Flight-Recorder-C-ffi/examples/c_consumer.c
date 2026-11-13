#include "StorageIOFlightRecorderJsonSDK.h"
#include <string.h>
int main(void){uint64_t session=0;if(storage_io_flight_recorder_json_open(&session))return 1;char *reply=storage_io_flight_recorder_json_call(session,"{",1);if(!reply){storage_io_flight_recorder_json_close(session);return 2;}int failed=strstr(reply,"\"ok\":false")==0;storage_io_flight_recorder_json_free(reply);if(storage_io_flight_recorder_json_close(session))return 3;return failed;}
