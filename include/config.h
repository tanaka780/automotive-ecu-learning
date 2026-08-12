/* インクルードガード */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>   /* uint8_t / uint16_t を使うために必要 */
#include <stdbool.h>  /* bool を使うために必要 */

/* 設定ファイルのデフォルトのファイル名 */
#define CONFIG_FILENAME "config.txt"

/* alert.h/status.hの閾値マクロを置き換える設定値。フィールド名はマクロ名を小文字にしたもの */
typedef struct {
    uint8_t  alert_speed_max;
    uint16_t alert_rpm_max;
    uint8_t  alert_temp_max;
    uint8_t  status_speed_warn;
    uint8_t  status_speed_crit;
    uint16_t status_rpm_warn;
    uint16_t status_rpm_crit;
    uint8_t  status_temp_warn;
    uint8_t  status_temp_crit;
} ConfigData;

/* 設定値を、alert.h/status.hの現行マクロ値で初期化する（設定ファイルが無い/壊れている場合のデフォルト値になる） */
void config_init(ConfigData *config);
/* 指定したファイルからKEY=VALUE形式（例: ALERT_SPEED_MAX=100）で設定値を読み込む。
   ファイルが無い場合はfalseを返し、configはconfig_initの値のまま変更しない */
bool config_load(ConfigData *config, const char *filename);
/* 設定値を1行でコンソールに出力する（確認用） */
void config_print(const ConfigData *config);

#endif /* CONFIG_H */
