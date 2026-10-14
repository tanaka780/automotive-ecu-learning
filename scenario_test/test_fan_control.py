"""冷却ファン制御・水温センサ故障時の冷却ファン制御（docs/scenarios.md）の結合テスト。

fixture.txtのTEMP@N行で実行の途中に水温を変え、sensor_sim_virtual（時間を仮想化した検証用のビルド）を
外から実行して期待結果と照合する。`make scenario`で実行する。仮想時間版はタスクが常に周期ちょうどに動き、
同じ入力なら出力が毎回同じになるので、「何秒目にファンがON/OFFするか」まで照合できる。
実時間のsensor_simでは、タスクの遅れで切り替わるサンプルがずれることがあり、秒単位の照合には使えない。

水温センサの確定の瞬間にファンが一瞬OFFになる誤り（Day58で修正）は、タスクの開始がずれたときにしか
起きないため、このテストでは見張れない。test/test_fan_ecu.cの単体テストが見張る。
復帰の瞬間の誤り（同じくDay58で修正）は、ずれが無い並びでも起きるので、FanSensorFaultTestで見張る。
"""
import os
import sys
import unittest

# リポジトリ直下からモジュール名で実行した場合（python3 -m unittest scenario_test.test_xxx）も
# sensor_sim_runnerが見つかるよう、このフォルダをimportの検索先に加える
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sensor_sim_runner import SENSOR_SIM_VIRTUAL, run_sensor_sim

# 70℃ → 4秒目80（ON閾値ちょうど） → 78・76（閾値の間） → 7秒目75（OFF閾値ちょうど） → 79（閾値の間） → 9秒目80。
# 90℃を超えると水温センサの故障判定に入るので、90℃以下だけを使う
HYSTERESIS_FIXTURE = (
    "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=70\n"
    "TEMP@4=80\nTEMP@5=78\nTEMP@6=76\nTEMP@7=75\nTEMP@8=79\nTEMP@9=80\n"
    "IGNITION=ON\n"
)

# 95℃で3秒目に故障確定 → 8秒目から78℃（閾値の間・正常範囲）で10秒目に復帰 → 14秒目から70℃（OFF閾値以下）
SENSOR_FAULT_FIXTURE = (
    "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=95\n"
    "TEMP@8=78\nTEMP@14=70\n"
    "IGNITION=ON\n"
)


def split_samples(lines):
    """標準出力の行を、"[Sample NN]"ごとに{サンプル番号: その間に出た行のリスト}へ分ける。

    "--- Stats"以降（サンプルループ終了後の出力）はどのサンプルにも含めない。
    test_fault_occurrence.pyにも同じ関数がある（使うのが2か所のうちは、それぞれに置く）。
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


def fan_events(samples):
    """ファン出力が変わった行（"[FAN] Fan ON (...)"/"[FAN] Fan OFF"）を{サンプル番号: 行}で返す。"""
    events = {}
    for number, lines in samples.items():
        for line in lines:
            if line.startswith("[FAN] Fan "):
                events[number] = line
    return events


def meter_value(samples, number, message, key):
    """number秒目のメーター表示（"[METER] message[...] ... key=値"）から、keyの値を返す。無ければNone。"""
    for line in samples[number]:
        if line.startswith("[METER] " + message + "["):
            for item in line.split():
                if item.startswith(key + "="):
                    return int(item[len(key) + 1:])
    return None


class FanHysteresisTest(unittest.TestCase):
    """冷却ファン制御：水温の上下に対し、ON閾値でON・閾値の間は保持・OFF閾値でOFFになる。"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": HYSTERESIS_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)
        cls.events = fan_events(cls.samples)

    def test_runs_to_the_end(self):
        """前提：20サンプル最後まで実行され、終了コードは0"""
        self.assertEqual(0, self.result.returncode)
        self.assertEqual(list(range(1, 21)), sorted(self.samples))

    def test_temperature_follows_fixture(self):
        """前提：TEMP@N行どおりに、ファン制御ECUへ届く水温が変わる"""
        expected = [70, 70, 70, 80, 78, 76, 75, 79] + [80] * 12
        actual = [meter_value(self.samples, n, "EngineStatus", "temp") for n in range(1, 21)]
        self.assertEqual(expected, actual)

    def test_fan_switches_only_at_thresholds(self):
        """ON閾値(80)の4秒目でON、OFF閾値(75)の7秒目でOFF、再びON閾値の9秒目でON。それ以外では切り替わらない"""
        self.assertEqual({4: "[FAN] Fan ON (temp)", 7: "[FAN] Fan OFF", 9: "[FAN] Fan ON (temp)"}, self.events)

    def test_meter_shows_fan_state_one_sample_later(self):
        """ファン状態がメーターECUに届き、表示される（ファン状態の送信は1000ms周期のため、1秒遅れで届く）"""
        expected = [0, 0, 0, 0, 1, 1, 1, 0, 0] + [1] * 11
        actual = [meter_value(self.samples, n, "FanStatus", "fan") for n in range(1, 21)]
        self.assertEqual(expected, actual)

    def test_no_sensor_fault(self):
        """前提：90℃以下なので水温センサの故障判定は出ず、通信の途絶も記録されない"""
        self.assertFalse([line for line in self.lines if line.startswith("[FAULT]")])
        for line in self.lines:
            if line.startswith("[FAN-DTC]") or line.startswith("[CAN-DTC]"):
                self.assertIn("NONE", line)


