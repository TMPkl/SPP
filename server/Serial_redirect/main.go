package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"sync"
)

const colorReset = "\033[0m"

var ansiPalette = []string{
	"\033[31m", // red
	"\033[32m", // green
	"\033[33m", // yellow
	"\033[34m", // blue
	"\033[35m", // magenta
	"\033[36m", // cyan
	"\033[91m", // bright red
	"\033[92m", // bright green
	"\033[93m", // bright yellow
	"\033[94m", // bright blue
	"\033[95m", // bright magenta
	"\033[96m", // bright cyan
}

var webPalette = []string{
	"#e63946",
	"#2a9d8f",
	"#f4a261",
	"#457b9d",
	"#d62828",
	"#3a86ff",
	"#ff006e",
	"#06d6a0",
	"#f77f00",
	"#118ab2",
	"#8338ec",
	"#00b4d8",
}

type clientStyle struct {
	ansi string
	web  string
}

var (
	colorMu      sync.Mutex
	styleByID    = make(map[string]clientStyle)
	nextColorIdx = 0
)

func styleForClient(id string) clientStyle {
	colorMu.Lock()
	defer colorMu.Unlock()

	if style, ok := styleByID[id]; ok {
		return style
	}

	idx := nextColorIdx
	style := clientStyle{
		ansi: ansiPalette[idx%len(ansiPalette)],
		web:  webPalette[idx%len(webPalette)],
	}
	nextColorIdx++
	styleByID[id] = style
	return style
}

type logEvent struct {
	Type  string `json:"type"`
	ID    string `json:"id"`
	Addr  string `json:"addr"`
	Line  string `json:"line"`
	Color string `json:"color"`
}

type eventHub struct {
	mu          sync.Mutex
	subscribers map[chan logEvent]struct{}
}

func newEventHub() *eventHub {
	return &eventHub{subscribers: make(map[chan logEvent]struct{})}
}

func (h *eventHub) subscribe() chan logEvent {
	h.mu.Lock()
	defer h.mu.Unlock()

	ch := make(chan logEvent, 128)
	h.subscribers[ch] = struct{}{}
	return ch
}

func (h *eventHub) unsubscribe(ch chan logEvent) {
	h.mu.Lock()
	defer h.mu.Unlock()

	if _, ok := h.subscribers[ch]; ok {
		delete(h.subscribers, ch)
		close(ch)
	}
}

func (h *eventHub) broadcast(event logEvent) {
	h.mu.Lock()
	defer h.mu.Unlock()

	for ch := range h.subscribers {
		select {
		case ch <- event:
		default:
			// Slow client: drop the event instead of blocking all senders.
		}
	}
}

var hub = newEventHub()

func loadIngestToken() string {
	envToken := strings.TrimSpace(os.Getenv("LOG_INGEST_TOKEN"))
	if envToken != "" {
		return envToken
	}

	data, err := os.ReadFile("ingest_token.txt")
	if err != nil {
		return ""
	}

	return strings.TrimSpace(string(data))
}

var ingestToken = loadIngestToken()

type ingestRequest struct {
	ID   string `json:"id"`
	Line string `json:"line"`
}

func indexHandler(w http.ResponseWriter, _ *http.Request) {
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	_, _ = w.Write([]byte(indexHTML))
}

func eventsHandler(w http.ResponseWriter, r *http.Request) {
	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "Streaming unsupported", http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")

	ch := hub.subscribe()
	defer hub.unsubscribe(ch)

	for {
		select {
		case <-r.Context().Done():
			return
		case event := <-ch:
			payload, err := json.Marshal(event)
			if err != nil {
				continue
			}
			_, _ = fmt.Fprintf(w, "data: %s\n\n", payload)
			flusher.Flush()
		}
	}
}

func ingestHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	if ingestToken == "" {
		http.Error(w, "Server token is not configured", http.StatusServiceUnavailable)
		return
	}

	token := strings.TrimSpace(r.Header.Get("X-Log-Token"))
	if token == "" {
		auth := strings.TrimSpace(r.Header.Get("Authorization"))
		if strings.HasPrefix(auth, "Bearer ") {
			token = strings.TrimSpace(strings.TrimPrefix(auth, "Bearer "))
		}
	}

	if token != ingestToken {
		http.Error(w, "Unauthorized", http.StatusUnauthorized)
		return
	}

	var req ingestRequest
	decoder := json.NewDecoder(http.MaxBytesReader(w, r.Body, 4*1024))
	if err := decoder.Decode(&req); err != nil {
		http.Error(w, "Invalid JSON body", http.StatusBadRequest)
		return
	}

	id := strings.TrimSpace(req.ID)
	if id == "" {
		id = "ESP-UNKNOWN"
	}

	line := strings.TrimSpace(req.Line)
	if line == "" {
		w.WriteHeader(http.StatusNoContent)
		return
	}

	style := styleForClient(id)
	fmt.Printf("%s[%s]%s %s\n", style.ansi, id, colorReset, line)
	hub.broadcast(logEvent{Type: "log", ID: id, Addr: r.RemoteAddr, Line: line, Color: style.web})
	w.WriteHeader(http.StatusAccepted)
}

