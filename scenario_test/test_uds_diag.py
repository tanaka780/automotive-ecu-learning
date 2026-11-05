"""CANでの診断（UDS、docs/scenarios.mdの診断コマンド）の結合テスト（Phase30）。

スキャンツール役（sim/uds_tester.c）に、uds_request.txtで「何サンプル目の後に何を送るか」を渡して実行し、
[TESTER]の行に出た要求と応答の組を、期待した応答と照合する。何サンプル目に何が返るかまで照合するため、
仮想時間版（sensor_sim_virtual）で実行する。

- 異常中・回復後・消去後：水温115℃（オーバーヒート）で始め、5サンプル目から80℃に戻す。
  記録の件数を、ステータスのマスクを変えて読み、全消去の後に0件になって、その状態が保存されることを確かめる
- イグニッションOFF：10サンプル目でOFF、13サンプル目でONにする。OFFの間の要求には応答せず（消去もされず）、
  ONに戻した後は応答する
- 否定応答：対応していないサービス・サブファンクション・長さの誤り・FFFFFF以外の消去に、決めた理由を返し、
  記録は消えない
"""
import os
import re
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sensor_sim_runner import SENSOR_SIM_VIRTUAL, run_in_dir, run_sensor_sim

OVERHEAT_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\nIGNITION=ON\n"
RECOVER_FIXTURE = "MODE=FIXED\nSPEED=50\nRPM=2000\nTEMP=115\nTEMP@5=80\nIGNITION=ON\n"
IGNITION_OFF_FIXTURE = OVERHEAT_FIXTURE + "IGNITION@10=OFF\nIGNITION@13=ON\n"

SAMPLE_LINE = re.compile(r"^\[Sample (\d+)\]$")


def transactions(lines):
    """[TESTER] Request の行と、その次の[TESTER]の行（Response ... か No response）を組にして、
    (送ったサンプル番号, 要求, 応答) の一覧を返す。応答は "Response " / "[TESTER] " を外した文字列にする"""
    result = []
    sample = 0
    pending = None
    for line in lines:
        m = SAMPLE_LINE.match(line)
        if m:
            sample = int(m.group(1))
        elif line.startswith("[TESTER] Request "):
            pending = (sample, line[len("[TESTER] Request "):])
        elif pending is not None and line.startswith("[TESTER] "):
            answer = line[len("[TESTER] "):]
            if answer.startswith("Response "):
                answer = answer[len("Response "):]
            result.append((pending[0], pending[1], answer))
            pending = None
    return result


def run_with_requests(fixture, requests):
    """fixture.txtとuds_request.txtを置いた一時ディレクトリで仮想時間版を実行し、結果と保存されたdtc_data.txtを返す"""
    with tempfile.TemporaryDirectory() as workdir:
        result = run_in_dir(workdir, {"fixture.txt": fixture, "uds_request.txt": requests}, SENSOR_SIM_VIRTUAL)
        with open(os.path.join(workdir, "dtc_data.txt")) as f:
            saved = f.read().split()
    return result, saved


class UdsReadAndClearTest(unittest.TestCase):
    """異常中・回復後・消去後に、記録の件数の読み出しと全消去が期待した応答を返す。"""

    @classmethod
    def setUpClass(cls):
        requests = ("REQ@3=19 01 FF\n"
                    "REQ@3=19 01 01\n"
                    "REQ@3=19 01 20\n"
                    "REQ@6=19 01 01\n"
                    "REQ@6=19 01 20\n"
                    "REQ@7=14 FF FF FF\n"
                    "REQ@7=19 01 FF\n"
                    "REQ@9=19 01 FF\n")
        cls.result, cls.saved = run_with_requests(RECOVER_FIXTURE, requests)
        cls.lines = cls.result.stdout.splitlines()
        cls.transactions = transactions(cls.lines)

    def test_runs_to_the_end(self):
        """前提：異常終了せず、8つの要求をすべて読み込み、すべてに応答が返る"""
        self.assertEqual(0, self.result.returncode)
        self.assertIn("[Sample 20]", self.lines)
        self.assertIn("[TESTER] Requests loaded: 8", self.lines)
        self.assertEqual(8, len(self.transactions))

    def test_count_while_active(self):
        """オーバーヒート中は、全ビット・bit0（今も異常）・bit5（消してから異常になった）のどのマスクでも1件"""
        self.assertEqual((3, "19 01 FF", "59 01 21 00 00 01"), self.transactions[0])
        self.assertEqual((3, "19 01 01", "59 01 21 00 00 01"), self.transactions[1])
        self.assertEqual((3, "19 01 20", "59 01 21 00 00 01"), self.transactions[2])

    def test_count_after_recovery(self):
        """水温が戻った後は、bit0（今も異常）では0件、bit5（消してから異常になった）では1件"""
        self.assertEqual((6, "19 01 01", "59 01 21 00 00 00"), self.transactions[3])
        self.assertEqual((6, "19 01 20", "59 01 21 00 00 01"), self.transactions[4])

    def test_clear_all(self):
        """全消去は肯定応答（54）を返し、エンジン監視ECUの記録を消す。消した直後から0件になる"""
        self.assertEqual((7, "14 FF FF FF", "54"), self.transactions[5])
        self.assertEqual((7, "19 01 FF", "59 01 21 00 00 00"), self.transactions[6])
        self.assertIn("[DTC] Cleared", self.lines)

    def test_stays_cleared(self):
        """異常が戻らなければ、消した後も0件のまま"""
        self.assertEqual((9, "19 01 FF", "59 01 21 00 00 00"), self.transactions[7])
        self.assertIn("[DTC] Overheat   NONE     occurrences: 0", self.lines)

    def test_cleared_record_is_saved(self):
        """消した状態が保存され、次の起動に引き継がれる（記録の回数・状態区分とフリーズフレームがすべて0）"""
        self.assertIn("[PERSIST] Saved DTC data", self.lines)
        self.assertEqual(["0"] * 13, self.saved)


