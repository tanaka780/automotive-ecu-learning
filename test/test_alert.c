#include <stdio.h>
#include <string.h>
#include <unistd.h>   /* dup, dup2, close, STDOUT_FILENO */
#include "sensor.h"
#include "alert.h"
#include "test_common.h"

/* alert_checkの標準出力キャプチャ専用の一時ファイル（本番データとは無関係） */
#define CAPTURE_FILENAME "test_alert_capture.txt"
#define CAPTURE_BUF_SIZE 256

/* alert_checkの標準出力をキャプチャし、buf に格納する。
   freopenだけでは元の標準出力（端末）に戻す手段が残らないため、
   事前にdup()でfdを退避し、キャプチャ後にdup2()で復元する */
static void capture_alert_check(const VehicleSensorData *data, const ConfigData *config, char *buf, size_t buf_size) {
    fflush(stdout);
    int saved_fd = dup(STDOUT_FILENO);

    if (freopen(CAPTURE_FILENAME, "w", stdout) == NULL) {
        /* freopen失敗時、規格上stdoutは既に閉じられているため先に復元してから空扱いにする */
        dup2(saved_fd, STDOUT_FILENO);
        close(saved_fd);
        buf[0] = '\0';
        return;
    }

    alert_check(data, config);
    fflush(stdout);

    dup2(saved_fd, STDOUT_FILENO);
    close(saved_fd);

    FILE *fp = fopen(CAPTURE_FILENAME, "r");
    size_t len = 0;
    if (fp != NULL) {
        len = fread(buf, 1, buf_size - 1, fp);
        fclose(fp);
    }
    buf[len] = '\0';
    remove(CAPTURE_FILENAME);   /* キャプチャ用の一時ファイルはこの呼び出し内で都度後片付けする */
}

/* 3項目とも閾値ちょうど（超過しない）では警告が出ないことを確認する */
static void test_no_alert_at_boundary(void) {
    VehicleSensorData d;
    d.speed       = ALERT_SPEED_MAX;
    d.rpm         = ALERT_RPM_MAX;
    d.temperature = ALERT_TEMP_MAX;

    char buf[CAPTURE_BUF_SIZE];
    capture_alert_check(&d, test_default_config(), buf, sizeof(buf));

    test_check("境界値ちょうどでは警告が出ない", strlen(buf) == 0);
}

/* speedのみ閾値超過のとき、Speedの警告だけが出ることを確認する */
static void test_alert_speed_only(void) {
    VehicleSensorData d;
    d.speed       = ALERT_SPEED_MAX + 1;
    d.rpm         = ALERT_RPM_MAX;
    d.temperature = ALERT_TEMP_MAX;

    char buf[CAPTURE_BUF_SIZE];
    capture_alert_check(&d, test_default_config(), buf, sizeof(buf));

    test_check("speed超過: [ALERT] Speedが出力される", strstr(buf, "[ALERT] Speed") != NULL);
    test_check("speedのみ超過: RPMは出力されない", strstr(buf, "[ALERT] RPM") == NULL);
    test_check("speedのみ超過: Tempは出力されない", strstr(buf, "[ALERT] Temp") == NULL);
}

/* rpmのみ閾値超過のとき、RPMの警告だけが出ることを確認する */
static void test_alert_rpm_only(void) {
    VehicleSensorData d;
    d.speed       = ALERT_SPEED_MAX;
    d.rpm         = ALERT_RPM_MAX + 1;
    d.temperature = ALERT_TEMP_MAX;

    char buf[CAPTURE_BUF_SIZE];
    capture_alert_check(&d, test_default_config(), buf, sizeof(buf));

    test_check("rpm超過: [ALERT] RPMが出力される", strstr(buf, "[ALERT] RPM") != NULL);
    test_check("rpmのみ超過: Speedは出力されない", strstr(buf, "[ALERT] Speed") == NULL);
    test_check("rpmのみ超過: Tempは出力されない", strstr(buf, "[ALERT] Temp") == NULL);
}

/* temperatureのみ閾値超過のとき、Tempの警告だけが出ることを確認する */
static void test_alert_temp_only(void) {
    VehicleSensorData d;
    d.speed       = ALERT_SPEED_MAX;
    d.rpm         = ALERT_RPM_MAX;
    d.temperature = ALERT_TEMP_MAX + 1;

    char buf[CAPTURE_BUF_SIZE];
    capture_alert_check(&d, test_default_config(), buf, sizeof(buf));

    test_check("temp超過: [ALERT] Tempが出力される", strstr(buf, "[ALERT] Temp") != NULL);
    test_check("tempのみ超過: Speedは出力されない", strstr(buf, "[ALERT] Speed") == NULL);
    test_check("tempのみ超過: RPMは出力されない", strstr(buf, "[ALERT] RPM") == NULL);
}

/* 3項目同時に閾値超過のとき、3件とも警告が出ることを確認する */
static void test_alert_all_three(void) {
    VehicleSensorData d;
    d.speed       = ALERT_SPEED_MAX + 1;
    d.rpm         = ALERT_RPM_MAX + 1;
    d.temperature = ALERT_TEMP_MAX + 1;

    char buf[CAPTURE_BUF_SIZE];
    capture_alert_check(&d, test_default_config(), buf, sizeof(buf));

    test_check("3項目超過: Speedが出力される", strstr(buf, "[ALERT] Speed") != NULL);
    test_check("3項目超過: RPMが出力される", strstr(buf, "[ALERT] RPM") != NULL);
    test_check("3項目超過: Tempが出力される", strstr(buf, "[ALERT] Temp") != NULL);
}

/* 非デフォルトのConfigDataを渡すと、閾値を下げた項目は新しい閾値で警告が出て、
   閾値を上げた項目はデフォルトなら警告が出る値でも警告が出ないことを確認する */
static void test_alert_reflects_custom_config(void) {
    ConfigData cfg = *test_default_config();
    cfg.alert_speed_max = 50;      /* デフォルト100から引き下げ */
    cfg.alert_rpm_max   = 6000;    /* デフォルト5000から引き上げ */

    VehicleSensorData d;
    d.speed       = 60;              /* デフォルト閾値(100)は超えないが、下げた閾値(50)は超える */
    d.rpm         = ALERT_RPM_MAX + 1; /* デフォルト閾値(5000)は超えるが、上げた閾値(6000)は超えない */
    d.temperature = ALERT_TEMP_MAX;

    char buf[CAPTURE_BUF_SIZE];
    capture_alert_check(&d, &cfg, buf, sizeof(buf));

    test_check("非デフォルトconfig: 閾値を下げたSpeedは警告が出る", strstr(buf, "[ALERT] Speed") != NULL);
    test_check("非デフォルトconfig: 閾値を上げたRPMは警告が出ない", strstr(buf, "[ALERT] RPM") == NULL);
    test_check("非デフォルトconfig: 変更していないTempは警告が出ない", strstr(buf, "[ALERT] Temp") == NULL);
}

int main(void) {
    test_no_alert_at_boundary();
    test_alert_speed_only();
    test_alert_rpm_only();
    test_alert_temp_only();
    test_alert_all_three();
    test_alert_reflects_custom_config();

    return test_summary();
}
