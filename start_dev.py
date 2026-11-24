#!/usr/bin/env python3
import sys
import subprocess
import time
import os
import signal
from pathlib import Path

ROOT = Path(__file__).resolve().parent

def free_port(port: int):
    try:
        res = subprocess.run(["netstat", "-ano"], capture_output=True, text=True)
        for line in res.stdout.splitlines():
            if f":{port}" in line and "LISTENING" in line:
                parts = line.strip().split()
                pid = parts[-1]
                if pid and pid != "0":
                    print(f"[*] Releasing port {port} (killing stale PID {pid})...")
                    subprocess.run(["taskkill", "/F", "/PID", pid], capture_output=True)
    except Exception as e:
        pass

def main():
    print("==================================================================")
    print("           Starting ChronoGit Local Development Environment       ")
    print("==================================================================")

    # Check if chronogit.exe exists
    exe_path = ROOT / "build" / "chronogit.exe"
    if not exe_path.exists():
        print(f"[!] Warning: {exe_path} not found. Building now with CMake...")
        res = subprocess.run(["cmake", "--build", "build"], cwd=str(ROOT))
        if res.returncode != 0:
            print("[x] Build failed! Please inspect compilation errors above.")
            sys.exit(1)

    free_port(8000)
    free_port(5173)

    print("[*] Starting FastAPI Backend on http://localhost:8000 ...")
    backend_proc = subprocess.Popen(
        [sys.executable, "-m", "uvicorn", "server.app:app", "--host", "0.0.0.0", "--port", "8000", "--log-level", "info"],
        cwd=str(ROOT)
    )

    print("[*] Starting Frontend Dev Server on http://localhost:5173 ...")
    frontend_proc = subprocess.Popen(
        [sys.executable, "web-visualizer/dev_server.py"],
        cwd=str(ROOT)
    )

    time.sleep(1.5)
    print("\n------------------------------------------------------------------")
    print("  🚀 ChronoGit is now live and running locally!")
    print("  🌐 Frontend Dashboard:     http://localhost:5173")
    print("  🔌 Backend REST API:       http://localhost:8000")
    print("  📚 API Documentation:      http://localhost:8000/docs")
    print("------------------------------------------------------------------")
    print("  Press Ctrl+C to shut down both servers.\n")

    try:
        while True:
            time.sleep(1)
            if backend_proc.poll() is not None:
                print("[!] Backend exited unexpectedly.")
                break
            if frontend_proc.poll() is not None:
                print("[!] Frontend exited unexpectedly.")
                break
    except KeyboardInterrupt:
        print("\n[*] Stopping ChronoGit services...")
    finally:
        for proc in [frontend_proc, backend_proc]:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    proc.kill()
        print("[✓] All services stopped cleanly.")

if __name__ == "__main__":
    main()
