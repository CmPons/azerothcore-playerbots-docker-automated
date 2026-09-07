"""Exercise bridge admission using mocked Pi calls; never touch the live service."""

import importlib.util
import os
from pathlib import Path
import subprocess
import threading
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


class BridgeAdmissionTests(unittest.TestCase):
    def setUp(self):
        spec = importlib.util.spec_from_file_location(
            "bridge_under_test", ROOT / "scripts/pi_ollama_bridge.py")
        self.bridge = importlib.util.module_from_spec(spec)
        # Tests must not inherit the user's provider, limits, or executable.
        with patch.dict(os.environ, {}, clear=True):
            spec.loader.exec_module(self.bridge)
        self.bridge.MAX_PER_MINUTE = 12
        self.now = 10000.0
        self.clock = patch.object(self.bridge.time, "monotonic", side_effect=lambda: self.now)
        self.clock.start()
        self.addCleanup(self.clock.stop)
        log_patch = patch.object(self.bridge, "log")
        self.log = log_patch.start()
        self.addCleanup(log_patch.stop)
        run_patch = patch.object(self.bridge.subprocess, "run", return_value=
                                 subprocess.CompletedProcess(["pi"], 0, '"hello raid"', ""))
        self.run = run_patch.start()
        self.addCleanup(run_patch.stop)

    def call(self):
        return self.bridge.call_pi("system", "prompt")

    def assert_slot_free(self):
        self.assertTrue(self.bridge.semaphore.acquire(blocking=False))
        self.bridge.semaphore.release()

    def test_default_and_service_example_disable_hourly_cap(self):
        self.assertEqual(self.bridge.MAX_PER_HOUR, 0)
        service = (ROOT / "scripts/pi-ollama-bridge.service.example").read_text()
        self.assertIn("Environment=PI_BRIDGE_MAX_PER_HOUR=0\n", service)
        self.assertIn("Environment=PI_BRIDGE_MAX_PER_MINUTE=12\n", service)
        self.assertIn("Environment=PI_BRIDGE_MAX_CONCURRENT=1\n", service)

    def test_busy_rejection_does_not_charge_or_invoke_pi(self):
        self.assertTrue(self.bridge.semaphore.acquire(blocking=False))
        try:
            self.assertEqual(self.call(), "")
            self.run.assert_not_called()
            self.assertEqual(list(self.bridge.minute_hits), [])
            self.assertEqual(list(self.bridge.hour_hits), [])
        finally:
            self.bridge.semaphore.release()
        self.assert_slot_free()

    def test_minute_rejections_do_not_consume_quota_or_leak_slot(self):
        for _ in range(12):
            self.assertEqual(self.call(), "hello raid")
        for _ in range(5):
            self.assertEqual(self.call(), "")
            self.assert_slot_free()
        self.assertEqual(self.run.call_count, 12)
        self.assertEqual(len(self.bridge.minute_hits), 12)
        self.assertEqual(len(self.bridge.hour_hits), 12)

    def test_optional_hour_rejection_does_not_charge_minute(self):
        self.bridge.MAX_PER_HOUR = 1
        self.assertEqual(self.call(), "hello raid")
        self.assertEqual(self.call(), "")
        self.assertEqual(self.run.call_count, 1)
        self.assertEqual(len(self.bridge.minute_hits), 1)
        self.assertEqual(len(self.bridge.hour_hits), 1)
        self.assert_slot_free()

    def test_no_hourly_cutoff_after_120_admitted_generations(self):
        for _ in range(180):
            self.assertEqual(self.call(), "hello raid")
            self.now += 6  # Ten/minute, all within one hour.
        self.assertEqual(self.run.call_count, 180)
        self.assertEqual(len(self.bridge.hour_hits), 180)

    def test_minute_slot_expires_at_exact_window_boundary(self):
        self.bridge.MAX_PER_MINUTE = 1
        self.assertEqual(self.call(), "hello raid")
        self.now += 59.999
        self.assertEqual(self.call(), "")
        self.now = 10060.0
        self.assertEqual(self.call(), "hello raid")
        self.assertEqual(self.run.call_count, 2)
        self.assertEqual(len(self.bridge.minute_hits), 1)

    def test_optional_hour_slot_expires_at_exact_window_boundary(self):
        self.bridge.MAX_PER_HOUR = 1
        self.assertEqual(self.call(), "hello raid")
        self.now += 3599
        self.assertEqual(self.call(), "")
        self.now += 1
        self.assertEqual(self.call(), "hello raid")
        self.assertEqual(len(self.bridge.hour_hits), 1)

    def test_wall_clock_is_not_used_for_admission(self):
        with patch.object(self.bridge.time, "time", side_effect=AssertionError("wall clock used")):
            self.assertEqual(self.call(), "hello raid")
        self.assertEqual(list(self.bridge.minute_hits), [self.now])

    def test_pi_errors_timeouts_and_launch_failures_release_slot(self):
        outcomes = (
            subprocess.CompletedProcess(["pi"], 1, "", "provider error"),
            subprocess.TimeoutExpired(["pi"], 75),
            OSError("could not start pi"),
        )
        for outcome in outcomes:
            with self.subTest(outcome=type(outcome).__name__):
                self.run.side_effect = outcome if isinstance(outcome, Exception) else None
                if not isinstance(outcome, Exception):
                    self.run.return_value = outcome
                self.assertEqual(self.call(), "")
                self.assert_slot_free()
        # These are admitted invocation attempts, not admission rejections.
        self.assertEqual(len(self.bridge.minute_hits), 3)
        self.assertEqual(len(self.bridge.hour_hits), 3)

    def test_rate_check_exception_also_releases_slot(self):
        with patch.object(self.bridge, "allowed_by_rate_limit", side_effect=RuntimeError("test")):
            self.assertEqual(self.call(), "")
        self.run.assert_not_called()
        self.assert_slot_free()

    def test_concurrent_generation_and_busy_rejection_charge_once(self):
        started, finish = threading.Event(), threading.Event()
        replies = []

        def generate(*args, **kwargs):
            started.set()
            if not finish.wait(3):
                raise AssertionError("test worker did not finish")
            return subprocess.CompletedProcess(["pi"], 0, "hello raid", "")

        self.run.side_effect = generate
        worker = threading.Thread(target=lambda: replies.append(self.call()))
        worker.start()
        try:
            self.assertTrue(started.wait(2))
            self.assertEqual(self.call(), "")
            self.assertEqual(len(self.bridge.minute_hits), 1)
            self.assertEqual(len(self.bridge.hour_hits), 1)
        finally:
            finish.set()
            worker.join(3)
        self.assertFalse(worker.is_alive())
        self.assertEqual(replies, ["hello raid"])
        self.assertEqual(self.run.call_count, 1)
        self.assert_slot_free()

    def test_rate_admission_is_atomic_with_multiple_workers(self):
        self.bridge.MAX_PER_MINUTE = 2
        self.bridge.semaphore = threading.BoundedSemaphore(4)
        gate = threading.Barrier(12)
        replies = []
        errors = []

        def request():
            try:
                gate.wait(timeout=3)
                replies.append(self.call())
            except Exception as exc:
                errors.append(exc)

        workers = [threading.Thread(target=request) for _ in range(12)]
        for worker in workers:
            worker.start()
        for worker in workers:
            worker.join(5)
        self.assertFalse(any(worker.is_alive() for worker in workers))
        self.assertEqual(errors, [])
        self.assertEqual(replies.count("hello raid"), 2)
        self.assertEqual(replies.count(""), 10)
        self.assertEqual(self.run.call_count, 2)
        self.assertEqual(len(self.bridge.minute_hits), 2)
        self.assertEqual(len(self.bridge.hour_hits), 2)
        acquired = 0
        try:
            for _ in range(4):
                self.assertTrue(self.bridge.semaphore.acquire(blocking=False))
                acquired += 1
        finally:
            for _ in range(acquired):
                self.bridge.semaphore.release()


if __name__ == "__main__":
    unittest.main()
