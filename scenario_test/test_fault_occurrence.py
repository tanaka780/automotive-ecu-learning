"""故障発生（docs/scenarios.md）の結合テスト。

fixture.txtでセンサ値とイグニッション（ON）を固定し、sensor_simを外から実行して期待結果と照合する。
`make scenario`で実行する。1条件につきsensor_simを最後まで1回動かす（本番版の条件は約20秒、仮想時間版の条件は数ミリ秒）。

このテストでは値を最後まで固定し、1サンプル目から最後まで異常（CRITICAL・値域外）が続く形で再現する。シナリオの流れのうち
「正常範囲で数回更新される」「正常範囲に戻る」「Ignition OFF」と、期待結果のうち「NORMALが3回連続すると
Recovered」は対象外（Degraded確定までを検証する）。水温だけは、fixture.txtのTEMP@N行で途中の値を変えられる
ようになり、水温のRecoveredはtest_fan_control.pyで確認している。

test_meter_shows_failsafe_value_and_fault_bitとtest_only_faulty_sensors_use_failsafe_valuesは、
メーターECU役の表示（[METER]の行）を照合している。Phase25ではメーター表示を対象外としているが、
エンジン監視ECUが送るCANの値（フェイルセーフ値・故障ビット）を確かめる手段が他に無いため残した。
3ECU化でメーターが別のプロセスになると、この2つは見直しが必要になる。

RpmSensorFaultTestは、仮想時間版（sensor_sim_virtual）で実行する。確かめたいのは、main.cが故障確定の状態から
警告灯データの中身を作る配線で、本番版と同じコードのため。仮想時間版は出力が毎回同じで、数ミリ秒で終わる。

Phase27で、水温は危険（CRITICAL）が続いても故障と確定せず、値域外（断線・ショート）が続いたときに確定する決まりに
変えた。そのため、1センサの条件は車速で確かめ、2センサの条件は車速の危険と水温のショート（TEMP=SHORT）の組み合わせにした。
ショートにしたのは、確定前の高い側の値域外の値（255）で、水温の警告が出ないこと（直前の値への差し替え）も確かめるため。
2センサの条件も、出力が毎回同じで速い仮想時間版で実行する。水温の危険が続いても確定しないこと（オーバーヒート）は、TempOverheatTestで確かめる。
"""
import os
import sys
import unittest

# リポジトリ直下からモジュール名で実行した場合（python3 -m unittest scenario_test.test_xxx）も
# sensor_sim_runnerが見つかるよう、このフォルダをimportの検索先に加える
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sensor_sim_runner import SENSOR_SIM_VIRTUAL, run_sensor_sim

# 車速だけがCRITICAL（status_checkは閾値を超えたらCRITICALなので、101以上が必要）。rpm・tempは正常範囲
SINGLE_FAULT_FIXTURE = "MODE=FIXED\nSPEED=110\nRPM=2000\nTEMP=70\nIGNITION=ON\n"

# 車速がCRITICAL、水温がショート（値域外）。rpmは正常範囲
MULTI_FAULT_FIXTURE = "MODE=FIXED\nSPEED=110\nRPM=2000\nTEMP=SHORT\nIGNITION=ON\n"

# 水温だけがCRITICAL（111以上・値域内の高い水温＝オーバーヒート）。speed・rpmは正常範囲
OVERHEAT_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\nIGNITION=ON\n"

