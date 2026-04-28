"""
api.py -- Flight Runway Management System
HTML frontend aur C++ engine ke beech ka bridge -- ye aaise hota hai middleware.
C++ protocol mein JSON nahi hai, sirf plain pipe text hai.
Python HTTP aur state parsing handle karta hai; C++ mein saari business logic hai.

Changes in this version:
  - AUTO_ALLOCATE_ALL command supported (/auto_allocate_all endpoint)
  - ALLOCATE with runway_id=-1 ab emergency flights ke liye bhi kaam karta hai
"""

from flask import Flask, request, jsonify, send_from_directory
import os, subprocess, threading

app = Flask(__name__, static_folder='.', static_url_path='')

# C++ engine ka path -- is file ke relative ../backend/ mein hota hai
BASE_DIR    = os.path.dirname(os.path.abspath(__file__))
ENGINE_PATH = os.path.join(BASE_DIR, '..', 'backend', 'flight_engine')

# Persistent C++ engine process -- baar baar start karna slow hoga isliye ek hi rakho
_engine_proc = None
_engine_lock = threading.Lock()  # thread safe banana padega -- ye aaise hota hai mutex


def _get_engine():
    """Engine process return karo, agar band ho gaya toh restart karo."""
    global _engine_proc
    if _engine_proc is None or _engine_proc.poll() is not None:
        if not os.path.exists(ENGINE_PATH):
            raise FileNotFoundError(
                f"C++ engine not found at: {ENGINE_PATH}\n"
                "Compile with:  g++ -O2 -std=c++17 -o backend/flight_engine "
                "backend/main.cpp backend/algorithms.cpp backend/handlers.cpp"
            )
        # subprocess banao stdin/stdout pipe ke saath
        _engine_proc = subprocess.Popen(
            [ENGINE_PATH],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1  # line buffered -- taaki flush turant kaam kare
        )
    return _engine_proc


def send_cmd(command: str) -> str:
    """Engine ko ek command bhejo, ek response line wapas lo."""
    with _engine_lock:
        eng = _get_engine()
        try:
            eng.stdin.write(command + '\n')
            eng.stdin.flush()
            line = eng.stdout.readline().rstrip('\n')
            return line
        except Exception as e:
            return f"ERROR|Engine error: {e}"


def send_cmd_multiline(command: str, end_marker: str) -> list:
    """Command bhejo, end_marker aane tak saari lines lo."""
    with _engine_lock:
        eng = _get_engine()
        try:
            eng.stdin.write(command + '\n')
            eng.stdin.flush()
            lines = []
            while True:
                line = eng.stdout.readline().rstrip('\n')
                if line == end_marker or not line:
                    break
                lines.append(line)
            return lines
        except Exception as e:
            return [f"ERROR|Engine error: {e}"]


# Parse helpers -- C++ ki pipe output ko Python dict mein badlo

def parse_flight(raw: str) -> dict:
    """Format: id|name|type|eta|departure|runway_id|status|active"""
    p = raw.split('|')
    if len(p) < 8:
        return None
    return {
        'id':             int(p[0]),
        'name':           p[1],
        'type':           p[2],
        'time':           int(p[3]),
        'departure_time': int(p[4]),
        'runway':         int(p[5]) if int(p[5]) != -1 else None,
        'status':         p[6],
        'active':         p[7] == '1',
        'new_time':       None
    }


def parse_runway(raw: str) -> dict:
    """Format: id|name|available|maintenance|current_flight_id[|Qfid...]"""
    p = raw.split('|')
    if len(p) < 5:
        return None
    # Q prefix wale entries queue ke flight ids hain
    queue_ids = [int(x[1:]) for x in p[5:] if x.startswith('Q')]
    return {
        'id':             int(p[0]),
        'name':           p[1],
        'available':      p[2] == '1',
        'maintenance':    p[3] == '1',
        'current_flight': int(p[4]) if int(p[4]) != -1 else None,
        'queue':          queue_ids
    }


def get_full_status() -> dict:
    """Engine se saare flights aur runways fetch karo."""
    with _engine_lock:
        eng = _get_engine()
        eng.stdin.write('STATUS\n')
        eng.stdin.flush()

        flights, runways = [], []
        section = None

        while True:
            line = eng.stdout.readline().rstrip('\n')
            if not line:
                break
            if line == 'FLIGHTS_BEGIN':
                section = 'flights'
            elif line == 'FLIGHTS_END':
                section = None
            elif line == 'RUNWAYS_BEGIN':
                section = 'runways'
            elif line == 'RUNWAYS_END':
                break
            elif section == 'flights':
                f = parse_flight(line)
                if f:
                    flights.append(f)
            elif section == 'runways':
                r = parse_runway(line)
                if r:
                    runways.append(r)

        # runway ki queue mein sirf ids hain -- unhe full flight objects se replace karo
        flight_map = {f['id']: f for f in flights}
        for r in runways:
            r['queue'] = [flight_map[qid] for qid in r['queue'] if qid in flight_map]

        return {'flights': flights, 'runways': runways}


