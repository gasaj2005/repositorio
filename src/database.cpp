#include "database.h"
#include "models/Alumno.h"
#include "models/Coordinacion.h"
#include "models/Tutor.h"
#include "models/Usuario.h"
#include "sqlite3.h"
#include <algorithm>
#include <iostream>

// Helper for SQL execution (SQLite)
static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
  return 0;
}

Database::Database(const std::string &path) : dbPath(path), db(nullptr) {
#ifdef USE_POSTGRES
  pg_conn = nullptr;
#endif
}

Database::~Database() {
  if (db) {
    sqlite3_close(db);
  }
  // pg_conn is unique_ptr, auto cleanup
}

bool Database::init() {
  std::cout << "DEBUG: Database::init() called" << std::endl;
#ifdef USE_POSTGRES
  std::cout << "DEBUG: Compiled with USE_POSTGRES" << std::endl;
  std::cout << "DEBUG: Checking Postgres..." << std::endl;
  const char *dbUrl = std::getenv("DATABASE_URL");
  std::cout << "DEBUG: DATABASE_URL: " << (dbUrl ? dbUrl : "NULL") << std::endl;
  if (dbUrl) {
    std::cout << "DEBUG: Found DATABASE_URL, attempting connection..."
              << std::endl;
    try {
      pg_conn = std::make_unique<pqxx::connection>(dbUrl);
      if (pg_conn->is_open()) {
        std::cout << "Connected to PostgreSQL!" << std::endl;

        // Create Tables (PostgreSQL Syntax)
        pqxx::work W(*pg_conn);

        W.exec0("CREATE TABLE IF NOT EXISTS Users ("
                "ID SERIAL PRIMARY KEY,"
                "Nombre TEXT NOT NULL,"
                "Apellidos TEXT NOT NULL,"
                "Correo TEXT NOT NULL UNIQUE,"
                "Contrasena TEXT NOT NULL,"
                "Rol TEXT NOT NULL,"
                "Carrera TEXT,"
                "Disponible INTEGER DEFAULT 1"
                ");");

        W.exec0("CREATE TABLE IF NOT EXISTS Assignments ("
                "TutorID INTEGER,"
                "StudentID INTEGER,"
                "PRIMARY KEY (TutorID, StudentID),"
                "FOREIGN KEY(TutorID) REFERENCES Users(ID),"
                "FOREIGN KEY(StudentID) REFERENCES Users(ID)"
                ");");

        W.exec0("CREATE TABLE IF NOT EXISTS Messages ("
                "ID SERIAL PRIMARY KEY,"
                "RemitenteID INTEGER,"
                "DestinatarioID INTEGER,"
                "Contenido TEXT,"
                "Fecha TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                ");");

        // Seed Coordinator
        pqxx::result r =
            W.exec("SELECT COUNT(*) FROM Users WHERE Rol='Coordinacion';");
        if (r[0][0].as<int>() == 0) {
          W.exec0("INSERT INTO Users (Nombre, Apellidos, Correo, Contrasena, "
                  "Rol) VALUES ('Super', 'Admin', 'coord@uco.es', 'admin', "
                  "'Coordinacion');");
        }

        W.commit();
        return true;
      }
    } catch (const std::exception &e) {
      std::cerr << "PostgreSQL Error: " << e.what() << std::endl;
      // Fallback (or fail? Let's fail if PG was requested but failed)
      return false;
    }
  } else {
    std::cout << "DEBUG: DATABASE_URL environment variable is NOT set or NULL."
              << std::endl;
  }
  // Fallthrough to SQLite if no ENV var
#else
  std::cout << "DEBUG: Compiled WITHOUT USE_POSTGRES" << std::endl;
#endif

  std::cout << "DEBUG: Falling back to SQLite..." << std::endl;
  int rc = sqlite3_open(dbPath.c_str(), &db);
  if (rc) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    return false;
  }

  // Create Tables
  const char *sql = "CREATE TABLE IF NOT EXISTS Users ("
                    "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "Nombre TEXT NOT NULL,"
                    "Apellidos TEXT NOT NULL,"
                    "Correo TEXT NOT NULL UNIQUE,"
                    "Contrasena TEXT NOT NULL,"
                    "Rol TEXT NOT NULL,"
                    "Carrera TEXT,"
                    "Disponible INTEGER DEFAULT 1" // 1 true, 0 false
                    ");"

                    "CREATE TABLE IF NOT EXISTS Assignments ("
                    "TutorID INTEGER,"
                    "StudentID INTEGER,"
                    "PRIMARY KEY (TutorID, StudentID),"
                    "FOREIGN KEY(TutorID) REFERENCES Users(ID),"
                    "FOREIGN KEY(StudentID) REFERENCES Users(ID)"
                    ");"

                    "CREATE TABLE IF NOT EXISTS Messages ("
                    "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "RemitenteID INTEGER,"
                    "DestinatarioID INTEGER,"
                    "Contenido TEXT,"
                    "Fecha DATETIME DEFAULT CURRENT_TIMESTAMP"
                    ");";

  char *zErrMsg = 0;
  rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
    return false;
  }

  // Seed Coordinator if empty
  std::string checkSql = "SELECT COUNT(*) FROM Users WHERE Rol='Coordinacion';";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, checkSql.c_str(), -1, &stmt, 0);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (sqlite3_column_int(stmt, 0) == 0) {
      // Add default coordinator
      sqlite3_finalize(stmt);
      std::string seedStr =
          "INSERT INTO Users (Nombre, Apellidos, Correo, Contrasena, Rol) "
          "VALUES ('Super', 'Admin', 'coord@uco.es', 'admin', 'Coordinacion');";
      sqlite3_exec(db, seedStr.c_str(), callback, 0, &zErrMsg);
    } else {
      sqlite3_finalize(stmt);
    }
  }

  return true;
}