# 回転数だけがCRITICAL（5000を超える必要がある）。speed・tempは正常範囲
RPM_FAULT_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=5500\nTEMP=70\nIGNITION=ON\n"


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
    """車速だけがCRITICALのまま、イグニッションON固定で20サンプル実行する。"""

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
        self.assertIn("[ALERT] Speed : 110 km/h  (limit: 100 km/h)", self.samples[1])

    def test_status_is_critical(self):
        """状態表示がCRITICALになり、Degraded後も状態判定は生の値のまま"""
        expected = "[STATUS] Speed: CRITICAL | RPM: NORMAL   | Temp: NORMAL  "
        self.assertIn(expected, self.samples[1])
        self.assertIn(expected, self.samples[20])

    def test_dtc_recorded_once_as_active(self):
        """該当センサのDTC発生回数が1件記録され、状態区分がACTIVEになる（他のセンサは記録されない）"""
        self.assertIn("[DTC] Speed      ACTIVE   occurrences: 1", self.lines)
        self.assertIn("[DTC] RPM        NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] TempSensor NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] Overheat   NONE     occurrences: 0", self.lines)

    def test_freeze_frame_has_raw_values(self):
        """最初にCRITICALが発生した瞬間の全センサ値（生の値）がフリーズフレームとして記録される"""
        self.assertIn("[DTC] Freeze Frame (trigger: Speed) Speed:110 km/h RPM:2000 Temp: 70 C", self.lines)

    def test_degraded_after_three_consecutive_criticals(self):
        """CRITICALが3回連続した3サンプル目で、[FAULT] Degraded: Speedが1回だけ出力される"""
        self.assertNotIn("[FAULT] Degraded: Speed", self.samples[1])
        self.assertNotIn("[FAULT] Degraded: Speed", self.samples[2])
        self.assertIn("[FAULT] Degraded: Speed", self.samples[3])
        self.assertEqual(1, self.lines.count("[FAULT] Degraded: Speed"))

    def test_alert_uses_failsafe_value_after_degraded(self):
        """Degraded後の警告はフェイルセーフ値（speed 0）で判定されるため、[ALERT]はサンプル1・2だけ"""
        alert_samples = [n for n, lines in self.samples.items()
                         if any(line.startswith("[ALERT]") for line in lines)]
        self.assertEqual([1, 2], alert_samples)

    def test_stats_use_failsafe_value_after_degraded(self):
        """統計はフェイルセーフ値で集計される（110が2回、0が18回で平均11）"""
        self.assertIn("Speed: min=  0  max=110  avg= 11 km/h", self.lines)

    def test_meter_shows_failsafe_value_and_fault_bit(self):
        """Degraded中、メーターのEngineStatusの車速はフェイルセーフ値になり、FaultStatusの車速ビットが1になる"""
        self.assertIn("[METER] EngineStatus[OK] speed=110 rpm=2000 temp=70", self.samples[2])
        self.assertIn("[METER] EngineStatus[OK] speed=0 rpm=2000 temp=70", self.samples[3])
        self.assertIn("[METER] FaultStatus[OK] speed=0 rpm=0 temp=0 overheat=0", self.samples[2])
        self.assertIn("[METER] FaultStatus[OK] speed=1 rpm=0 temp=0 overheat=0", self.samples[3])

    def test_no_recovery_while_critical_continues(self):
        """CRITICALが続いている間は復帰しない（[FAULT] Recoveredが出力されない）"""
        self.assertFalse(any(line.startswith("[FAULT] Recovered") for line in self.lines))