def get_history() -> list:
    """Engine se flight history fetch karo."""
    with _engine_lock:
        eng = _get_engine()
        eng.stdin.write('HISTORY_FLIGHTS\n')
        eng.stdin.flush()
        history = []
        while True:
            line = eng.stdout.readline().rstrip('\n')
            if not line or line == 'HISTORY_END':
                break
            if line == 'HISTORY_BEGIN':
                continue
            f = parse_flight(line)
            if f:
                history.append({
                    'flight':         f['name'],
                    'type':           f['type'],
                    'runway':         f['runway'] if f['runway'] else '--',
                    'time':           f['time'],
                    'departure_time': f['departure_time'],
                    'queued':         f['status'] == 'queued',
                    'status':         f['status']
                })
        return history


# CORS -- browser cross-origin request allow karne ke liye zaroori hai
@app.after_request
def add_cors(resp):
    resp.headers['Access-Control-Allow-Origin']  = '*'
    resp.headers['Access-Control-Allow-Headers'] = 'Content-Type'
    resp.headers['Access-Control-Allow-Methods'] = 'GET, POST, OPTIONS'
    return resp

@app.route('/<path:path>', methods=['OPTIONS'])
def options(path): return '', 204


# Static files -- HTML/CSS/JS serve karne ke liye
@app.route('/')
def index(): return send_from_directory('.', 'index.html')

@app.route('/<path:path>')
def static_files(path): return send_from_directory('.', path)


# Flight routes

@app.route('/add_flight', methods=['POST'])
def add_flight():
    d     = request.json
    fid   = d.get('id')
    name  = str(d.get('name', '')).strip().replace(' ', '_')
    ftype = d.get('type', 'normal')
    eta   = d.get('time')
    dep   = d.get('departure_time', -1)
    if dep is None:
        dep = -1
    if not fid or not name or eta is None:
        return jsonify({'error': 'id, name and arrival ETA are required.'})
    resp = send_cmd(f"ADD_FLIGHT {fid} {name} {ftype} {eta} {dep}")
    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})
    return jsonify({'message': resp.split('|', 1)[1]})


@app.route('/delete_flight', methods=['POST'])
def delete_flight():
    fid  = request.json.get('id')
    resp = send_cmd(f"DELETE_FLIGHT {fid}")
    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})
    return jsonify({'message': resp.split('|', 1)[1]})


@app.route('/show_flights', methods=['POST'])
def show_flights():
    status = get_full_status()
    return jsonify({'flights': status['flights']})


@app.route('/search_flight', methods=['POST'])
def search_flight():
    query   = str(request.json.get('query', '')).lower()
    status  = get_full_status()
    results = [f for f in status['flights']
               if str(f['id']) == query or f['name'].lower() == query]
    return jsonify({'result': results})


@app.route('/flight_history', methods=['POST'])
def flight_history():
    h = get_history()
    return jsonify({'history': h, 'total': len(h)})


# Runway routes

@app.route('/add_runway', methods=['POST'])
def add_runway():
    d    = request.json
    rid  = d.get('id')
    name = str(d.get('name', '')).strip().replace(' ', '-')
    if not rid or not name:
        return jsonify({'error': 'id and name are required.'})
    resp = send_cmd(f"ADD_RUNWAY {rid} {name}")
    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})
    return jsonify({'message': resp.split('|', 1)[1]})


@app.route('/delete_runway', methods=['POST'])
def delete_runway():
    rid  = request.json.get('id')
    resp = send_cmd(f"DELETE_RUNWAY {rid}")
    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})
    return jsonify({'message': resp.split('|', 1)[1]})


@app.route('/maintenance_runway', methods=['POST'])
def maintenance_runway():
    rid  = request.json.get('id')
    resp = send_cmd(f"MAINTENANCE {rid}")
    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})
    return jsonify({'message': resp.split('|', 1)[1]})


@app.route('/search_runway', methods=['POST'])
def search_runway():
    query   = str(request.json.get('query', '')).lower()
    status  = get_full_status()
    results = [r for r in status['runways']
               if str(r['id']) == query or r['name'].lower() == query]
    return jsonify({'result': results})


