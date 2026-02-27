import json
import os
import uuid
import hashlib
from flask import Flask, render_template_string, request, jsonify, abort

app = Flask(__name__)

# --- VERİTABANI (KLASÖR) YAPILANDIRMASI ---
DB_DIR = "database"
QUIZ_DIR = os.path.join(DB_DIR, "quizzes")
USERS_FILE = os.path.join(DB_DIR, "users.json")
PEPPER = "Kahoot_Secret_Key_99"

for path in [DB_DIR, QUIZ_DIR]:
    if not os.path.exists(path): os.makedirs(path)
if not os.path.exists(USERS_FILE):
    with open(USERS_FILE, "w") as f: json.dump({}, f)

def get_users():
    with open(USERS_FILE, "r") as f: return json.load(f)

def save_users(data):
    with open(USERS_FILE, "w") as f: json.dump(data, f, indent=4)

def secure_hash(browser_hash, username):
    return hashlib.sha256((browser_hash + username + PEPPER).encode()).hexdigest()

# --- ARAYÜZ VE MANTIK (HTML + SAF CSS + JS) ---
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <title>Kahoot - Ultimate Edition</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        :root { --p: #46178f; --red: #e21b3c; --blue: #1368ce; --yellow: #ffa602; --green: #26890c; --bg: #f2f2f2; }
        body { font-family: 'Segoe UI', sans-serif; background: var(--bg); margin: 0; color: #333; text-align: center; }
        .container { width: 90%; max-width: 600px; margin: 40px auto; }
        .card { background: white; padding: 30px; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.1); margin-bottom: 20px; }
        
        input, select { width: 100%; padding: 15px; margin: 10px 0; border: 2px solid #ddd; border-radius: 8px; box-sizing: border-box; font-size: 1rem; }
        input:focus { border-color: var(--p); outline: none; }
        
        button { width: 100%; padding: 15px; border: none; border-radius: 8px; font-weight: bold; cursor: pointer; transition: 0.2s; font-size: 1.1rem; color: white; margin-top: 5px; }
        button:hover { opacity: 0.9; transform: scale(1.02); }
        .btn-main { background: var(--p); }
        .btn-sec { background: #555; }
        .btn-danger { background: var(--red); }
        
        .header { background: white; padding: 20px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); display: flex; justify-content: space-between; align-items: center; }
        
        /* 4 Şıklı Oyun Grid'i */
        .ans-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; padding: 20px; }
        .ans-btn { height: 120px; font-size: 1.5rem; display: flex; align-items: center; justify-content: center; box-shadow: 0 6px 0 rgba(0,0,0,0.2); border-radius: 10px; }
        .ans-btn:active { transform: translateY(4px); box-shadow: 0 2px 0 rgba(0,0,0,0.2); }
        .btn-0 { background: var(--red); } .btn-1 { background: var(--blue); }
        .btn-2 { background: var(--yellow); } .btn-3 { background: var(--green); }
        
        .hidden { display: none; }
        .quiz-item { background: #f9f9f9; padding: 15px; border-left: 5px solid var(--p); margin: 10px 0; border-radius: 5px; text-align: left; }
    </style>
</head>
<body onload="checkSession()">

<div id="auth-ui" class="container">
    <div class="card">
        <h1 style="color:var(--p)">Kahoot Login</h1>
        <input type="text" id="user" placeholder="Kullanıcı Adı">
        <input type="password" id="pass" placeholder="Şifre">
        <button class="btn-main" onclick="runAuth('login')">GİRİŞ YAP</button>
        <button class="btn-sec" onclick="runAuth('register')">KAYIT OL</button>
        <p id="auth-msg" style="font-size:14px; margin-top:10px;"></p>
    </div>
</div>

<div id="dashboard-ui" class="hidden">
    <div class="header">
        <h2 style="color:var(--p); margin:0;">Dashboard</h2>
        <div>
            <span id="user-display" style="font-weight:bold; margin-right:15px;"></span>
            <button onclick="logout()" style="width:auto; padding:8px 15px; background:#ddd; color:#333;">Çıkış</button>
        </div>
    </div>
    
    <div class="container">
        <div class="card" style="background:var(--p); color:white;">
            <h2>Oyuna Katıl</h2>
            <input type="text" id="join-pin" placeholder="Quiz PIN (ID) Girin" style="text-align:center; font-weight:bold;">
            <button onclick="joinGame()" style="background:white; color:var(--p);">GİRİŞ</button>
        </div>

        <div id="admin-panel" class="hidden">
            <div class="card">
                <h3 style="color:var(--green)">Quiz Yönetimi (Admin)</h3>
                
                <input type="text" id="q-title" placeholder="Yeni Quiz Başlığı">
                <button class="btn-main" onclick="createQuiz()">Boş Quiz JSON Oluştur</button>
                <hr style="margin:20px 0; border:0.5px solid #eee;">
                
                <h4>Soru Ekle (4 Şıklı)</h4>
                <input type="text" id="target-id" placeholder="Quiz ID Girin">
                <input type="text" id="s-q" placeholder="Soru Metni">
                <div style="display:flex; gap:10px;">
                    <input type="text" id="s-0" placeholder="Kırmızı Şık">
                    <input type="text" id="s-1" placeholder="Mavi Şık">
                </div>
                <div style="display:flex; gap:10px;">
                    <input type="text" id="s-2" placeholder="Sarı Şık">
                    <input type="text" id="s-3" placeholder="Yeşil Şık">
                </div>
                <select id="s-correct">
                    <option value="0">Doğru Cevap: Kırmızı</option>
                    <option value="1">Doğru Cevap: Mavi</option>
                    <option value="2">Doğru Cevap: Sarı</option>
                    <option value="3">Doğru Cevap: Yeşil</option>
                </select>
                <button onclick="addQuestion()" style="background:var(--green)">Soruyu JSON'a Ekle</button>
            </div>

            <div class="card">
                <h3>Sistemdeki Quizler</h3>
                <input type="text" id="search-input" onkeyup="searchQuizzes()" placeholder="Başlık veya içerik ara...">
                <div id="quiz-list"></div>
            </div>
        </div>
    </div>
</div>

<div id="game-ui" class="hidden">
    <div class="header">
        <h2 id="game-title" style="margin:0;">Quiz</h2>
        <button onclick="exitGame()" class="btn-danger" style="width:auto; padding:8px 15px;">Oyundan Çık</button>
    </div>
    
    <div class="container" style="max-width:800px;">
        <div class="card" style="padding: 40px; font-size: 2rem;">
            <span id="question-text">Soru Yükleniyor...</span>
        </div>
        
        <div class="ans-grid">
            <button class="ans-btn btn-0" onclick="submitAnswer(0)" id="opt0"></button>
            <button class="ans-btn btn-1" onclick="submitAnswer(1)" id="opt1"></button>
            <button class="ans-btn btn-2" onclick="submitAnswer(2)" id="opt2"></button>
            <button class="ans-btn btn-3" onclick="submitAnswer(3)" id="opt3"></button>
        </div>
        <h2 id="feedback" style="margin-top:20px;"></h2>
    </div>
</div>

<script>
    // --- BROWSER HASHING (AES-256 Mantığı / SHA-256) ---
    async function hashInBrowser(p) {
        const msg = new TextEncoder().encode(p);
        const hash = await crypto.subtle.digest('SHA-256', msg);
        return Array.from(new Uint8Array(hash)).map(b => b.toString(16).padStart(2, '0')).join('');
    }

    // --- OTURUM (F5) YÖNETİMİ ---
    function checkSession() {
        const user = localStorage.getItem('username');
        const role = localStorage.getItem('role');
        const gameData = localStorage.getItem('currentQuiz');
        
        if (user) {
            // Kullanıcı giriş yapmış
            document.getElementById('auth-ui').classList.add('hidden');
            document.getElementById('user-display').innerText = user + " (" + role + ")";
            
            if (role === 'admin') {
                document.getElementById('admin-panel').classList.remove('hidden');
                searchQuizzes();
            }

            // Devam eden oyun var mı?
            if (gameData) {
                document.getElementById('dashboard-ui').classList.add('hidden');
                document.getElementById('game-ui').classList.remove('hidden');
                renderQuestion();
            } else {
                document.getElementById('dashboard-ui').classList.remove('hidden');
            }
        }
    }

    // --- AUTH (GİRİŞ/KAYIT) İŞLEMLERİ ---
    async function runAuth(type) {
        const u = document.getElementById('user').value.toLowerCase().trim();
        const p_raw = document.getElementById('pass').value;
        if(!u || !p_raw) return;

        const p_hash = await hashInBrowser(p_raw); // Tarayıcıda şifrele
        
        const res = await fetch('/api/auth', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({user: u, hash: p_hash, action: type})
        });
        const data = await res.json();

        const msgBox = document.getElementById('auth-msg');
        if (data.success) {
            if (type === 'login') {
                localStorage.setItem('username', u);
                localStorage.setItem('role', data.role);
                checkSession(); // Sistemi yenile
            } else {
                msgBox.innerText = data.msg; msgBox.style.color = "green";
            }
        } else {
            msgBox.innerText = data.msg; msgBox.style.color = "red";
        }
    }

    function logout() {
        localStorage.clear();
        location.reload();
    }

    // --- ADMİN İŞLEMLERİ ---
    async function createQuiz() {
        const title = document.getElementById('q-title').value;
        const res = await fetch('/api/admin/create', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({title})});
        alert("Quiz ID: " + (await res.json()).id);
        searchQuizzes();
    }

    async function addQuestion() {
        const payload = {
            id: document.getElementById('target-id').value,
            q: document.getElementById('s-q').value,
            options: [
                document.getElementById('s-0').value, document.getElementById('s-1').value,
                document.getElementById('s-2').value, document.getElementById('s-3').value
            ],
            correct: parseInt(document.getElementById('s-correct').value)
        };
        await fetch('/api/admin/add_question', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(payload)});
        alert("4 Şıklı Soru Eklendi!");
    }

    async function searchQuizzes() {
        const q = document.getElementById('search-input').value;
        const res = await fetch(`/api/search?q=${q}`);
        const data = await res.json();
        const list = document.getElementById('quiz-list');
        list.innerHTML = "";
        data.forEach(item => {
            list.innerHTML += `<div class="quiz-item">
                <strong>${item.title}</strong> (ID: ${item.id}) - ${item.questions.length} Soru
                <button onclick="deleteQuiz('${item.id}')" class="btn-danger" style="padding:5px; font-size:12px; float:right; width:auto;">Sil</button>
            </div>`;
        });
    }

    async function deleteQuiz(id) {
        if(!confirm('Kalıcı olarak silinecek?')) return;
        await fetch('/api/admin/delete', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({id})});
        searchQuizzes();
    }

    // --- OYUN İŞLEMLERİ ---
    async function joinGame() {
        const pin = document.getElementById('join-pin').value;
        const res = await fetch(`/api/quiz/${pin}`);
        if (res.status === 404) return alert("Hatalı PIN!");
        
        const quizData = await res.json();
        if(quizData.questions.length === 0) return alert("Bu quizde soru yok!");

        localStorage.setItem('currentQuiz', JSON.stringify(quizData));
        localStorage.setItem('qIndex', 0);
        checkSession();
    }

    function renderQuestion() {
        const quiz = JSON.parse(localStorage.getItem('currentQuiz'));
        const index = parseInt(localStorage.getItem('qIndex'));

        if (index >= quiz.questions.length) {
            document.getElementById('question-text').innerHTML = "Oyun Bitti! 🎉";
            document.querySelector('.ans-grid').classList.add('hidden');
            return;
        }

        const q = quiz.questions[index];
        document.getElementById('game-title').innerText = quiz.title + " (" + (index+1) + "/" + quiz.questions.length + ")";
        document.getElementById('question-text').innerText = q.question;
        
        document.getElementById('opt0').innerText = q.options[0] || "";
        document.getElementById('opt1').innerText = q.options[1] || "";
        document.getElementById('opt2').innerText = q.options[2] || "";
        document.getElementById('opt3').innerText = q.options[3] || "";
        document.getElementById('feedback').innerText = "";
    }

    function submitAnswer(choice) {
        const quiz = JSON.parse(localStorage.getItem('currentQuiz'));
        let index = parseInt(localStorage.getItem('qIndex'));
        const correct = quiz.questions[index].correct;
        
        const feedback = document.getElementById('feedback');
        if (choice === correct) {
            feedback.innerText = "DOĞRU! ✔️"; feedback.style.color = "green";
        } else {
            feedback.innerText = "YANLIŞ! ❌"; feedback.style.color = "red";
        }

        // Bir sonraki soruya geç (F5 koruması için LocalStorage güncelle)
        setTimeout(() => {
            localStorage.setItem('qIndex', index + 1);
            renderQuestion();
        }, 1500);
    }

    function exitGame() {
        localStorage.removeItem('currentQuiz');
        localStorage.removeItem('qIndex');
        location.reload();
    }
