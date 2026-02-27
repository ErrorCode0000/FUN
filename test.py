import json
import os
import hashlib
import uuid
from flask import Flask, render_template_string, request, jsonify, session, abort

app = Flask(__name__)
app.secret_key = os.urandom(32)

DB_DIR = "database"
QUIZ_DIR = os.path.join(DB_DIR, "quizzes")
if not os.path.exists(QUIZ_DIR): os.makedirs(QUIZ_DIR)

# --- ARAYÜZ (Gelişmiş Oyun Ekranı) ---
HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <title>Kahoot Pro - Play</title>
    <style>
        :root { --p: #46178f; --s: #eb670f; --r: #e21b3c; --b: #1368ce; --g: #26890c; --y: #ffa602; }
        body { font-family: 'Segoe UI', sans-serif; background: #f2f2f2; margin: 0; text-align: center; }
        .card { background: white; padding: 30px; border-radius: 15px; width: 400px; margin: 50px auto; box-shadow: 0 10px 30px rgba(0,0,0,0.1); }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 20px; }
        .ans-btn { padding: 30px; color: white; border: none; border-radius: 8px; font-size: 18px; cursor: pointer; transition: 0.2s; }
        .ans-btn:hover { transform: scale(1.02); opacity: 0.9; }
        .btn-0 { background: var(--r); } .btn-1 { background: var(--b); }
        .btn-2 { background: var(--y); } .btn-3 { background: var(--g); }
        .hidden { display: none; }
        input { width: 80%; padding: 12px; margin: 10px 0; border: 2px solid #ddd; border-radius: 8px; }
        .header { background: var(--p); color: white; padding: 20px; margin-bottom: 20px; }
    </style>
</head>
<body>

<div id="lobby">
    <div class="header"><h1>Kahoot Pro</h1></div>
    <div class="card">
        <h3>PIN Koduyla Katıl</h3>
        <input type="text" id="join-pin" placeholder="Quiz ID (Örn: a1b2c3)">
        <button onclick="joinGame()" style="padding:12px 25px; background:var(--g); color:white; border:none; border-radius:8px; cursor:pointer;">Giriş Yap</button>
        <hr>
        <button onclick="toggleAdmin()" style="background:none; border:none; color: #888; cursor:pointer; font-size:12px;">Yönetici Paneli</button>
    </div>
</div>

<div id="game-screen" class="hidden">
    <div class="header"><h2 id="q-title-display">Soru</h2></div>
    <div class="container" style="max-width:800px; margin:auto; padding:20px;">
        <h1 id="question-text">Soru metni buraya gelecek...</h1>
        <div class="grid" id="options-grid">
            <button class="ans-btn btn-0" onclick="checkAnswer(0)" id="opt0"></button>
            <button class="ans-btn btn-1" onclick="checkAnswer(1)" id="opt1"></button>
            <button class="ans-btn btn-2" onclick="checkAnswer(2)" id="opt2"></button>
            <button class="ans-btn btn-3" onclick="checkAnswer(3)" id="opt3"></button>
        </div>
        <h2 id="feedback" style="margin-top:20px;"></h2>
    </div>
</div>

<div id="admin-ui" class="hidden">
    <div class="card" style="width:600px;">
        <h2>Yeni Quiz & Soru Ekle</h2>
        <input type="text" id="new-q-title" placeholder="Quiz Başlığı">
        <button onclick="createQuiz()">Quiz JSON Oluştur</button>
        <hr>
        <input type="text" id="target-id" placeholder="Quiz ID">
        <input type="text" id="s-soru" placeholder="Soru">
        <input type="text" id="s-0" placeholder="A Şıkkı">
        <input type="text" id="s-1" placeholder="B Şıkkı">
        <input type="text" id="s-2" placeholder="C Şıkkı">
        <input type="text" id="s-3" placeholder="D Şıkkı">
        <select id="s-dogru" style="width:85%; padding:10px; margin:10px 0;">
            <option value="0">Doğru: A</option><option value="1">Doğru: B</option>
            <option value="2">Doğru: C</option><option value="3">Doğru: D</option>
        </select>
        <button onclick="addQuestion()" style="background:var(--p); color:white;">Soruyu Kaydet</button>
        <button onclick="toggleAdmin()" style="background:#555; color:white;">Geri Dön</button>
    </div>
</div>

<script>
    let currentQuiz = null;
    let qIndex = 0;

    function toggleAdmin() {
        document.getElementById('lobby').classList.toggle('hidden');
        document.getElementById('admin-ui').classList.toggle('hidden');
    }

    async function createQuiz() {
        const title = document.getElementById('new-q-title').value;
        const res = await fetch('/api/admin/create', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({title})
        });
        const data = await res.json();
        alert("Quiz Oluşturuldu! ID: " + data.id);
    }

    async function addQuestion() {
        const payload = {
            id: document.getElementById('target-id').value,
            q: document.getElementById('s-soru').value,
            options: [
                document.getElementById('s-0').value,
                document.getElementById('s-1').value,
                document.getElementById('s-2').value,
                document.getElementById('s-3').value
            ],
            correct: parseInt(document.getElementById('s-dogru').value)
        };
        await fetch('/api/admin/add_question', {
            method: 'POST', headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(payload)
        });
        alert("Soru eklendi!");
    }

    async function joinGame() {
        const pin = document.getElementById('join-pin').value;
        const res = await fetch(`/api/quiz/${pin}`);
        if (res.status === 404) return alert("Hatalı PIN!");
        
        currentQuiz = await res.json();
        qIndex = 0;
        showQuestion();
        document.getElementById('lobby').classList.add('hidden');
        document.getElementById('game-screen').classList.remove('hidden');
    }

    function showQuestion() {
        if (qIndex >= currentQuiz.questions.length) {
            document.getElementById('game-screen').innerHTML = "<h1>Oyun Bitti! Tebrikler.</h1><button onclick='location.reload()'>Başa Dön</button>";
            return;
        }
        const q = currentQuiz.questions[qIndex];
        document.getElementById('q-title-display').innerText = currentQuiz.title + " (" + (qIndex+1) + "/" + currentQuiz.questions.length + ")";
        document.getElementById('question-text').innerText = q.question;
        document.getElementById('opt0').innerText = q.options[0];
        document.getElementById('opt1').innerText = q.options[1];
        document.getElementById('opt2').innerText = q.options[2];
        document.getElementById('opt3').innerText = q.options[3];
        document.getElementById('feedback').innerText = "";
    }

    function checkAnswer(choice) {
        const correct = currentQuiz.questions[qIndex].correct;
        const feedback = document.getElementById('feedback');
        if (choice === correct) {
            feedback.innerText = "DOĞRU! 🎉";
            feedback.style.color = "green";
        } else {
            feedback.innerText = "YANLIŞ! ❌";
            feedback.style.color = "red";
        }
        setTimeout(() => {
            qIndex++;
            showQuestion();
        }, 1500);
    }
</script>
</body>
</html>
'''

# --- BACKEND (JSON Veri Yönetimi) ---

@app.route('/')
def home(): return render_template_string(HTML_TEMPLATE)

@app.route('/api/admin/create', methods=['POST'])
def create():
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
                "options": request.json['options'],
                "correct": request.json['correct']
            })
            f.seek(0); json.dump(data, f, indent=4); f.truncate()
    return jsonify({"success": True})

@app.route('/api/quiz/<pin>')
def get_quiz(pin):
    path = os.path.join(QUIZ_DIR, f"{pin}.json")
    if not os.path.exists(path): abort(404)
    with open(path, "r") as f:
        return jsonify(json.load(f))

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
