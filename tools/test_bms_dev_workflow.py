"""Exercise workflow routing with tool stubs in user temp; never build or flash images."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(os.name == "nt", "The workflow requires Windows PowerShell")
class WorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        parent = Path(tempfile.gettempdir()) / "CodexTemp/103-309-copy"
        parent.mkdir(parents=True, exist_ok=True)
        cls.fixture = tempfile.TemporaryDirectory(prefix="workflow-test-", dir=parent)
        cls.root = Path(cls.fixture.name)
        tools = cls.root / "tools"
        tools.mkdir()
        shutil.copyfile(ROOT / "tools/bms_dev_workflow.ps1", tools / "bms_dev_workflow.ps1")
        project = Path("103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx")
        (cls.root / project.parent).mkdir(parents=True)
        shutil.copyfile(ROOT / project, cls.root / project)
        stub = '''import json, os, sys
from pathlib import Path
name = Path(__file__).stem
with open(os.environ["WORKFLOW_TEST_CALLS"], "a", encoding="utf-8") as calls:
    calls.write(json.dumps({"name": name, "args": sys.argv[1:]}) + "\\n")
if name == "check_bms_no_bin" and "--result-json" in sys.argv:
    axf = Path(__file__).parent.parent / "checked.axf"
    axf.write_text("tool stub", encoding="utf-8")
    Path(sys.argv[sys.argv.index("--result-json") + 1]).write_text(json.dumps({"axf": str(axf)}), encoding="utf-8")
sys.exit(7 if os.environ.get("WORKFLOW_TEST_FAIL") == name else 0)
'''
        for name in ("project_check", "run_soc_host_c_test", "run_current_calibration_host_c_test",
                     "check_bms_no_bin", "soc_online_monitor"):
            (tools / (name + ".py")).write_text(stub, encoding="utf-8")
        cls.uv4 = cls.root / "fake uv4.cmd"
        cls.uv4.write_text('''@echo off
>>"%WORKFLOW_TEST_CALLS%" echo {"name": "image-build", "args": []}
:parse
if "%~1"=="" exit /b 2
if /i "%~1"=="-o" goto log
shift
goto parse
:log
>"%~2" echo fake target - 0 Error(s), 0 Warning(s)
exit /b 0
''', encoding="utf-8")
        (tools / "soc_flash_app_safe.ps1").write_text('''$record = @{name="safe-flash"; args=@($args)}
[IO.File]::AppendAllText($env:WORKFLOW_TEST_CALLS, (($record | ConvertTo-Json -Compress) + "`n"))
exit 0
''', encoding="utf-8")
        (tools / "stlink_snapshot.ps1").write_text('''$record = @{name="probe"; args=@($args)}
[IO.File]::AppendAllText($env:WORKFLOW_TEST_CALLS, (($record | ConvertTo-Json -Compress) + "`n"))
exit 0
''', encoding="utf-8")

    @classmethod
    def tearDownClass(cls):
        cls.fixture.cleanup()

    def run_workflow(self, *arguments, fail="", success=True):
        calls = self.root / "calls.jsonl"
        calls.unlink(missing_ok=True)
        env = os.environ.copy()
        env["WORKFLOW_TEST_CALLS"] = str(calls)
        env["WORKFLOW_TEST_FAIL"] = fail
        result = subprocess.run(
            ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
             str(self.root / "tools/bms_dev_workflow.ps1"), "-Uv4", str(self.uv4), *arguments],
            cwd=self.root, env=env, capture_output=True, text=True, errors="replace", timeout=30)
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertFalse(list(self.root.rglob("*.bin")))
        self.assertFalse(list(self.root.rglob("*.hex")))
        return [json.loads(line) for line in calls.read_text(encoding="utf-8-sig").splitlines()] if calls.exists() else []

    def test_quick_only_runs_related_host_tests(self):
        calls = self.run_workflow()
        self.assertEqual([call["name"] for call in calls],
                         ["run_soc_host_c_test", "run_current_calibration_host_c_test"])

    def test_project_check_requires_explicit_option(self):
        calls = self.run_workflow("-Mode", "quick", "-ProjectCheck", fail="project_check")
        self.assertEqual([call["name"] for call in calls],
                         ["project_check", "run_soc_host_c_test", "run_current_calibration_host_c_test"])

    def test_strict_project_check_stops_on_failure(self):
        calls = self.run_workflow("-StrictProjectCheck", fail="project_check", success=False)
        self.assertEqual([call["name"] for call in calls], ["project_check"])

    def test_build_defaults_to_no_image_for_both_targets(self):
        for target in ("FD_Release", "FD_Debug"):
            with self.subTest(target=target):
                calls = self.run_workflow("-Mode", "build", "-Target", target)
                self.assertEqual([call["name"] for call in calls], ["check_bms_no_bin"])
                self.assertEqual(calls[0]["args"][:4], ["--target", target, "--uv4", str(self.uv4)])

    def test_full_defaults_to_no_image(self):
        calls = self.run_workflow("-Mode", "full")
        self.assertEqual([call["name"] for call in calls],
                         ["project_check", "run_soc_host_c_test", "run_current_calibration_host_c_test",
                          "check_bms_no_bin"])

    def test_failed_no_image_build_does_not_fall_back_to_image(self):
        calls = self.run_workflow("-Mode", "build", fail="check_bms_no_bin", success=False)
        self.assertEqual([call["name"] for call in calls], ["check_bms_no_bin"])

    def test_full_probe_uses_the_new_no_image_axf(self):
        calls = self.run_workflow("-Mode", "full", "-Probe")
        self.assertEqual([call["name"] for call in calls][-2:], ["check_bms_no_bin", "probe"])
        self.assertEqual(calls[-1]["args"], ["-Elf", str(self.root / "checked.axf")])

    def test_image_build_requires_explicit_option(self):
        calls = self.run_workflow("-Mode", "build", "-GenerateImage")
        self.assertEqual([call["name"] for call in calls], ["image-build"])

    def test_image_option_rejected_for_quick(self):
        self.assertEqual(self.run_workflow("-GenerateImage", success=False), [])

    def test_flash_requests_use_existing_safe_script(self):
        for arguments in (("-Mode", "flash"), ("-Mode", "full", "-Flash")):
            with self.subTest(arguments=arguments):
                calls = self.run_workflow(*arguments)
                self.assertEqual([call["name"] for call in calls][-2:], ["image-build", "safe-flash"])
                self.assertTrue(calls[-1]["args"][1].endswith("FD_Release.bin"))
                self.assertIn("-Flash", calls[-1]["args"])

    def test_monitor_csv_is_outside_worktree(self):
        calls = self.run_workflow("-Port", "COM_TEST")
        monitor = calls[-1]
        self.assertEqual(monitor["name"], "soc_online_monitor")
        csv = Path(monitor["args"][monitor["args"].index("--csv") + 1])
        self.assertTrue(csv.is_absolute())
        self.assertIn(Path(tempfile.gettempdir()), csv.parents)
        self.assertNotIn(self.root, csv.parents)


if __name__ == "__main__":
    unittest.main()
