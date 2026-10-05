"""シナリオ検証（scenario_test/）で共通に使う、sensor_simの実行関数。

ファイル名がtest_で始まらないため、unittest discoverはこのファイルをテストとして実行しない。
"""
import os
import subprocess
import tempfile

# このファイルの1つ上がリポジトリ直下。sensor_simは絶対パスで指定し、作業ディレクトリだけを差し替える
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SENSOR_SIM = os.path.join(REPO_ROOT, "sensor_sim")

# 通常は約20秒で終わる。止まった場合にテストごと固まらないための上限
TIMEOUT_SEC = 60


def run_in_dir(workdir, files):
    """workdirにfilesの{ファイル名: 内容}を書き込んでから、workdirでsensor_simを実行し、結果を返す。

    workdirに前回の実行で保存されたファイル（dtc_data.txt等）があれば、そのまま読み込まれる。
    """
    for name, text in files.items():
        with open(os.path.join(workdir, name), "w") as f:
            f.write(text)
    # イグニッションOFFで終わるとcmd_read_lineが入力待ちになるため、標準入力にはEOFを渡す
    return subprocess.run(
        [SENSOR_SIM],
        cwd=workdir,
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
        timeout=TIMEOUT_SEC,
    )


def run_sensor_sim(files):
    """filesの{ファイル名: 内容}だけを置いた一時ディレクトリでsensor_simを実行し、結果を返す。

    config.txt・dtc_data.txt等はすべて相対パスで読み書きされるため、作業ディレクトリを
    一時ディレクトリにすれば本番のdtc_data.txtを上書きしない。
    """
    with tempfile.TemporaryDirectory() as workdir:
        return run_in_dir(workdir, files)
