"""Build-cache regression checks. Run: python -m unittest discover -s tests -p test_shader_build.py"""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class ShaderBuildTests(unittest.TestCase):
    def setUp(self):
        repo = Path(__file__).resolve().parents[1]
        self.build = (repo / "build").resolve()
        self.build.mkdir(exist_ok=True)
        self.root = Path(tempfile.mkdtemp(prefix="shader-build-test-", dir=self.build)).resolve()
        (self.root / "tools").mkdir()
        shutil.copy2(repo / "tools/BuildShaders.ps1", self.root / "tools/BuildShaders.ps1")
        self.sources = self.root / "source/shaders"
        self.sources.mkdir(parents=True)
        self.source = self.sources / "test.vert"
        self.source.write_text("void main() {}\n", encoding="utf-8")
        self.job = dict(kind="embed", sources=["test.vert"], output="test.h",
                        namespace="Test", symbols=["VertexSource"])
        self.manifest([self.job])
        self.output = self.sources / "generated/test.h"
        self.command = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
                        "-File", str(self.root / "tools/BuildShaders.ps1")]

    def tearDown(self):
        # Only remove this test's verified temporary directory in the workspace.
        self.assertEqual(self.root.parent, self.build)
        self.assertTrue(self.root.name.startswith("shader-build-test-"))
        shutil.rmtree(self.root)

    def manifest(self, jobs):
        (self.sources / "manifest.json").write_text(
            json.dumps(dict(version=1, jobs=jobs)), encoding="utf-8")

    def run_build(self, *args, succeeds=True):
        result = subprocess.run(self.command + list(args), capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, succeeds, result.stdout + result.stderr)
        return result

    def test_freshness_recovery_and_atomic_publication(self):
        self.run_build("-Verify", succeeds=False)
        self.assertFalse(self.output.exists())
        self.run_build()
        self.run_build("-Verify")
        stamp = self.output.stat().st_mtime_ns
        self.run_build()
        self.assertEqual(stamp, self.output.stat().st_mtime_ns)

        # Windows Git checkouts can rewrite source and generated header newlines.
        self.source.write_bytes(self.source.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        self.output.write_bytes(self.output.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        self.run_build("-Verify")
        stamp = self.output.stat().st_mtime_ns
        self.run_build()
        self.assertEqual(stamp, self.output.stat().st_mtime_ns)

        self.source.write_text("void main() { /* changed */ }\n", encoding="utf-8")
        self.run_build("-Verify", succeeds=False)
        self.run_build()
        self.assertIn("changed", self.output.read_text())
        self.output.write_text("corrupted", encoding="utf-8")
        self.run_build("-Verify", succeeds=False)
        self.run_build()
        self.run_build("-Verify")
        self.output.unlink()
        self.run_build()
        self.run_build("-Verify")

        # A failed later job must not publish earlier pending outputs or receipts.
        original = self.output.read_bytes()
        receipt = self.sources / "generated/build.json"
        original_receipt = receipt.read_bytes()
        self.source.write_text("void main() { /* pending */ }\n", encoding="utf-8")
        self.manifest([self.job, dict(self.job, kind="invalid", output="bad.h")])
        self.run_build(succeeds=False)
        self.assertEqual(original, self.output.read_bytes())
        self.assertEqual(original_receipt, receipt.read_bytes())

        # Independent project prebuilds can arrive together.
        self.manifest([self.job])
        processes = [subprocess.Popen(self.command, stdout=subprocess.PIPE, stderr=subprocess.PIPE) for _ in range(2)]
        for process in processes:
            stdout, stderr = process.communicate(timeout=30)
            self.assertEqual(process.returncode, 0, stdout + stderr)
        self.run_build("-Verify")
        self.assertIn("pending", self.output.read_text())


if __name__ == "__main__":
    unittest.main()
