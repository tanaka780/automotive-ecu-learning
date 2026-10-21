"""電源再投入（docs/scenarios.md）の結合テスト。

同じ一時ディレクトリでsensor_simを3回続けて実行し、1回目に保存されたdtc_data.txtが次の起動で
読み込まれることを、標準出力と照合して確認する。`make scenario`で実行する。約20秒×3回かかる。

- 1回目：config.txtだけを置き（dtc_data.txtは無い）、水温CRITICAL・イグニッションON固定で実行する
- 2回目：イグニッションOFF固定で実行する。OFFの間はDTCが更新されないため、最後に表示されるDTCが
  そのまま「起動直後に読み込まれた内容」になる（DTCの一覧は起動直後には表示されないため、この形で観測する）
- 3回目：1回目と違う値で水温CRITICAL・イグニッションON固定で実行する

別に、前の形（値11個）のdtc_data.txtを置いて1回起動し、壊れたファイルと同じく扱うことも確認する（Phase28）。
これは仮想時間版（sensor_sim_virtual）で実行する。確かめたいのは保存ファイルの読み込みと起動時自己診断の配線で、
本番版と同じコードのため。仮想時間版は出力が毎回同じで、数ミリ秒で終わる。

プログラムを起動し直さず、実行の途中でエンジンを切ってかけ直した場合（IGNITION@N行）も、起動し直したときと
同じく数え直し、故障の確定をやり直すことを確認する（仮想時間版。何サンプル目に確定するかまで照合するため）。
通信の途絶の途中でかけ直した場合に、受信するECUが途絶の判断をやり直すことも、同じく確認する。

通信の途絶の記録が、メーターECU役・ファン制御ECU役それぞれのファイルに保存され、次の起動で引き継がれることも、
同じ一時ディレクトリで5回続けて起動して確認する（Phase29、仮想時間版）。途絶の判断の途中経過は引き継がないこと、
点検コマンドの"clear"で3つのECUの記録が消え、消えた状態が保存されることも、同じ5回の中で確かめる。
"""
import os
import sys
import tempfile
import unittest

# リポジトリ直下からモジュール名で実行した場合（python3 -m unittest scenario_test.test_xxx）も
# sensor_sim_runnerが見つかるよう、このフォルダをimportの検索先に加える
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sensor_sim_runner import SENSOR_SIM_VIRTUAL, run_in_dir, run_sensor_sim

# 開ければ読み込めた扱いになるconfig.txt（POSTの結果をdtc_data.txtの有無だけで決めるために置く）
VALID_CONFIG = "LOG_LEVEL=0\n"

FIRST_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\nIGNITION=ON\n"
SECOND_FIXTURE = "IGNITION=OFF\n"
# フリーズフレームが上書きされていないことを見分けるため、1回目と違う値にする
THIRD_FIXTURE = "MODE=FIXED\nSPEED=60\nRPM=2000\nTEMP=119\nIGNITION=ON\n"

FIRST_FREEZE_FRAME = "[DTC] Freeze Frame (trigger: Overheat) Speed: 50 km/h RPM:2000 Temp:115 C"