class MultiSensorFaultTest(unittest.TestCase):
    """車速の危険と水温のショート（値域外）が同時に続いたまま、イグニッションON固定で20サンプル実行する（仮想時間版）。"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": MULTI_FAULT_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)

    def test_runs_to_the_end_with_ignition_on(self):
        """前提：イグニッションがONに固定され、サンプルループが最後まで実行される"""
        self.assertIn("[FIXTURE] Ignition fixed: ON", self.lines)
        self.assertEqual(0, self.result.returncode)
        self.assertEqual(list(range(1, 21)), sorted(self.samples))

    def test_both_sensors_recorded_as_dtc(self):
        """車速は危険、水温は値域外と、理由が違っても、両方ともDTCとして記録される。
        水温の値域外は、オーバーヒートではなく水温センサの故障の記録になる（Phase28）"""
        self.assertIn("[STATUS] Speed: CRITICAL | RPM: NORMAL   | Temp: OUTRANGE", self.samples[1])
        self.assertIn("[DTC] Speed      ACTIVE   occurrences: 1", self.lines)
        self.assertIn("[DTC] TempSensor ACTIVE   occurrences: 1", self.lines)
        self.assertIn("[DTC] Overheat   NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] RPM        NONE     occurrences: 0", self.lines)

    def test_freeze_frame_trigger_is_earlier_sensor(self):
        """フリーズフレームの原因は、配列の並び順（speed→rpm→temp）が早い方のSpeedになる"""
        self.assertIn("[DTC] Freeze Frame (trigger: Speed) Speed:110 km/h RPM:2000 Temp:255 C", self.lines)

    def test_both_sensors_degraded_together(self):
        """両方のセンサが3サンプル目でDegradedになる（車速は危険3回、水温は値域外3回）"""
        self.assertFalse([line for line in self.samples[2] if line.startswith("[FAULT]")])
        self.assertIn("[FAULT] Degraded: Speed", self.samples[3])
        self.assertIn("[FAULT] Degraded: Temp", self.samples[3])

    def test_only_faulty_sensors_use_failsafe_values(self):
        """フェイルセーフ値に差し替わるのはDegradedのセンサだけで、rpmは生の値のまま。
        水温はフェイルセーフ値(25)を送るが、水温センサの故障中なのでメーターは「--」と表示する（Phase27）"""
        self.assertIn("[METER] EngineStatus[OK] speed=0 rpm=2000 temp=--", self.samples[3])
        self.assertIn("[METER] FaultStatus[OK] speed=1 rpm=0 temp=1 overheat=0", self.samples[3])

    def test_out_of_range_value_not_used_before_degraded(self):
        """確定前の1・2サンプル目も、ショートの値(255)は警告・送信・統計に使われない。
        値域内の値がまだ一度も無いので、直前の値の代わりにフェイルセーフ値(25)を使う（Phase27）"""
        for n in (1, 2):
            alerts = [line for line in self.samples[n] if line.startswith("[ALERT]")]
            self.assertEqual(["[ALERT] Speed : 110 km/h  (limit: 100 km/h)"], alerts)
            self.assertIn("[METER] EngineStatus[OK] speed=110 rpm=2000 temp=25", self.samples[n])
        self.assertIn("Temp : min= 25  max= 25  avg= 25 C", self.lines)


class RpmSensorFaultTest(unittest.TestCase):
    """回転数だけがCRITICALのまま、イグニッションON固定で20サンプル実行する（仮想時間版）。
    温度・速度の条件では立たない、警告灯データの回転数のビットを確かめる。"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": RPM_FAULT_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)

    def test_runs_to_the_end_with_ignition_on(self):
        """前提：イグニッションがONに固定され、サンプルループが最後まで実行される"""
        self.assertIn("[FIXTURE] Ignition fixed: ON", self.lines)
        self.assertEqual(0, self.result.returncode)
        self.assertEqual(list(range(1, 21)), sorted(self.samples))

    def test_meter_shows_failsafe_rpm_and_rpm_fault_bit(self):
        """確定前の2サンプル目は回転数の生の値とビット0、確定した3サンプル目からは
        フェイルセーフ値（rpm 800）と回転数のビット1がメーターに届く"""
        self.assertIn("[METER] EngineStatus[OK] speed=50 rpm=5500 temp=70", self.samples[2])
        self.assertIn("[METER] FaultStatus[OK] speed=0 rpm=0 temp=0 overheat=0", self.samples[2])
        self.assertIn("[FAULT] Degraded: RPM", self.samples[3])
        self.assertIn("[METER] EngineStatus[OK] speed=50 rpm=800 temp=70", self.samples[3])
        self.assertIn("[METER] FaultStatus[OK] speed=0 rpm=1 temp=0 overheat=0", self.samples[3])

    def test_other_fault_bits_never_set(self):
        """回転数の確定異常で、速度・温度のビットは一度も立たない
        （回転数のビットを、他のセンサの状態から作る誤りを見つけるため）"""
        fault_lines = [line for line in self.lines if line.startswith("[METER] FaultStatus[")]
        self.assertTrue(fault_lines)
        for line in fault_lines:
            self.assertIn("speed=0", line)
            self.assertIn("temp=0", line)


