#include <stdio.h>
#include "config.h"
#include "test_common.h"

/* 本番のconfig.txtを壊さないよう、テスト専用のファイル名を使う */
#define TEST_CONFIG_FILENAME "test_config.txt"

/* テキストをそのままファイルへ書き込む（config_loadを経由せず、任意の内容のファイルを再現するため） */
static void write_raw(const char *filename, const char *text) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        return;   /* テスト環境で書き込めない場合は何もしない（通常は起こらない） */
    }
    fputs(text, fp);
    fclose(fp);
}

/* 全9キーを含むファイルを読み込むと、全てのフィールドに反映されることを確認する */
static void test_load_all_keys(void) {
    write_raw(TEST_CONFIG_FILENAME,
        "ALERT_SPEED_MAX=90\n"
        "ALERT_RPM_MAX=5500\n"
        "ALERT_TEMP_MAX=95\n"
        "STATUS_SPEED_WARN=70\n"
        "STATUS_SPEED_CRIT=90\n"
        "STATUS_RPM_WARN=4500\n"
        "STATUS_RPM_CRIT=5500\n"
        "STATUS_TEMP_WARN=85\n"
        "STATUS_TEMP_CRIT=95\n");

    ConfigData config;
    config_init(&config);
    bool ok = config_load(&config, TEST_CONFIG_FILENAME);

    test_check("全キー読み込み: 読み込みは成功する", ok);
    test_check("全キー読み込み: alert_speed_maxが反映される", config.alert_speed_max == 90);
    test_check("全キー読み込み: alert_rpm_maxが反映される", config.alert_rpm_max == 5500);
    test_check("全キー読み込み: alert_temp_maxが反映される", config.alert_temp_max == 95);
    test_check("全キー読み込み: status_speed_warnが反映される", config.status_speed_warn == 70);
    test_check("全キー読み込み: status_speed_critが反映される", config.status_speed_crit == 90);
    test_check("全キー読み込み: status_rpm_warnが反映される", config.status_rpm_warn == 4500);
    test_check("全キー読み込み: status_rpm_critが反映される", config.status_rpm_crit == 5500);
    test_check("全キー読み込み: status_temp_warnが反映される", config.status_temp_warn == 85);
    test_check("全キー読み込み: status_temp_critが反映される", config.status_temp_crit == 95);
}

/* 未知のキー・値欠落・数値以外の行は無視され、正常な行だけが反映されることを確認する */
static void test_load_ignores_invalid_lines(void) {
    write_raw(TEST_CONFIG_FILENAME,
        "UNKNOWN_KEY=123\n"
        "ALERT_SPEED_MAX=\n"
        "STATUS_TEMP_WARN=abc\n"
        "ALERT_RPM_MAX=6000\n");

    ConfigData config;
    config_init(&config);
    bool ok = config_load(&config, TEST_CONFIG_FILENAME);

    const ConfigData *def = test_default_config();

    test_check("不正行の無視: 読み込みは成功する(ファイルは存在する)", ok);
    test_check("不正行の無視: 正常な行(ALERT_RPM_MAX)は反映される", config.alert_rpm_max == 6000);
    test_check("不正行の無視: 値欠落の行の影響を受けずalert_speed_maxはデフォルトのまま",
          config.alert_speed_max == def->alert_speed_max);
    test_check("不正行の無視: 数値以外の行の影響を受けずstatus_temp_warnはデフォルトのまま",
          config.status_temp_warn == def->status_temp_warn);
}

/* 値域外の値は無視され、他の正常な値は反映されることを確認する */
static void test_load_ignores_out_of_range_values(void) {
    write_raw(TEST_CONFIG_FILENAME,
        "ALERT_SPEED_MAX=200\n"
        "STATUS_TEMP_WARN=10\n"
        "LOG_LEVEL=5\n"
        "ALERT_RPM_MAX=6000\n");

    ConfigData config;
    config_init(&config);
    bool ok = config_load(&config, TEST_CONFIG_FILENAME);

    const ConfigData *def = test_default_config();

    test_check("値域外の無視: 読み込みは成功する(ファイルは存在する)", ok);
    test_check("値域外の無視: 正常な行(ALERT_RPM_MAX)は反映される", config.alert_rpm_max == 6000);
    test_check("値域外の無視: speedの上限(120)を超えるalert_speed_maxはデフォルトのまま",
          config.alert_speed_max == def->alert_speed_max);
    test_check("値域外の無視: tempの下限(25)未満のstatus_temp_warnはデフォルトのまま",
          config.status_temp_warn == def->status_temp_warn);
    test_check("値域外の無視: LOG_LEVELの範囲(0〜2)外のlog_levelはデフォルトのまま",
          config.log_level == def->log_level);
}

/* 同じキーが複数回書かれた場合、後に書かれた値が採用される（後勝ち）ことを確認する */
static void test_load_duplicate_key_last_wins(void) {
    write_raw(TEST_CONFIG_FILENAME,
        "ALERT_SPEED_MAX=80\n"
        "ALERT_SPEED_MAX=120\n");

    ConfigData config;
    config_init(&config);
    bool ok = config_load(&config, TEST_CONFIG_FILENAME);

    test_check("キー重複: 読み込みは成功する", ok);
    test_check("キー重複: 後に書かれた値が採用される(後勝ち)", config.alert_speed_max == 120);
}

int main(void) {
    test_load_all_keys();
    test_load_ignores_invalid_lines();
    test_load_ignores_out_of_range_values();
    test_load_duplicate_key_last_wins();

    remove(TEST_CONFIG_FILENAME);   /* テスト専用ファイルの後片付け */

    return test_summary();
}
