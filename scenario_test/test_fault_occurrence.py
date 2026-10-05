"""故障発生（docs/scenarios.md）の結合テスト。

fixture.txtでセンサ値とイグニッション（ON）を固定し、sensor_simを外から実行して期待結果と照合する。
`make scenario`で実行する。1条件につきsensor_simを最後まで1回動かすため、約20秒×条件数かかる。

fixture.txtは1回の実行中ずっと同じ値しか出せないため、1サンプル目から最後までCRITICALが続く形でしか
再現できない。シナリオの流れのうち「正常範囲で数回更新される」「正常範囲に戻る」「Ignition OFF」と、
期待結果のうち「NORMALが3回連続するとRecovered」は対象外（Degraded確定までを検証する）。
"""
import os
import sys
import unittest

# リポジトリ直下からモジュール名で実行した場合（python3 -m unittest scenario_test.test_xxx）も
# sensor_sim_runnerが見つかるよう、このフォルダをimportの検索先に加える
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sensor_sim_runner import run_sensor_sim

# 水温だけがCRITICAL（status_checkは閾値を超えたらCRITICALなので、91以上が必要）。speed・rpmは正常範囲
SINGLE_FAULT_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=95\nIGNITION=ON\n"

# 車速と水温が同時にCRITICAL。rpmは正常範囲
MULTI_FAULT_FIXTURE = "MODE=FIXED\nSPEED=110\nRPM=2000\nTEMP=95\nIGNITION=ON\n"


def split_samples(lines):
    """標準出力の行を、"[Sample NN]"ごとに{サンプル番号: その間に出た行のリスト}へ分ける。

    "--- Stats"以降（統計・DTC一覧など、サンプルループ終了後の出力）はどのサンプルにも含めない。
    """
    samples = {}
    current = None
    for line in lines:
        if line.startswith("[Sample "):
            current = int(line[len("[Sample "):-1])
            samples[current] = []
        elif line.startswith("--- Stats"):
            current = None
        elif current is not None:
            samples[current].append(line)
    return samples


class SingleSensorFaultTest(unittest.TestCase):
    """水温だけがCRITICALのまま、イグニッションON固定で20サンプル実行する。"""

    @classmethod
    def setUpClass(cls):
        # 実行は1回だけ。以下の各テストは同じ実行結果を読むだけで、書き換えない
        cls.result = run_sensor_sim({"fixture.txt": SINGLE_FAULT_FIXTURE})
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)

    def test_runs_to_the_end_with_ignition_on(self):
        """前提：イグニッションがONに固定され、サンプルループが最後まで実行される"""
        self.assertIn("[FIXTURE] Ignition fixed: ON", self.lines)
        self.assertEqual(0, self.result.returncode)
        self.assertEqual(list(range(1, 21)), sorted(self.samples))

    def test_alert_when_threshold_exceeded(self):
        """閾値を超えた瞬間、[ALERT]警告ログが出力される"""
        self.assertIn("[ALERT] Temp  :  95 C      (limit: 90 C)", self.samples[1])

    def test_status_is_critical(self):
        """状態表示がCRITICALになり、Degraded後も状態判定は生の値のまま"""
        expected = "[STATUS] Speed: NORMAL   | RPM: NORMAL   | Temp: CRITICAL"
        self.assertIn(expected, self.samples[1])
        self.assertIn(expected, self.samples[20])

    def test_dtc_recorded_once_as_active(self):
        """該当センサのDTC発生回数が1件記録され、状態区分がACTIVEになる（他のセンサは記録されない）"""
        self.assertIn("[DTC] Temp  ACTIVE   CRITICAL occurrences: 1", self.lines)
        self.assertIn("[DTC] Speed NONE     CRITICAL occurrences: 0", self.lines)
        self.assertIn("[DTC] RPM   NONE     CRITICAL occurrences: 0", self.lines)

    def test_freeze_frame_has_raw_values(self):
        """最初にCRITICALが発生した瞬間の全センサ値（生の値）がフリーズフレームとして記録される"""
        self.assertIn("[DTC] Freeze Frame (trigger: Temp) Speed: 50 km/h RPM:2000 Temp: 95 C", self.lines)

    def test_degraded_after_three_consecutive_criticals(self):
        """CRITICALが3回連続した3サンプル目で、[FAULT] Degraded: Tempが1回だけ出力される"""
        self.assertNotIn("[FAULT] Degraded: Temp", self.samples[1])
        self.assertNotIn("[FAULT] Degraded: Temp", self.samples[2])
        self.assertIn("[FAULT] Degraded: Temp", self.samples[3])
        self.assertEqual(1, self.lines.count("[FAULT] Degraded: Temp"))

    def test_alert_uses_failsafe_value_after_degraded(self):
        """Degraded後の警告はフェイルセーフ値（temp 25）で判定されるため、[ALERT]はサンプル1・2だけ"""
        alert_samples = [n for n, lines in self.samples.items()
                         if any(line.startswith("[ALERT]") for line in lines)]
        self.assertEqual([1, 2], alert_samples)

    def test_stats_use_failsafe_value_after_degraded(self):
        """統計はフェイルセーフ値で集計される（95が2回、25が18回で平均32）"""
        self.assertIn("Temp : min= 25  max= 95  avg= 32 C", self.lines)

    def test_meter_shows_failsafe_value_and_fault_bit(self):
        """Degraded中、メーターのEngineStatusの水温はフェイルセーフ値になり、FaultStatusの水温ビットが1になる"""
        self.assertIn("[METER] EngineStatus[OK] speed=50 rpm=2000 temp=95", self.samples[2])
        self.assertIn("[METER] EngineStatus[OK] speed=50 rpm=2000 temp=25", self.samples[3])
        self.assertIn("[METER] FaultStatus[OK] speed=0 rpm=0 temp=0", self.samples[2])
        self.assertIn("[METER] FaultStatus[OK] speed=0 rpm=0 temp=1", self.samples[3])

    def test_no_recovery_while_critical_continues(self):
        """CRITICALが続いている間は復帰しない（[FAULT] Recoveredが出力されない）"""
        self.assertFalse(any(line.startswith("[FAULT] Recovered") for line in self.lines))