class TempOverheatTest(unittest.TestCase):
    """水温だけが値域内の危険（115℃、オーバーヒート）のまま、イグニッションON固定で20サンプル実行する（仮想時間版）。
    水温の危険が続いても、センサの故障とは確定せず、本当の水温のまま扱い、警告灯データで高温を知らせることを確かめる（Phase27）"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": OVERHEAT_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)

    def test_runs_to_the_end_with_ignition_on(self):
        """前提：イグニッションがONに固定され、サンプルループが最後まで実行される"""
        self.assertIn("[FIXTURE] Ignition fixed: ON", self.lines)
        self.assertEqual(0, self.result.returncode)
        self.assertEqual(list(range(1, 21)), sorted(self.samples))

    def test_status_stays_critical_without_degraded(self):
        """状態は最後まで危険のままで、[FAULT]の行（Degraded・Recovered）は一度も出ない"""
        expected = "[STATUS] Speed: NORMAL   | RPM: NORMAL   | Temp: CRITICAL"
        self.assertIn(expected, self.samples[1])
        self.assertIn(expected, self.samples[20])
        self.assertFalse([line for line in self.lines if line.startswith("[FAULT]")])

    def test_alert_continues_with_real_value(self):
        """差し替えないので、本当の水温で全サンプルに[ALERT]が出る"""
        for n in range(1, 21):
            self.assertIn("[ALERT] Temp  : 115 C      (limit: 110 C)", self.samples[n])

    def test_meter_shows_real_value_without_fault_bit(self):
        """メーターには最後まで本当の水温(115)が届き、水温センサの故障のビットは立たず、
        高温のビットが1サンプル目から最後まで1になる"""
        self.assertIn("[METER] EngineStatus[OK] speed=50 rpm=2000 temp=115", self.samples[3])
        self.assertIn("[METER] EngineStatus[OK] speed=50 rpm=2000 temp=115", self.samples[20])
        fault_lines = [line for line in self.lines if line.startswith("[METER] FaultStatus[")]
        self.assertTrue(fault_lines)
        for line in fault_lines:
            self.assertTrue(line.endswith("speed=0 rpm=0 temp=0 overheat=1"), line)

    def test_dtc_and_stats_use_real_value(self):
        """DTCは危険に入った瞬間に、水温センサの故障ではなくオーバーヒートの記録として1回記録され（Phase28）、
        統計も本当の水温で集計される"""
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 1", self.lines)
        self.assertIn("[DTC] TempSensor NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] Freeze Frame (trigger: Overheat) Speed: 50 km/h RPM:2000 Temp:115 C", self.lines)
        self.assertIn("Temp : min=115  max=115  avg=115 C", self.lines)

    def test_fan_on_by_real_temperature(self):
        """ファン制御ECUは、受信した本当の水温(115)で1サンプル目にONにし、最後まで止めない"""
        fan_lines = [line for line in self.lines if line.startswith("[FAN] Fan")]
        self.assertEqual(["[FAN] Fan ON (temp)"], fan_lines)
        self.assertIn("[FAN] Fan ON (temp)", self.samples[1])


# 115℃（オーバーヒート）→ 4秒目にショート（値域外）→ 6秒目に確定 → 12秒目に115℃（値域内の危険）へ戻り14秒目に復帰
OVERHEAT_THEN_SHORT_FIXTURE = (
    "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\n"
    "TEMP@4=SHORT\nTEMP@12=115\n"
    "IGNITION=ON\n"
)


class OverheatThenSensorFaultTest(unittest.TestCase):
    """オーバーヒートの途中で水温センサが壊れ、高温のまま直る（仮想時間版）。
    オーバーヒートとセンサの故障は見分けられないので、高温の通知を残して安全側に倒すことを確かめる（Phase27）"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": OVERHEAT_THEN_SHORT_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)

    def test_runs_to_the_end_with_ignition_on(self):
        """前提：イグニッションがONに固定され、サンプルループが最後まで実行される"""
        self.assertEqual(0, self.result.returncode)
        self.assertEqual(list(range(1, 21)), sorted(self.samples))

    def test_degraded_by_short_and_recovered_while_still_hot(self):
        """ショートが3回続いた6サンプル目に確定し、値域内の危険(115)が3回続いた14サンプル目に復帰する
        （高温のままでも、値域内ならセンサは直ったと判断する）"""
        fault_lines = {n: [line for line in lines if line.startswith("[FAULT]")] for n, lines in self.samples.items()}
        self.assertEqual(["[FAULT] Degraded: Temp"], fault_lines[6])
        self.assertEqual(["[FAULT] Recovered: Temp"], fault_lines[14])
        self.assertEqual(2, sum(len(lines) for lines in fault_lines.values()))

    def test_overheat_bit_stays_through_sensor_fault(self):
        """高温のビットは、ショート中も確定中も、壊れる前の115℃で判断して1のまま（Phase27）"""
        fault_lines = [line for line in self.lines if line.startswith("[METER] FaultStatus[")]
        self.assertTrue(fault_lines)
        for line in fault_lines:
            self.assertTrue(line.endswith("overheat=1"), line)

    def test_meter_temperature(self):
        """メーターの水温は、確定前のショート中（4・5サンプル目）は直前の115、確定中（6〜13）は「--」、復帰後は115"""
        expected = ["115"] * 5 + ["--"] * 8 + ["115"] * 7
        actual = []
        for n in range(1, 21):
            line = [l for l in self.samples[n] if l.startswith("[METER] EngineStatus[")][0]
            actual.append(line.split("temp=")[1])
        self.assertEqual(expected, actual)

    def test_alert_stops_only_while_degraded(self):
        """[ALERT]は、確定中（6〜13サンプル目）だけ止まる。警告はフェイルセーフ値(25)で判断するため。
        高温のビットは残るので、運転手への知らせとはずれる（READMEの既知の制約）"""
        alert_samples = [n for n, lines in self.samples.items()
                         if any(line.startswith("[ALERT]") for line in lines)]
        self.assertEqual(list(range(1, 6)) + list(range(14, 21)), alert_samples)

    def test_fan_stays_on(self):
        """ファンは1サンプル目にONになり、最後まで止まらない"""
        fan_lines = [line for line in self.lines if line.startswith("[FAN] Fan")]
        self.assertEqual(["[FAN] Fan ON (temp)"], fan_lines)

    def test_records_kept_separately(self):
        """記録は2つに分かれる。オーバーヒートは、判断に使える水温（ショート中も壊れる前の115℃）で決めるので、
        ショートを挟んでもACTIVEのまま1回。水温センサの故障は、ショートで1回数え、直ったのでHISTORY（Phase28）"""
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 1", self.lines)
        self.assertIn("[DTC] TempSensor HISTORY  occurrences: 1", self.lines)
        self.assertIn("[DTC] Freeze Frame (trigger: Overheat) Speed: 50 km/h RPM:2000 Temp:115 C", self.lines)