bool Database::registrarAlumno(const std::string &nombre,
                               const std::string &apellidos,
                               const std::string &correo,
                               const std::string &pass,
                               const std::string &carrera) {
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      W.exec_params("INSERT INTO Users (Nombre, Apellidos, Correo, Contrasena, "
                    "Rol, Carrera) VALUES ($1, $2, $3, $4, 'Alumno', $5)",
                    nombre, apellidos, correo, pass, carrera);
      W.commit();
      return true;
    } catch (...) {
      return false;
    }
  }
#endif
  std::string sql = "INSERT INTO Users (Nombre, Apellidos, Correo, Contrasena, "
                    "Rol, Carrera) VALUES (?, ?, ?, ?, 'Alumno', ?);";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, nombre.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, apellidos.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, correo.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, pass.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 5, carrera.c_str(), -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool Database::registrarTutor(const std::string &nombre,
                              const std::string &apellidos,
                              const std::string &correo,
                              const std::string &pass) {
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      W.exec_params("INSERT INTO Users (Nombre, Apellidos, Correo, Contrasena, "
                    "Rol, Disponible) VALUES ($1, $2, $3, $4, 'Tutor', 1)",
                    nombre, apellidos, correo, pass);
      W.commit();
      return true;
    } catch (...) {
      return false;
    }
  }
#endif
  std::string sql =
      "INSERT INTO Users (Nombre, Apellidos, Correo, Contrasena, Rol, "
      "Disponible) VALUES (?, ?, ?, ?, 'Tutor', 1);"; // Default visible
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, nombre.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, apellidos.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, correo.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 4, pass.c_str(), -1, SQLITE_STATIC);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

std::shared_ptr<Usuario> Database::autenticarUsuario(const std::string &correo,
                                                     const std::string &pass) {
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      pqxx::result r = W.exec_params(
          "SELECT ID, Nombre, Apellidos, Rol, Carrera, Disponible FROM Users "
          "WHERE Correo = $1 AND Contrasena = $2",
          correo, pass);

      if (r.size() > 0) {
        int id = r[0][0].as<int>();
        std::string nom = r[0][1].c_str();
        std::string ap = r[0][2].c_str();
        std::string rol = r[0][3].c_str();

        if (rol == "Alumno") {
          std::string carrera = r[0][4].is_null() ? "" : r[0][4].c_str();
          return std::make_shared<Alumno>(id, nom, ap, correo, pass, carrera);
        } else if (rol == "Tutor") {
          bool disp = r[0][5].as<int>() != 0;
          return std::make_shared<Tutor>(id, nom, ap, correo, pass, disp);
        } else {
          return std::make_shared<Coordinacion>(id, nom, ap, correo, pass);
        }
      }
      return nullptr;
    } catch (...) {
      return nullptr;
    }
  }