class PowerCycleTest(unittest.TestCase):
    """1回目の起動で記録したDTCが、2回目・3回目の起動に引き継がれる。"""

    @classmethod
    def setUpClass(cls):
        # 3回とも同じディレクトリで実行し、前回保存されたdtc_data.txtを次の起動で読み込ませる
        with tempfile.TemporaryDirectory() as workdir:
            cls.first = run_in_dir(workdir, {"config.txt": VALID_CONFIG, "fixture.txt": FIRST_FIXTURE})
            cls.second = run_in_dir(workdir, {"fixture.txt": SECOND_FIXTURE})
            cls.third = run_in_dir(workdir, {"fixture.txt": THIRD_FIXTURE})
        cls.first_lines = cls.first.stdout.splitlines()
        cls.second_lines = cls.second.stdout.splitlines()
        cls.third_lines = cls.third.stdout.splitlines()

    def test_all_runs_complete(self):
        """前提：3回とも異常終了せず、サンプルループが最後まで実行される"""
        for result in (self.first, self.second, self.third):
            self.assertEqual(0, result.returncode)
            self.assertIn("[Sample 20]", result.stdout.splitlines())

    def test_first_boot_starts_from_no_dtc(self):
        """保存ファイルが無い初回起動は、DTC0件の初期状態から開始する（記録されるのはオーバーヒートの1件だけ）"""
        self.assertIn("[PERSIST] No saved data (first run)", self.first_lines)
        self.assertIn("[DTC] Speed      NONE     occurrences: 0", self.first_lines)
        self.assertIn("[DTC] RPM        NONE     occurrences: 0", self.first_lines)
        self.assertIn("[DTC] TempSensor NONE     occurrences: 0", self.first_lines)
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 1", self.first_lines)

    def test_dtc_saved_at_end_of_first_boot(self):
        """1回目の終了時にDTC記録がdtc_data.txtへ保存される"""
        self.assertIn("[PERSIST] Saved DTC data", self.first_lines)

    def test_post_depends_on_saved_dtc(self):
        """POSTは、dtc_data.txtが無い1回目はdid not pass、config.txtと両方読み込める2回目はpassed"""
        self.assertIn("[POST] Self-check did not pass, continuing with defaults", self.first_lines)
        self.assertIn("[POST] Self-check passed", self.second_lines)

    def test_dtc_inherited_right_after_reboot(self):
        """2回目の起動直後、1回目で記録されたDTCの発生回数・状態区分が引き継がれている"""
        # 前提：2回目はOFFのままで、DTCが更新される処理が一度も動いていない
        self.assertIn("[FIXTURE] Ignition fixed: OFF", self.second_lines)
        self.assertNotIn("[IGN] ON", self.second_lines)
        self.assertIn("[PERSIST] Loaded saved DTC data", self.second_lines)
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 1", self.second_lines)
        self.assertIn("[DTC] TempSensor NONE     occurrences: 0", self.second_lines)
        self.assertIn("[DTC] Speed      NONE     occurrences: 0", self.second_lines)
        self.assertIn("[DTC] RPM        NONE     occurrences: 0", self.second_lines)

    def test_freeze_frame_inherited(self):
        """フリーズフレームも引き継がれる"""
        self.assertIn(FIRST_FREEZE_FRAME, self.second_lines)

    def test_occurrence_counted_again_after_reboot(self):
        """前回状態がNORMALに戻っているため、再起動後もCRITICALなら発生回数がもう1つ増える"""
        self.assertIn("[STATUS] Speed: NORMAL   | RPM: NORMAL   | Temp: CRITICAL", self.third_lines)
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 2", self.third_lines)

    def test_freeze_frame_not_overwritten_after_reboot(self):
        """再起動後にCRITICALが発生しても、フリーズフレームは最初の値（Speed 50・Temp 115）のまま"""
        self.assertIn("Speed:  60 km/h | RPM: 2000 | Temp: 119 C", self.third_lines)
        self.assertIn(FIRST_FREEZE_FRAME, self.third_lines)


# 記録の種類を分ける前（Phase27まで）の形のdtc_data.txt。センサ3つ×（回数・状態区分）とフリーズフレームの5個で11個。
# 0件から始まったことを見分けるため、回数とフリーズフレームは0以外にする
OLD_FORMAT_DTC_DATA = "3\n2\n1\n2\n4\n1\n1\n0\n110\n2000\n70\n"


