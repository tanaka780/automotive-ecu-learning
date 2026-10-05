"""設定ファイル異常時のフェイルセーフ（docs/scenarios.md）の結合テスト。

sensor_simは改修せず、外からファイルを置いて実行し、標準出力と終了コードを期待結果と照合する。
`make scenario`で実行する。1条件につきsensor_simを最後まで1回動かすため、約20秒×条件数かかる。
"""
import os
import sys
import unittest

# リポジトリ直下からモジュール名で実行した場合（python3 -m unittest scenario_test.test_xxx）も
# sensor_sim_runnerが見つかるよう、このフォルダをimportの検索先に加える
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sensor_sim_runner import run_sensor_sim  # noqa: E402

# config_initのデフォルト値（alert.h/status.h）のまま動いているときの、config_printの出力
DEFAULT_CONFIG_LINE = (
    "[CONFIG] AlertMax(speed=100,rpm=5000,temp=90) "
    "StatusWarn(speed=80,rpm=4000,temp=80) "
    "StatusCrit(speed=100,rpm=5000,temp=90)"
)

# 値が11個揃った、DTC0件のdtc_data.txt（persist.cの保存形式）
VALID_DTC_DATA = "0\n" * 11

# 全行が反映されないconfig.txt（数値でない値・"="の無い行・値域外の値）
BROKEN_CONFIG = "ALERT_SPEED_MAX=abc\nGARBAGE\nSTATUS_TEMP_CRIT=999\n"


class NoConfigFirstBootTest(unittest.TestCase):
    """config.txtが無く、dtc_data.txtも無い状態（初回起動相当）で起動する。"""

    @classmethod
    def setUpClass(cls):
        # 実行は1回だけ。以下の各テストは同じ実行結果を読むだけで、書き換えない
        cls.result = run_sensor_sim({})
        cls.lines = cls.result.stdout.splitlines()

    def test_thresholds_stay_default(self):
        """閾値はalert.h/status.hのデフォルト値のまま動作する"""
        self.assertIn(DEFAULT_CONFIG_LINE, self.lines)

    def test_log_level_stays_info(self):
        """ログレベルはデフォルト（LOG_INFO、全ログ表示）のまま動作する"""
        self.assertIn("[PERSIST] Saved DTC data", self.lines)

    def test_runs_to_the_end(self):
        """プログラムが異常終了せず、サンプルループが最後まで実行される"""
        self.assertEqual(0, self.result.returncode)
        self.assertIn("[Sample 20]", self.lines)

    def test_reports_no_config_file(self):
        """[CONFIG] No config file (using defaults)のログが表示される"""
        self.assertIn("[CONFIG] No config file (using defaults)", self.lines)

    def test_post_did_not_pass(self):
        """[POST] Self-check did not pass, continuing with defaultsが表示される"""
        self.assertIn("[POST] Self-check did not pass, continuing with defaults", self.lines)


class NoConfigWithSavedDtcTest(unittest.TestCase):
    """config.txtだけが無い状態で起動する。

    空のディレクトリではdtc_data.txtも無いため、POSTはconfig.txtの有無に関係なく失敗する。
    読み込めるdtc_data.txtを先に置き、POSTの失敗がconfig.txtによるものだと切り分ける。
    """

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"dtc_data.txt": VALID_DTC_DATA})
        cls.lines = cls.result.stdout.splitlines()

    def test_saved_dtc_is_loaded(self):
        """前提：dtc_data.txtは読み込めている"""
        self.assertIn("[PERSIST] Loaded saved DTC data", self.lines)

    def test_post_did_not_pass(self):
        """config.txtが無いだけで[POST] Self-check did not pass, continuing with defaultsになる"""
        self.assertIn("[POST] Self-check did not pass, continuing with defaults", self.lines)


class BrokenConfigTest(unittest.TestCase):
    """config.txtはあるが内容が壊れている状態で起動する（dtc_data.txtは読み込める）。"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"config.txt": BROKEN_CONFIG, "dtc_data.txt": VALID_DTC_DATA})
        cls.lines = cls.result.stdout.splitlines()

    def test_reports_loaded_config_file(self):
        """[CONFIG] Loaded config fileが表示される"""
        self.assertIn("[CONFIG] Loaded config file", self.lines)

    def test_thresholds_stay_default(self):
        """解釈できない行・値域外の値は無視され、閾値はデフォルト値のまま動作する"""
        self.assertIn(DEFAULT_CONFIG_LINE, self.lines)

    def test_runs_to_the_end(self):
        """プログラムが異常終了せず、サンプルループが最後まで実行される"""
        self.assertEqual(0, self.result.returncode)
        self.assertIn("[Sample 20]", self.lines)

    def test_post_passed(self):
        """ファイルは読み込めているため[POST] Self-check passedになる"""
        self.assertIn("[POST] Self-check passed", self.lines)


if __name__ == "__main__":
    unittest.main()