#endif
  std::string sql = "SELECT ID, Nombre, Apellidos, Rol, Carrera, Disponible "
                    "FROM Users WHERE Correo = ? AND Contrasena = ?;";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  sqlite3_bind_text(stmt, 1, correo.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, pass.c_str(), -1, SQLITE_STATIC);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    std::string nom =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    std::string ap =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    std::string rol =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));

    std::shared_ptr<Usuario> user;
    if (rol == "Alumno") {
      const char *deg =
          reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
      std::string carrera = deg ? deg : "";
      user = std::make_shared<Alumno>(id, nom, ap, correo, pass, carrera);

      // Check for assigned tutor
      // For simplicity, do a quick separate query or modify this one.
      // Here, we just return the object, we can lazy load tutor if needed.
    } else if (rol == "Tutor") {
      bool disp = sqlite3_column_int(stmt, 5) != 0;
      user = std::make_shared<Tutor>(id, nom, ap, correo, pass, disp);
    } else {
      user = std::make_shared<Coordinacion>(id, nom, ap, correo, pass);
    }
    sqlite3_finalize(stmt);
    return user;
  }
  sqlite3_finalize(stmt);
  return nullptr;
}

std::vector<Alumno> Database::getAllAlumnos() {
  std::vector<Alumno> list;
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      pqxx::result r =
          W.exec("SELECT ID, Nombre, Apellidos, Correo, Contrasena, Carrera "
                 "FROM Users WHERE Rol='Alumno'");
      for (const auto &row : r) {
        int id = row[0].as<int>();
        std::string n = row[1].c_str();
        std::string a = row[2].c_str();
        std::string c = row[3].c_str();
        std::string p = row[4].c_str();
        std::string car = row[5].is_null() ? "" : row[5].c_str();

        Alumno alum(id, n, a, c, p, car);

        // Check assignment
        pqxx::result rs = W.exec_params(
            "SELECT TutorID FROM Assignments WHERE StudentID = $1", id);
        if (rs.size() > 0) {
          alum.setTutor(rs[0][0].as<int>());
        }
        list.push_back(alum);
      }
      return list;
    } catch (...) {
      return list;
    }
  }
#endif
  std::string sql = "SELECT ID, Nombre, Apellidos, Correo, Contrasena, Carrera "
                    "FROM Users WHERE Rol='Alumno';";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    std::string n = (const char *)sqlite3_column_text(stmt, 1);
    std::string a = (const char *)sqlite3_column_text(stmt, 2);
    std::string c = (const char *)sqlite3_column_text(stmt, 3);
    std::string p = (const char *)sqlite3_column_text(stmt, 4);
    std::string car = (const char *)sqlite3_column_text(stmt, 5);

    Alumno alum(id, n, a, c, p, car);

    // Check assignment
    sqlite3_stmt *subStmt;
    std::string subSql = "SELECT TutorID FROM Assignments WHERE StudentID = ?";
    sqlite3_prepare_v2(db, subSql.c_str(), -1, &subStmt, 0);
    sqlite3_bind_int(subStmt, 1, id);
    if (sqlite3_step(subStmt) == SQLITE_ROW) {
      alum.setTutor(sqlite3_column_int(subStmt, 0));
    }
    sqlite3_finalize(subStmt);

    list.push_back(alum);
  }
  sqlite3_finalize(stmt);
  return list;
}

std::vector<Tutor> Database::getAllTutors() {
  std::vector<Tutor> list;
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      pqxx::result r =
          W.exec("SELECT ID, Nombre, Apellidos, Correo, Contrasena, Disponible "
                 "FROM Users WHERE Rol='Tutor'");
      for (const auto &row : r) {
        int id = row[0].as<int>();
        std::string n = row[1].c_str();
        std::string a = row[2].c_str();
        std::string c = row[3].c_str();
        std::string p = row[4].c_str();
        bool d = row[5].as<int>() != 0;
        list.emplace_back(id, n, a, c, p, d);
      }
      return list;
    } catch (...) {
      return list;
    }
  }
#endif
  std::string sql = "SELECT ID, Nombre, Apellidos, Correo, Contrasena, "
                    "Disponible FROM Users WHERE Rol='Tutor';";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    std::string n = (const char *)sqlite3_column_text(stmt, 1);
    std::string a = (const char *)sqlite3_column_text(stmt, 2);
    std::string c = (const char *)sqlite3_column_text(stmt, 3);
    std::string p = (const char *)sqlite3_column_text(stmt, 4);
    bool d = sqlite3_column_int(stmt, 5);
    list.emplace_back(id, n, a, c, p, d);
  }
  sqlite3_finalize(stmt);
  return list;
}

bool Database::asignarTutor(int alumnoId, int tutorId) {
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      W.exec_params("DELETE FROM Assignments WHERE StudentID = $1", alumnoId);
      W.exec_params(
          "INSERT INTO Assignments (TutorID, StudentID) VALUES ($1, $2)",
          tutorId, alumnoId);
      W.commit();
      return true;
    } catch (...) {
      return false;
    }
  }