class FanSensorFaultTest(unittest.TestCase):
    """水温センサ故障時の冷却ファン制御：確定中も復帰後もファンを止めず、水温が下がってからOFFにする。"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": SENSOR_FAULT_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)
        cls.events = fan_events(cls.samples)

    def test_runs_to_the_end(self):
        """前提：20サンプル最後まで実行され、終了コードは0"""
        self.assertEqual(0, self.result.returncode)
        self.assertEqual(list(range(1, 21)), sorted(self.samples))

    def test_degraded_and_recovered(self):
        """前提：95℃が3回続いた3秒目に確定し、78℃が3回続いた10秒目に復帰する"""
        self.assertIn("[FAULT] Degraded: Temp", self.samples[3])
        self.assertIn("[FAULT] Recovered: Temp", self.samples[10])

    def test_substituted_temperature_is_sent(self):
        """前提：確定中（3〜9秒目）は、ファン制御ECUへ届く水温がフェイルセーフ値(25)に差し替わる"""
        expected = [95, 95] + [25] * 7 + [78] * 4 + [70] * 7
        actual = [meter_value(self.samples, n, "EngineStatus", "temp") for n in range(1, 21)]
        self.assertEqual(expected, actual)

    def test_fan_not_turned_off_by_substituted_temperature(self):
        """1秒目に95℃でONになり、確定中も、閾値の間の78℃で復帰した後もOFFにしない。
        OFFになるのは、OFF閾値以下の70℃になった14秒目だけ"""
        self.assertEqual({1: "[FAN] Fan ON (temp)", 14: "[FAN] Fan OFF"}, self.events)

    def test_meter_shows_fan_on_until_temperature_drops(self):
        """メーターECUには、2〜14秒目はON、15秒目からOFFが届く（1秒遅れ）"""
        expected = [0] + [1] * 13 + [0] * 6
        actual = [meter_value(self.samples, n, "FanStatus", "fan") for n in range(1, 21)]
        self.assertEqual(expected, actual)

    def test_no_link_lost(self):
        """前提：通信の途絶は記録されない（メーターの表示が、途絶中の古い値ではなく届いた値であること）"""
        for line in self.lines:
            if line.startswith("[FAN-DTC]") or line.startswith("[CAN-DTC]"):
                self.assertIn("NONE", line)


if __name__ == "__main__":
    unittest.main()
