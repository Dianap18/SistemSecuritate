const express = require("express");
const sqlite3 = require("sqlite3").verbose();// driver pentru baza de date
const cors = require("cors"); // Ce face: Este un paznic de securitate. În mod normal, browserul blochează cererile venite de pe 
// alt domeniu/port din motive de securitate. cors() le permite tuturor să vorbească cu serverul tău.
const bodyParser = require("body-parser"); //Când ESP32 sau pagina Web trimite date (de exemplu: { "pin": "1234" }), ele vin prin 
// "fir" ca un text brut. body-parser ia textul ăla și îl transformă într-un obiect JavaScript frumos pe care îl poți folosi cu 
// req.body.pin.
const path = require("path"); //creaaza calea catre fisiere 

const app = express();
//Ce este: Aceasta este "piesa de rezistență". Express este framework-ul care transformă Node.js într-un server web. Ce face: Fără 
// el, ar fi extrem de greu să asculți cereri HTTP (GET, POST). El se ocupă de "rute" (adresele alea gen /generate-otp sau /users).
const PORT = 3000;

app.use(cors());
app.use(bodyParser.json());

// se face conexiunea la baza de date 
const db = new sqlite3.Database("./database.db", (err) => {
  if (err) console.error("Eroare DB:", err.message);
  else console.log("DB OK");
});

// se face baza de date daca nu exista deja
db.run(`
CREATE TABLE IF NOT EXISTS users (
  user_id INTEGER PRIMARY KEY AUTOINCREMENT,
  pin TEXT,
  fingerprint_id INTEGER,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP
)
`);

let currentOtp = null;
// o variabilă, dar atenție, o voi modifica pe parcurs

// leaga serverul de pagin html
app.get("/", (req, res) => {
  //res (Response): Răspunsul pe care serverul i-l dă vizitatorului.
  res.sendFile(path.join(__dirname, "index.html"));
  //__dirname: Este o variabilă magică din Node.js care înseamnă "Folderul în care mă aflu chiar acum". (De exemplu: 
  // C:\Users\Student\ProiectSecuritate). "index.html": Numele fișierului pe care îl cauți. path.join(...): Ea lipește cele două 
  // bucăți (C:\...\Proiect + index.html).
});

// Generare OTP (Admin)
app.post("/generate-otp", (req, res) => {
  //Ce face: Deschide un "ghișeu" la adresa /generate-otp care acceptă doar cereri de tip POST. De ce POST: Pentru că această 
  // acțiune creează ceva nou (un cod nou), nu doar citește date existente.
  const otp = Math.floor(Math.random() * 10000).toString().padStart(4, "0");
  //Math.random() * 10000: Generează un număr între 0 și 9999 (ex: poate ieși 5). .toString(): Îl face text ("5"). .padStart(4, "0"): 
  // Asta e magia. Îi zice calculatorului: "Asigură-te că textul are fix 4 caractere. Dacă e mai scurt, completează cu cifra 0 la început".
  currentOtp = otp; 
  
  console.log("OTP Generat:", otp);
  res.json({ success: true, otp });
});

// Listare Useri
app.get("/users", (req, res) => {
  //Ce face: Deschide o cale (endpoint) pentru cereri de tip GET. De ce GET: Pentru că doar "citim" sau "cerem" date, nu modificăm 
  // nimic în server.
  db.all("SELECT * FROM users ORDER BY user_id DESC", [], (err, rows) => {
    // all pentru ca vrem sa luam tot 
    // [] - nu cautam un user anume dupa ceva
    if (err) 
      return res.status(500).json({ error: err.message });
    res.json(rows);
    //Livrarea: Dacă totul e OK, luăm lista (rows) și o trimitem browserului.
  });
});

// Verificare OTP (ESP32)
app.post("/verify-otp", (req, res) => {
  // Traducere: "Caută în pachetul primit (req.body) o proprietate numită otp, scoate valoarea din ea și pune-o într-o variabilă 
  // locală cu același nume."
  const { otp } = req.body;

  if (!currentOtp) {
    return res.json({ success: false, message: "Nu există un OTP activ." });
  }

  // identic caracter cu caracter
  if (otp === currentOtp) {
    currentOtp = null; // dupa ce e folosit il facem inactiv
    console.log("OTP Corect!");
    res.json({ success: true });
  } else {
    console.log("OTP Greșit:", otp);
    res.json({ success: false, message: "Cod incorect" });
  }
});

// Setare PIN
app.post("/set-pin", (req, res) => {
  const { pin } = req.body;
  if (!pin || pin.length !== 4) return res.status(400).json({ success: false, message: "PIN invalid" });

  db.get("SELECT user_id FROM users WHERE pin=?", [pin], (err, row) => {
    if (row) return res.json({ success: false, message: "Nu se accepta acest PIN" });
    // daca la setare pin mai exista unul deja, atunci sa apara un mesaj 

    db.run("INSERT INTO users(pin) VALUES(?)", [pin], function (err) {
      // Acesta se numește Placeholder sau Interogare Parametrizată. De ce e acolo: Este o măsură de securitate împotriva SQL Injection
      if (err) return res.status(500).json({ success: false });
      res.json({ success: true, user_id: this.lastID });
      // avem nevoie de id pentru a sti la ce user sa ii asocieze amprenta pe care o inregistreaza
    });
  });
});

// Amprentă
app.post("/enroll-fingerprint", (req, res) => {
  const { user_id, fingerprint_id } = req.body;
  // finger id unde s-a salvat amprenta asta in memoria senzorului
  if (!user_id || fingerprint_id === undefined) return res.status(400).json({ success: false });

  db.run("UPDATE users SET fingerprint_id=? WHERE user_id=?", [fingerprint_id, user_id], function (err) {
    if (err) return res.status(500).json({ success: false });
    res.json({ success: true });
  });
});

// Auth
app.post("/auth", (req, res) => {
  const { pin, fingerprint_id } = req.body;
  
  if (pin) {
    db.get("SELECT user_id FROM users WHERE pin=?", [pin], (err, row) => {
      if (row) res.json({ success: true, user_id: row.user_id });
      else res.json({ success: false });
    });
    return;
  }
  if (fingerprint_id !== undefined) {
    db.get("SELECT user_id FROM users WHERE fingerprint_id=?", [fingerprint_id], (err, row) => {
      if (row) res.json({ success: true, user_id: row.user_id });
      else res.json({ success: false });
    });
    return;
  }
  res.json({ success: false });
});

// pornirea serverului
app.listen(PORT, () => {
  console.log(`Server on port ${PORT}`);
});