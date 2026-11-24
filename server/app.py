import os
import subprocess
import json
from pathlib import Path
from typing import List, Optional
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

app = FastAPI(title="ChronoGit Backend API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

PROJECT_ROOT = Path(__file__).resolve().parent.parent
CHRONOGIT_EXE = PROJECT_ROOT / "build" / "chronogit.exe"

# Active working repository for the visualizer/UI
DEMO_REPO_DIR = PROJECT_ROOT / "demo_repo"

def get_chronogit_cmd(cwd: Path) -> List[str]:
    if not CHRONOGIT_EXE.exists():
        raise HTTPException(status_code=500, detail=f"chronogit.exe not found at {CHRONOGIT_EXE}. Please build the project first.")
    return [str(CHRONOGIT_EXE)]

def run_chronogit(args: List[str], cwd: Path) -> subprocess.CompletedProcess:
    cmd = get_chronogit_cmd(cwd) + args
    res = subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True)
    return res

class CommitRequest(BaseModel):
    message: str
    author_name: Optional[str] = "ChronoGit User"
    author_email: Optional[str] = "user@chronogit.local"

class StageRequest(BaseModel):
    paths: List[str]

class BranchRequest(BaseModel):
    name: str

class CheckoutRequest(BaseModel):
    target: str
    create_branch: Optional[bool] = False
    force: Optional[bool] = False

class MergeRequest(BaseModel):
    branch: str
    abort: Optional[bool] = False

class FileWriteRequest(BaseModel):
    path: str
    content: str

@app.get("/api/health")
def health_check():
    return {
        "status": "healthy",
        "engine": "ChronoGit C++20",
        "binary_exists": CHRONOGIT_EXE.exists(),
        "repo_initialized": (DEMO_REPO_DIR / ".chronogit").exists()
    }

@app.post("/api/demo/setup")
def setup_demo_repository():
    DEMO_REPO_DIR.mkdir(parents=True, exist_ok=True)
    git_dir = DEMO_REPO_DIR / ".chronogit"
    
    # Initialize repository
    if not git_dir.exists():
        run_chronogit(["init", "-b", "main"], DEMO_REPO_DIR)
        
        # Create initial files
        (DEMO_REPO_DIR / "README.md").write_text("# ChronoGit Project Demo\nDemonstrating C++20 CAS version control engine.\n", encoding="utf-8")
        (DEMO_REPO_DIR / "main.cpp").write_text("#include <iostream>\n\nint main() {\n    std::cout << \"Hello from ChronoGit!\" << std::endl;\n    return 0;\n}\n", encoding="utf-8")
        (DEMO_REPO_DIR / "calculator.py").write_text("def add(a, b):\n    return a + b\n\ndef subtract(a, b):\n    return a - b\n", encoding="utf-8")
        
        run_chronogit(["add", "."], DEMO_REPO_DIR)
        run_chronogit(["commit", "-m", "Initial commit: project structure and core files"], DEMO_REPO_DIR)
        
        # Create feature branch
        run_chronogit(["branch", "feature/multiply"], DEMO_REPO_DIR)
        run_chronogit(["checkout", "feature/multiply"], DEMO_REPO_DIR)
        
        # Modify calculator.py on feature branch
        calc_content = (DEMO_REPO_DIR / "calculator.py").read_text(encoding="utf-8")
        calc_content += "\ndef multiply(a, b):\n    return a * b\n"
        (DEMO_REPO_DIR / "calculator.py").write_text(calc_content, encoding="utf-8")
        run_chronogit(["add", "calculator.py"], DEMO_REPO_DIR)
        run_chronogit(["commit", "-m", "feature: implement multiply function"], DEMO_REPO_DIR)
        
        # Switch back to main and make another commit
        run_chronogit(["checkout", "main"], DEMO_REPO_DIR)
        (DEMO_REPO_DIR / "README.md").write_text("# ChronoGit Project Demo\nDemonstrating C++20 CAS engine with multi-branch DAG visualization.\n", encoding="utf-8")
        run_chronogit(["add", "README.md"], DEMO_REPO_DIR)
        run_chronogit(["commit", "-m", "docs: update documentation on main branch"], DEMO_REPO_DIR)
    
    return {"status": "success", "message": "Demo repository is ready.", "path": str(DEMO_REPO_DIR)}

@app.get("/api/status")
def get_status():
    if not (DEMO_REPO_DIR / ".chronogit").exists():
        setup_demo_repository()

    res = run_chronogit(["status"], DEMO_REPO_DIR)
    # Parse status output
    output = res.stdout
    lines = output.splitlines()

    branch = "unknown"
    staged = []
    unstaged = []
    untracked = []

    section = None
    for line in lines:
        sline = line.strip()
        if sline.startswith("On branch "):
            branch = sline.replace("On branch ", "").strip()
        elif "Changes to be committed:" in sline:
            section = "staged"
        elif "Changes not staged for commit:" in sline:
            section = "unstaged"
        elif "Untracked files:" in sline:
            section = "untracked"
        elif sline.startswith("new file:") or sline.startswith("modified:") or sline.startswith("deleted:"):
            parts = sline.split(":", 1)
            f_type = parts[0].strip()
            f_path = parts[1].strip()
            if section == "staged":
                staged.append({"type": f_type, "path": f_path})
            elif section == "unstaged":
                unstaged.append({"type": f_type, "path": f_path})
        elif section == "untracked" and sline and not sline.startswith("(") and not sline.startswith("no changes"):
            untracked.append(sline)

    return {
        "branch": branch,
        "is_clean": "nothing to commit" in output,
        "staged": staged,
        "unstaged": unstaged,
        "untracked": untracked,
        "raw_output": output
    }

