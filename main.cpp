#include "crow.h"
#include "src/database.h"
#include "src/models/Alumno.h"
#include "src/models/Coordinacion.h"
#include "src/models/Tutor.h"
#include "src/models/Usuario.h"

int main() {
  Database db("app.db");
  if (!db.init()) {
    return 1;
  }

  crow::SimpleApp app;

  // --- Static Files ---
  CROW_ROUTE(app, "/css/<string>")
  ([](const std::string &filename) {
    crow::response res;
    res.set_static_file_info("static/css/" + filename);
    return res;
  });

  // --- Login (Route to Page) ---
  CROW_ROUTE(app, "/")
  ([]() {
    auto page = crow::mustache::load("login.html");
    return page.render();
  });

  // --- API: Login ---
  CROW_ROUTE(app, "/api/login")
      .methods(crow::HTTPMethod::POST)([&db](const crow::request &req) {
        auto x = crow::json::load(req.body);
        if (!x)
          return crow::response(400);

        std::string email = x["email"].s();
        std::string pswd = x["password"].s();

        auto user = db.autenticarUsuario(email, pswd);
        if (user) {
          crow::json::wvalue res;
          res["success"] = true;
          res["id"] = user->getId();
          res["role"] = user->getRol();
          res["redirect"] = "/" + ((user->getRol() == "Coordinacion")
                                       ? std::string("coordinator")
                                       : (user->getRol() == "Tutor"
                                              ? std::string("tutor")
                                              : std::string("student")));
          return crow::response(res);
        } else {
          crow::json::wvalue res;
          res["success"] = false;
          return crow::response(401, res); // Unauthorized
        }
      });

  // --- Dashboards ---
  CROW_ROUTE(app, "/coordinator")
  ([]() {
    auto page = crow::mustache::load("coordinator.html");
    return page.render();
  });

  CROW_ROUTE(app, "/tutor")
  ([]() {
    auto page = crow::mustache::load("tutor.html");
    return page.render();
  });

  CROW_ROUTE(app, "/student")
  ([]() {
    auto page = crow::mustache::load("student.html");
    return page.render();
  });

  // --- API: Coordinator (HU1.1, HU1.2, HU2.1, HU2.2, HU3.3) ---
  // List Users
  CROW_ROUTE(app, "/api/users")
  ([&db](const crow::request &req) {
    crow::json::wvalue x;

    auto alums = db.getAllAlumnos();
    auto tutors = db.getAllTutors();

    int i = 0;
    for (auto &a : alums) {
      x["alumnos"][i]["id"] = a.getId();
      x["alumnos"][i]["nombre"] = a.getNombre() + " " + a.getApellidos();
      x["alumnos"][i]["carrera"] = a.getCarrera();
      x["alumnos"][i]["tutorId"] = a.getTutor();
      i++;
    }

    i = 0;
    for (auto &t : tutors) {
      x["tutors"][i]["id"] = t.getId();
      x["tutors"][i]["nombre"] = t.getNombre() + " " + t.getApellidos();
      x["tutors"][i]["disponible"] = t.isDisponible();
      i++;
    }
    return x;
  });

  // Add User (HU1.1, HU1.2)
  CROW_ROUTE(app, "/api/users/add")
      .methods(crow::HTTPMethod::POST)([&db](const crow::request &req) {
        auto x = crow::json::load(req.body);
        if (!x)
          return crow::response(400);

        std::string role = x["role"].s();
        std::string nom = x["nombre"].s();
        std::string ap = x["apellidos"].s();
        std::string email = x["email"].s();
        std::string pwd = x["password"].s();

        if (role == "Alumno") {
          std::string carr = x["carrera"].s();
          bool ok = db.registrarAlumno(nom, ap, email, pwd, carr);
          return crow::response(ok ? 200 : 500);
        } else if (role == "Tutor") {
          bool ok = db.registrarTutor(nom, ap, email, pwd);
          return crow::response(ok ? 200 : 500);
        }
        return crow::response(400);
      });

  // Assign Tutor (HU2.1 Auto, HU2.2 Manual)
  CROW_ROUTE(app, "/api/assign")
      .methods(crow::HTTPMethod::POST)([&db](const crow::request &req) {
        auto x = crow::json::load(req.body);
        if (!x)
          return crow::response(400);

        if (x.has("auto") && x["auto"].b()) {
          db.autoAsignarTutores();
          return crow::response(200);
        } else {
          int sid = x["studentId"].i();
          int tid = x["tutorId"].i();
          bool ok = db.asignarTutor(sid, tid);
          return crow::response(ok ? 200 : 500);
        }
      });

  // View All Chats (HU3.3)
  CROW_ROUTE(app, "/api/chats/all")
  ([&db]() {
    auto history = db.getAllChats();
    crow::json::wvalue x;
    int i = 0;
    for (auto &m : history) {
      x["msgs"][i]["from"] = m.remitenteId;
      x["msgs"][i]["to"] = m.destinatarioId;
      x["msgs"][i]["content"] = m.contenido;
      x["msgs"][i]["date"] = m.fecha;
      i++;
    }
    return crow::response(x);
  });

  // --- API: Tutor (HU1.3, HU2.3, HU3.2) ---
  CROW_ROUTE(app, "/api/tutor/<int>/students")
  ([&db](int id) {
    auto students = db.getAlumnosByTutor(id);
    crow::json::wvalue x;
    int i = 0;
    for (auto &s : students) {
      x["students"][i]["id"] = s.getId();
      x["students"][i]["nombre"] = s.getNombre() + " " + s.getApellidos();
      x["students"][i]["carrera"] = s.getCarrera();
      i++;
    }
    return crow::response(x);
  });

  // --- API: Chat (HU3.1, HU3.2) Shared ---
  // Send
  CROW_ROUTE(app, "/api/chat/send")
      .methods(crow::HTTPMethod::POST)([&db](const crow::request &req) {
        auto x = crow::json::load(req.body);
        if (!x)
          return crow::response(400);

        int from = x["from"].i();
        int to = x["to"].i();
        std::string msg = x["content"].s();

        bool ok = db.guardarMensaje(from, to, msg);
        return crow::response(ok ? 200 : 500);
      });

  // Get History
  CROW_ROUTE(app, "/api/chat/history")
  ([&db](const crow::request &req) {
    char *fromStr = req.url_params.get("u1");
    char *toStr = req.url_params.get("u2");

    if (!fromStr || !toStr)
      return crow::response(400);

    int u1 = std::stoi(fromStr);
    int u2 = std::stoi(toStr);

    auto history = db.getChatHistory(u1, u2);
    crow::json::wvalue x;
    int i = 0;
    for (auto &m : history) {
      x["msgs"][i]["id"] = m.id;
      x["msgs"][i]["from"] = m.remitenteId;
      x["msgs"][i]["content"] = m.contenido;
      x["msgs"][i]["date"] = m.fecha;
      i++;
    }
    return crow::response(x);
  });

  // --- API: Delete Operations ---
  CROW_ROUTE(app, "/api/users/delete")
      .methods(crow::HTTPMethod::POST)([&db](const crow::request &req) {
        auto x = crow::json::load(req.body);
        if (!x)
          return crow::response(400);
        int id = x["id"].i();
        bool ok = db.deleteUser(id);
        return crow::response(ok ? 200 : 500);
      });

  CROW_ROUTE(app, "/api/chat/delete")
      .methods(crow::HTTPMethod::POST)([&db](const crow::request &req) {
        auto x = crow::json::load(req.body);
        if (!x) {
          return crow::response(400);
        }
        int id = x["id"].i();
        bool ok = db.deleteMessage(id);
        return crow::response(ok ? 200 : 500);
      });

  char *portStr = std::getenv("PORT");
  int port = 18080;
  if (portStr) {
    port = std::stoi(portStr);
  }
  app.port(port).multithreaded().run();
}