#endif
  // Check if assignment exists
  std::string sqlDel = "DELETE FROM Assignments WHERE StudentID = ?;";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sqlDel.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, alumnoId);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  std::string sqlIns =
      "INSERT INTO Assignments (TutorID, StudentID) VALUES (?, ?);";
  sqlite3_prepare_v2(db, sqlIns.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, tutorId);
  sqlite3_bind_int(stmt, 2, alumnoId);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

// Logic for HU2.1
void Database::autoAsignarTutores() {
  // Very simple round robin implementation for now, or based on availability
  std::vector<Tutor> tutors = getAllTutors();
  std::vector<Alumno> students = getAllAlumnos();

  // Filter available tutors
  std::vector<int> availTutors;
  for (const auto &t : tutors) {
    if (t.isDisponible())
      availTutors.push_back(t.getId());
  }

  if (availTutors.empty())
    return;

  // Distribute students who don't have a tutor
  int tIdx = 0;
  for (auto &s : students) {
    if (s.getTutor() == -1) {
      asignarTutor(s.getId(), availTutors[tIdx]);
      tIdx = (tIdx + 1) % availTutors.size();
    }
  }
}

std::vector<Alumno> Database::getAlumnosByTutor(int tutorId) {
  std::vector<Alumno> list;
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      pqxx::result r = W.exec_params("SELECT u.ID, u.Nombre, u.Apellidos, "
                                     "u.Correo, u.Contrasena, u.Carrera "
                                     "FROM Users u "
                                     "JOIN Assignments a ON u.ID = a.StudentID "
                                     "WHERE a.TutorID = $1",
                                     tutorId);

      for (const auto &row : r) {
        int id = row[0].as<int>();
        std::string n = row[1].c_str();
        std::string a = row[2].c_str();
        std::string c = row[3].c_str();
        std::string p = row[4].c_str();
        std::string car = row[5].is_null() ? "" : row[5].c_str();

        Alumno alum(id, n, a, c, p, car);
        alum.setTutor(tutorId);
        list.push_back(alum);
      }
      return list;
    } catch (...) {
      return list;
    }
  }
#endif
  std::string sql =
      "SELECT u.ID, u.Nombre, u.Apellidos, u.Correo, u.Contrasena, u.Carrera "
      "FROM Users u "
      "JOIN Assignments a ON u.ID = a.StudentID "
      "WHERE a.TutorID = ?;";

  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, tutorId);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    std::string n = (const char *)sqlite3_column_text(stmt, 1);
    std::string a = (const char *)sqlite3_column_text(stmt, 2);
    std::string c = (const char *)sqlite3_column_text(stmt, 3);
    std::string p = (const char *)sqlite3_column_text(stmt, 4);
    std::string car = (const char *)sqlite3_column_text(stmt, 5);

    Alumno alum(id, n, a, c, p, car);
    alum.setTutor(tutorId);
    list.push_back(alum);
  }
  sqlite3_finalize(stmt);
  return list;
}

bool Database::guardarMensaje(int remitenteId, int destinatarioId,
                              const std::string &contenido) {
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      W.exec_params("INSERT INTO Messages (RemitenteID, DestinatarioID, "
                    "Contenido) VALUES ($1, $2, $3)",
                    remitenteId, destinatarioId, contenido);
      W.commit();
      return true;
    } catch (...) {
      return false;
    }
  }
#endif
  std::string sql = "INSERT INTO Messages (RemitenteID, DestinatarioID, "
                    "Contenido) VALUES (?, ?, ?);";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, remitenteId);
  sqlite3_bind_int(stmt, 2, destinatarioId);
  sqlite3_bind_text(stmt, 3, contenido.c_str(), -1, SQLITE_STATIC);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

std::vector<MensajeChat> Database::getChatHistory(int userId1, int userId2) {
  std::vector<MensajeChat> msgs;
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      pqxx::result r =
          W.exec_params("SELECT ID, RemitenteID, DestinatarioID, Contenido, "
                        "Fecha FROM Messages "
                        "WHERE (RemitenteID = $1 AND DestinatarioID = $2) OR "
                        "(RemitenteID = $3 AND DestinatarioID = $4) "
                        "ORDER BY Fecha ASC",
                        userId1, userId2, userId2, userId1);

      for (const auto &row : r) {
        MensajeChat m;
        m.id = row[0].as<int>();
        m.remitenteId = row[1].as<int>();
        m.destinatarioId = row[2].as<int>();
        m.contenido = row[3].c_str();
        m.fecha = row[4].c_str(); // PG timestamp string is usable
        msgs.push_back(m);
      }
      return msgs;
    } catch (...) {
      return msgs;
    }
  }