# 115℃（オーバーヒート）→ 5秒目に100℃（危険の閾値以下・注意）へ下がる
OVERHEAT_CLEARS_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\nTEMP@5=100\nIGNITION=ON\n"


class OverheatClearsTest(unittest.TestCase):
    """水温が危険の閾値以下に下がると、高温の通知が解除される（仮想時間版、Phase27）"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": OVERHEAT_CLEARS_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()
        cls.samples = split_samples(cls.lines)

    def test_overheat_bit_follows_temperature(self):
        """高温のビットは、115℃の1〜4サンプル目は1、100℃に下がった5サンプル目から0
        （どのサンプルも、その秒に届いたビットはすべて同じ値）。センサの故障の判定は出ない"""
        expected = [1] * 4 + [0] * 16
        actual = []
        for n in range(1, 21):
            bits = {line.split("overheat=")[1] for line in self.samples[n]
                    if line.startswith("[METER] FaultStatus[")}
            actual.append(int(bits.pop()) if len(bits) == 1 else bits)
        self.assertEqual(expected, actual)
        self.assertFalse([line for line in self.lines if line.startswith("[FAULT]")])


# 70℃ → 3秒目だけ115℃（オーバーヒート） → 4秒目から70℃
OVERHEAT_ONE_SECOND_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=70\nTEMP@3=115\nTEMP@4=70\nIGNITION=ON\n"


class OverheatOneSecondTest(unittest.TestCase):
    """オーバーヒートが1秒だけのとき、その秒のうちに記録する（仮想時間版、Phase28）。
    記録の確認は、判断に使える水温を更新する故障確定の後に呼ぶ。順番が逆だと1秒遅れて、
    フリーズフレームに次の秒の70℃が残る"""

    @classmethod
    def setUpClass(cls):
        cls.result = run_sensor_sim({"fixture.txt": OVERHEAT_ONE_SECOND_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()

    def test_recorded_in_the_overheat_second(self):
        """フリーズフレームは115℃の秒のもので、オーバーヒートは1回記録され、今は正常（HISTORY）"""
        self.assertEqual(0, self.result.returncode)
        self.assertIn("[DTC] Freeze Frame (trigger: Overheat) Speed: 50 km/h RPM:2000 Temp:115 C", self.lines)
        self.assertIn("[DTC] Overheat   HISTORY  occurrences: 1", self.lines)


# 警告灯データ（確定異常のビット）の送信だけを2.5〜12.5秒目に止める。ゲージデータ（水温の値）は届き続ける
DROP_FAULT_STATUS = "MODE=DROP\nTARGET=FAULT_STATUS\nSTART_MS=2500\nEND_MS=12500\n"
# 途絶中に水温センサが壊れる（5秒目に断線）。7秒目に確定し、ゲージデータの水温は常温（25）に差し替わる
BREAK_DURING_LOSS_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=70\nTEMP@5=OPEN\nIGNITION=ON\n"
NORMAL_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=70\nIGNITION=ON\n"
METER_TEMP_UNKNOWN = "[METER] EngineStatus[OK] speed=50 rpm=2000 temp=--"
METER_TEMP_70 = "[METER] EngineStatus[OK] speed=50 rpm=2000 temp=70"


class FaultStatusLostMeterTest(unittest.TestCase):
    """警告灯データが途絶している間は、届いた水温が本当の値か差し替えた常温か分からないので、メーターは「--」と表示する
    （仮想時間版、Phase29の完了後）。途絶前のビットのままだと、途絶中にセンサが壊れたとき常温（25）を表示していた"""

    @classmethod
    def setUpClass(cls):
        cls.normal = run_sensor_sim({"fixture.txt": NORMAL_FIXTURE, "can_fault.txt": DROP_FAULT_STATUS}, SENSOR_SIM_VIRTUAL)
        cls.broken = run_sensor_sim({"fixture.txt": BREAK_DURING_LOSS_FIXTURE, "can_fault.txt": DROP_FAULT_STATUS},
                                    SENSOR_SIM_VIRTUAL)

    @staticmethod
    def samples_with(result, line):
        return [n for n, s in split_samples(result.stdout.splitlines()).items() if line in s]

    def test_loss_and_recovery_samples(self):
        """前提：2つとも4サンプル目に途絶、14サンプル目に回復する"""
        for result in (self.normal, self.broken):
            self.assertEqual(0, result.returncode)
            self.assertEqual([4], self.samples_with(result, "[CAN] FaultStatus link lost"))
            self.assertEqual([14], self.samples_with(result, "[CAN] FaultStatus link recovered"))

    def test_temp_unknown_only_while_lost(self):
        """水温が正常のまま：途絶している4〜13サンプル目だけ「--」で、前後は本当の水温"""
        self.assertEqual(list(range(4, 14)), self.samples_with(self.normal, METER_TEMP_UNKNOWN))
        self.assertEqual([1, 2, 3] + list(range(14, 21)), self.samples_with(self.normal, METER_TEMP_70))

    def test_no_room_temperature_shown_when_sensor_breaks_during_loss(self):
        """途絶中にセンサが壊れても、差し替えた常温（25）を表示しない。回復後は確定異常の通知で「--」のまま"""
        lines = self.broken.stdout.splitlines()
        self.assertIn("[FAULT] Degraded: Temp", split_samples(lines)[7])
        self.assertFalse([line for line in lines if line.startswith("[METER] EngineStatus") and "temp=25" in line])
        self.assertEqual(list(range(4, 21)), self.samples_with(self.broken, METER_TEMP_UNKNOWN))


if __name__ == "__main__":
    unittest.main()
