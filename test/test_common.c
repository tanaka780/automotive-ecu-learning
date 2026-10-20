#include <stdio.h>
#include <stdbool.h>  /* bool を使うために必要 */
#include "test_common.h"
#include "status.h"

/* テスト実行ファイルごとに1つずつ持つカウント（test_diagとtest_persistは別バイナリなので共有されない） */
static int g_pass = 0;
static int g_fail = 0;

void test_check(const char *label, int condition) {
    if (condition) {
        printf("[PASS] %s\n", label);
        g_pass++;
    } else {
        printf("[FAIL] %s\n", label);
        g_fail++;
    }
}

const ConfigData *test_default_config(void) {
    static ConfigData config;
    static bool initialized = false;

    if (!initialized) {
        config_init(&config);
        initialized = true;
    }
    return &config;
}

void test_run_sample(DtcRecord *dtc, const VehicleSensorData *data) {
    SensorStatus status;
    status_check(&status, data, test_default_config());
    /* 本体は判断に使える水温（値域外なら直前の正しい値、faultmgr.c）で高温を決めるが、ここでは故障確定を使わないので、
       生の水温で決める。値域外のときは高温としない（値域外が続く間の高温は、diag_checkを直接呼ぶテストで確かめる） */
    diag_check(dtc, &status, data, status_is_overheat(data->temperature, test_default_config()));
}

void test_feed(DtcRecord *dtc, uint8_t speed, uint16_t rpm, uint8_t temperature) {
    VehicleSensorData d;
    d.speed       = speed;
    d.rpm         = rpm;
    d.temperature = temperature;
    test_run_sample(dtc, &d);
}

int test_summary(void) {
    printf("\n--- Test Summary: %d passed, %d failed ---\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