#endif
  std::string sql =
      "SELECT ID, RemitenteID, DestinatarioID, Contenido, Fecha FROM Messages "
      "WHERE (RemitenteID = ? AND DestinatarioID = ?) OR (RemitenteID = ? AND "
      "DestinatarioID = ?) "
      "ORDER BY Fecha ASC;";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, userId1);
  sqlite3_bind_int(stmt, 2, userId2);
  sqlite3_bind_int(stmt, 3, userId2);
  sqlite3_bind_int(stmt, 4, userId1);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    MensajeChat m;
    m.id = sqlite3_column_int(stmt, 0);
    m.remitenteId = sqlite3_column_int(stmt, 1);
    m.destinatarioId = sqlite3_column_int(stmt, 2);
    m.contenido = (const char *)sqlite3_column_text(stmt, 3);
    m.fecha = (const char *)sqlite3_column_text(stmt, 4);
    msgs.push_back(m);
  }
  sqlite3_finalize(stmt);
  return msgs;
}

std::vector<MensajeChat> Database::getAllChats() {
  std::vector<MensajeChat> msgs;
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      pqxx::result r =
          W.exec("SELECT ID, RemitenteID, DestinatarioID, Contenido, Fecha "
                 "FROM Messages ORDER BY Fecha DESC");

      for (const auto &row : r) {
        MensajeChat m;
        m.id = row[0].as<int>();
        m.remitenteId = row[1].as<int>();
        m.destinatarioId = row[2].as<int>();
        m.contenido = row[3].c_str();
        m.fecha = row[4].c_str();
        msgs.push_back(m);
      }
      return msgs;
    } catch (...) {
      return msgs;
    }
  }
#endif
  std::string sql = "SELECT ID, RemitenteID, DestinatarioID, Contenido, Fecha "
                    "FROM Messages ORDER BY Fecha DESC;";
  sqlite3_stmt *stmt;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    MensajeChat m;
    m.id = sqlite3_column_int(stmt, 0);
    m.remitenteId = sqlite3_column_int(stmt, 1);
    m.destinatarioId = sqlite3_column_int(stmt, 2);
    m.contenido = (const char *)sqlite3_column_text(stmt, 3);
    m.fecha = (const char *)sqlite3_column_text(stmt, 4);
    msgs.push_back(m);
  }
  sqlite3_finalize(stmt);
  return msgs;
}

bool Database::deleteUser(int id) {
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      // Cascade delete manually just in case constraints aren't set to cascade
      W.exec_params(
          "DELETE FROM Assignments WHERE TutorID = $1 OR StudentID = $1", id);
      W.exec_params(
          "DELETE FROM Messages WHERE RemitenteID = $1 OR DestinatarioID = $1",
          id);
      W.exec_params("DELETE FROM Users WHERE ID = $1", id);
      W.commit();
      return true;
    } catch (...) {
      return false;
    }
  }
#endif
  // SQLite
  // Assignments
  sqlite3_stmt *stmt;
  std::string sql1 =
      "DELETE FROM Assignments WHERE TutorID = ? OR StudentID = ?;";
  sqlite3_prepare_v2(db, sql1.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, id);
  sqlite3_bind_int(stmt, 2, id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // Messages
  std::string sql2 =
      "DELETE FROM Messages WHERE RemitenteID = ? OR DestinatarioID = ?;";
  sqlite3_prepare_v2(db, sql2.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, id);
  sqlite3_bind_int(stmt, 2, id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  // User
  std::string sql3 = "DELETE FROM Users WHERE ID = ?;";
  sqlite3_prepare_v2(db, sql3.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return rc == SQLITE_DONE;
}

bool Database::deleteMessage(int id) {
#ifdef USE_POSTGRES
  if (pg_conn) {
    try {
      pqxx::work W(*pg_conn);
      W.exec_params("DELETE FROM Messages WHERE ID = $1", id);
      W.commit();
      return true;
    } catch (...) {
      return false;
    }
  }
#endif
  sqlite3_stmt *stmt;
  std::string sql = "DELETE FROM Messages WHERE ID = ?;";
  sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
  sqlite3_bind_int(stmt, 1, id);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}
