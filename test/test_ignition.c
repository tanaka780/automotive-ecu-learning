#include <stdio.h>
#include <string.h>
#include <unistd.h>   /* dup, dup2, close, STDOUT_FILENO */
#include "ignition.h"
#include "test_common.h"

/* ignition_checkの標準出力キャプチャ専用の一時ファイル（本番データとは無関係） */
#define CAPTURE_FILENAME "test_ignition_capture.txt"
#define CAPTURE_BUF_SIZE 256

/* ignition_checkの標準出力をキャプチャし、buf に格納する。
   freopenだけでは元の標準出力（端末）に戻す手段が残らないため、
   事前にdup()でfdを退避し、キャプチャ後にdup2()で復元する */
static void capture_ignition_check(const Ignition *ignition, char *buf, size_t buf_size) {
    fflush(stdout);
    int saved_fd = dup(STDOUT_FILENO);

    if (freopen(CAPTURE_FILENAME, "w", stdout) == NULL) {
        /* freopen失敗時、規格上stdoutは既に閉じられているため先に復元してから空扱いにする */
        dup2(saved_fd, STDOUT_FILENO);
        close(saved_fd);
        buf[0] = '\0';
        return;
    }

    ignition_check(ignition);
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

/* OFF→OFF（遷移なし）では何も出力されないことを確認する */
static void test_no_event_off_to_off(void) {
    Ignition ign;
    ign.previous = IGNITION_OFF;
    ign.current  = IGNITION_OFF;

    char buf[CAPTURE_BUF_SIZE];
    capture_ignition_check(&ign, buf, sizeof(buf));

    test_check("OFF->OFF: 出力なし", strlen(buf) == 0);
}

/* ON→ON（遷移なし）では何も出力されないことを確認する */
static void test_no_event_on_to_on(void) {
    Ignition ign;
    ign.previous = IGNITION_ON;
    ign.current  = IGNITION_ON;

    char buf[CAPTURE_BUF_SIZE];
    capture_ignition_check(&ign, buf, sizeof(buf));

    test_check("ON->ON: 出力なし", strlen(buf) == 0);
}

/* OFF→ON（遷移あり）で遷移イベントが出力されることを確認する */
static void test_event_off_to_on(void) {
    Ignition ign;
    ign.previous = IGNITION_OFF;
    ign.current  = IGNITION_ON;

    char buf[CAPTURE_BUF_SIZE];
    capture_ignition_check(&ign, buf, sizeof(buf));

    test_check("OFF->ON: [IGN] OFF -> ON が出力される", strstr(buf, "[IGN] OFF -> ON") != NULL);
}

/* ON→OFF（遷移あり）で遷移イベントが出力されることを確認する */
static void test_event_on_to_off(void) {
    Ignition ign;
    ign.previous = IGNITION_ON;
    ign.current  = IGNITION_OFF;

    char buf[CAPTURE_BUF_SIZE];
    capture_ignition_check(&ign, buf, sizeof(buf));

    test_check("ON->OFF: [IGN] ON -> OFF が出力される", strstr(buf, "[IGN] ON -> OFF") != NULL);
}

int main(void) {
    test_no_event_off_to_off();
    test_no_event_on_to_on();
    test_event_off_to_on();
    test_event_on_to_off();

    return test_summary();
}
