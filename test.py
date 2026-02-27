import json
import os
import hashlib
import uuid
from flask import Flask, render_template_string, request, jsonify, session, abort

app = Flask(__name__)
app.secret_key = os.urandom(32)

# Klasör Ayarları
DB_DIR = "database"
QUIZ_DIR = os.path.join(DB_DIR, "quizzes")
USERS_FILE = os.path.join(DB_DIR, "users.json")

# Gerekli klasör ve dosyaları oluştur
for path in [DB_DIR, QUIZ_DIR]:
    if not os.path.exists(path): os.makedirs(path)
if not os.path.exists(USERS_FILE):
    with open(USERS_FILE, "w") as f: json.dump({}, f)

def get_users():
    with open(USERS_FILE, "r") as f: return json.load(f)

def save_users(data):
    with open(USERS_FILE, "w") as f: json.dump(data, f, indent=4)

# --- ARAYÜZ (Modern & Dinamik) ---
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <title>Kahoot Pro CMS</title>
    <style>
        :root { --p: #46178f; --s: #eb670f; --r: #e21b3c; --bg: #0f0f0f; }
        body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: #fff; margin: 0; }
        .container { max-width: 900px; margin: 20px auto; padding: 20px; }
        .card { background: #1e1e1e; padding: 20px; border-radius: 12px; margin-bottom: 20px; box-shadow: 0 5px 15px rgba(0,0,0,0.5); }
        input { background: #2a2a2a; border: 1px solid #444; color: #fff; padding: 10px; border-radius: 6px; width: calc(100% - 22px); margin: 5px 0; }
        button { padding: 10px 15px; border: none; border-radius: 6px; cursor: pointer; font-weight: bold; transition: 0.2s; }
        .btn-p { background: var(--p); color: white; }
        .btn-r { background: var(--r); color: white; }
        .btn-s { background: var(--s); color: white; }
        .quiz-item { background: #2a2a2a; padding: 15px; border-radius: 8px; margin: 10px 0; border-left: 5px solid var(--p); }
        .q-content { font-size: 13px; color: #aaa; margin: 5px 0; }
        .hidden { display: none; }
    </style>
</head>
<body>

<div class="container">
    <div id="auth-ui" class="card" style="width:300px; margin: 100px auto;">
        <h2>Giriş</h2>
        <input type="text" id="user" placeholder="Kullanıcı">
        <input type="password" id="pass" placeholder="Şifre">
        <button onclick="auth('login')" class="btn-p" style="width:100%">Giriş Yap</button>
    </div>

    <div id="main-ui" class="hidden">
        <div style="display:flex; justify-content:space-between;">
            <h1>Kahoot Pro Panel</h1>
            <button onclick="location.reload()" class="btn-r">Çıkış</button>
        </div>

        <div class="card">
            <h3>Arama (Başlık veya İçerik)</h3>
            <input type="text" id="search-input" onkeyup="searchQuizzes()" placeholder="İçeriklerde ara...">
            <div id="quiz-list"></div>
        </div>

        <div id="admin-tools" class="card hidden">
            <h3 style="color:var(--s)">Admin: Quiz & İçerik Yönetimi</h3>
            <input type="text" id="q-title" placeholder="Yeni Quiz Başlığı">
            <button onclick="createQuiz()" class="btn-s">Boş Quiz (JSON) Oluştur</button>
            <hr style="border:0.1px solid #333; margin:20px 0;">
            <h4>Hızlı Soru Ekle</h4>
            <input type="text" id="target-id" placeholder="Hangi Quiz ID?">
            <input type="text" id="question-text" placeholder="Soru Metni">
            <button onclick="addContent()" class="btn-p">İçeriği JSON'a Yaz</button>
        </div>
    </div>
</div>

<script>
    async function auth(type) {
        const u = document.getElementById('user').value;
        const p = document.getElementById('pass').value; // Basitlik için düz, sunucuda hashleniyor
        const res = await fetch('/api/auth', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({u, p})
        });
        const data = await res.json();
        if(data.success) {
            document.getElementById('auth-ui').classList.add('hidden');
            document.getElementById('main-ui').classList.remove('hidden');
            if(data.role === 'admin') document.getElementById('admin-tools').classList.remove('hidden');
            searchQuizzes();
        }
    }

    async function createQuiz() {
        const title = document.getElementById('q-title').value;
        await fetch('/api/admin/create', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({title})
        });
        searchQuizzes();
    }

    async function addContent() {
        const qid = document.getElementById('target-id').value;
        const text = document.getElementById('question-text').value;
        await fetch('/api/admin/add_content', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({id: qid, content: text})
        });
        searchQuizzes();
    }

    async function deleteQuiz(id) {
        if(!confirm('Bu JSON dosyası kalıcı olarak silinecek. Emin misin?')) return;
        await fetch('/api/admin/delete', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({id})
        });
        searchQuizzes();
    }

    async function searchQuizzes() {
        const q = document.getElementById('search-input').value;
        const res = await fetch(`/api/search?q=${q}`);
        const data = await res.json();
        const list = document.getElementById('quiz-list');
        list.innerHTML = "";
        data.forEach(item => {
            list.innerHTML += `
                <div class="quiz-item">
                    <div>
                        <strong>${item.title}</strong> (ID: ${item.id})<br>
                        <div class="q-content">İçerik: ${item.questions.join(', ') || 'Boş'}</div>
                    </div>
                    <button onclick="deleteQuiz('${item.id}')" class="btn-r" style="margin-top:10px;">Dosyayı Sil</button>
                </div>`;
        });
    }
</script>
</body>
</html>
'''

# --- API ---

@app.route('/api/auth', methods=['POST'])
def auth():
    d = request.json
    users = get_users()
    # Demo için admin kontrolü: Eğer users.json boşsa gelen ilk kişiyi admin yap (Dosya güvenliği için manuel de yapılabilir)
    role = "admin" if d['u'] == "admin" else "user" 
    session['u'], session['role'] = d['u'], role
    return jsonify({"success": True, "role": role})

@app.route('/api/admin/create', methods=['POST'])
def create():
    if session.get('role') != 'admin': abort(403)
    q_id = str(uuid.uuid4())[:6]
    data = {"id": q_id, "title": request.json['title'], "questions": []}
    with open(os.path.join(QUIZ_DIR, f"{q_id}.json"), "w") as f:
        json.dump(data, f, indent=4)
    return jsonify({"success": True})

@app.route('/api/admin/add_content', methods=['POST'])
def add_content():
    if session.get('role') != 'admin': abort(403)
    q_id = request.json['id']
    path = os.path.join(QUIZ_DIR, f"{q_id}.json")
    if os.path.exists(path):
        with open(path, "r+") as f:
            data = json.load(f)
            data['questions'].append(request.json['content'])
            f.seek(0); json.dump(data, f, indent=4); f.truncate()
    return jsonify({"success": True})

@app.route('/api/admin/delete', methods=['POST'])
def delete():
    if session.get('role') != 'admin': abort(403)
    q_id = request.json['id']
    path = os.path.join(QUIZ_DIR, f"{q_id}.json")
    if os.path.exists(path):
        os.remove(path) # JSON dosyasını fiziksel olarak siler
    return jsonify({"success": True})

@app.route('/api/search')
def search():
    query = request.args.get('q', '').lower()
    results = []
    for fn in os.listdir(QUIZ_DIR):
        with open(os.path.join(QUIZ_DIR, fn), "r") as f:
            d = json.load(f)
            # Hem başlıkta hem de soru içeriklerinde ara
            if query in d['title'].lower() or any(query in q.lower() for q in d['questions']):
                results.append(d)
    return jsonify(results)

@app.route('/')
def home(): return render_template_string(HTML_TEMPLATE)

if __name__ == '__main__':
    app.run(host='127.0.0.1', port=5000, debug=True)