class MultiSensorFaultTest(unittest.TestCase):
    """車速と水温が同時にCRITICALのまま、イグニッションON固定で20サンプル実行する。"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": MULTI_FAULT_FIXTURE})
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)

    def test_runs_to_the_end_with_ignition_on(self):
        """前提：イグニッションがONに固定され、サンプルループが最後まで実行される"""
        self.assertIn("[FIXTURE] Ignition fixed: ON", self.lines)
        self.assertEqual(0, self.result.returncode)
        self.assertEqual(list(range(1, 21)), sorted(self.samples))

    def test_both_sensors_recorded_as_dtc(self):
        """複数センサが同時に閾値を超えた場合、両方ともDTCとして記録される"""
        self.assertIn("[DTC] Speed ACTIVE   CRITICAL occurrences: 1", self.lines)
        self.assertIn("[DTC] Temp  ACTIVE   CRITICAL occurrences: 1", self.lines)
        self.assertIn("[DTC] RPM   NONE     CRITICAL occurrences: 0", self.lines)

    def test_freeze_frame_trigger_is_earlier_sensor(self):
        """フリーズフレームの原因は、配列の並び順（speed→rpm→temp）が早い方のSpeedになる"""
        self.assertIn("[DTC] Freeze Frame (trigger: Speed) Speed:110 km/h RPM:2000 Temp: 95 C", self.lines)

    def test_both_sensors_degraded_together(self):
        """両方のセンサが3サンプル目でDegradedになる"""
        self.assertIn("[FAULT] Degraded: Speed", self.samples[3])
        self.assertIn("[FAULT] Degraded: Temp", self.samples[3])

    def test_only_faulty_sensors_use_failsafe_values(self):
        """フェイルセーフ値に差し替わるのはDegradedのセンサだけで、rpmは生の値のまま"""
        self.assertIn("[METER] EngineStatus[OK] speed=0 rpm=2000 temp=25", self.samples[3])
        self.assertIn("[METER] FaultStatus[OK] speed=1 rpm=0 temp=1", self.samples[3])


if __name__ == "__main__":
    unittest.main()