class UdsIgnitionOffTest(unittest.TestCase):
    """イグニッションOFFの間はエンジン監視ECUに電気が来ていないので応答せず、ONに戻すと応答する。"""

    @classmethod
    def setUpClass(cls):
        requests = ("REQ@11=19 01 FF\n"
                    "REQ@11=14 FF FF FF\n"
                    "REQ@14=19 01 FF\n")
        cls.result, cls.saved = run_with_requests(IGNITION_OFF_FIXTURE, requests)
        cls.lines = cls.result.stdout.splitlines()
        cls.transactions = transactions(cls.lines)

    def test_runs_to_the_end(self):
        """前提：異常終了せず、10サンプル目にOFF、13サンプル目にONになる"""
        self.assertEqual(0, self.result.returncode)
        self.assertIn("[Sample 20]", self.lines)
        self.assertIn("[IGN] ON -> OFF", self.lines)
        self.assertEqual(3, len(self.transactions))

    def test_no_response_while_off(self):
        """OFFの間の要求には、読み出しにも消去にも応答しない（待ち時間P2を過ぎて「応答なし」）"""
        self.assertEqual((11, "19 01 FF", "No response"), self.transactions[0])
        self.assertEqual((11, "14 FF FF FF", "No response"), self.transactions[1])

    def test_not_cleared_while_off(self):
        """OFFの間の消去の要求では消さない。ONに戻すと、OFFの間の要求には答えないまま応答する"""
        self.assertNotIn("[DTC] Cleared", self.lines)
        self.assertEqual((14, "19 01 FF", "59 01 21 00 00 01"), self.transactions[2])
        self.assertIn("[DTC] Overheat   ACTIVE   occurrences: 2", self.lines)


class UdsNegativeResponseTest(unittest.TestCase):
    """対応していない要求には、決めた理由（NRC）の否定応答を返し、記録は消えない。"""

    @classmethod
    def setUpClass(cls):
        requests = ("REQ@3=22 F1 90\n"
                    "REQ@3=19 02 FF\n"
                    "REQ@3=19 01\n"
                    "REQ@3=14 00 05 00\n"
                    "REQ@3=14 FF FF\n"
                    "REQ@4=19 01 FF\n")
        cls.result, cls.saved = run_with_requests(OVERHEAT_FIXTURE, requests)
        cls.lines = cls.result.stdout.splitlines()
        cls.transactions = transactions(cls.lines)

    def test_runs_to_the_end(self):
        """前提：異常終了せず、6つの要求すべてに応答が返る"""
        self.assertEqual(0, self.result.returncode)
        self.assertIn("[Sample 20]", self.lines)
        self.assertEqual(6, len(self.transactions))

    def test_negative_responses(self):
        """対応していないサービスは0x11、サブファンクションは0x12、長さの誤りは0x13、FFFFFF以外の消去は0x31"""
        self.assertEqual((3, "22 F1 90", "7F 22 11"), self.transactions[0])
        self.assertEqual((3, "19 02 FF", "7F 19 12"), self.transactions[1])
        self.assertEqual((3, "19 01", "7F 19 13"), self.transactions[2])
        self.assertEqual((3, "14 00 05 00", "7F 14 31"), self.transactions[3])
        self.assertEqual((3, "14 FF FF", "7F 14 13"), self.transactions[4])

    def test_record_not_cleared_by_rejected_requests(self):
        """拒否した消去の要求では記録が消えない"""
        self.assertNotIn("[DTC] Cleared", self.lines)
        self.assertEqual((4, "19 01 FF", "59 01 21 00 00 01"), self.transactions[5])


class UdsNoRequestFileTest(unittest.TestCase):
    """uds_request.txtが無ければ、スキャンツール役は何も送らず、何も表示しない。"""

    def test_no_tester_lines(self):
        result = run_sensor_sim({"fixture.txt": OVERHEAT_FIXTURE}, SENSOR_SIM_VIRTUAL)
        self.assertEqual(0, result.returncode)
        self.assertFalse([line for line in result.stdout.splitlines() if line.startswith("[TESTER]")])


if __name__ == "__main__":
    unittest.main()
