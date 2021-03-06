//
// Created by gubin on 17-12-5.
//


#include "recipe.h"

#if !defined(_WIN32)
#include <unistd.h>
#include <signal.h>
#endif

static int end = 0;

const char *CLIENT_ID = "23f901e0-ccc3-11e7-bf1a-59e9355b22c6";


const int RECIPE_PARAM_LEN = 5;


const int RECIPE_LEN = 5;


#if !defined(_WIN32)
static void signal_handler(int sig_num) {
    signal(sig_num, signal_handler);  // Reinstantiate signal handler
    end = 1;
}
#endif



int main(){

#if !defined(_WIN32)
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
#endif

    init_device_recipes(CLIENT_ID);

    IOTSEED_LOGGER *log = iotseed_create_console_log("111",CLIENT_ID);

    for(int i=0; i < RECIPE_LEN; ++i){
        IOTSEED_RECIPE* r  = create_recipe();
        for(int j=0; j < RECIPE_PARAM_LEN; ++j){
            double value = 14;
            create_recipe_param(r, "acc", "m/s/s", &value, R_VAL_DOUBLE_T);
        }
    }

    active_recipe(1);

    IOTSEED_RECIPE *t = get_actived_recipe();

    printf("%d\n",t->Index);

    write_device_recipes(log);


    while(!end){
#if !defined(_WIN32)
        sleep(1);
#else
		Sleep(1000);
#endif
    }

}

