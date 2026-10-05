"""シナリオ検証（scenario_test/）のパッケージ初期化。

リポジトリ直下から`python3 -m unittest scenario_test.test_xxx`のようにモジュール名で指定して実行すると、
このフォルダがimportの検索先（sys.path）に入らず、sensor_sim_runnerが見つからない。
`make scenario`（unittest discover -s scenario_test）が内部で行っているのと同じように、
このフォルダを検索先に加えておく。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