</script>
</body>
</html>
'''

# --- BACKEND API ENDPOINTLERİ ---

@app.route('/')
def home(): 
    return render_template_string(HTML_TEMPLATE)

@app.route('/api/auth', methods=['POST'])
def auth():
    d = request.json
    u, b_hash, action = d['user'], d['hash'], d['action']
    f_hash = secure_hash(b_hash, u)
    users = get_users()

    if action == 'register':
        if u in users: return jsonify({"success": False, "msg": "Kullanıcı adı alınmış!"})
        # Geliştirme kolaylığı için 'admin' yazan kişi admin olur, diğerleri user.
        role = "admin" if u == "admin" else "user"
        users[u] = {"password": f_hash, "role": role}
        save_users(users)
        return jsonify({"success": True, "msg": "Kayıt Başarılı!"})

    else: # Login
        if u in users and users[u]['password'] == f_hash:
            return jsonify({"success": True, "role": users[u]['role']})
        return jsonify({"success": False, "msg": "Hatalı Giriş!"})

@app.route('/api/admin/create', methods=['POST'])
def create_q():
    q_id = str(uuid.uuid4())[:6]
    data = {"id": q_id, "title": request.json['title'], "questions": []}
    with open(os.path.join(QUIZ_DIR, f"{q_id}.json"), "w") as f:
        json.dump(data, f, indent=4)
    return jsonify({"success": True, "id": q_id})

@app.route('/api/admin/add_question', methods=['POST'])
def add_q():
    q_id = request.json['id']
    path = os.path.join(QUIZ_DIR, f"{q_id}.json")
    if os.path.exists(path):
        with open(path, "r+") as f:
            data = json.load(f)
            data['questions'].append({
                "question": request.json['q'],
                "options": request.json['options'], # Artık 4 şık geliyor
                "correct": request.json['correct']
            })
            f.seek(0); json.dump(data, f, indent=4); f.truncate()
    return jsonify({"success": True})

@app.route('/api/admin/delete', methods=['POST'])
def del_q():
    q_id = request.json['id']
    path = os.path.join(QUIZ_DIR, f"{q_id}.json")
    if os.path.exists(path): os.remove(path)
    return jsonify({"success": True})

@app.route('/api/search')
def search():
    query = request.args.get('q', '').lower()
    results = []
    for fn in os.listdir(QUIZ_DIR):
        with open(os.path.join(QUIZ_DIR, fn), "r") as f:
            d = json.load(f)
            if query in d['title'].lower() or any(query in q['question'].lower() for q in d['questions']):
                results.append(d)
    return jsonify(results)

@app.route('/api/quiz/<pin>')
def get_quiz(pin):
    path = os.path.join(QUIZ_DIR, f"{pin}.json")
    if not os.path.exists(path): abort(404)
    with open(path, "r") as f: return jsonify(json.load(f))

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