class OldFormatSaveFileTest(unittest.TestCase):
    """前の形（値11個）のdtc_data.txtを置いて起動する（Phase28）。

    イグニッションOFF固定で動かし、最後に表示されるDTCを「起動直後に読み込まれた内容」として見る
    （PowerCycleTestの2回目と同じ見方）。
    """

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as workdir:
            cls.result = run_in_dir(workdir, {"config.txt": VALID_CONFIG, "dtc_data.txt": OLD_FORMAT_DTC_DATA,
                                              "fixture.txt": SECOND_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.lines = cls.result.stdout.splitlines()

    def test_runs_to_the_end(self):
        """前提：異常終了せず、サンプルループが最後まで実行される。OFFのままでDTCが更新されない"""
        self.assertEqual(0, self.result.returncode)
        self.assertIn("[Sample 20]", self.lines)
        self.assertNotIn("[IGN] ON", self.lines)

    def test_treated_as_corrupted(self):
        """壊れたファイルと同じく扱い、POSTはdid not passになる（config.txtは読み込めている）"""
        self.assertIn("[PERSIST] Saved data is corrupted, ignoring", self.lines)
        self.assertIn("[POST] Self-check did not pass, continuing with defaults", self.lines)

    def test_starts_from_no_dtc(self):
        """前の形の記録は引き継がず、DTC0件・フリーズフレームなしの初期状態から開始する"""
        self.assertIn("[DTC] Speed      NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] RPM        NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] TempSensor NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] Overheat   NONE     occurrences: 0", self.lines)
        self.assertIn("[DTC] Freeze Frame: not captured (no DTC occurred)", self.lines)


# 5秒目にエンジンを切り、8秒目にかけ直す（5〜7サンプル目はOFF）。値は最後まで変えない
RESTART_LINES = "IGNITION=ON\nIGNITION@5=OFF\nIGNITION@8=ON\n"
OVERHEAT_RESTART_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\n" + RESTART_LINES
OPEN_RESTART_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=OPEN\n" + RESTART_LINES
SPEED_RESTART_FIXTURE = "MODE=FIXED\nSPEED=110\nRPM=2000\nTEMP=70\n" + RESTART_LINES


def split_samples(lines):
    """標準出力の行を、"[Sample NN]"ごとに{サンプル番号: その間に出た行のリスト}へ分ける
    （test_fault_occurrence.pyと同じ。"--- Stats"以降はどのサンプルにも含めない）。"""
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


class InRunRestartTest(unittest.TestCase):
    """実行の途中でエンジンを切ってかけ直すと、起動し直したときと同じく数え直し、故障の確定をやり直す。"""

    @classmethod
    def setUpClass(cls):
        cls.overheat = run_sensor_sim({"fixture.txt": OVERHEAT_RESTART_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.open = run_sensor_sim({"fixture.txt": OPEN_RESTART_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.speed = run_sensor_sim({"fixture.txt": SPEED_RESTART_FIXTURE}, SENSOR_SIM_VIRTUAL)

    def test_runs_with_restart(self):
        """前提：3つとも最後まで動き、5秒目に切って8秒目にかけ直す"""
        for result in (self.overheat, self.open, self.speed):
            self.assertEqual(0, result.returncode)
            samples = split_samples(result.stdout.splitlines())
            self.assertIn("[IGN] ON -> OFF", samples[5])
            self.assertIn("[IGN] OFF -> ON", samples[8])

    def test_overheat_counted_again(self):
        """115℃のまま：かけ直した後もオーバーヒートなら2回目を数え、フリーズフレームは最初のまま"""
        lines = self.overheat.stdout.splitlines()
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 2", lines)
        self.assertIn("[DTC] Freeze Frame (trigger: Overheat) Speed: 50 km/h RPM:2000 Temp:115 C", lines)

    def test_temp_sensor_fault_confirmed_again(self):
        """最初から断線：かけ直した8秒目から数え直し、10秒目にもう一度確定する（解ける行は出ない）"""
        lines = self.open.stdout.splitlines()
        samples = split_samples(lines)
        confirmed = [n for n, s in samples.items() if "[FAULT] Degraded: Temp" in s]
        self.assertEqual([3, 10], confirmed)
        self.assertFalse(any(line.startswith("[FAULT] Recovered") for line in lines))
        self.assertIn("[DTC] TempSensor ACTIVE   occurrences: 2", lines)

    def test_speed_fault_confirmed_again(self):
        """車速110のまま：かけ直した後、確定し直すまで（8・9秒目）は、メーターに本当の車速と故障なしが届く"""
        samples = split_samples(self.speed.stdout.splitlines())
        confirmed = [n for n, s in samples.items() if "[FAULT] Degraded: Speed" in s]
        self.assertEqual([3, 10], confirmed)
        for n in (8, 9):
            self.assertIn("[METER] EngineStatus[OK] speed=110 rpm=2000 temp=70", samples[n])
            self.assertIn("[METER] FaultStatus[OK] speed=0 rpm=0 temp=0 overheat=0", samples[n])
            self.assertNotIn("[METER] FaultStatus[OK] speed=1 rpm=0 temp=0 overheat=0", samples[n])
        self.assertIn("[METER] EngineStatus[OK] speed=0 rpm=2000 temp=70", samples[10])
        self.assertIn("[DTC] Speed      ACTIVE   occurrences: 2", self.speed.stdout.splitlines())


# 通信の途絶の途中でかけ直す2条件。値は正常のまま、can_fault.txtでゲージデータの送信を止める
NORMAL_RESTART_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=70\n" + RESTART_LINES
# 2.5秒目から止める：切る前に受け取れないのは4サンプル目の1回だけで、まだ途絶にならない
DROP_BEFORE_OFF = "MODE=DROP\nTARGET=ENGINE_STATUS\nSTART_MS=2500\nEND_MS=14500\n"
# 0.5秒目から止める：切る前（4サンプル目）に途絶と確定し、途絶のまま切ってかけ直す
DROP_ACROSS_OFF = "MODE=DROP\nTARGET=ENGINE_STATUS\nSTART_MS=500\nEND_MS=14500\n"
LINK_ECUS = ("[CAN]", "[FAN]")   # 途絶を見つけて記録する、メーターECU役とファン制御ECU役


class InRunRestartLinkTest(unittest.TestCase):
    """通信の途絶の途中でかけ直すと、受信するECUも起動したときと同じく、かけ直してから3回受け取れないときに途絶とする。
    通信故障はPhase25で対象外としたが、かけ直したときの扱いをエンジン監視ECUとそろえたことを確かめるため、ここで扱う。"""

    @classmethod
    def setUpClass(cls):
        cls.before_off = run_sensor_sim({"fixture.txt": NORMAL_RESTART_FIXTURE, "can_fault.txt": DROP_BEFORE_OFF},
                                        SENSOR_SIM_VIRTUAL)
        cls.across_off = run_sensor_sim({"fixture.txt": NORMAL_RESTART_FIXTURE, "can_fault.txt": DROP_ACROSS_OFF},
                                        SENSOR_SIM_VIRTUAL)

    @staticmethod
    def samples_with(result, line):
        return [n for n, s in split_samples(result.stdout.splitlines()).items() if line in s]

    def test_runs_with_restart(self):
        """前提：2つとも最後まで動き、14.5秒目に送信が戻って18サンプル目に復帰する"""
        for result in (self.before_off, self.across_off):
            self.assertEqual(0, result.returncode)
            for ecu in LINK_ECUS:
                self.assertEqual([18], self.samples_with(result, f"{ecu} EngineStatus link recovered"))

    def test_count_before_off_not_added(self):
        """切る前に受け取れなかった1回を足さず、かけ直した8サンプル目から3回目の10サンプル目に途絶とする"""
        for ecu in LINK_ECUS:
            self.assertEqual([10], self.samples_with(self.before_off, f"{ecu} EngineStatus link lost"))

    def test_lost_across_off_counted_again(self):
        """切る前の途絶はかけ直すと解け（ログは出ない）、途絶が続いていれば10サンプル目にもう1回数える"""
        lines = self.across_off.stdout.splitlines()
        for ecu in LINK_ECUS:
            self.assertEqual([4, 10], self.samples_with(self.across_off, f"{ecu} EngineStatus link lost"))
        self.assertIn("[CAN-DTC] EngineStatus  HISTORY  link lost occurrences: 2", lines)
        self.assertIn("[FAN-DTC] EngineStatus  HISTORY  link lost occurrences: 2", lines)


# 値は正常のまま、ファン状態の送信（ファン制御ECU役→メーターECU役）を0.5秒目から14.5秒目まで止める。
# 途絶を記録するのはメーターECU役だけなので、2つのECUのファイルの中身が違い、取り違えて保存・読み込みすると見分けられる
LINK_FIXTURE_ON = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=70\nIGNITION=ON\n"
DROP_FAN_STATUS = "MODE=DROP\nTARGET=FAN_STATUS\nSTART_MS=500\nEND_MS=14500\n"
METER_FAN_LOST_1 = "[CAN-DTC] FanStatus     HISTORY  link lost occurrences: 1"
FAN_NOTHING = ("[FAN-DTC] FaultStatus   NONE     link lost occurrences: 0",
               "[FAN-DTC] EngineStatus  NONE     link lost occurrences: 0",
               "[FAN-DTC] FanStatus     NONE     link lost occurrences: 0")


class LinkRecordPowerCycleTest(unittest.TestCase):
    """通信の途絶の記録が、それを見つけたECUごとのファイルに保存され、次の起動で引き継がれる（Phase29）。

    1回目：途絶を起こしてON固定、2回目：OFF固定（読み込んだ記録がそのまま表示される）、3回目：1回目と同じ条件、
    4回目：OFF固定で点検コマンド"clear"を入力、5回目：OFF固定（4回目に消して保存した記録が読み込まれる）。
    5回とも同じ一時ディレクトリで動かす（can_fault.txtは1回目に置いたものが残る。OFFの回は送受信しない）。
    """

    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as workdir:
            cls.first = run_in_dir(workdir, {"config.txt": VALID_CONFIG, "fixture.txt": LINK_FIXTURE_ON,
                                             "can_fault.txt": DROP_FAN_STATUS}, SENSOR_SIM_VIRTUAL)
            cls.second = run_in_dir(workdir, {"fixture.txt": SECOND_FIXTURE}, SENSOR_SIM_VIRTUAL)
            cls.third = run_in_dir(workdir, {"fixture.txt": LINK_FIXTURE_ON}, SENSOR_SIM_VIRTUAL)
            cls.fourth = run_in_dir(workdir, {"fixture.txt": SECOND_FIXTURE}, SENSOR_SIM_VIRTUAL, "clear\n")
            cls.fifth = run_in_dir(workdir, {"fixture.txt": SECOND_FIXTURE}, SENSOR_SIM_VIRTUAL)
        cls.first_lines = cls.first.stdout.splitlines()
        cls.second_lines = cls.second.stdout.splitlines()
        cls.third_lines = cls.third.stdout.splitlines()
        cls.fourth_lines = cls.fourth.stdout.splitlines()
        cls.fifth_lines = cls.fifth.stdout.splitlines()

    def test_saved_per_ecu_at_first_boot(self):
        """1回目：ファイルが無いので0件から始め、終了時にECUごとに保存する（メーターECU役だけが途絶を1回記録し、復帰してHISTORY）"""
        for ecu in ("meter", "fan"):
            self.assertIn(f"[PERSIST] No saved {ecu} DTC data (first run)", self.first_lines)
            self.assertIn(f"[PERSIST] Saved {ecu} DTC data", self.first_lines)
        self.assertIn(METER_FAN_LOST_1, self.first_lines)
        for line in FAN_NOTHING:
            self.assertIn(line, self.first_lines)

    def test_inherited_right_after_reboot(self):
        """2回目（OFFのまま）：読み込んだ記録が引き継がれている。起動時自己診断は、エンジン監視ECUのファイルだけで決まる"""
        for ecu in ("meter", "fan"):
            self.assertIn(f"[PERSIST] Loaded saved {ecu} DTC data", self.second_lines)
        self.assertNotIn("[IGN] ON", self.second_lines)
        self.assertIn(METER_FAN_LOST_1, self.second_lines)   # メーターの記録はメーターに戻る（取り違えない）
        for line in FAN_NOTHING:
            self.assertIn(line, self.second_lines)
        self.assertIn("[POST] Self-check passed", self.second_lines)

    def test_judgment_restarts_and_counts_again(self):
        """3回目：途絶の判断の途中経過は引き継がないので、1回目と同じサンプルで改めて途絶とし、2回目を数える"""
        def lost_samples(lines):
            return [n for n, s in split_samples(lines).items() if "[CAN] FanStatus link lost" in s]
        self.assertEqual(1, len(lost_samples(self.first_lines)))
        self.assertEqual(lost_samples(self.first_lines), lost_samples(self.third_lines))
        self.assertIn("[CAN-DTC] FanStatus     HISTORY  link lost occurrences: 2", self.third_lines)
        for line in FAN_NOTHING:
            self.assertIn(line, self.third_lines)

    def test_clear_removes_all_ecus_and_is_saved(self):
        """4回目に"clear"を入力すると3つのECUの記録が全部消え、消えた状態が保存されて5回目に読み込まれる（段階2）"""
        self.assertIn("[DTC] Cleared", self.fourth_lines)
        self.assertIn("[CMD] Cleared records of all ECUs", self.fourth_lines)
        for ecu in ("meter", "fan"):
            self.assertIn(f"[PERSIST] Loaded saved {ecu} DTC data", self.fifth_lines)
        self.assertIn("[CAN-DTC] FanStatus     NONE     link lost occurrences: 0", self.fifth_lines)
        self.assertNotIn("[CMD] Cleared records of all ECUs", self.fifth_lines)


if __name__ == "__main__":
    unittest.main()