# Allocation routes

@app.route('/allocate_runway', methods=['POST'])
def allocate_runway():
    """
    Ek flight allocate karo.
    Agar runway_id nahi diya ya -1 hai toh AUTO mode use hoga -- har type ke liye
    (emergency preempt bhi kar sakta hai AUTO mode mein).
    """
    data    = request.json
    fid     = data.get('flight_id')
    rid     = data.get('runway_id')

    if fid is None:
        return jsonify({'error': 'flight_id is required.'})

    rid_val = rid if rid is not None else -1
    resp    = send_cmd(f"ALLOCATE {fid} {rid_val}")

    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})

    parts    = resp.split('|')
    msg      = parts[1] if len(parts) > 1 else resp
    response = {'message': msg}

    # agar conflict hua tha toh uski info bhi include karo
    if len(parts) >= 4 and parts[2] in ('PREEMPTION', 'CONFLICT'):
        response['conflict'] = {'type': parts[2], 'message': parts[3]}

    return jsonify(response)


@app.route('/auto_allocate_all', methods=['POST'])
def auto_allocate_all():
    """
    Saare pending flights ko ek saath best available runways pe allocate karo.
    Emergency flights ko priority milti hai (Algorithm 2 ordering C++ engine mein).
    Summary return karta hai: allocated, queued, failed counts aur per-flight log.
    """
    resp = send_cmd("AUTO_ALLOCATE_ALL")

    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})

    # Wire format: OK|AUTO_ALLOC|allocated=N queued=M failed=K|log
    parts   = resp.split('|', 3)
    summary = parts[2] if len(parts) > 2 else ''
    log_str = parts[3] if len(parts) > 3 else ''

    # summary se counts parse karo
    counts = {}
    for token in summary.split():
        k, _, v = token.partition('=')
        counts[k] = int(v) if v.isdigit() else v

    # per-flight log list banao -- semicolon se split karo
    log_entries = [e.strip() for e in log_str.split(';') if e.strip()]

    return jsonify({
        'message':   f"Auto-allocation complete: "
                     f"{counts.get('allocated', 0)} landing now, "
                     f"{counts.get('queued', 0)} queued, "
                     f"{counts.get('failed', 0)} could not be assigned.",
        'allocated': counts.get('allocated', 0),
        'queued':    counts.get('queued', 0),
        'failed':    counts.get('failed', 0),
        'log':       log_entries
    })


@app.route('/set_delay_time', methods=['POST'])
def set_delay_time():
    fid      = request.json.get('flight_id')
    new_time = request.json.get('new_time')
    if fid is None or new_time is None or new_time < 0:
        return jsonify({'error': 'flight_id and valid new_time required.'})
    resp     = send_cmd(f"SET_DELAY {fid} {new_time}")
    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})
    parts    = resp.split('|')
    msg      = parts[1] if len(parts) > 1 else resp
    response = {'message': msg}
    # auto allocation hua tha toh uski info bhi bhejo
    if len(parts) >= 4 and parts[2] in ('AUTO_ALLOC', 'MOVED_TO_FREE', 'REQUEUED'):
        response['auto_alloc'] = parts[3]
        response['alloc_type'] = parts[2]
    return jsonify(response)


@app.route('/clear_runway', methods=['POST'])
def clear_runway():
    rid  = request.json.get('runway_id')
    resp = send_cmd(f"CLEAR {rid}")
    if resp.startswith('ERROR'):
        return jsonify({'error': resp.split('|', 1)[1]})
    return jsonify({'message': resp.split('|', 1)[1]})


# Status endpoint -- frontend yahi poll karta hai
@app.route('/status')
def status():
    s = get_full_status()
    h = get_history()
    return jsonify({
        'total_flights': len(s['flights']),
        'total_runways': len(s['runways']),
        'flights':  s['flights'],
        'runways':  s['runways'],
        'history':  h
    })


if __name__ == '__main__':
    if not os.path.exists(ENGINE_PATH):
        print(f"[ERROR] C++ engine not found at: {ENGINE_PATH}")
        print("    Compile with:")
        print("    g++ -O2 -std=c++17 -o backend/flight_engine \\")
        print("        backend/main.cpp backend/algorithms.cpp backend/handlers.cpp")
    else:
        print(f"[OK] C++ engine found: {ENGINE_PATH}")
    print("[INFO] Flight Management System starting -- http://localhost:5000")
    app.run(debug=True, host='0.0.0.0', port=5000)
