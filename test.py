import json
import os
import hashlib
import uuid
from flask import Flask, render_template_string, request, jsonify, session, abort

app = Flask(__name__)
app.secret_key = os.urandom(32)

# Klasör Yapısını Hazırla
DB_DIR = "database"
QUIZ_DIR = os.path.join(DB_DIR, "quizzes")
USERS_FILE = os.path.join(DB_DIR, "users.json")

for path in [DB_DIR, QUIZ_DIR]:
    if not os.path.exists(path):
        os.makedirs(path)

if not os.path.exists(USERS_FILE):
    with open(USERS_FILE, "w") as f: json.dump({}, f)

def get_users():
    with open(USERS_FILE, "r") as f: return json.load(f)

def save_users(data):
    with open(USERS_FILE, "w") as f: json.dump(data, f, indent=4)

# --- ARAYÜZ (Gelişmiş Search ve Quiz Yönetimi) ---
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <title>Kahoot Pro Enterprise</title>
    <style>
        :root { --p: #46178f; --s: #eb670f; --bg: #111; --card: #1e1e1e; }
        body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: #eee; margin: 0; }
        .container { max-width: 1000px; margin: auto; padding: 20px; }
        .card { background: var(--card); padding: 20px; border-radius: 12px; box-shadow: 0 4px 20px rgba(0,0,0,0.5); }
        input, select { background: #2a2a2a; border: 1px solid #444; color: white; padding: 12px; border-radius: 6px; width: 100%; margin: 10px 0; }
        button { background: var(--p); color: white; border: none; padding: 12px 20px; border-radius: 6px; cursor: pointer; font-weight: bold; }
        .search-box { border-bottom: 2px solid var(--s); margin-bottom: 20px; padding-bottom: 20px; }
        .quiz-item { background: #2a2a2a; padding: 15px; margin: 10px 0; border-radius: 8px; display: flex; justify-content: space-between; align-items: center; }
        .hidden { display: none; }
        .badge { background: var(--s); padding: 4px 8px; border-radius: 4px; font-size: 12px; }
    </style>
</head>
<body>

<div class="container">
    <div id="auth-ui" class="card" style="width:350px; margin: 100px auto;">
        <h2>Kahoot Pro Login</h2>
        <input type="text" id="user" placeholder="Kullanıcı">
        <input type="password" id="pass" placeholder="Şifre">
        <button onclick="auth('login')" style="width:100%">Giriş</button>
        <button onclick="auth('register')" style="width:100%; background:#444; margin-top:5px;">Kayıt</button>
    </div>

    <div id="main-ui" class="hidden">
        <div style="display:flex; justify-content:space-between; align-items:center;">
            <h1>Dashboard</h1>
            <button onclick="location.reload()" style="background:#cc2d3b">Çıkış</button>
        </div>

        <div class="card search-box">
            <h3>Quiz Arama</h3>
            <input type="text" id="search-input" onkeyup="searchQuizzes()" placeholder="Quiz başlığı veya ID ara...">
            <div id="quiz-list"></div>
        </div>

        <div id="admin-tools" class="card hidden">
            <h3 style="color:var(--s)">Yeni Quiz Oluştur (Admin)</h3>
            <input type="text" id="q-title" placeholder="Quiz Başlığı">
            <button onclick="createQuiz()">Quiz JSON Oluştur</button>
        </div>
    </div>
</div>

<script>
    async function hash(p) {
        const msg = new TextEncoder().encode(p);
        const hash = await crypto.subtle.digest('SHA-256', msg);
        return Array.from(new Uint8Array(hash)).map(b => b.toString(16).padStart(2, '0')).join('');
    }

    async function auth(type) {
        const u = document.getElementById('user').value;
        const p = await hash(document.getElementById('pass').value);
        const res = await fetch('/api/auth', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({u, p, action: type})
        });
        const data = await res.json();
        if(data.success) {
            document.getElementById('auth-ui').classList.add('hidden');
            document.getElementById('main-ui').classList.remove('hidden');
            if(data.role === 'admin') document.getElementById('admin-tools').classList.remove('hidden');
            searchQuizzes();
        } else alert(data.msg);
    }

    async function createQuiz() {
        const title = document.getElementById('q-title').value;
        await fetch('/api/admin/create_quiz', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({title})
        });
        searchQuizzes();
    }

    async function searchQuizzes() {
        const query = document.getElementById('search-input').value;
        const res = await fetch(`/api/search?q=${query}`);
        const data = await res.json();
        const list = document.getElementById('quiz-list');
        list.innerHTML = "";
        data.forEach(q => {
            list.innerHTML += `
                <div class="quiz-item">
                    <span><strong>${q.title}</strong> <br><small>${q.id}</small></span>
                    <span class="badge">JSON Aktif</span>
                </div>`;
        });
    }
</script>
</body>
</html>
'''

@app.route('/')
def index(): return render_template_string(HTML_TEMPLATE)

@app.route('/api/auth', methods=['POST'])
def auth():
    d = request.json
    users = get_users()
    u, p = d['u'], d['p']
    
    if d['action'] == 'register':
        if u in users: return jsonify({"success": False, "msg": "Mevcut!"})
        users[u] = {"p": p, "role": "user"}
        save_users(users)
        return jsonify({"success": True})
    
    if u in users and users[u]['p'] == p:
        session['u'] = u
        session['role'] = users[u]['role']
        return jsonify({"success": True, "role": users[u]['role']})
    return jsonify({"success": False, "msg": "Hatalı!"})

@app.route('/api/admin/create_quiz', methods=['POST'])
def create_quiz():
    if session.get('role') != 'admin': abort(404)
    q_id = str(uuid.uuid4())[:8]
    title = request.json.get('title')
    quiz_data = {"id": q_id, "title": title, "questions": []}
    
    with open(os.path.join(QUIZ_DIR, f"quiz_{q_id}.json"), "w") as f:
        json.dump(quiz_data, f, indent=4)
    return jsonify({"success": True})

@app.route('/api/search')
def search():
    query = request.args.get('q', '').lower()
    results = []
    for filename in os.listdir(QUIZ_DIR):
        if filename.endswith(".json"):
            with open(os.path.join(QUIZ_DIR, filename), "r") as f:
                data = json.load(f)
                if query in data['title'].lower() or query in data['id'].lower():
                    results.append(data)
    return jsonify(results)

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000, debug=True)