func handle(conn net.Conn) {
	defer conn.Close()

	addr := conn.RemoteAddr().String()

	reader := bufio.NewReader(conn)

	id, err := reader.ReadString('\n')
	if err != nil {
		fmt.Println("Failed to read ID:", err)
		return
	}

	id = strings.TrimSpace(id)
	if id == "" {
		id = addr
	}

	style := styleForClient(id)
	fmt.Printf("%sConnected: %s (%s)%s\n", style.ansi, id, addr, colorReset)
	hub.broadcast(logEvent{Type: "connect", ID: id, Addr: addr, Line: "connected", Color: style.web})

	// 2. Czytaj logi
	for {
		line, err := reader.ReadString('\n')
		if err != nil {
			fmt.Printf("%sDisconnected: %s (%s)%s\n", style.ansi, id, addr, colorReset)
			hub.broadcast(logEvent{Type: "disconnect", ID: id, Addr: addr, Line: "disconnected", Color: style.web})
			return
		}

		line = strings.TrimRight(line, "\r\n")
		if line == "" {
			continue
		}

		fmt.Printf("%s[%s]%s %s\n", style.ansi, id, colorReset, line)
		hub.broadcast(logEvent{Type: "log", ID: id, Addr: addr, Line: line, Color: style.web})
	}
}
func otaHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet && r.Method != http.MethodHead {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	binPath := filepath.Clean("./firmware.bin")
	info, err := os.Stat(binPath)
	if err != nil || info.IsDir() {
		http.Error(w, "File not found", http.StatusNotFound)
		return
	}

	w.Header().Set("Content-Type", "application/octet-stream")
	w.Header().Set("Content-Disposition", "attachment; filename=firmware.bin")
	http.ServeFile(w, r, binPath)

}

func main() {
	mux := http.NewServeMux()
	mux.HandleFunc("/", indexHandler)
	mux.HandleFunc("/events", eventsHandler)
	mux.HandleFunc("/ingest", ingestHandler)
	mux.HandleFunc("/ota", otaHandler)

	go func() {
		fmt.Println("Web preview listening on :6101")
		if ingestToken == "" {
			fmt.Println("WARNING: LOG_INGEST_TOKEN/ingest_token.txt is empty, /ingest will reject requests")
		}
		if err := http.ListenAndServe(":6101", mux); err != nil {
			panic(err)
		}
	}()

	listener, err := net.Listen("tcp", ":6110")
	if err != nil {
		panic(err)
	}

	fmt.Println("Server listening on :6110")

	for {
		conn, err := listener.Accept()
		if err != nil {
			continue
		}

		go handle(conn)
	}
}

const indexHTML = `<!doctype html>
<html lang="pl">
<head>
	<meta charset="utf-8" />
	<meta name="viewport" content="width=device-width, initial-scale=1" />
	<title>ESP Live Logs</title>
	<style>
		:root {
			--bg: #0f1720;
			--panel: #182433;
			--text: #e5e7eb;
			--muted: #9ca3af;
			--border: #263446;
		}
		body {
			margin: 0;
			font-family: "JetBrains Mono", "Fira Code", monospace;
			background: radial-gradient(circle at top, #1f2b3c 0%, var(--bg) 45%);
			color: var(--text);
		}
		.wrap {
			max-width: 1100px;
			margin: 0 auto;
			padding: 18px;
		}
		.head {
			display: flex;
			justify-content: space-between;
			align-items: center;
			gap: 12px;
			margin-bottom: 14px;
		}
		.title {
			font-size: 20px;
			font-weight: 700;
			letter-spacing: 0.3px;
		}
		.status {
			font-size: 13px;
			color: var(--muted);
		}
		.panel {
			border: 1px solid var(--border);
			background: color-mix(in srgb, var(--panel) 92%, black);
			border-radius: 12px;
			overflow: hidden;
			box-shadow: 0 16px 40px rgba(0, 0, 0, 0.35);
		}
		#logs {
			height: 72vh;
			overflow: auto;
			padding: 14px;
			white-space: pre-wrap;
			line-height: 1.35;
			font-size: 13px;
		}
		.row { margin-bottom: 4px; }
		.id {
			font-weight: 700;
			margin-right: 8px;
		}
		.meta {
			color: var(--muted);
			font-style: italic;
		}
	</style>
</head>
<body>
	<div class="wrap">
		<div class="head">
			<div class="title">ESP Live Logs</div>
			<div id="status" class="status">Connecting...</div>
		</div>
		<div class="panel">
			<div id="logs"></div>
		</div>
	</div>

	<script>
		const logs = document.getElementById('logs');
		const status = document.getElementById('status');

		function addRow(msg) {
			const row = document.createElement('div');
			row.className = 'row';

			const id = document.createElement('span');
			id.className = 'id';
			id.textContent = '[' + msg.id + ']';
			id.style.color = msg.color || '#9ca3af';
			row.appendChild(id);

			const text = document.createElement('span');
			if (msg.type === 'log') {
				text.textContent = msg.line;
			} else {
				text.className = 'meta';
				text.textContent = msg.type.toUpperCase() + ' (' + msg.addr + ')';
			}
			row.appendChild(text);

			logs.appendChild(row);
			logs.scrollTop = logs.scrollHeight;
		}

		const source = new EventSource('/events');
		source.onopen = () => { status.textContent = 'Connected'; };
		source.onerror = () => { status.textContent = 'Reconnecting...'; };
		source.onmessage = (event) => {
			try {
				const msg = JSON.parse(event.data);
				addRow(msg);
			} catch (_) {}
		};
	</script>
</body>
</html>`
