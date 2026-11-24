import sys
import http.server
import socket
import socketserver
import urllib.request
import urllib.error
from pathlib import Path

PORT = 5173
BACKEND_URL = "http://127.0.0.1:8000"
WEB_DIR = Path(__file__).resolve().parent

class DualStackServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    address_family = socket.AF_INET6
    daemon_threads = True

    def server_bind(self):
        try:
            self.socket.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
        except Exception:
            pass
        return super().server_bind()

class ViteProxyHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_DIR), **kwargs)

    def do_GET(self):
        if self.path.startswith("/api/"):
            self.proxy_request()
        else:
            if self.path == "/" or not (WEB_DIR / self.path.lstrip("/")).exists():
                self.path = "/index.html"
            super().do_GET()

    def do_POST(self):
        if self.path.startswith("/api/"):
            self.proxy_request()
        else:
            self.send_error(404, "Not Found")

    def proxy_request(self):
        target_url = f"{BACKEND_URL}{self.path}"
        content_length = int(self.headers.get("Content-Length", 0))
        post_data = self.rfile.read(content_length) if content_length > 0 else None

        req = urllib.request.Request(target_url, data=post_data, method=self.command)
        for header, val in self.headers.items():
            if header.lower() not in ["host", "content-length"]:
                req.add_header(header, val)

        try:
            with urllib.request.urlopen(req) as resp:
                self.send_response(resp.status)
                for h, val in resp.headers.items():
                    self.send_header(h, val)
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(resp.read())
        except urllib.error.HTTPError as e:
            self.send_response(e.code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(e.read())
        except Exception as e:
            self.send_response(502)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(f'{{"error": "Backend offline or unreachable: {str(e)}"}}'.encode())

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        self.end_headers()

class ThreadedServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True

if __name__ == "__main__":
    try:
        httpd = DualStackServer(("::", PORT), ViteProxyHandler)
        print(f"[*] Dual-stack (IPv6 + IPv4) listener bound on port {PORT}")
    except Exception as e:
        print(f"[*] IPv6 bind failed ({e}), falling back to IPv4 0.0.0.0")
        httpd = ThreadedServer(("0.0.0.0", PORT), ViteProxyHandler)

    print(f"\n==================================================================")
    print(f"  ChronoGit Frontend Dev Server running at: http://localhost:{PORT}")
    print(f"  Direct IPv4 access at: http://127.0.0.1:{PORT}")
    print(f"  Proxying /api/ calls to FastAPI backend at: {BACKEND_URL}")
    print(f"==================================================================\n")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down frontend server.")