@app.get("/api/log")
def get_log(limit: int = 50):
    if not (DEMO_REPO_DIR / ".chronogit").exists():
        setup_demo_repository()

    res = run_chronogit(["log", "--json", "-n", str(limit)], DEMO_REPO_DIR)
    if res.returncode != 0:
        return {"commits": []}
    
    try:
        commits = json.loads(res.stdout)
    except Exception:
        commits = []

    # Get branches
    b_res = run_chronogit(["branch"], DEMO_REPO_DIR)
    branches = []
    current_branch = None
    for line in b_res.stdout.splitlines():
        line = line.strip()
        if line.startswith("* "):
            bname = line[2:].strip()
            branches.append({"name": bname, "is_current": True})
            current_branch = bname
        elif line:
            branches.append({"name": line.strip(), "is_current": False})

    return {
        "commits": commits,
        "branches": branches,
        "current_branch": current_branch
    }

@app.get("/api/branches")
def list_branches():
    if not (DEMO_REPO_DIR / ".chronogit").exists():
        setup_demo_repository()

    res = run_chronogit(["branch"], DEMO_REPO_DIR)
    branches = []
    for line in res.stdout.splitlines():
        line = line.strip()
        if line.startswith("* "):
            branches.append({"name": line[2:].strip(), "is_current": True})
        elif line:
            branches.append({"name": line.strip(), "is_current": False})
    return {"branches": branches}

@app.post("/api/branch/create")
def create_branch(req: BranchRequest):
    res = run_chronogit(["branch", req.name], DEMO_REPO_DIR)
    if res.returncode != 0:
        raise HTTPException(status_code=400, detail=res.stderr or res.stdout)
    return {"status": "success", "message": f"Created branch {req.name}"}

@app.post("/api/branch/delete")
def delete_branch(req: BranchRequest):
    res = run_chronogit(["branch", "-d", req.name], DEMO_REPO_DIR)
    if res.returncode != 0:
        raise HTTPException(status_code=400, detail=res.stderr or res.stdout)
    return {"status": "success", "message": f"Deleted branch {req.name}"}

@app.post("/api/checkout")
def checkout(req: CheckoutRequest):
    args = ["checkout"]
    if req.create_branch:
        args += ["-b", req.target]
    else:
        args.append(req.target)
    if req.force:
        args.append("-f")

    res = run_chronogit(args, DEMO_REPO_DIR)
    if res.returncode != 0:
        raise HTTPException(status_code=400, detail=res.stderr or res.stdout)
    return {"status": "success", "message": res.stdout}

@app.post("/api/stage")
def stage_files(req: StageRequest):
    res = run_chronogit(["add"] + req.paths, DEMO_REPO_DIR)
    if res.returncode != 0:
        raise HTTPException(status_code=400, detail=res.stderr or res.stdout)
    return {"status": "success", "message": "Files staged successfully"}

@app.post("/api/commit")
def create_commit(req: CommitRequest):
    args = ["commit", "-m", req.message]
    if req.author_name:
        args += ["--author", req.author_name, req.author_email or "user@chronogit.local"]
    res = run_chronogit(args, DEMO_REPO_DIR)
    if res.returncode != 0:
        raise HTTPException(status_code=400, detail=res.stderr or res.stdout)
    return {"status": "success", "output": res.stdout}

@app.get("/api/diff")
def get_diff(staged: bool = False, commit1: Optional[str] = None, commit2: Optional[str] = None):
    args = ["diff"]
    if staged:
        args.append("--staged")
    if commit1:
        args.append(commit1)
    if commit2:
        args.append(commit2)

    res = run_chronogit(args, DEMO_REPO_DIR)
    return {"diff": res.stdout}

@app.post("/api/merge")
def merge_branch(req: MergeRequest):
    args = ["merge"]
    if req.abort:
        args.append("--abort")
    else:
        args.append(req.branch)

    res = run_chronogit(args, DEMO_REPO_DIR)
    is_conflict = "CONFLICT" in res.stdout or res.returncode != 0
    return {
        "status": "conflict" if is_conflict else "success",
        "output": res.stdout,
        "error": res.stderr if res.returncode != 0 else None
    }

@app.get("/api/fsck")
def run_fsck():
    res = run_chronogit(["fsck"], DEMO_REPO_DIR)
    return {
        "healthy": res.returncode == 0,
        "output": res.stdout
    }

@app.post("/api/gc")
def run_gc(dry_run: bool = False):
    args = ["gc"]
    if dry_run:
        args.append("--dry-run")
    res = run_chronogit(args, DEMO_REPO_DIR)
    return {"output": res.stdout}

@app.get("/api/benchmarks")
def get_benchmarks():
    bench_file = PROJECT_ROOT / "benchmarks" / "benchmark_results.json"
    if bench_file.exists():
        try:
            return json.loads(bench_file.read_text(encoding="utf-8"))
        except Exception:
            pass
    return []

@app.post("/api/files/write")
def write_file(req: FileWriteRequest):
    target = DEMO_REPO_DIR / req.path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(req.content, encoding="utf-8")
    return {"status": "success", "message": f"Written to {req.path}"}

@app.get("/api/files/read")
def read_file(path: str):
    target = DEMO_REPO_DIR / path
    if not target.exists():
        raise HTTPException(status_code=404, detail="File not found")
    return {"content": target.read_text(encoding="utf-8")}
