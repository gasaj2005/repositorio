#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <memory>
// Forward declarations
struct sqlite3;
class Usuario;
class Alumno;
class Tutor;

struct MensajeChat {
    int id;
    int remitenteId;
    int destinatarioId;
    std::string contenido;
    std::string fecha;
};

class Database {
private:
    sqlite3* db;
    std::string dbPath;

public:
    Database(const std::string& path);
    ~Database();

    bool init();

    // User Management
    bool registrarAlumno(const std::string& nombre, const std::string& apellidos, const std::string& correo, const std::string& pass, const std::string& carrera);
    bool registrarTutor(const std::string& nombre, const std::string& apellidos, const std::string& correo, const std::string& pass);
    std::shared_ptr<Usuario> autenticarUsuario(const std::string& correo, const std::string& pass);
    
    // Getters
    std::vector<Alumno> getAllAlumnos();
    std::vector<Tutor> getAllTutors();
    std::vector<Alumno> getAlumnosByTutor(int tutorId);

    // Assignment
    bool asignarTutor(int alumnoId, int tutorId);
    void autoAsignarTutores(); // HU2.1

    // Chat
    bool guardarMensaje(int remitenteId, int destinatarioId, const std::string& contenido);
    std::vector<MensajeChat> getChatHistory(int userId1, int userId2);
    std::vector<MensajeChat> getAllChats(); // For Coordinator

};

#endif // DATABASE_H
